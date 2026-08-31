//! ## LSP Client
//!
//! Language Server Protocol client implementation.
//! Manages a child process (the language server) and communicates via
//! JSON-RPC over stdin/stdout with Content-Length framing.
//!
//! Replaces `LspClient` (QObject-based) from the original C++ codebase.
//! All protocol data crosses the C FFI boundary as JSON strings.

use std::ffi::c_void;
use std::io::{BufRead, BufReader, Read, Write};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::mpsc::{self, Sender, SyncSender};
use std::sync::{Arc, Mutex};
use std::thread;

use serde_json::{json, Value};

use crate::framer::LengthPrefixedFramer;

// ── Callback type aliases ──────────────────────────────────────────
type CbStrMsg = extern "C" fn(*const std::os::raw::c_char, *mut c_void);
type CbDiag = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);
type CbLspResult = extern "C" fn(i32, *const std::os::raw::c_char, *mut c_void);

pub struct LspClient {
    process: Option<Child>,
    write_tx: Option<SyncSender<Vec<u8>>>,
    writer_thread: Option<thread::JoinHandle<()>>,
    reader_thread: Option<thread::JoinHandle<()>>,
    shutdown_tx: Option<Sender<()>>,
    request_id: AtomicI32,
    initialized: bool,
    root_uri: String,
    framer: Arc<Mutex<LengthPrefixedFramer>>,

    // Callbacks
    on_server_started: Option<CbStrMsg>,
    on_server_started_data: *mut c_void,
    on_server_failed: Option<CbStrMsg>,
    on_server_failed_data: *mut c_void,
    on_diagnostics: Option<CbDiag>,
    on_diagnostics_data: *mut c_void,
    on_completion: Option<CbLspResult>,
    on_completion_data: *mut c_void,
    on_definition: Option<CbLspResult>,
    on_definition_data: *mut c_void,
    on_hover: Option<CbLspResult>,
    on_hover_data: *mut c_void,
    on_references: Option<CbLspResult>,
    on_references_data: *mut c_void,
    on_code_action: Option<CbLspResult>,
    on_code_action_data: *mut c_void,
}

// SAFETY: callbacks are C function pointers; user_data is managed by C++
unsafe impl Send for LspClient {}
unsafe impl Sync for LspClient {}

impl LspClient {
    pub fn new() -> Self {
        Self {
            process: None,
            write_tx: None,
            writer_thread: None,
            reader_thread: None,
            shutdown_tx: None,
            request_id: AtomicI32::new(1),
            initialized: false,
            root_uri: String::new(),
            framer: Arc::new(Mutex::new(LengthPrefixedFramer::new())),
            on_server_started: None,
            on_server_started_data: std::ptr::null_mut(),
            on_server_failed: None,
            on_server_failed_data: std::ptr::null_mut(),
            on_diagnostics: None,
            on_diagnostics_data: std::ptr::null_mut(),
            on_completion: None,
            on_completion_data: std::ptr::null_mut(),
            on_definition: None,
            on_definition_data: std::ptr::null_mut(),
            on_hover: None,
            on_hover_data: std::ptr::null_mut(),
            on_references: None,
            on_references_data: std::ptr::null_mut(),
            on_code_action: None,
            on_code_action_data: std::ptr::null_mut(),
        }
    }

    pub fn start_server(
        &mut self,
        command: &str,
        args: &[&str],
        root_uri: &str,
    ) -> Result<(), String> {
        self.root_uri = root_uri.to_string();

        let mut child = Command::new(command)
            .args(args)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|e| format!("Failed to start language server: {}", e))?;

        let stdin = child.stdin.take()
            .ok_or_else(|| "Failed to capture stdin".to_string())?;
        let stdout = child.stdout.take()
            .ok_or_else(|| "Failed to capture stdout".to_string())?;

        // Writer thread: ALL writes to the server's stdin happen here, never
        // on the caller (GUI) thread. A stuck server fills the pipe and a
        // blocking write_all() on the main thread would freeze the whole app
        // mid-keystroke. The channel is bounded so a dead server can't grow
        // memory without bound; if it fills up, the newest message is dropped
        // (the server is stuck anyway — the next delivered message carries
        // fresher state once it drains).
        let (write_tx, write_rx) = mpsc::sync_channel::<Vec<u8>>(32);
        let writer_handle = thread::spawn(move || {
            let mut stdin = stdin;
            while let Ok(msg) = write_rx.recv() {
                if stdin.write_all(msg.as_slice()).is_err() || stdin.flush().is_err() {
                    break; // server is gone
                }
            }
        });

        self.write_tx = Some(write_tx);
        self.writer_thread = Some(writer_handle);
        self.process = Some(child);

        let framer = self.framer.clone();
        let (tx, rx) = mpsc::channel();

        // Spawn reader thread
        let handle = thread::spawn(move || {
            let mut reader = BufReader::new(stdout);

            loop {
                // Check for shutdown
                if rx.try_recv().is_ok() {
                    break;
                }

                // Read content-length header
                let mut content_length: Option<usize> = None;

                // Read headers until empty line
                loop {
                    let mut line = String::new();
                    match reader.read_line(&mut line) {
                        Ok(0) => return, // EOF
                        Ok(_) => {
                            let trimmed = line.trim();
                            if trimmed.is_empty() {
                                break; // End of headers
                            }
                            if let Some(val) = trimmed.strip_prefix("Content-Length:") {
                                content_length = val.trim().parse::<usize>().ok();
                            }
                        }
                        Err(_) => return,
                    }
                }

                // Read body
                if let Some(len) = content_length {
                    let mut body = vec![0u8; len];
                    match reader.read_exact(&mut body) {
                        Ok(_) => {
                            let mut f = framer.lock().unwrap();
                            f.feed(&body);
                        }
                        Err(_) => return,
                    }
                }
            }
        });

        self.reader_thread = Some(handle);
        self.shutdown_tx = Some(tx);

        // Fire server started callback — scoped CString
        if let Some(cb) = self.on_server_started {
            let msg = std::ffi::CString::new("started").unwrap_or_default();
            cb(msg.as_ptr(), self.on_server_started_data);
        }

        Ok(())
    }

    pub fn stop_server(&mut self) {
        if let Some(tx) = self.shutdown_tx.take() {
            let _ = tx.send(());
        }
        if let Some(mut process) = self.process.take() {
            // Kill the child FIRST: closing its stdin pipe unblocks the writer
            // thread if it is mid-write_all() to a full pipe, and closing
            // stdout unblocks the reader thread.
            let _ = process.kill();
            let _ = process.wait();
        }
        self.write_tx = None; // close the channel -> writer thread exits
        if let Some(handle) = self.writer_thread.take() {
            let _ = handle.join();
        }
        if let Some(handle) = self.reader_thread.take() {
            let _ = handle.join();
        }
        self.initialized = false;
    }

    pub fn is_running(&self) -> bool {
        self.process.is_some()
    }

    pub fn feed_message(&mut self, json_data: &str) {
        // Parse the JSON-RPC message and dispatch
        if let Ok(msg) = serde_json::from_str::<Value>(json_data) {
            self.handle_rpc_message(&msg);
        }
    }

    fn send_raw(&mut self, json: &Value) {
        let body = serde_json::to_string(json).unwrap_or_default();
        let header = format!("Content-Length: {}\r\n\r\n", body.len());
        let mut msg = Vec::with_capacity(header.len() + body.len());
        msg.extend_from_slice(header.as_bytes());
        msg.extend_from_slice(body.as_bytes());
        if let Some(ref tx) = self.write_tx {
            // try_send never blocks the caller (GUI thread). When the bounded
            // queue is full the newest message is dropped rather than stalling.
            let _ = tx.try_send(msg);
        }
    }

    fn next_request_id(&self) -> i32 {
        self.request_id.fetch_add(1, Ordering::Relaxed)
    }

    fn send_request(&mut self, method: &str, params: Value) -> i32 {
        let id = self.next_request_id();
        let msg = json!({
            "jsonrpc": "2.0",
            "id": id,
            "method": method,
            "params": params
        });
        self.send_raw(&msg);
        id
    }

    fn send_notification(&mut self, method: &str, params: Value) {
        let msg = json!({
            "jsonrpc": "2.0",
            "method": method,
            "params": params
        });
        self.send_raw(&msg);
    }

    fn handle_rpc_message(&mut self, msg: &Value) {
        if let Some(method) = msg.get("method").and_then(|m| m.as_str()) {
            match method {
                "textDocument/publishDiagnostics" => {
                    self.handle_diagnostics(msg);
                }
                "window/logMessage" | "window/showMessage" => {
                    // Can be ignored or forwarded
                }
                _ => {
                    // Unknown notification
                }
            }
        } else if let Some(id) = msg.get("id").and_then(|i| i.as_i64()) {
            // It's a response
            self.handle_response(id as i32, msg);
        }
    }

    fn handle_diagnostics(&mut self, msg: &Value) {
        if let Some(params) = msg.get("params") {
            let uri = params.get("uri").and_then(|u| u.as_str()).unwrap_or("");
            let diags = params.get("diagnostics").map(|d| d.to_string()).unwrap_or_default();

            if let Some(cb) = self.on_diagnostics {
                let c_uri = std::ffi::CString::new(uri).unwrap_or_default();
                let c_diags = std::ffi::CString::new(&diags[..]).unwrap_or_default();
                cb(c_uri.as_ptr(), c_diags.as_ptr(), self.on_diagnostics_data);
            }
        }
    }

    fn handle_response(&mut self, id: i32, msg: &Value) {
        let result = msg.get("result").map(|r| r.to_string()).unwrap_or_default();

        // Dispatch to the right callback based on stored request type
        if let Some(cb) = self.on_completion {
            let c_result = std::ffi::CString::new(&result[..]).unwrap_or_default();
            cb(id, c_result.as_ptr(), self.on_completion_data);
        }
    }

    // ── Protocol methods ───────────────────────────────────────────

    pub fn initialize(&mut self, root_uri: &str, _language_id: &str) {
        let params = json!({
            "processId": null,
            "clientInfo": {
                "name": "Scriptura",
                "version": "0.1.0"
            },
            "capabilities": {},
            "rootUri": root_uri,
            "workspaceFolders": [
                {"uri": root_uri, "name": "workspace"}
            ]
        });
        self.send_request("initialize", params);
    }

    pub fn initialized(&mut self) {
        self.initialized = true;
        self.send_notification("initialized", json!({}));
    }

    pub fn did_open(&mut self, uri: &str, language_id: &str, text: &str) {
        let params = json!({
            "textDocument": {
                "uri": uri,
                "languageId": language_id,
                "version": 1,
                "text": text
            }
        });
        self.send_notification("textDocument/didOpen", params);
    }

    pub fn did_change(&mut self, uri: &str, text: &str) {
        let params = json!({
            "textDocument": {
                "uri": uri,
                "version": 2
            },
            "contentChanges": [
                {"text": text}
            ]
        });
        self.send_notification("textDocument/didChange", params);
    }

    pub fn did_close(&mut self, uri: &str) {
        let params = json!({
            "textDocument": {
                "uri": uri
            }
        });
        self.send_notification("textDocument/didClose", params);
    }

    pub fn shutdown(&mut self) {
        let _ = self.send_request("shutdown", json!({}));
    }

    pub fn exit(&mut self) {
        self.send_notification("exit", json!({}));
    }

    pub fn completion(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/completion", params)
    }

    pub fn definition(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/definition", params)
    }

    pub fn hover(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/hover", params)
    }

    pub fn references(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character},
            "context": {"includeDeclaration": true}
        });
        self.send_request("textDocument/references", params)
    }

    pub fn rename(&mut self, uri: &str, line: i32, character: i32, new_name: &str) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character},
            "newName": new_name
        });
        self.send_request("textDocument/rename", params)
    }

    pub fn code_action(&mut self, uri: &str, start_line: i32, start_char: i32, end_line: i32, end_char: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": start_line, "character": start_char},
                "end": {"line": end_line, "character": end_char}
            },
            "context": {
                "diagnostics": []
            }
        });
        self.send_request("textDocument/codeAction", params)
    }

    pub fn document_symbol(&mut self, uri: &str) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri}
        });
        self.send_request("textDocument/documentSymbol", params)
    }

    pub fn workspace_symbol(&mut self, query: &str) -> i32 {
        let params = json!({
            "query": query
        });
        self.send_request("workspace/symbol", params)
    }

    pub fn formatting(&mut self, uri: &str, options: &str) -> i32 {
        let opts: Value = serde_json::from_str(options).unwrap_or(json!({}));
        let params = json!({
            "textDocument": {"uri": uri},
            "options": opts
        });
        self.send_request("textDocument/formatting", params)
    }

    pub fn range_formatting(&mut self, uri: &str, start_line: i32, start_char: i32, end_line: i32, end_char: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": start_line, "character": start_char},
                "end": {"line": end_line, "character": end_char}
            },
            "options": {}
        });
        self.send_request("textDocument/rangeFormatting", params)
    }

    pub fn signature_help(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/signatureHelp", params)
    }

    pub fn declaration(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/declaration", params)
    }

    pub fn type_definition(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/typeDefinition", params)
    }

    pub fn implementation(&mut self, uri: &str, line: i32, character: i32) -> i32 {
        let params = json!({
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}
        });
        self.send_request("textDocument/implementation", params)
    }

    // ── Callback setters ───────────────────────────────────────────

    pub fn set_on_server_started(&mut self, cb: CbStrMsg, data: *mut c_void) {
        self.on_server_started = Some(cb);
        self.on_server_started_data = data;
    }

    pub fn set_on_server_failed(&mut self, cb: CbStrMsg, data: *mut c_void) {
        self.on_server_failed = Some(cb);
        self.on_server_failed_data = data;
    }

    pub fn set_on_diagnostics(&mut self, cb: CbDiag, data: *mut c_void) {
        self.on_diagnostics = Some(cb);
        self.on_diagnostics_data = data;
    }

    pub fn set_on_completion(&mut self, cb: CbLspResult, data: *mut c_void) {
        self.on_completion = Some(cb);
        self.on_completion_data = data;
    }

    pub fn set_on_definition(&mut self, cb: CbLspResult, data: *mut c_void) {
        self.on_definition = Some(cb);
        self.on_definition_data = data;
    }

    pub fn set_on_hover(&mut self, cb: CbLspResult, data: *mut c_void) {
        self.on_hover = Some(cb);
        self.on_hover_data = data;
    }

    pub fn set_on_references(&mut self, cb: CbLspResult, data: *mut c_void) {
        self.on_references = Some(cb);
        self.on_references_data = data;
    }

    pub fn set_on_code_action(&mut self, cb: CbLspResult, data: *mut c_void) {
        self.on_code_action = Some(cb);
        self.on_code_action_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::c_void;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_server_started(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_client() {
        let c = LspClient::new();
        assert!(!c.is_running());
    }

    #[test]
    fn test_request_id_increments() {
        let c = LspClient::new();
        assert_eq!(c.request_id.load(Ordering::Relaxed), 1);
    }

    #[test]
    fn test_stop_server_without_start() {
        let mut c = LspClient::new();
        c.stop_server(); // Should not panic
    }

    #[test]
    fn test_feed_message_invalid_json() {
        let mut c = LspClient::new();
        c.feed_message("not valid json"); // Should not panic
    }

    #[test]
    fn test_feed_message_valid() {
        let mut c = LspClient::new();
        c.feed_message(r#"{"jsonrpc": "2.0", "method": "test"}"#); // Should not panic
    }

    #[test]
    fn test_start_server_invalid_command() {
        let mut c = LspClient::new();
        let result = c.start_server("/nonexistent/lsp", &[], "file:///test");
        assert!(result.is_err());
    }

    #[test]
    fn test_set_callbacks() {
        let mut c = LspClient::new();
        c.set_on_server_started(test_server_started, std::ptr::null_mut());
        c.set_on_server_failed(test_server_started, std::ptr::null_mut());
    }

    #[test]
    fn test_protocol_methods_without_server() {
        let mut c = LspClient::new();
        // These should not panic even without a server
        c.initialize("file:///root", "rust");
        c.initialized();
        c.did_open("file:///test.rs", "rust", "fn main() {}");
        c.did_change("file:///test.rs", "fn main() { println!(\"hi\"); }");
        c.did_close("file:///test.rs");
        c.shutdown();
        c.exit();
    }

    #[test]
    fn test_completion_request() {
        let mut c = LspClient::new();
        let id = c.completion("file:///test.rs", 0, 0);
        assert!(id > 0);
    }

    #[test]
    fn test_definition_request() {
        let mut c = LspClient::new();
        let id = c.definition("file:///test.rs", 5, 10);
        assert!(id > 0);
    }

    #[test]
    fn test_hover_request() {
        let mut c = LspClient::new();
        let id = c.hover("file:///test.rs", 3, 7);
        assert!(id > 0);
    }

    #[test]
    fn test_references_request() {
        let mut c = LspClient::new();
        let id = c.references("file:///test.rs", 1, 2);
        assert!(id > 0);
    }

    #[test]
    fn test_rename_request() {
        let mut c = LspClient::new();
        let id = c.rename("file:///test.rs", 1, 2, "new_name");
        assert!(id > 0);
    }

    #[test]
    fn test_code_action_request() {
        let mut c = LspClient::new();
        let id = c.code_action("file:///test.rs", 0, 0, 5, 10);
        assert!(id > 0);
    }

    #[test]
    fn test_document_symbol_request() {
        let mut c = LspClient::new();
        let id = c.document_symbol("file:///test.rs");
        assert!(id > 0);
    }

    #[test]
    fn test_workspace_symbol_request() {
        let mut c = LspClient::new();
        let id = c.workspace_symbol("query");
        assert!(id > 0);
    }

    #[test]
    fn test_formatting_request() {
        let mut c = LspClient::new();
        let id = c.formatting("file:///test.rs", r#"{"tabSize": 4}"#);
        assert!(id > 0);
    }

    #[test]
    fn test_range_formatting_request() {
        let mut c = LspClient::new();
        let id = c.range_formatting("file:///test.rs", 0, 0, 5, 10);
        assert!(id > 0);
    }

    #[test]
    fn test_signature_help_request() {
        let mut c = LspClient::new();
        let id = c.signature_help("file:///test.rs", 0, 0);
        assert!(id > 0);
    }

    #[test]
    fn test_declaration_request() {
        let mut c = LspClient::new();
        let id = c.declaration("file:///test.rs", 0, 0);
        assert!(id > 0);
    }

    #[test]
    fn test_type_definition_request() {
        let mut c = LspClient::new();
        let id = c.type_definition("file:///test.rs", 0, 0);
        assert!(id > 0);
    }

    #[test]
    fn test_implementation_request() {
        let mut c = LspClient::new();
        let id = c.implementation("file:///test.rs", 0, 0);
        assert!(id > 0);
    }

    #[test]
    fn test_request_ids_are_unique() {
        let mut c = LspClient::new();
        let id1 = c.completion("", 0, 0);
        let id2 = c.definition("", 0, 0);
        let id3 = c.hover("", 0, 0);
        assert!(id1 != id2);
        assert!(id2 != id3);
        assert!(id3 > id2);
        assert!(id2 > id1);
    }

    #[test]
    fn test_feed_diagnostics_message() {
        let mut c = LspClient::new();
        let diag_msg = r#"{
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": "file:///test.rs",
                "diagnostics": [{"message": "test", "severity": 1}]
            }
        }"#;
        c.feed_message(diag_msg); // Should not panic
    }

    #[test]
    fn test_feed_response_message() {
        let mut c = LspClient::new();
        let resp_msg = r#"{
            "jsonrpc": "2.0",
            "id": 1,
            "result": {"items": []}
        }"#;
        c.feed_message(resp_msg); // Should not panic
    }
}
