//! ## FFI Exports
//!
//! All C-compatible FFI functions that bridge Rust backends to the C++ Qt UI.
//! Complex data crosses the boundary as JSON strings.
//! Callbacks use C function pointers for async notification.

use std::ffi::{c_void, CString};
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

use crate::lsp::LspClient;
use crate::dap::DapClient;
use crate::debug_session::DebugSession;
use crate::debug_config::DebugConfigurationManager;
use crate::eventbus::EventBus;
use crate::plugin::PluginManager;
use crate::plugin::PluginCrashHandler;
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
unsafe fn ptr_to_str<'a>(ptr: *const c_char) -> &'a str {
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
    arr.as_mut_ptr()
}

#[no_mangle]
pub extern "C" fn rust_pm_free_strings(strs: *mut *mut c_char, len: usize) {
    if strs.is_null() { return; }
    for i in 0..len {
        let ptr = unsafe { *strs.add(i) };
        if !ptr.is_null() {
            unsafe { let _ = CString::from_raw(ptr); }
        }
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
    arr.as_mut_ptr()
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
    arr.as_mut_ptr()
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

#[no_mangle]
pub extern "C" fn rust_dep_resolver_resolve(
    r: *mut RustDependencyResolver, json_metadata: *const c_char
) -> bool {
    let r = unsafe { &mut *(r as *mut DependencyResolver) };
    // For the FFI, we accept one plugin at a time
    let fake_id = "plugin";
    r.add_plugin(fake_id, unsafe { ptr_to_str(json_metadata) }).is_ok()
}

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
    arr.as_mut_ptr()
}

#[no_mangle]
pub extern "C" fn rust_dep_resolver_free_order(strs: *mut *mut c_char, len: usize) {
    rust_pm_free_strings(strs, len);
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
    arr.as_mut_ptr()
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
    arr.as_mut_ptr()
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
    arr.as_mut_ptr()
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
    arr.as_mut_ptr()
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
