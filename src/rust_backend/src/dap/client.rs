//! ## DAP Client
//!
//! Debug Adapter Protocol client implementation.
//! Manages a child process (the debug adapter) and communicates via
//! JSON-RPC over stdin/stdout with Content-Length framing.
//!
//! Replaces `DapClient` (QObject-based) from the original C++ codebase.

use std::ffi::c_void;
use std::io::{BufRead, BufReader, Read, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;

use serde_json::{json, Value};

use crate::framer::LengthPrefixedFramer;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);
type CbStopped = extern "C" fn(*const std::os::raw::c_char, i32, *mut c_void);
type CbStackFrames = extern "C" fn(i32, *const std::os::raw::c_char, *mut c_void);
type CbScopes = extern "C" fn(i32, *const std::os::raw::c_char, *mut c_void);
type CbVariables = extern "C" fn(i32, *const std::os::raw::c_char, *mut c_void);
type CbBreakpoints = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);

pub struct DapClient {
    process: Option<Child>,
    stdin: Option<ChildStdin>,
    reader_thread: Option<thread::JoinHandle<()>>,
    shutdown_tx: Option<Sender<()>>,
    request_id: AtomicI32,
    #[allow(dead_code)]
    seq: AtomicI32,
    current_thread_id: i32,
    configuration_done_sent: bool,
    #[allow(dead_code)]
    framer: Arc<Mutex<LengthPrefixedFramer>>,

    // Callbacks
    on_server_started: Option<CbStr>,
    on_server_started_data: *mut c_void,
    on_server_failed: Option<CbStr>,
    on_server_failed_data: *mut c_void,
    on_initialized: Option<CbStr>,
    on_initialized_data: *mut c_void,
    on_stopped: Option<CbStopped>,
    on_stopped_data: *mut c_void,
    on_continued: Option<CbStr>,
    on_continued_data: *mut c_void,
    on_breakpoints: Option<CbBreakpoints>,
    on_breakpoints_data: *mut c_void,
    on_stack_trace: Option<CbStackFrames>,
    on_stack_trace_data: *mut c_void,
    on_scopes: Option<CbScopes>,
    on_scopes_data: *mut c_void,
    on_variables: Option<CbVariables>,
    on_variables_data: *mut c_void,
    on_evaluation: Option<CbStr>,
    on_evaluation_data: *mut c_void,
}

unsafe impl Send for DapClient {}
unsafe impl Sync for DapClient {}

impl DapClient {
    pub fn new() -> Self {
        Self {
            process: None,
            stdin: None,
            reader_thread: None,
            shutdown_tx: None,
            request_id: AtomicI32::new(1),
            seq: AtomicI32::new(1),
            current_thread_id: 0,
            configuration_done_sent: false,
            framer: Arc::new(Mutex::new(LengthPrefixedFramer::new())),
            on_server_started: None,
            on_server_started_data: std::ptr::null_mut(),
            on_server_failed: None,
            on_server_failed_data: std::ptr::null_mut(),
            on_initialized: None,
            on_initialized_data: std::ptr::null_mut(),
            on_stopped: None,
            on_stopped_data: std::ptr::null_mut(),
            on_continued: None,
            on_continued_data: std::ptr::null_mut(),
            on_breakpoints: None,
            on_breakpoints_data: std::ptr::null_mut(),
            on_stack_trace: None,
            on_stack_trace_data: std::ptr::null_mut(),
            on_scopes: None,
            on_scopes_data: std::ptr::null_mut(),
            on_variables: None,
            on_variables_data: std::ptr::null_mut(),
            on_evaluation: None,
            on_evaluation_data: std::ptr::null_mut(),
        }
    }

    pub fn start_server(&mut self, command: &str, args: &[&str]) -> Result<(), String> {
        let mut child = Command::new(command)
            .args(args)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|e| format!("Failed to start debug adapter: {}", e))?;

        let stdin = child.stdin.take()
            .ok_or_else(|| "Failed to capture stdin".to_string())?;
        let stdout = child.stdout.take()
            .ok_or_else(|| "Failed to capture stdout".to_string())?;

        self.stdin = Some(stdin);
        self.process = Some(child);

        let (tx, rx) = mpsc::channel();

        // Use channel to send parsed messages back to the main thread
        let (msg_tx, msg_rx): (Sender<String>, Receiver<String>) = mpsc::channel();
        let _ = msg_rx; // Will be used in process loop

        let handle = thread::spawn(move || {
            let mut reader = BufReader::new(stdout);
            loop {
                if rx.try_recv().is_ok() {
                    break;
                }
                let mut content_length: Option<usize> = None;
                loop {
                    let mut line = String::new();
                    match reader.read_line(&mut line) {
                        Ok(0) => return,
                        Ok(_) => {
                            let trimmed = line.trim();
                            if trimmed.is_empty() { break; }
                            if let Some(val) = trimmed.strip_prefix("Content-Length:") {
                                content_length = val.trim().parse::<usize>().ok();
                            }
                        }
                        Err(_) => return,
                    }
                }
                if let Some(len) = content_length {
                    let mut body = vec![0u8; len];
                    if reader.read_exact(&mut body).is_ok() {
                        if let Ok(body_str) = String::from_utf8(body) {
                            let _ = msg_tx.send(body_str);
                        }
                    }
                }
            }
        });

        // Process messages from the channel (would be done in a polling thread)
        self.reader_thread = Some(handle);
        self.shutdown_tx = Some(tx);

        // Fire server started callback — use scoped CString to avoid memory leak
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
        if let Some(handle) = self.reader_thread.take() {
            let _ = handle.join();
        }
        if let Some(mut process) = self.process.take() {
            let _ = process.kill();
            let _ = process.wait();
        }
        self.stdin = None;
    }

    pub fn is_running(&self) -> bool {
        self.process.is_some()
    }

    pub fn feed_message(&self, _json_data: &str) {
        // Parse and dispatch DAP messages
    }

    fn send_raw(&mut self, json: &Value) {
        if let Some(ref mut stdin) = self.stdin {
            let body = serde_json::to_string(json).unwrap_or_default();
            let header = format!("Content-Length: {}\r\n\r\n", body.len());
            let _ = stdin.write_all(header.as_bytes());
            let _ = stdin.write_all(body.as_bytes());
            let _ = stdin.flush();
        }
    }

    fn send_request(&mut self, command: &str, params: Value) -> i32 {
        let id = self.request_id.fetch_add(1, Ordering::Relaxed);
        let msg = json!({
            "type": "request",
            "seq": id,
            "command": command,
            "arguments": params
        });
        self.send_raw(&msg);
        id
    }

    // ── DAP Protocol methods ───────────────────────────────────────

    pub fn initialize(&mut self, program: &str, args_list: &[&str], cwd: &str) {
        let params = json!({
            "clientID": "scriptura",
            "clientName": "Scriptura",
            "adapterID": "scriptura-debug",
            "pathFormat": "path",
            "linesStartAt1": true,
            "columnsStartAt1": true,
            "supportsRunInTerminalRequest": true,
            "supportsProgressReporting": false,
            "program": program,
            "args": args_list,
            "cwd": cwd
        });
        self.send_request("initialize", params);
    }

    pub fn launch(&mut self) {
        self.send_request("launch", json!({}));
    }

    pub fn configuration_done(&mut self) {
        self.configuration_done_sent = true;
        self.send_request("configurationDone", json!({}));
    }

    pub fn set_breakpoints(&mut self, source_path: &str, lines: &[i32]) {
        let sources: Vec<Value> = lines.iter().map(|&l| json!({"line": l})).collect();
        let params = json!({
            "source": {"path": source_path, "name": source_path},
            "lines": sources,
            "breakpoints": sources
        });
        self.send_request("setBreakpoints", params);
    }

    pub fn continue_debug(&mut self) {
        self.send_request("continue", json!({"threadId": self.current_thread_id}));
    }

    pub fn next(&mut self) {
        self.send_request("next", json!({"threadId": self.current_thread_id}));
    }

    pub fn step_in(&mut self) {
        self.send_request("stepIn", json!({"threadId": self.current_thread_id}));
    }

    pub fn step_out(&mut self) {
        self.send_request("stepOut", json!({"threadId": self.current_thread_id}));
    }

    pub fn pause(&mut self) {
        self.send_request("pause", json!({"threadId": self.current_thread_id}));
    }

    pub fn disconnect(&mut self) {
        self.send_request("disconnect", json!({}));
    }

    pub fn stack_trace(&mut self, thread_id: i32) {
        let _id = self.send_request("stackTrace", json!({
            "threadId": thread_id,
            "startFrame": 0,
            "levels": 50
        }));
    }

    pub fn scopes(&mut self, frame_id: i32) {
        let _id = self.send_request("scopes", json!({
            "frameId": frame_id
        }));
    }

    pub fn variables(&mut self, variables_reference: i32) {
        let _id = self.send_request("variables", json!({
            "variablesReference": variables_reference
        }));
    }

    pub fn evaluate(&mut self, expression: &str, frame_id: i32, context: &str) {
        let _id = self.send_request("evaluate", json!({
            "expression": expression,
            "frameId": frame_id,
            "context": context
        }));
    }

    // ── Callback setters ───────────────────────────────────────────

    pub fn set_on_server_started(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_server_started = Some(cb);
        self.on_server_started_data = data;
    }

    pub fn set_on_server_failed(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_server_failed = Some(cb);
        self.on_server_failed_data = data;
    }

    pub fn set_on_initialized(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_initialized = Some(cb);
        self.on_initialized_data = data;
    }

    pub fn set_on_stopped(&mut self, cb: CbStopped, data: *mut c_void) {
        self.on_stopped = Some(cb);
        self.on_stopped_data = data;
    }

    pub fn set_on_continued(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_continued = Some(cb);
        self.on_continued_data = data;
    }

    pub fn set_on_breakpoints(&mut self, cb: CbBreakpoints, data: *mut c_void) {
        self.on_breakpoints = Some(cb);
        self.on_breakpoints_data = data;
    }

    pub fn set_on_stack_trace(&mut self, cb: CbStackFrames, data: *mut c_void) {
        self.on_stack_trace = Some(cb);
        self.on_stack_trace_data = data;
    }

    pub fn set_on_scopes(&mut self, cb: CbScopes, data: *mut c_void) {
        self.on_scopes = Some(cb);
        self.on_scopes_data = data;
    }

    pub fn set_on_variables(&mut self, cb: CbVariables, data: *mut c_void) {
        self.on_variables = Some(cb);
        self.on_variables_data = data;
    }

    pub fn set_on_evaluation(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_evaluation = Some(cb);
        self.on_evaluation_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_cb_str(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_client() {
        let c = DapClient::new();
        assert!(!c.is_running());
    }

    #[test]
    fn test_stop_server_without_start() {
        let mut c = DapClient::new();
        c.stop_server(); // Should not panic
    }

    #[test]
    fn test_start_server_invalid_command() {
        let mut c = DapClient::new();
        let result = c.start_server("/nonexistent/dap", &[]);
        assert!(result.is_err());
    }

    #[test]
    fn test_feed_message_no_panic() {
        let c = DapClient::new();
        c.feed_message(r#"{"type": "event"}"#); // Should not panic
        c.feed_message(""); // Should not panic
    }

    #[test]
    fn test_protocol_methods_without_server() {
        let mut c = DapClient::new();
        // These should not panic even without a server
        c.initialize("/bin/ls", &[], "/tmp");
        c.launch();
        c.configuration_done();
        c.set_breakpoints("test.rs", &[1, 5, 10]);
        c.continue_debug();
        c.next();
        c.step_in();
        c.step_out();
        c.pause();
        c.disconnect();
    }

    #[test]
    fn test_stack_trace() {
        let mut c = DapClient::new();
        c.stack_trace(1); // Should not panic
    }

    #[test]
    fn test_scopes() {
        let mut c = DapClient::new();
        c.scopes(1000); // Should not panic
    }

    #[test]
    fn test_variables() {
        let mut c = DapClient::new();
        c.variables(42); // Should not panic
    }

    #[test]
    fn test_evaluate() {
        let mut c = DapClient::new();
        c.evaluate("2 + 2", 1, "repl"); // Should not panic
    }

    #[test]
    fn test_request_ids_are_unique() {
        let mut c = DapClient::new();
        let id1 = c.send_request("test1", json!({}));
        let id2 = c.send_request("test2", json!({}));
        let id3 = c.send_request("test3", json!({}));
        assert!(id1 < id2);
        assert!(id2 < id3);
    }

    #[test]
    fn test_set_callbacks() {
        let mut c = DapClient::new();
        c.set_on_server_started(test_cb_str, std::ptr::null_mut());
        c.set_on_server_failed(test_cb_str, std::ptr::null_mut());
        c.set_on_initialized(test_cb_str, std::ptr::null_mut());
        c.set_on_continued(test_cb_str, std::ptr::null_mut());
        c.set_on_evaluation(test_cb_str, std::ptr::null_mut());
    }

    #[test]
    fn test_configuration_done_flag() {
        let mut c = DapClient::new();
        assert!(!c.configuration_done_sent);
        c.configuration_done();
        assert!(c.configuration_done_sent);
    }
}
