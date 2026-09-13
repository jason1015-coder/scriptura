//! ## FFI Exports
//!
//! All C-compatible FFI functions that bridge Rust backends to the C++ Qt UI.
//! Complex data crosses the boundary as JSON strings.
//! Callbacks use C function pointers for async notification.

use std::ffi::{c_void, CString};
use std::mem;
use std::os::raw::c_char;

// ── C callback type aliases (must match rust_backend.h) ───────────────

/// Callback: (const char* data, void* user_data)
pub type OnStringMessage = extern "C" fn(*const c_char, *mut c_void);
/// Callback: (const char* event, const char* json_data, void* user_data)
pub type OnEvent = extern "C" fn(*const c_char, *const c_char, *mut c_void);

/// Callback: (const char* reason, int thread_id, void* user_data)
pub type OnDapStopped = extern "C" fn(*const c_char, i32, *mut c_void);
/// Callback: (int thread_id, const char* json_frames, void* user_data)
pub type OnStackFrames = extern "C" fn(i32, *const c_char, *mut c_void);
/// Callback: (int frame_id, const char* json_scopes, void* user_data)
pub type OnScopes = extern "C" fn(i32, *const c_char, *mut c_void);
/// Callback: (int var_ref, const char* json_vars, void* user_data)
pub type OnVariables = extern "C" fn(i32, *const c_char, *mut c_void);
/// Callback: (const char* source, const char* json_breakpoints, void* user_data)
pub type OnDapBreakpoints = extern "C" fn(*const c_char, *const c_char, *mut c_void);
/// Callback: (const char* plugin_id, const char* json_data, void* user_data)
pub type OnPluginEvent = extern "C" fn(*const c_char, *const c_char, *mut c_void);
/// Callback: (const char* task_id, int current, int total, void* user_data)
pub type OnProgress = extern "C" fn(*const c_char, i32, i32, *mut c_void);

use crate::app::ScripturaApp;
use crate::lsp::LspClient;
use crate::dap::DapClient;
use crate::debug_session::DebugSession;
use crate::debug_config::DebugConfigurationManager;
use crate::eventbus::EventBus;
use crate::plugin::PluginManager;
use crate::plugin::PluginCrashHandler;
use crate::app_crash::AppCrashHandler;
use crate::registry::PluginRegistry;
use crate::service_locator::ServiceLocator;
use crate::dependency_resolver::DependencyResolver;
use crate::task_runner::TaskRunner;
use crate::updater::Updater;
use crate::plugin_updater::PluginUpdater;
use crate::version_fetcher::VersionFetcher;
use crate::workspace::Workspace;
use crate::config_validator::ConfigValidator;
use crate::archive_extractor::ArchiveExtractor;
use crate::permission::PermissionManager;
use crate::framer::LengthPrefixedFramer;
use crate::language_registry::LanguageRegistry;
use crate::language_server_manager::LanguageServerManager;
use crate::ui_actions::UiActionHandler;
use crate::text_buffer::TextBuffer;
use crate::bookmark_engine::BookmarkStore;
use crate::snippet_engine::SnippetStore;

// ── Opaque handle types ─────────────────────────────────────────────
// These are the types that C++ sees as pointers. Rust never dereferences
// them directly — they're just cast to/from the real Rust types.

pub enum RustEventBus {}
pub enum RustLspClient {}
pub enum RustDapClient {}
pub enum RustDebugSession {}
pub enum RustPluginManager {}
pub enum RustPluginRegistry {}
pub enum RustTaskRunner {}
pub enum RustUpdater {}
pub enum RustPluginUpdater {}
pub enum RustVersionFetcher {}
pub enum RustWorkspace {}
pub enum RustConfigValidator {}
pub enum RustArchiveExtractor {}
pub enum RustPermissionManager {}
pub enum RustLengthPrefixedFramer {}
pub enum RustDependencyResolver {}
pub enum RustServiceLocator {}
pub enum RustLanguageRegistry {}
pub enum RustLanguageServerManager {}
pub enum RustDebugConfigurationManager {}
pub enum RustPluginCrashHandler {}
pub enum RustAppCrashHandler {}
pub enum RustUiActionHandler {}
pub enum RustTextBuffer {}
pub enum RustBookmarkStore {}
pub enum RustSnippetStore {}
pub enum RustScripturaApp {}

// ── Helper macros ────────────────────────────────────────────────────

macro_rules! make_new {
    ($ffi_name:ident, $rust_type:ty, $handle_type:ty) => {
        #[no_mangle]
        pub extern "C" fn $ffi_name() -> *mut $handle_type {
            Box::into_raw(Box::new(<$rust_type>::new())) as *mut $handle_type
        }
    };
}

macro_rules! make_free {
    ($ffi_name:ident, $rust_type:ty, $handle_type:ty) => {
        #[no_mangle]
        pub extern "C" fn $ffi_name(ptr: *mut $handle_type) {
            if !ptr.is_null() {
                unsafe { let _ = Box::from_raw(ptr as *mut $rust_type); }
            }
        }
    };
}

/// Helper to convert a raw c_char pointer to a Rust &str.
pub(crate) unsafe fn ptr_to_str<'a>(ptr: *const c_char) -> &'a str {
    crate::cstr_to_str(ptr)
}

// ═══════════════════════════════════════════════════════════════════════
//  EventBus
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_eventbus_new, EventBus, RustEventBus);
make_free!(rust_eventbus_free, EventBus, RustEventBus);

#[no_mangle]
pub extern "C" fn rust_eventbus_subscribe(
    bus: *mut RustEventBus,
    event: *const c_char,
    callback: OnEvent,
    user_data: *mut c_void,
) -> u64 {
    let bus = unsafe { &*(bus as *mut EventBus) };
    bus.subscribe(unsafe { ptr_to_str(event) }, callback, user_data)
}

#[no_mangle]
pub extern "C" fn rust_eventbus_unsubscribe(
    bus: *mut RustEventBus,
    event: *const c_char,
    sub_id: u64,
) {
    let bus = unsafe { &*(bus as *mut EventBus) };
    bus.unsubscribe(unsafe { ptr_to_str(event) }, sub_id);
}

#[no_mangle]
pub extern "C" fn rust_eventbus_publish(
    bus: *mut RustEventBus,
    event: *const c_char,
    json_data: *const c_char,
) {
    let bus = unsafe { &*(bus as *mut EventBus) };
    bus.publish(unsafe { ptr_to_str(event) }, unsafe { ptr_to_str(json_data) });
}

#[no_mangle]
pub extern "C" fn rust_eventbus_has_subscribers(
    bus: *mut RustEventBus,
    event: *const c_char,
) -> bool {
    let bus = unsafe { &*(bus as *mut EventBus) };
    bus.has_subscribers(unsafe { ptr_to_str(event) })
}

// ═══════════════════════════════════════════════════════════════════════
//  Framer (LengthPrefixedFramer)
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_framer_new, LengthPrefixedFramer, RustLengthPrefixedFramer);
make_free!(rust_framer_free, LengthPrefixedFramer, RustLengthPrefixedFramer);

// Framer functions are inline-defined in framer.rs
// (feed, next_message, on_message)

// ═══════════════════════════════════════════════════════════════════════
//  LSP Client
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_lsp_client_new, LspClient, RustLspClient);
make_free!(rust_lsp_client_free, LspClient, RustLspClient);

#[no_mangle]
pub extern "C" fn rust_lsp_start_server(
    client: *mut RustLspClient,
    command: *const c_char,
    args: *const *const c_char,
    args_len: usize,
    root_uri: *const c_char,
) -> bool {
    let c = unsafe { &mut *(client as *mut LspClient) };
    let cmd = unsafe { ptr_to_str(command) };
    let uri = unsafe { ptr_to_str(root_uri) };
    let mut args_vec: Vec<&str> = Vec::with_capacity(args_len);
    if !args.is_null() {
        for i in 0..args_len {
            let arg_ptr = unsafe { *args.add(i) };
            args_vec.push(unsafe { ptr_to_str(arg_ptr) });
        }
    }
    c.start_server(cmd, &args_vec, uri).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_lsp_stop_server(client: *mut RustLspClient) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.stop_server();
}

#[no_mangle]
pub extern "C" fn rust_lsp_is_running(client: *const RustLspClient) -> bool {
    let c = unsafe { &*(client as *const LspClient) };
    c.is_running()
}

#[no_mangle]
pub extern "C" fn rust_lsp_initialize(
    client: *mut RustLspClient, root_uri: *const c_char, language_id: *const c_char
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.initialize(unsafe { ptr_to_str(root_uri) }, unsafe { ptr_to_str(language_id) });
}

#[no_mangle]
pub extern "C" fn rust_lsp_initialized(client: *mut RustLspClient) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.initialized();
}

#[no_mangle]
pub extern "C" fn rust_lsp_did_open(
    client: *mut RustLspClient, uri: *const c_char, lang_id: *const c_char, text: *const c_char
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.did_open(unsafe { ptr_to_str(uri) }, unsafe { ptr_to_str(lang_id) }, unsafe { ptr_to_str(text) });
}

#[no_mangle]
pub extern "C" fn rust_lsp_did_change(client: *mut RustLspClient, uri: *const c_char, text: *const c_char) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.did_change(unsafe { ptr_to_str(uri) }, unsafe { ptr_to_str(text) });
}

#[no_mangle]
pub extern "C" fn rust_lsp_did_close(client: *mut RustLspClient, uri: *const c_char) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.did_close(unsafe { ptr_to_str(uri) });
}

#[no_mangle]
pub extern "C" fn rust_lsp_shutdown(client: *mut RustLspClient) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.shutdown();
}

#[no_mangle]
pub extern "C" fn rust_lsp_exit(client: *mut RustLspClient) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.exit();
}

macro_rules! lsp_request_fn {
    ($name:ident, $method:ident) => {
        #[no_mangle]
        pub extern "C" fn $name(
            client: *mut RustLspClient, uri: *const c_char, line: i32, character: i32
        ) -> i32 {
            let c = unsafe { &mut *(client as *mut LspClient) };
            c.$method(unsafe { ptr_to_str(uri) }, line, character)
        }
    };
}

macro_rules! lsp_request_with_end_fn {
    ($name:ident, $method:ident) => {
        #[no_mangle]
        pub extern "C" fn $name(
            client: *mut RustLspClient,
            uri: *const c_char,
            start_line: i32, start_char: i32,
            end_line: i32, end_char: i32,
        ) -> i32 {
            let c = unsafe { &mut *(client as *mut LspClient) };
            c.$method(unsafe { ptr_to_str(uri) }, start_line, start_char, end_line, end_char)
        }
    };
}

lsp_request_fn!(rust_lsp_completion, completion);
lsp_request_fn!(rust_lsp_definition, definition);
lsp_request_fn!(rust_lsp_hover, hover);
lsp_request_fn!(rust_lsp_references, references);
lsp_request_fn!(rust_lsp_signature_help, signature_help);
lsp_request_fn!(rust_lsp_declaration, declaration);
lsp_request_fn!(rust_lsp_type_definition, type_definition);
lsp_request_fn!(rust_lsp_implementation, implementation);

lsp_request_with_end_fn!(rust_lsp_code_action, code_action);
lsp_request_with_end_fn!(rust_lsp_range_formatting, range_formatting);

#[no_mangle]
pub extern "C" fn rust_lsp_rename(
    client: *mut RustLspClient, uri: *const c_char, line: i32, character: i32, new_name: *const c_char
) -> i32 {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.rename(unsafe { ptr_to_str(uri) }, line, character, unsafe { ptr_to_str(new_name) })
}

#[no_mangle]
pub extern "C" fn rust_lsp_document_symbol(client: *mut RustLspClient, uri: *const c_char) -> i32 {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.document_symbol(unsafe { ptr_to_str(uri) })
}

#[no_mangle]
pub extern "C" fn rust_lsp_workspace_symbol(client: *mut RustLspClient, query: *const c_char) -> i32 {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.workspace_symbol(unsafe { ptr_to_str(query) })
}

#[no_mangle]
pub extern "C" fn rust_lsp_formatting(
    client: *mut RustLspClient, uri: *const c_char, json_options: *const c_char
) -> i32 {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.formatting(unsafe { ptr_to_str(uri) }, unsafe { ptr_to_str(json_options) })
}

#[no_mangle]
pub extern "C" fn rust_lsp_feed_message(client: *mut RustLspClient, json_data: *const c_char) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.feed_message(unsafe { ptr_to_str(json_data) });
}

// ── LSP callback setters ─────────────────────────────────────────────
macro_rules! lsp_callback_setter_str {
    ($name:ident, $field:ident) => {
        #[no_mangle]
        pub extern "C" fn $name(
            client: *mut RustLspClient, cb: OnStringMessage, user_data: *mut c_void
        ) {
            let c = unsafe { &mut *(client as *mut LspClient) };
            c.$field(cb, user_data);
        }
    };
}

lsp_callback_setter_str!(rust_lsp_on_server_started, set_on_server_started);
lsp_callback_setter_str!(rust_lsp_on_server_failed, set_on_server_failed);
#[no_mangle]
pub extern "C" fn rust_lsp_on_diagnostics(
    client: *mut RustLspClient,
    cb: extern "C" fn(*const c_char, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_diagnostics(cb, user_data);
}

#[no_mangle]
pub extern "C" fn rust_lsp_on_completion(
    client: *mut RustLspClient,
    cb: extern "C" fn(i32, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_completion(cb, user_data);
}

#[no_mangle]
pub extern "C" fn rust_lsp_on_definition(
    client: *mut RustLspClient,
    cb: extern "C" fn(i32, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_definition(cb, user_data);
}
#[no_mangle]
pub extern "C" fn rust_lsp_on_hover(
    client: *mut RustLspClient,
    cb: extern "C" fn(i32, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_hover(cb, user_data);
}

#[no_mangle]
pub extern "C" fn rust_lsp_on_references(
    client: *mut RustLspClient,
    cb: extern "C" fn(i32, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_references(cb, user_data);
}

#[no_mangle]
pub extern "C" fn rust_lsp_on_code_action(
    client: *mut RustLspClient,
    cb: extern "C" fn(i32, *const c_char, *mut c_void),
    user_data: *mut c_void,
) {
    let c = unsafe { &mut *(client as *mut LspClient) };
    c.set_on_code_action(cb, user_data);
}

// ═══════════════════════════════════════════════════════════════════════
//  DAP Client
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_dap_client_new, DapClient, RustDapClient);
make_free!(rust_dap_client_free, DapClient, RustDapClient);

#[no_mangle]
pub extern "C" fn rust_dap_start_server(
    client: *mut RustDapClient, command: *const c_char,
    args: *const *const c_char, args_len: usize
) -> bool {
    let c = unsafe { &mut *(client as *mut DapClient) };
    let cmd = unsafe { ptr_to_str(command) };
    let mut args_vec: Vec<&str> = Vec::with_capacity(args_len);
    if !args.is_null() {
        for i in 0..args_len {
            args_vec.push(unsafe { ptr_to_str(*args.add(i)) });
        }
    }
    c.start_server(cmd, &args_vec).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_dap_stop_server(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.stop_server();
}

#[no_mangle]
pub extern "C" fn rust_dap_is_running(client: *const RustDapClient) -> bool {
    let c = unsafe { &*(client as *const DapClient) };
    c.is_running()
}

#[no_mangle]
pub extern "C" fn rust_dap_initialize(
    client: *mut RustDapClient, program: *const c_char,
    args: *const *const c_char, args_len: usize, cwd: *const c_char
) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    let prog = unsafe { ptr_to_str(program) };
    let cwd_s = unsafe { ptr_to_str(cwd) };
    let mut args_vec: Vec<&str> = Vec::with_capacity(args_len);
    if !args.is_null() {
        for i in 0..args_len {
            args_vec.push(unsafe { ptr_to_str(*args.add(i)) });
        }
    }
    c.initialize(prog, &args_vec, cwd_s);
}

#[no_mangle]
pub extern "C" fn rust_dap_launch(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.launch();
}

#[no_mangle]
pub extern "C" fn rust_dap_configuration_done(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.configuration_done();
}

#[no_mangle]
pub extern "C" fn rust_dap_set_breakpoints(
    client: *mut RustDapClient, source_path: *const c_char,
    lines: *const i32, lines_len: usize
) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    let path = unsafe { ptr_to_str(source_path) };
    let mut lines_vec: Vec<i32> = Vec::with_capacity(lines_len);
    if !lines.is_null() {
        for i in 0..lines_len {
            lines_vec.push(unsafe { *lines.add(i) });
        }
    }
    c.set_breakpoints(path, &lines_vec);
}

#[no_mangle]
pub extern "C" fn rust_dap_continue(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.continue_debug();
}

#[no_mangle]
pub extern "C" fn rust_dap_next(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.next();
}

#[no_mangle]
pub extern "C" fn rust_dap_step_in(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.step_in();
}

#[no_mangle]
pub extern "C" fn rust_dap_step_out(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.step_out();
}

#[no_mangle]
pub extern "C" fn rust_dap_pause(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.pause();
}

#[no_mangle]
pub extern "C" fn rust_dap_disconnect(client: *mut RustDapClient) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.disconnect();
}

#[no_mangle]
pub extern "C" fn rust_dap_stack_trace(client: *mut RustDapClient, thread_id: i32) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.stack_trace(thread_id);
}

#[no_mangle]
pub extern "C" fn rust_dap_scopes(client: *mut RustDapClient, frame_id: i32) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.scopes(frame_id);
}

#[no_mangle]
pub extern "C" fn rust_dap_variables(client: *mut RustDapClient, var_ref: i32) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.variables(var_ref);
}

#[no_mangle]
pub extern "C" fn rust_dap_evaluate(
    client: *mut RustDapClient, expression: *const c_char, frame_id: i32, context: *const c_char
) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.evaluate(unsafe { ptr_to_str(expression) }, frame_id, unsafe { ptr_to_str(context) });
}

#[no_mangle]
pub extern "C" fn rust_dap_feed_message(client: *mut RustDapClient, json_data: *const c_char) {
    let c = unsafe { &mut *(client as *mut DapClient) };
    c.feed_message(unsafe { ptr_to_str(json_data) });
}

// ── DAP callback setters ─────────────────────────────────────────────

#[no_mangle]
pub extern "C" fn rust_dap_on_server_started(c: *mut RustDapClient, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_server_started(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_server_failed(c: *mut RustDapClient, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_server_failed(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_initialized(c: *mut RustDapClient, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_initialized(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_stopped(c: *mut RustDapClient, cb: OnDapStopped, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_stopped(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_continued(c: *mut RustDapClient, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_continued(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_breakpoints(c: *mut RustDapClient, cb: OnDapBreakpoints, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_breakpoints(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_stack_trace(c: *mut RustDapClient, cb: OnStackFrames, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_stack_trace(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_scopes(c: *mut RustDapClient, cb: OnScopes, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_scopes(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_variables(c: *mut RustDapClient, cb: OnVariables, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_variables(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_dap_on_evaluation(c: *mut RustDapClient, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(c as *mut DapClient)).set_on_evaluation(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Plugin Manager
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_plugin_manager_new, PluginManager, RustPluginManager);
make_free!(rust_plugin_manager_free, PluginManager, RustPluginManager);

#[no_mangle]
pub extern "C" fn rust_pm_load_plugins(pm: *mut RustPluginManager, path: *const c_char) -> bool {
    let pm = unsafe { &mut *(pm as *mut PluginManager) };
    pm.load_plugins(unsafe { ptr_to_str(path) }).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_pm_load_plugin(pm: *mut RustPluginManager, file_path: *const c_char) -> bool {
    let pm = unsafe { &mut *(pm as *mut PluginManager) };
    pm.load_plugin(unsafe { ptr_to_str(file_path) }).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_pm_unload_plugin(pm: *mut RustPluginManager, id: *const c_char) {
    let pm = unsafe { &mut *(pm as *mut PluginManager) };
    pm.unload_plugin(unsafe { ptr_to_str(id) });
}

#[no_mangle]
pub extern "C" fn rust_pm_unload_all(pm: *mut RustPluginManager) {
    let pm = unsafe { &mut *(pm as *mut PluginManager) };
    pm.unload_all();
}

#[no_mangle]
pub extern "C" fn rust_pm_is_loaded(pm: *const RustPluginManager, id: *const c_char) -> bool {
    let pm = unsafe { &*(pm as *const PluginManager) };
    pm.is_loaded(unsafe { ptr_to_str(id) })
}

#[no_mangle]
pub extern "C" fn rust_pm_plugin_version(pm: *const RustPluginManager, id: *const c_char) -> *mut c_char {
    let pm = unsafe { &*(pm as *const PluginManager) };
    pm.plugin_version(unsafe { ptr_to_str(id) })
        .map(crate::str_to_cstring)
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn rust_pm_list_loaded(pm: *const RustPluginManager, out_len: *mut usize) -> *mut *mut c_char {
    let pm = unsafe { &*(pm as *const PluginManager) };
    let list = pm.list_loaded();
    let len = list.len();
    unsafe { *out_len = len; }
    if len == 0 { return std::ptr::null_mut(); }
    let mut arr = Vec::with_capacity(len);
    for s in list {
        arr.push(crate::str_to_cstring(&s));
    }
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_pm_free_strings(strs: *mut *mut c_char, len: usize) {
    if strs.is_null() { return; }
    unsafe {
        // Free each individual CString
        for i in 0..len {
            let ptr = *strs.add(i);
            if !ptr.is_null() {
                let _ = CString::from_raw(ptr);
            }
        }
        // Free the outer array buffer (reconstruct Vec to drop it)
        let _ = Vec::from_raw_parts(strs, len, len);
    }
}

#[no_mangle]
pub extern "C" fn rust_pm_build_dep_graph(
    pm: *mut RustPluginManager, metadata_jsons: *const *const c_char, count: usize
) -> bool {
    let pm = unsafe { &mut *(pm as *mut PluginManager) };
    let mut vec: Vec<&str> = Vec::with_capacity(count);
    for i in 0..count {
        vec.push(unsafe { ptr_to_str(*metadata_jsons.add(i)) });
    }
    pm.build_dependency_graph(&vec).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_pm_topological_sort(
    pm: *const RustPluginManager, out_len: *mut usize
) -> *mut *mut c_char {
    let pm = unsafe { &*(pm as *const PluginManager) };
    let order = pm.topological_sort();
    unsafe { *out_len = order.len(); }
    if order.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = order.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

// ── Plugin Manager Callback Setters ──────────────────────────────────

#[no_mangle]
pub extern "C" fn rust_pm_on_plugin_loaded(pm: *mut RustPluginManager, cb: OnPluginEvent, u: *mut c_void) {
    unsafe { (&mut *(pm as *mut PluginManager)).set_on_loaded(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_pm_on_plugin_unloaded(pm: *mut RustPluginManager, cb: OnPluginEvent, u: *mut c_void) {
    unsafe { (&mut *(pm as *mut PluginManager)).set_on_unloaded(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_pm_on_plugin_error(pm: *mut RustPluginManager, cb: OnPluginEvent, u: *mut c_void) {
    unsafe { (&mut *(pm as *mut PluginManager)).set_on_error(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Plugin Crash Handler
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_crash_handler_new, PluginCrashHandler, RustPluginCrashHandler);
make_free!(rust_crash_handler_free, PluginCrashHandler, RustPluginCrashHandler);

#[no_mangle]
pub extern "C" fn rust_crash_handler_on_crash(
    h: *mut RustPluginCrashHandler, cb: OnPluginEvent, u: *mut c_void
) {
    unsafe { (&mut *(h as *mut PluginCrashHandler)).set_on_crash(cb, u); }
}

#[no_mangle]
pub extern "C" fn rust_crash_handler_report_crash(
    h: *mut RustPluginCrashHandler,
    plugin_id: *const c_char,
    error: *const c_char,
) {
    if h.is_null() || plugin_id.is_null() || error.is_null() {
        return;
    }
    unsafe {
        let handler = &*(h as *mut PluginCrashHandler);
        let id = if !plugin_id.is_null() {
            std::ffi::CStr::from_ptr(plugin_id).to_str().unwrap_or("")
        } else {
            ""
        };
        let err = if !error.is_null() {
            std::ffi::CStr::from_ptr(error).to_str().unwrap_or("")
        } else {
            ""
        };
        handler.report_crash(id, err);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Plugin Registry
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_plugin_registry_new, PluginRegistry, RustPluginRegistry);
make_free!(rust_plugin_registry_free, PluginRegistry, RustPluginRegistry);

#[no_mangle]
pub extern "C" fn rust_plugin_registry_set_url(reg: *mut RustPluginRegistry, url: *const c_char) {
    unsafe { (&*(reg as *mut PluginRegistry)).set_url(ptr_to_str(url)); }
}

#[no_mangle]
pub extern "C" fn rust_plugin_registry_get_url(reg: *mut RustPluginRegistry) -> *mut c_char {
    crate::str_to_cstring(&unsafe { &*(reg as *mut PluginRegistry) }.get_url())
}

#[no_mangle]
pub extern "C" fn rust_plugin_registry_check_updates(reg: *mut RustPluginRegistry) {
    unsafe { (&*(reg as *mut PluginRegistry)).check_updates(); }
}

#[no_mangle]
pub extern "C" fn rust_plugin_registry_upgrade_available(
    reg: *mut RustPluginRegistry, id: *const c_char, current_ver: *const c_char
) -> bool {
    unsafe { (&*(reg as *mut PluginRegistry)).upgrade_available(ptr_to_str(id), ptr_to_str(current_ver)) }
}

#[no_mangle]
pub extern "C" fn rust_plugin_registry_on_update(reg: *mut RustPluginRegistry, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(reg as *mut PluginRegistry)).set_on_update(cb, u); }
}

#[no_mangle]
pub extern "C" fn rust_plugin_registry_on_install_failed(reg: *mut RustPluginRegistry, cb: OnPluginEvent, u: *mut c_void) {
    unsafe { (&mut *(reg as *mut PluginRegistry)).set_on_install_failed(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Service Locator
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_service_locator_new, ServiceLocator, RustServiceLocator);
make_free!(rust_service_locator_free, ServiceLocator, RustServiceLocator);

#[no_mangle]
pub extern "C" fn rust_service_locator_register(
    sl: *mut RustServiceLocator, id: *const c_char, service: *mut c_void
) {
    unsafe { (&*(sl as *mut ServiceLocator)).register(ptr_to_str(id), service); }
}

#[no_mangle]
pub extern "C" fn rust_service_locator_get(
    sl: *mut RustServiceLocator, id: *const c_char
) -> *mut c_void {
    unsafe { (&*(sl as *mut ServiceLocator)).get(ptr_to_str(id)).unwrap_or(std::ptr::null_mut()) }
}

#[no_mangle]
pub extern "C" fn rust_service_locator_unregister(
    sl: *mut RustServiceLocator, id: *const c_char
) {
    unsafe { (&*(sl as *mut ServiceLocator)).unregister(ptr_to_str(id)); }
}

#[no_mangle]
pub extern "C" fn rust_service_locator_has(
    sl: *mut RustServiceLocator, id: *const c_char
) -> bool {
    unsafe { (&*(sl as *mut ServiceLocator)).has(ptr_to_str(id)) }
}

#[no_mangle]
pub extern "C" fn rust_service_locator_list(
    sl: *mut RustServiceLocator, out_len: *mut usize
) -> *mut *mut c_char {
    let list = unsafe { (&*(sl as *mut ServiceLocator)).list() };
    unsafe { *out_len = list.len(); }
    if list.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = list.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_service_locator_free_list(strs: *mut *mut c_char, len: usize) {
    rust_pm_free_strings(strs, len);
}

// ═══════════════════════════════════════════════════════════════════════
//  Dependency Resolver
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_dep_resolver_new, DependencyResolver, RustDependencyResolver);
make_free!(rust_dep_resolver_free, DependencyResolver, RustDependencyResolver);

/// Add a plugin with its metadata JSON to the resolver.
/// Returns true on success, false on parse failure (check rust_last_error()).
#[no_mangle]
pub extern "C" fn rust_dep_resolver_add_plugin(
    r: *mut RustDependencyResolver,
    id: *const c_char,
    metadata_json: *const c_char,
) -> bool {
    let r = unsafe { &mut *(r as *mut DependencyResolver) };
    r.add_plugin(unsafe { ptr_to_str(id) }, unsafe { ptr_to_str(metadata_json) }).is_ok()
}

/// Resolve all added plugins and return the topological sort order.
/// Returns an allocated array of C strings (caller must free via
/// rust_dep_resolver_free_order). Sets out_len to the number of items.
/// Returns null on error (e.g. circular dependency).
#[no_mangle]
pub extern "C" fn rust_dep_resolver_order(
    r: *mut RustDependencyResolver, out_len: *mut usize
) -> *mut *mut c_char {
    let r = unsafe { &*(r as *const DependencyResolver) };
    let order = r.resolve_order();
    unsafe { *out_len = order.len(); }
    if order.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = order.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_dep_resolver_free_order(strs: *mut *mut c_char, len: usize) {
    rust_pm_free_strings(strs, len);
}

/// Clear all registered plugins from the resolver.
#[no_mangle]
pub extern "C" fn rust_dep_resolver_clear(r: *mut RustDependencyResolver) {
    unsafe {
        let r = &mut *(r as *mut DependencyResolver);
        *r = DependencyResolver::new();
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Task Runner
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_task_runner_new, TaskRunner, RustTaskRunner);
make_free!(rust_task_runner_free, TaskRunner, RustTaskRunner);

#[no_mangle]
pub extern "C" fn rust_task_runner_load(
    runner: *mut RustTaskRunner, json_tasks: *const c_char
) -> bool {
    let r = unsafe { &mut *(runner as *mut TaskRunner) };
    r.load(unsafe { ptr_to_str(json_tasks) }).is_ok()
}

#[no_mangle]
pub extern "C" fn rust_task_runner_run(runner: *mut RustTaskRunner, task_name: *const c_char) {
    let r = unsafe { &mut *(runner as *mut TaskRunner) };
    r.run(unsafe { ptr_to_str(task_name) });
}

#[no_mangle]
pub extern "C" fn rust_task_runner_stop(runner: *mut RustTaskRunner) {
    let r = unsafe { &mut *(runner as *mut TaskRunner) };
    r.stop();
}

#[no_mangle]
pub extern "C" fn rust_task_runner_available(
    runner: *mut RustTaskRunner, out_len: *mut usize
) -> *mut *mut c_char {
    let r = unsafe { &mut *(runner as *mut TaskRunner) };
    let avail = r.available_tasks();
    unsafe { *out_len = avail.len(); }
    if avail.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = avail.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_task_runner_on_started(r: *mut RustTaskRunner, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(r as *mut TaskRunner)).set_on_started(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_task_runner_on_finished(r: *mut RustTaskRunner, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(r as *mut TaskRunner)).set_on_finished(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_task_runner_on_output(r: *mut RustTaskRunner, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(r as *mut TaskRunner)).set_on_output(cb, u); }
}
#[no_mangle]
pub extern "C" fn rust_task_runner_on_error(r: *mut RustTaskRunner, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(r as *mut TaskRunner)).set_on_error(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Updater
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_updater_new, Updater, RustUpdater);
make_free!(rust_updater_free, Updater, RustUpdater);

#[no_mangle]
pub extern "C" fn rust_updater_check(
    updater: *mut RustUpdater, current_version: *const c_char, update_url: *const c_char
) {
    let u = unsafe { &mut *(updater as *mut Updater) };
    u.check(unsafe { ptr_to_str(current_version) }, unsafe { ptr_to_str(update_url) });
}

#[no_mangle]
pub extern "C" fn rust_updater_is_update_available(updater: *const RustUpdater) -> bool {
    unsafe { (&*(updater as *const Updater)).is_update_available() }
}

#[no_mangle]
pub extern "C" fn rust_updater_latest_version(updater: *const RustUpdater) -> *mut c_char {
    crate::str_to_cstring(unsafe { (&*(updater as *const Updater)).latest_version() })
}

#[no_mangle]
pub extern "C" fn rust_updater_on_update_available(u: *mut RustUpdater, cb: OnStringMessage, user: *mut c_void) {
    unsafe { (&mut *(u as *mut Updater)).set_on_update_available(cb, user); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Plugin Updater
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_plugin_updater_new, PluginUpdater, RustPluginUpdater);
make_free!(rust_plugin_updater_free, PluginUpdater, RustPluginUpdater);

#[no_mangle]
pub extern "C" fn rust_plugin_updater_check(
    pu: *mut RustPluginUpdater, plugin_id: *const c_char, current_version: *const c_char
) {
    unsafe { (&*(pu as *mut PluginUpdater)).check(ptr_to_str(plugin_id), ptr_to_str(current_version)); }
}

#[no_mangle]
pub extern "C" fn rust_plugin_updater_on_update(pu: *mut RustPluginUpdater, cb: OnPluginEvent, u: *mut c_void) {
    unsafe { (&mut *(pu as *mut PluginUpdater)).set_on_update(cb, u); }
}

#[no_mangle]
pub extern "C" fn rust_plugin_updater_on_progress(pu: *mut RustPluginUpdater, cb: OnProgress, u: *mut c_void) {
    unsafe { (&mut *(pu as *mut PluginUpdater)).set_on_progress(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Version Fetcher
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_version_fetcher_new, VersionFetcher, RustVersionFetcher);
make_free!(rust_version_fetcher_free, VersionFetcher, RustVersionFetcher);

#[no_mangle]
pub extern "C" fn rust_version_fetcher_fetch(vf: *mut RustVersionFetcher, url: *const c_char) {
    unsafe { (&mut *(vf as *mut VersionFetcher)).fetch(ptr_to_str(url)); }
}

#[no_mangle]
pub extern "C" fn rust_version_fetcher_latest(vf: *mut RustVersionFetcher) -> *mut c_char {
    crate::str_to_cstring(unsafe { (&*(vf as *mut VersionFetcher)).latest() })
}

#[no_mangle]
pub extern "C" fn rust_version_fetcher_on_fetched(vf: *mut RustVersionFetcher, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(vf as *mut VersionFetcher)).set_on_fetched(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Workspace
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_workspace_new, Workspace, RustWorkspace);
make_free!(rust_workspace_free, Workspace, RustWorkspace);

#[no_mangle]
pub extern "C" fn rust_workspace_load(ws: *mut RustWorkspace, path: *const c_char) -> bool {
    unsafe { (&mut *(ws as *mut Workspace)).load(ptr_to_str(path)).is_ok() }
}

#[no_mangle]
pub extern "C" fn rust_workspace_save(ws: *mut RustWorkspace) -> bool {
    unsafe { (&*(ws as *mut Workspace)).save().is_ok() }
}

#[no_mangle]
pub extern "C" fn rust_workspace_save_as(ws: *mut RustWorkspace, path: *const c_char) -> bool {
    unsafe { (&mut *(ws as *mut Workspace)).save_as(ptr_to_str(path)).is_ok() }
}

#[no_mangle]
pub extern "C" fn rust_workspace_folders(ws: *mut RustWorkspace, out_len: *mut usize) -> *mut *mut c_char {
    let folders = unsafe { (&*(ws as *mut Workspace)).folders().to_vec() };
    unsafe { *out_len = folders.len(); }
    if folders.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = folders.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_workspace_set_folders(
    ws: *mut RustWorkspace, folders: *const *const c_char, count: usize
) {
    let ws = unsafe { &mut *(ws as *mut Workspace) };
    let mut vec: Vec<String> = Vec::with_capacity(count);
    for i in 0..count {
        vec.push(unsafe { ptr_to_str(*folders.add(i)) }.to_string());
    }
    ws.set_folders(vec);
}

#[no_mangle]
pub extern "C" fn rust_workspace_get_settings(ws: *mut RustWorkspace) -> *mut c_char {
    let s = unsafe { (&*(ws as *mut Workspace)).settings() };
    crate::str_to_cstring(&s)
}

#[no_mangle]
pub extern "C" fn rust_workspace_set_settings(ws: *mut RustWorkspace, json_settings: *const c_char) {
    unsafe { (&mut *(ws as *mut Workspace)).set_settings(ptr_to_str(json_settings)); }
}

#[no_mangle]
pub extern "C" fn rust_workspace_recent_files(ws: *mut RustWorkspace, out_len: *mut usize) -> *mut *mut c_char {
    let files = unsafe { (&*(ws as *mut Workspace)).recent_files().to_vec() };
    unsafe { *out_len = files.len(); }
    if files.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = files.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_workspace_add_recent(ws: *mut RustWorkspace, path: *const c_char) {
    unsafe { (&mut *(ws as *mut Workspace)).add_recent_file(ptr_to_str(path)); }
}

#[no_mangle]
pub extern "C" fn rust_workspace_path(ws: *mut RustWorkspace) -> *mut c_char {
    crate::str_to_cstring(unsafe { (&*(ws as *mut Workspace)).path() })
}

#[no_mangle]
pub extern "C" fn rust_workspace_is_loaded(ws: *mut RustWorkspace) -> bool {
    unsafe { (&*(ws as *mut Workspace)).is_loaded() }
}

// ═══════════════════════════════════════════════════════════════════════
//  Config Validator
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_config_validator_new, ConfigValidator, RustConfigValidator);
make_free!(rust_config_validator_free, ConfigValidator, RustConfigValidator);

#[no_mangle]
pub extern "C" fn rust_config_validator_validate(
    cv: *mut RustConfigValidator, json_config: *const c_char, schema_json: *const c_char
) -> *mut c_char {
    let result = unsafe {
        (&*(cv as *mut ConfigValidator)).validate(ptr_to_str(json_config), ptr_to_str(schema_json))
    };
    crate::str_to_cstring(&result)
}

#[no_mangle]
pub extern "C" fn rust_config_validator_on_error(cv: *mut RustConfigValidator, cb: OnStringMessage, u: *mut c_void) {
    unsafe { (&mut *(cv as *mut ConfigValidator)).set_on_error(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Archive Extractor
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_archive_extractor_new, ArchiveExtractor, RustArchiveExtractor);
make_free!(rust_archive_extractor_free, ArchiveExtractor, RustArchiveExtractor);

#[no_mangle]
pub extern "C" fn rust_archive_extractor_extract(
    ae: *mut RustArchiveExtractor, archive_data: *const u8, data_len: usize, dest_dir: *const c_char
) -> bool {
    let data = unsafe { std::slice::from_raw_parts(archive_data, data_len) };
    unsafe { (&*(ae as *mut ArchiveExtractor)).extract(data, ptr_to_str(dest_dir)).is_ok() }
}

#[no_mangle]
pub extern "C" fn rust_archive_extractor_on_progress(
    ae: *mut RustArchiveExtractor, cb: OnProgress, u: *mut c_void
) {
    unsafe { (&mut *(ae as *mut ArchiveExtractor)).set_on_progress(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Permission Manager
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_permission_manager_new, PermissionManager, RustPermissionManager);
make_free!(rust_permission_manager_free, PermissionManager, RustPermissionManager);

#[no_mangle]
pub extern "C" fn rust_permission_manager_check(
    pm: *mut RustPermissionManager, plugin_id: *const c_char, perm: i32
) -> bool {
    unsafe { (&*(pm as *mut PermissionManager)).check(ptr_to_str(plugin_id), perm) }
}

#[no_mangle]
pub extern "C" fn rust_permission_manager_request(
    pm: *mut RustPermissionManager, plugin_id: *const c_char, perm: i32
) {
    unsafe { (&*(pm as *mut PermissionManager)).request(ptr_to_str(plugin_id), perm); }
}

#[no_mangle]
pub extern "C" fn rust_permission_manager_grant(
    pm: *mut RustPermissionManager, plugin_id: *const c_char, perm: i32
) {
    unsafe { (&*(pm as *mut PermissionManager)).grant(ptr_to_str(plugin_id), perm); }
}

#[no_mangle]
pub extern "C" fn rust_permission_manager_revoke(
    pm: *mut RustPermissionManager, plugin_id: *const c_char, perm: i32
) {
    unsafe { (&*(pm as *mut PermissionManager)).revoke(ptr_to_str(plugin_id), perm); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Debug Session
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_debug_session_new, DebugSession, RustDebugSession);
make_free!(rust_debug_session_free, DebugSession, RustDebugSession);

#[no_mangle]
pub extern "C" fn rust_debug_session_start(
    session: *mut RustDebugSession, config_json: *const c_char
) {
    unsafe { (&mut *(session as *mut DebugSession)).start(ptr_to_str(config_json)); }
}

#[no_mangle]
pub extern "C" fn rust_debug_session_stop(session: *mut RustDebugSession) {
    unsafe { (&mut *(session as *mut DebugSession)).stop(); }
}

#[no_mangle]
pub extern "C" fn rust_debug_session_on_state_change(
    session: *mut RustDebugSession, cb: OnStringMessage, u: *mut c_void
) {
    unsafe { (&mut *(session as *mut DebugSession)).set_on_state_change(cb, u); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Debug Configuration Manager
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_debug_config_manager_new, DebugConfigurationManager, RustDebugConfigurationManager);
make_free!(rust_debug_config_manager_free, DebugConfigurationManager, RustDebugConfigurationManager);

#[no_mangle]
pub extern "C" fn rust_debug_config_manager_load(
    m: *mut RustDebugConfigurationManager, json_config: *const c_char
) -> bool {
    unsafe { (&mut *(m as *mut DebugConfigurationManager)).load(ptr_to_str(json_config)).is_ok() }
}

#[no_mangle]
pub extern "C" fn rust_debug_config_manager_list(
    m: *mut RustDebugConfigurationManager, out_len: *mut usize
) -> *mut *mut c_char {
    let list = unsafe { (&*(m as *mut DebugConfigurationManager)).list() };
    unsafe { *out_len = list.len(); }
    if list.is_empty() { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = list.into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_debug_config_manager_get(
    m: *mut RustDebugConfigurationManager, name: *const c_char
) -> *mut c_char {
    let result = unsafe { (&*(m as *mut DebugConfigurationManager)).get(ptr_to_str(name)) };
    result.map(crate::str_to_cstring).unwrap_or(std::ptr::null_mut())
}

// ═══════════════════════════════════════════════════════════════════════
//  Language Registry
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_language_registry_new, LanguageRegistry, RustLanguageRegistry);
make_free!(rust_language_registry_free, LanguageRegistry, RustLanguageRegistry);

#[no_mangle]
pub extern "C" fn rust_language_registry_register(
    lr: *mut RustLanguageRegistry,
    lang_id: *const c_char, name: *const c_char,
    extensions: *const c_char,
    server_command: *const c_char, server_args: *const c_char
) {
    let cmd = if server_command.is_null() { None } else { Some(unsafe { ptr_to_str(server_command) }) };
    let args: Vec<&str> = if server_args.is_null() {
        Vec::new()
    } else {
        unsafe { ptr_to_str(server_args) }.split(',').collect()
    };
    unsafe {
        (&mut *(lr as *mut LanguageRegistry)).register(
            ptr_to_str(lang_id), ptr_to_str(name), ptr_to_str(extensions),
            cmd, &args
        );
    }
}

#[no_mangle]
pub extern "C" fn rust_language_registry_unregister(
    lr: *mut RustLanguageRegistry, lang_id: *const c_char
) {
    unsafe { (&mut *(lr as *mut LanguageRegistry)).unregister(ptr_to_str(lang_id)); }
}

#[no_mangle]
pub extern "C" fn rust_language_registry_get(
    lr: *mut RustLanguageRegistry, lang_id: *const c_char
) -> *mut c_char {
    let result = unsafe { (&*(lr as *mut LanguageRegistry)).get(ptr_to_str(lang_id)) };
    result.map(|s| crate::str_to_cstring(&s)).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn rust_language_registry_detect(
    lr: *mut RustLanguageRegistry, filename: *const c_char
) -> *mut c_char {
    let result = unsafe { (&*(lr as *mut LanguageRegistry)).detect(ptr_to_str(filename)) };
    result.map(crate::str_to_cstring).unwrap_or(std::ptr::null_mut())
}

/// Return a JSON array of all registered language names.
#[no_mangle]
pub extern "C" fn rust_language_registry_names_json(
    lr: *mut RustLanguageRegistry,
) -> *mut c_char {
    let reg = unsafe { &*(lr as *mut LanguageRegistry) };
    let names: Vec<String> = reg.languages();
    crate::str_to_cstring(&serde_json::to_string(&names).unwrap_or_default())
}

/// Full definition of one language as JSON (null if unknown).
#[no_mangle]
pub extern "C" fn rust_language_registry_definition_json(
    lr: *mut RustLanguageRegistry, lang_id: *const c_char,
) -> *mut c_char {
    let reg = unsafe { &*(lr as *mut LanguageRegistry) };
    reg.definition_json(unsafe { ptr_to_str(lang_id) })
        .map(|s| crate::str_to_cstring(&s))
        .unwrap_or(std::ptr::null_mut())
}

/// JSON array of ALL language definitions, in registration order.
#[no_mangle]
pub extern "C" fn rust_language_registry_all_definitions_json(
    lr: *mut RustLanguageRegistry,
) -> *mut c_char {
    let reg = unsafe { &*(lr as *mut LanguageRegistry) };
    crate::str_to_cstring(&reg.definitions_json())
}

/// Register/overwrite a language from a full definition JSON document.
/// Returns true on success; false on parse failure (see rust_last_error()).
#[no_mangle]
pub extern "C" fn rust_language_registry_register_definition_json(
    lr: *mut RustLanguageRegistry, definition_json: *const c_char,
) -> bool {
    let reg = unsafe { &mut *(lr as *mut LanguageRegistry) };
    match reg.register_definition_json(unsafe { ptr_to_str(definition_json) }) {
        Ok(()) => true,
        Err(e) => {
            crate::set_last_error(&e);
            false
        }
    }
}

/// Language id for a file path (falls back to "text").
#[no_mangle]
pub extern "C" fn rust_language_registry_language_for_file(
    lr: *mut RustLanguageRegistry, path: *const c_char,
) -> *mut c_char {
    let reg = unsafe { &*(lr as *mut LanguageRegistry) };
    crate::str_to_cstring(&reg.language_for_file(unsafe { ptr_to_str(path) }))
}

// ═══════════════════════════════════════════════════════════════════════
//  Language Server Manager
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_ls_manager_new, LanguageServerManager, RustLanguageServerManager);
make_free!(rust_ls_manager_free, LanguageServerManager, RustLanguageServerManager);

#[no_mangle]
pub extern "C" fn rust_ls_manager_start(
    m: *mut RustLanguageServerManager,
    lang_id: *const c_char, command: *const c_char,
    args: *const *const c_char, args_len: usize,
    root_uri: *const c_char
) {
    let mut args_vec: Vec<&str> = Vec::with_capacity(args_len);
    for i in 0..args_len {
        args_vec.push(unsafe { ptr_to_str(*args.add(i)) });
    }
    unsafe {
        let _ = (&*(m as *mut LanguageServerManager)).start(
            ptr_to_str(lang_id), ptr_to_str(command), &args_vec, ptr_to_str(root_uri)
        );
    }
}

#[no_mangle]
pub extern "C" fn rust_ls_manager_stop(m: *mut RustLanguageServerManager, lang_id: *const c_char) {
    unsafe { (&*(m as *mut LanguageServerManager)).stop(ptr_to_str(lang_id)); }
}

#[no_mangle]
pub extern "C" fn rust_ls_manager_stop_all(m: *mut RustLanguageServerManager) {
    unsafe { (&*(m as *mut LanguageServerManager)).stop_all(); }
}

// ═══════════════════════════════════════════════════════════════════════
//  Diff Engine
// ═══════════════════════════════════════════════════════════════════════

/// Compute diff between two texts and return JSON array of hunks
#[no_mangle]
pub extern "C" fn rust_diff_compute(left: *const c_char, right: *const c_char) -> *mut c_char {
    let l = unsafe { ptr_to_str(left) };
    let r = unsafe { ptr_to_str(right) };
    crate::str_to_cstring(&crate::diff_engine::compute_diff_json(l, r))
}

// ═══════════════════════════════════════════════════════════════════════
//  Encoding Engine
// ═══════════════════════════════════════════════════════════════════════

/// Detect encoding of a file
#[no_mangle]
pub extern "C" fn rust_encoding_detect(file_path: *const c_char) -> *mut c_char {
    let path = unsafe { ptr_to_str(file_path) };
    crate::str_to_cstring(&crate::encoding_engine::detect_encoding(path))
}

/// Detect line ending style
#[no_mangle]
pub extern "C" fn rust_encoding_detect_line_ending(file_path: *const c_char) -> *mut c_char {
    let path = unsafe { ptr_to_str(file_path) };
    crate::str_to_cstring(&crate::encoding_engine::detect_line_ending(path))
}

/// Check if file has BOM
#[no_mangle]
pub extern "C" fn rust_encoding_has_bom(file_path: *const c_char) -> bool {
    let path = unsafe { ptr_to_str(file_path) };
    crate::encoding_engine::has_bom(path)
}

/// Convert line endings
#[no_mangle]
pub extern "C" fn rust_encoding_convert_line_endings(
    content: *const c_char, from_style: *const c_char, to_style: *const c_char
) -> *mut c_char {
    let c = unsafe { ptr_to_str(content) };
    let from = unsafe { ptr_to_str(from_style) };
    let to = unsafe { ptr_to_str(to_style) };
    crate::str_to_cstring(&crate::encoding_engine::convert_line_endings(c, from, to))
}

/// Read file with specific encoding
#[no_mangle]
pub extern "C" fn rust_encoding_read(file_path: *const c_char, encoding: *const c_char) -> *mut c_char {
    let path = unsafe { ptr_to_str(file_path) };
    let enc = unsafe { ptr_to_str(encoding) };
    match crate::encoding_engine::read_file_with_encoding(path, enc) {
        Ok(content) => crate::str_to_cstring(&content),
        Err(e) => crate::str_to_cstring(&e),
    }
}

/// Write file with specific encoding
#[no_mangle]
pub extern "C" fn rust_encoding_write(
    file_path: *const c_char, content: *const c_char, encoding: *const c_char
) -> bool {
    let path = unsafe { ptr_to_str(file_path) };
    let c = unsafe { ptr_to_str(content) };
    let enc = unsafe { ptr_to_str(encoding) };
    crate::encoding_engine::write_file_with_encoding(path, c, enc).is_ok()
}

/// Get supported encodings as JSON array
#[no_mangle]
pub extern "C" fn rust_encoding_supported() -> *mut c_char {
    let encodings = crate::encoding_engine::supported_encodings();
    let mut json = String::from("[");
    for (i, (name, display)) in encodings.iter().enumerate() {
        if i > 0 { json.push(','); }
        json.push_str(&format!(r#"{{"name":"{}","display":"{}"}}"#, name, display));
    }
    json.push(']');
    crate::str_to_cstring(&json)
}

// ═══════════════════════════════════════════════════════════════════════
//  Blame Engine
// ═══════════════════════════════════════════════════════════════════════

/// Parse git blame porcelain output and return JSON
#[no_mangle]
pub extern "C" fn rust_blame_parse(output: *const c_char) -> *mut c_char {
    let out = unsafe { ptr_to_str(output) };
    crate::str_to_cstring(&crate::blame_engine::parse_blame_json(out))
}

// ═══════════════════════════════════════════════════════════════════════
//  Emmet Engine
// ═══════════════════════════════════════════════════════════════════════

/// Expand Emmet abbreviation to HTML/CSS
#[no_mangle]
pub extern "C" fn rust_emmet_expand(abbreviation: *const c_char) -> *mut c_char {
    let abbr = unsafe { ptr_to_str(abbreviation) };
    crate::str_to_cstring(&crate::emmet_engine::expand_emmet(abbr))
}

/// Expand Emmet and return JSON with expanded text and CSS flag
#[no_mangle]
pub extern "C" fn rust_emmet_expand_json(abbreviation: *const c_char) -> *mut c_char {
    let abbr = unsafe { ptr_to_str(abbreviation) };
    crate::str_to_cstring(&crate::emmet_engine::expand_emmet_json(abbr))
}

/// Check if text is a CSS shorthand
#[no_mangle]
pub extern "C" fn rust_emmet_is_css_shorthand(text: *const c_char) -> bool {
    let t = unsafe { ptr_to_str(text) };
    crate::emmet_engine::is_css_shorthand(t)
}

// ═══════════════════════════════════════════════════════════════════════
//  Session Engine
// ═══════════════════════════════════════════════════════════════════════

/// Check if a saved session exists
#[no_mangle]
pub extern "C" fn rust_session_has_saved() -> bool {
    crate::session_engine::has_saved_session()
}

/// Clear saved session
#[no_mangle]
pub extern "C" fn rust_session_clear() -> bool {
    crate::session_engine::clear_session().is_ok()
}

/// Load session JSON and return it
#[no_mangle]
pub extern "C" fn rust_session_load() -> *mut c_char {
    match crate::session_engine::load_session() {
        Ok(session) => {
            let json = serde_json::to_string(&session).unwrap_or_default();
            crate::str_to_cstring(&json)
        }
        Err(_) => std::ptr::null_mut(),
    }
}

/// Save session from JSON string
#[no_mangle]
pub extern "C" fn rust_session_save(json: *const c_char) -> bool {
    let data = unsafe { ptr_to_str(json) };
    if let Ok(session) = serde_json::from_str::<crate::session_engine::EditorSession>(data) {
        crate::session_engine::save_session(&session).is_ok()
    } else {
        false
    }
}

/// Check if hot exit data exists
#[no_mangle]
pub extern "C" fn rust_session_has_hot_exit() -> bool {
    crate::session_engine::hot_exit_dir().join("index.json").exists()
}

/// Load hot exit tabs JSON
#[no_mangle]
pub extern "C" fn rust_session_load_hot_exit() -> *mut c_char {
    match crate::session_engine::load_hot_exit() {
        Ok(tabs) => {
            let json = serde_json::to_string(&tabs).unwrap_or_default();
            crate::str_to_cstring(&json)
        }
        Err(_) => std::ptr::null_mut(),
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Test Engine
// ═══════════════════════════════════════════════════════════════════════

/// Detect test framework from project files
#[no_mangle]
pub extern "C" fn rust_test_detect_framework(project_path: *const c_char) -> *mut c_char {
    let path = unsafe { ptr_to_str(project_path) };
    crate::str_to_cstring(&crate::test_engine::detect_framework(path))
}

/// Build test command for the given framework
#[no_mangle]
pub extern "C" fn rust_test_build_command(
    framework: *const c_char,
    project_path: *const c_char,
    filter: *const c_char,
) -> *mut c_char {
    let fw = unsafe { ptr_to_str(framework) };
    let path = unsafe { ptr_to_str(project_path) };
    let filt = unsafe { ptr_to_str(filter) };
    crate::str_to_cstring(&crate::test_engine::build_command(fw, path, filt))
}

/// Parse test output into JSON TestSuite
#[no_mangle]
pub extern "C" fn rust_test_parse_output(
    framework: *const c_char,
    output: *const c_char,
) -> *mut c_char {
    let fw = unsafe { ptr_to_str(framework) };
    let out = unsafe { ptr_to_str(output) };
    let suite = crate::test_engine::parse_output(fw, out);
    let json = serde_json::to_string(&suite).unwrap_or_default();
    crate::str_to_cstring(&json)
}

// ═══════════════════════════════════════════════════════════════════════
//  UI Action Handler
// ═══════════════════════════════════════════════════════════════════════
// Every user interaction is routed through this handler. Rust validates the
// action + payload and decides; C++/Qt only draws the widgets and executes
// the returned commands. See ui_actions.rs and include/scriptura/ui_actions.h.

make_new!(rust_ui_actions_new, UiActionHandler, RustUiActionHandler);
make_free!(rust_ui_actions_free, UiActionHandler, RustUiActionHandler);

/// Route a user action through Rust. Returns a JSON document:
/// `{ "commands": [ { "cmd": ... }, ... ], "error": null | "message" }`.
/// On a rejected action the command list is empty and `error` is non-null;
/// the caller must free the returned string with rust_free_string().
#[no_mangle]
pub extern "C" fn rust_ui_actions_handle(
    h: *mut RustUiActionHandler,
    action: *const c_char,
    payload: *const c_char,
) -> *mut c_char {
    if h.is_null() || action.is_null() {
        return crate::str_to_cstring(
            &serde_json::json!({ "commands": [], "error": "null handler or action" }).to_string(),
        );
    }
    let h = unsafe { &mut *(h as *mut UiActionHandler) };
    let action_str = unsafe { ptr_to_str(action) };
    let payload_str = if payload.is_null() { "{}" } else { unsafe { ptr_to_str(payload) } };
    crate::str_to_cstring(&h.handle(action_str, payload_str))
}

/// Audit trail of handled actions (most recent last). Caller frees the
/// array with rust_pm_free_strings().
#[no_mangle]
pub extern "C" fn rust_ui_actions_log(
    h: *mut RustUiActionHandler,
    out_len: *mut usize,
) -> *mut *mut c_char {
    if h.is_null() {
        unsafe { *out_len = 0; }
        return std::ptr::null_mut();
    }
    let entries = unsafe { (&*(h as *mut UiActionHandler)).audit_log() };
    unsafe { *out_len = entries.len(); }
    if entries.is_empty() {
        return std::ptr::null_mut();
    }
    let mut arr: Vec<*mut c_char> = entries
        .into_iter()
        .map(|s| crate::str_to_cstring(&s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_text_buffer_new() -> *mut RustTextBuffer {
    Box::into_raw(Box::new(TextBuffer::new())) as *mut RustTextBuffer
}

#[no_mangle]
pub extern "C" fn rust_text_buffer_free(p: *mut RustTextBuffer) {
    if !p.is_null() {
        unsafe { let _ = Box::from_raw(p as *mut TextBuffer); }
    }
}

#[no_mangle]
pub extern "C" fn rust_text_set(b: *mut RustTextBuffer, s: *const c_char) {
    if b.is_null() { return; }
    unsafe { (&mut *(b as *mut TextBuffer)).set_text(ptr_to_str(s)); }
}

#[no_mangle]
pub extern "C" fn rust_text_get(b: *const RustTextBuffer) -> *mut c_char {
    if b.is_null() { return crate::str_to_cstring(""); }
    let s = unsafe { (&*(b as *const TextBuffer)).text() }.to_string();
    crate::str_to_cstring(&s)
}

#[no_mangle]
pub extern "C" fn rust_text_version(b: *const RustTextBuffer) -> u64 {
    if b.is_null() { return 0; }
    unsafe { (&*(b as *const TextBuffer)).version() }
}

#[no_mangle]
pub extern "C" fn rust_text_line_count(b: *const RustTextBuffer) -> usize {
    if b.is_null() { return 1; }
    unsafe { (&*(b as *const TextBuffer)).line_count() }
}

#[no_mangle]
pub extern "C" fn rust_text_line(b: *const RustTextBuffer, l: u32) -> *mut c_char {
    if b.is_null() { return crate::str_to_cstring(""); }
    let s = unsafe { (&*(b as *const TextBuffer)).line_text(l as usize) };
    crate::str_to_cstring(&s)
}

#[no_mangle]
pub extern "C" fn rust_text_insert(
    b: *mut RustTextBuffer, line: u32, col: u32,
    s: *const c_char, ol: *mut u32, oc: *mut u32,
) {
    if b.is_null() { return; }
    let t = unsafe { ptr_to_str(s) }.to_string();
    let (l, c) = unsafe { (&mut *(b as *mut TextBuffer)).insert(line, col, &t) };
    if !ol.is_null() { unsafe { *ol = l; } }
    if !oc.is_null() { unsafe { *oc = c; } }
}

#[no_mangle]
pub extern "C" fn rust_text_delete(
    b: *mut RustTextBuffer, sl: u32, sc: u32, el: u32, ec: u32,
) -> *mut c_char {
    if b.is_null() { return crate::str_to_cstring(""); }
    let d = unsafe { (&mut *(b as *mut TextBuffer)).delete_range(sl, sc, el, ec) };
    crate::str_to_cstring(&d)
}

#[no_mangle]
pub extern "C" fn rust_text_undo(b: *mut RustTextBuffer) -> bool {
    if b.is_null() { return false; }
    unsafe { (&mut *(b as *mut TextBuffer)).undo() }
}

#[no_mangle]
pub extern "C" fn rust_text_redo(b: *mut RustTextBuffer) -> bool {
    if b.is_null() { return false; }
    unsafe { (&mut *(b as *mut TextBuffer)).redo() }
}

#[no_mangle]
pub extern "C" fn rust_edit_smart_indent(
    line_text: *const c_char, tab_width: u32,
) -> *mut c_char {
    let t = unsafe { ptr_to_str(line_text) }.to_string();
    let s = crate::edit_engine::smart_indent_insert(&t, tab_width as usize);
    crate::str_to_cstring(&s)
}

#[no_mangle]
pub extern "C" fn rust_edit_bracket_decision(
    typed_utf8: *const c_char, next_utf8: *const c_char, has_next: bool,
) -> i32 {
    // Returns: 0 = insert-normal, 1 = auto-close, 2 = skip-over.
    let t = unsafe { ptr_to_str(typed_utf8) }.chars().next().unwrap_or('\0');
    let n: Option<char> = if !has_next { None }
    else { unsafe { ptr_to_str(next_utf8) }.chars().next() };
    match crate::edit_engine::bracket_decision(t, n) {
        crate::edit_engine::BracketDecision::InsertNormal => 0,
        crate::edit_engine::BracketDecision::AutoClose { .. } => 1,
        crate::edit_engine::BracketDecision::SkipOver => 2,
    }
}

#[no_mangle]
pub extern "C" fn rust_edit_bracket_close(
    typed_utf8: *const c_char, out_buf: *mut c_char, out_len: usize,
) -> bool {
    let t = unsafe { ptr_to_str(typed_utf8) }.chars().next().unwrap_or('\0');
    let close = match t {
        '(' => ')', '[' => ']', '{' => '}', '"' => '"', '\'' => '\'',
        _ => return false,
    };
    let s = close.to_string();
    let bytes = s.as_bytes();
    if out_buf.is_null() || out_len < bytes.len() + 1 { return false; }
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), out_buf as *mut u8, bytes.len());
        *(out_buf.add(bytes.len())) = 0;
    }
    true
}

/// Next occurrence in UTF-16 (Qt document position) space.
/// Returns true + sets out_start/out_end on hit.
#[no_mangle]
pub extern "C" fn rust_edit_next_occurrence_utf16(
    text: *const c_char, needle: *const c_char, from_utf16: usize,
    out_start: *mut usize, out_end: *mut usize,
) -> bool {
    let t = unsafe { ptr_to_str(text) }.to_string();
    let n = unsafe { ptr_to_str(needle) }.to_string();
    match crate::edit_engine::next_occurrence_utf16(&t, &n, from_utf16) {
        Some((s, e)) => {
            if !out_start.is_null() { unsafe { *out_start = s; } }
            if !out_end.is_null() { unsafe { *out_end = e; } }
            true
        }
        None => false,
    }
}

/// All occurrences in UTF-16 space as JSON [[start,end],...].
#[no_mangle]
pub extern "C" fn rust_edit_all_occurrences_utf16(
    text: *const c_char, needle: *const c_char,
) -> *mut c_char {
    let t = unsafe { ptr_to_str(text) }.to_string();
    let n = unsafe { ptr_to_str(needle) }.to_string();
    if n.is_empty() { return crate::str_to_cstring("[]"); }
    // Reuse find_all on chars, then map to UTF-16 offsets.
    let tchars: Vec<char> = t.chars().collect();
    let nchars: Vec<char> = n.chars().collect();
    let mut out = String::from("[");
    let mut first = true;
    let mut i = 0;
    while i + nchars.len() <= tchars.len() {
        if tchars[i..i + nchars.len()] == nchars[..] {
            let s = crate::edit_engine::char_offset_to_utf16(&t, i);
            let e = crate::edit_engine::char_offset_to_utf16(&t, i + nchars.len());
            if !first { out.push(','); }
            first = false;
            out.push_str(&format!("[{},{}]", s, e));
            i += nchars.len().max(1);
        } else { i += 1; }
    }
    out.push(']');
    crate::str_to_cstring(&out)
}

#[no_mangle]
pub extern "C" fn rust_search_fuzzy(
    pattern: *const c_char, text: *const c_char,
) -> i32 {
    let p = unsafe { ptr_to_str(pattern) }.to_string();
    let t = unsafe { ptr_to_str(text) }.to_string();
    crate::search_engine::fuzzy_score(&p, &t) as i32
}

#[no_mangle]
pub extern "C" fn rust_fold_compute(
    text: *const c_char, indent_based: bool,
) -> *mut c_char {
    let t = unsafe { ptr_to_str(text) }.to_string();
    let lines: Vec<String> = t.split('\n').map(|s| s.to_string()).collect();
    let mut regions = if indent_based {
        crate::fold_engine::detect_indent_folds(&lines)
    } else {
        crate::fold_engine::detect_brace_folds(&lines)
    };
    let keyword = crate::fold_engine::detect_keyword_folds(&lines);
    regions.extend(keyword);
    regions.sort_by_key(|r| (r.start_line, r.end_line));
    crate::str_to_cstring(&serde_json::to_string(&regions).unwrap_or("[]".into()))
}

#[no_mangle]
pub extern "C" fn rust_brackets_compute(text: *const c_char) -> *mut c_char {
    let t = unsafe { ptr_to_str(text) }.to_string();
    let pairs = crate::bracket_engine::find_pairs(&t);
    crate::str_to_cstring(&serde_json::to_string(&pairs).unwrap_or("[]".into()))
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_new() -> *mut RustBookmarkStore {
    Box::into_raw(Box::new(BookmarkStore::new())) as *mut RustBookmarkStore
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_free(p: *mut RustBookmarkStore) {
    if !p.is_null() {
        unsafe { let _ = Box::from_raw(p as *mut BookmarkStore); }
    }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_toggle(
    s: *mut RustBookmarkStore, file: *const c_char, line: u32, text: *const c_char,
) -> i32 {
    if s.is_null() { return -2; }
    let f = unsafe { ptr_to_str(file) }.to_string();
    let t = unsafe { ptr_to_str(text) }.to_string();
    match unsafe { (&mut *(s as *mut BookmarkStore)).toggle(&f, line, &t) } {
        Some(id) => id,
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_json(s: *const RustBookmarkStore) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring("[]"); }
    let v = unsafe { (&*(s as *const BookmarkStore)).all() };
    crate::str_to_cstring(&serde_json::to_string(&v).unwrap_or("[]".into()))
}

#[no_mangle]
pub extern "C" fn rust_snippets_new() -> *mut RustSnippetStore {
    Box::into_raw(Box::new(SnippetStore::new())) as *mut RustSnippetStore
}

#[no_mangle]
pub extern "C" fn rust_snippets_free(p: *mut RustSnippetStore) {
    if !p.is_null() {
        unsafe { let _ = Box::from_raw(p as *mut SnippetStore); }
    }
}

#[no_mangle]
pub extern "C" fn rust_snippet_expand(
    body: *const c_char, filename: *const c_char,
) -> *mut c_char {
    let b = unsafe { ptr_to_str(body) }.to_string();
    let f = unsafe { ptr_to_str(filename) }.to_string();
    let (text, stops) = crate::snippet_engine::expand(&b, &f);
    let arr: Vec<serde_json::Value> = stops.iter().map(|s|
        serde_json::json!({"offset": s.offset, "len": s.len})).collect();
    crate::str_to_cstring(
        &serde_json::json!({"text": text, "tabStops": arr}).to_string())
}

// ── Bookmark JSON (legacy Qt/camelCase shape) ──
#[no_mangle]
pub extern "C" fn rust_bookmarks_to_qt_json(
    s: *const RustBookmarkStore,
) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring("[]"); }
    let v = unsafe { (&*(s as *const BookmarkStore)).to_qt_json() };
    crate::str_to_cstring(&v)
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_load_qt_json(
    s: *mut RustBookmarkStore, json: *const c_char,
) {
    if s.is_null() { return; }
    let j = unsafe { ptr_to_str(json) }.to_string();
    unsafe { (&mut *(s as *mut BookmarkStore)).load_qt_json(&j); }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_remove(
    s: *mut RustBookmarkStore, id: i32,
) -> bool {
    if s.is_null() { return false; }
    unsafe { (&mut *(s as *mut BookmarkStore)).remove(id).is_some() }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_clear(s: *mut RustBookmarkStore) {
    if s.is_null() { return; }
    unsafe { (&mut *(s as *mut BookmarkStore)).clear(); }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_clear_file(
    s: *mut RustBookmarkStore, file: *const c_char,
) {
    if s.is_null() { return; }
    let f = unsafe { ptr_to_str(file) }.to_string();
    unsafe { (&mut *(s as *mut BookmarkStore)).clear_file(&f); }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_is_bookmarked(
    s: *const RustBookmarkStore, file: *const c_char, line: u32,
) -> bool {
    if s.is_null() { return false; }
    let f = unsafe { ptr_to_str(file) }.to_string();
    unsafe { (&*(s as *const BookmarkStore)).is_bookmarked(&f, line) }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_at(
    s: *const RustBookmarkStore, file: *const c_char, line: u32,
) -> i32 {
    if s.is_null() { return -1; }
    let f = unsafe { ptr_to_str(file) }.to_string();
    unsafe { (&*(s as *const BookmarkStore)).at(&f, line).unwrap_or(-1) }
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_next(
    s: *const RustBookmarkStore, file: *const c_char, current_line: i64,
) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring(""); }
    let f = unsafe { ptr_to_str(file) }.to_string();
    let r = unsafe { (&*(s as *const BookmarkStore)).next_after(&f, current_line) };
    crate::str_to_cstring(
        &serde_json::to_string(&r).unwrap_or("null".into()))
}

#[no_mangle]
pub extern "C" fn rust_bookmarks_prev(
    s: *const RustBookmarkStore, file: *const c_char, current_line: i64,
) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring(""); }
    let f = unsafe { ptr_to_str(file) }.to_string();
    let r = unsafe { (&*(s as *const BookmarkStore)).prev_before(&f, current_line) };
    crate::str_to_cstring(
        &serde_json::to_string(&r).unwrap_or("null".into()))
}

// ═══════════════════════════════════════════════════════════════════════
//  Application Crash Handler
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_app_crash_new, AppCrashHandler, RustAppCrashHandler);
make_free!(rust_app_crash_free, AppCrashHandler, RustAppCrashHandler);

#[no_mangle]
pub extern "C" fn rust_app_crash_install(h: *mut RustAppCrashHandler) {
    if h.is_null() { return; }
    unsafe { (&mut *(h as *mut AppCrashHandler)).install(); }
}

#[no_mangle]
pub extern "C" fn rust_app_crash_dump_path(h: *const RustAppCrashHandler) -> *mut c_char {
    if h.is_null() { return std::ptr::null_mut(); }
    let path = unsafe { (&*(h as *const AppCrashHandler)).dump_path() };
    crate::str_to_cstring(&path.to_string_lossy())
}

// ═══════════════════════════════════════════════════════════════════════
//  Snippet Store
// ═══════════════════════════════════════════════════════════════════════

make_new!(rust_snippet_store_new, SnippetStore, RustSnippetStore);
make_free!(rust_snippet_store_free, SnippetStore, RustSnippetStore);

#[no_mangle]
pub extern "C" fn rust_snippet_store_add(
    s: *mut RustSnippetStore, snippet_json: *const c_char,
) -> bool {
    if s.is_null() || snippet_json.is_null() { return false; }
    let j = unsafe { ptr_to_str(snippet_json) };
    match serde_json::from_str::<crate::snippet_engine::Snippet>(j) {
        Ok(snippet) => {
            unsafe { (&mut *(s as *mut SnippetStore)).add(snippet) };
            true
        }
        Err(_) => false,
    }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_update(
    s: *mut RustSnippetStore, snippet_json: *const c_char,
) -> bool {
    if s.is_null() || snippet_json.is_null() { return false; }
    let j = unsafe { ptr_to_str(snippet_json) };
    match serde_json::from_str::<crate::snippet_engine::Snippet>(j) {
        Ok(snippet) => {
            unsafe { (&mut *(s as *mut SnippetStore)).update(snippet) };
            true
        }
        Err(_) => false,
    }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_remove(
    s: *mut RustSnippetStore, id: *const c_char,
) -> bool {
    if s.is_null() { return false; }
    let i = unsafe { ptr_to_str(id) };
    unsafe { (&mut *(s as *mut SnippetStore)).remove(i) }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_get(
    s: *const RustSnippetStore, id: *const c_char,
) -> *mut c_char {
    if s.is_null() { return std::ptr::null_mut(); }
    let i = unsafe { ptr_to_str(id) };
    let result = unsafe { (&*(s as *const SnippetStore)).get(i) };
    match result {
        Some(sn) => crate::str_to_cstring(&serde_json::to_string(sn).unwrap_or_default()),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_all(
    s: *const RustSnippetStore, out_len: *mut usize,
) -> *mut *mut c_char {
    if s.is_null() { unsafe { *out_len = 0; } return std::ptr::null_mut(); }
    let list = unsafe { (&*(s as *const SnippetStore)).all() };
    let len = list.len();
    unsafe { *out_len = len; }
    if len == 0 { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = list.iter()
        .map(|s| crate::str_to_cstring(&serde_json::to_string(s).unwrap_or_default()))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_for_language(
    s: *const RustSnippetStore, language: *const c_char, out_len: *mut usize,
) -> *mut *mut c_char {
    if s.is_null() { unsafe { *out_len = 0; } return std::ptr::null_mut(); }
    let l = unsafe { ptr_to_str(language) };
    let list = unsafe { (&*(s as *const SnippetStore)).for_language(l) };
    let len = list.len();
    unsafe { *out_len = len; }
    if len == 0 { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = list.iter()
        .map(|sn| crate::str_to_cstring(&serde_json::to_string(sn).unwrap_or_default()))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_prefixes(
    s: *const RustSnippetStore, out_len: *mut usize,
) -> *mut *mut c_char {
    if s.is_null() { unsafe { *out_len = 0; } return std::ptr::null_mut(); }
    let list = unsafe { (&*(s as *const SnippetStore)).prefixes() };
    let len = list.len();
    unsafe { *out_len = len; }
    if len == 0 { return std::ptr::null_mut(); }
    let mut arr: Vec<*mut c_char> = list.iter()
        .map(|s| crate::str_to_cstring(s))
        .collect();
    arr.shrink_to_fit();
    let ptr = arr.as_mut_ptr();
    mem::forget(arr);
    ptr
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_has_prefix(
    s: *const RustSnippetStore, prefix: *const c_char, language: *const c_char,
) -> bool {
    if s.is_null() { return false; }
    let p = unsafe { ptr_to_str(prefix) };
    let l = unsafe { ptr_to_str(language) };
    unsafe { (&*(s as *const SnippetStore)).has_prefix(p, l) }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_find(
    s: *const RustSnippetStore, prefix: *const c_char, language: *const c_char,
) -> *mut c_char {
    if s.is_null() { return std::ptr::null_mut(); }
    let p = unsafe { ptr_to_str(prefix) };
    let l = unsafe { ptr_to_str(language) };
    let result = unsafe { (&*(s as *const SnippetStore)).find_for_prefix(p, l) };
    match result {
        Some(sn) => crate::str_to_cstring(&serde_json::to_string(sn).unwrap_or_default()),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_save(
    s: *const RustSnippetStore,
) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring("[]"); }
    let json = unsafe { (&*(s as *const SnippetStore)).save_to_json() };
    crate::str_to_cstring(&json)
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_load(
    s: *mut RustSnippetStore, json: *const c_char,
) -> usize {
    if s.is_null() { return 0; }
    let j = unsafe { ptr_to_str(json) };
    unsafe { (&mut *(s as *mut SnippetStore)).load_from_json(j) }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_import(
    s: *mut RustSnippetStore, json: *const c_char,
) -> usize {
    if s.is_null() { return 0; }
    let j = unsafe { ptr_to_str(json) };
    unsafe { (&mut *(s as *mut SnippetStore)).import_from_json(j) }
}

#[no_mangle]
pub extern "C" fn rust_snippet_store_export(
    s: *const RustSnippetStore,
) -> *mut c_char {
    if s.is_null() { return crate::str_to_cstring("[]"); }
    let json = unsafe { (&*(s as *const SnippetStore)).export_to_json() };
    crate::str_to_cstring(&json)
}

#[no_mangle]
pub extern "C" fn rust_snippet_substitute_variables(
    body: *const c_char, vars_json: *const c_char,
) -> *mut c_char {
    let b = unsafe { ptr_to_str(body) };
    let v = if vars_json.is_null() {
        vec![]
    } else {
        match serde_json::from_str::<Vec<(String, String)>>(unsafe { ptr_to_str(vars_json) }) {
            Ok(v) => v,
            Err(_) => return crate::str_to_cstring(b),
        }
    };
    let vars: Vec<(&str, &str)> = v.iter().map(|(k, val)| (k.as_str(), val.as_str())).collect();
    let result = crate::snippet_engine::subst_datetime(b, &vars);
    crate::str_to_cstring(&result)
}

// ═══════════════════════════════════════════════════════════════════════
//  Application Core
// ═══════════════════════════════════════════════════════════════════════
make_new!(rust_app_new, ScripturaApp, RustScripturaApp);
make_free!(rust_app_free, ScripturaApp, RustScripturaApp);

#[no_mangle]
pub extern "C" fn rust_app_initialize(app: *mut RustScripturaApp) {
    if app.is_null() { return; }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.initialize();
}

#[no_mangle]
pub extern "C" fn rust_app_shutdown(app: *mut RustScripturaApp) {
    if app.is_null() { return; }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.shutdown();
}

// ═══════════════════════════════════════════════════════════════════════
//  Application Core — Accessor FFI Functions
// ═══════════════════════════════════════════════════════════════════════
#[no_mangle]
pub extern "C" fn rust_app_workspace(app: *mut RustScripturaApp) -> *mut crate::workspace::Workspace {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.workspace() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_eventbus(app: *mut RustScripturaApp) -> *mut crate::eventbus::EventBus {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.event_bus() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_pluginmanager(app: *mut RustScripturaApp) -> *mut crate::plugin::PluginManager {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.plugin_manager() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_taskrunner(app: *mut RustScripturaApp) -> *mut crate::task_runner::TaskRunner {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.task_runner() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_updater(app: *mut RustScripturaApp) -> *mut crate::updater::Updater {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.updater() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_configvalidator(app: *mut RustScripturaApp) -> *mut crate::config_validator::ConfigValidator {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.config_validator() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_pluginregistry(app: *mut RustScripturaApp) -> *mut crate::registry::PluginRegistry {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.plugin_registry() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_permissionmanager(app: *mut RustScripturaApp) -> *mut crate::permission::PermissionManager {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.permission_manager() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_servicelocator(app: *mut RustScripturaApp) -> *mut crate::service_locator::ServiceLocator {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.service_locator() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_dependencyresolver(app: *mut RustScripturaApp) -> *mut crate::dependency_resolver::DependencyResolver {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.dependency_resolver() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_archiveextractor(app: *mut RustScripturaApp) -> *mut crate::archive_extractor::ArchiveExtractor {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.archive_extractor() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_lspclient(app: *mut RustScripturaApp) -> *mut crate::lsp::LspClient {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.lsp_client() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_dapclient(app: *mut RustScripturaApp) -> *mut crate::dap::DapClient {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.dap_client() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_crashhandler(app: *mut RustScripturaApp) -> *mut crate::app_crash::AppCrashHandler {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.crash_handler() as *mut _
}
#[no_mangle]
pub extern "C" fn rust_app_uiactionshandler(app: *mut RustScripturaApp) -> *mut crate::ui_actions::UiActionHandler {
    if app.is_null() { return std::ptr::null_mut(); }
    let app: &mut ScripturaApp = unsafe { &mut *(app as *mut ScripturaApp) };
    app.ui_actions() as *mut _
}
