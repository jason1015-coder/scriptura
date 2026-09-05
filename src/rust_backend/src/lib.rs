//! # Scriptura Rust Backend Library
//!
//! Replaces all C++ backend services with safe Rust implementations.
//! Exposes a C FFI interface for the Qt/C++ UI layer to consume.
//!
//! ## Architecture
//! Each backend module is implemented as a standalone Rust struct with
//! C-compatible FFI functions. Complex data (JSON, diagnostics, etc.)
//! crosses the FFI boundary as null-terminated C strings or plain structs.
//!
//! Callbacks use C function pointers to notify the C++ side of async events.
//! The C++ adapter layer translates these callbacks into Qt signals.

// Many types and functions in this crate are consumed by the C++ side
// via FFI and appear "unused" to Rust's dead code analysis.
#![allow(dead_code)]

mod archive_extractor;
mod config_validator;
mod dap;
mod debug_config;
mod debug_session;
mod dependency_resolver;
mod eventbus;
mod framer;
mod language_registry;
mod language_server_manager;
mod lsp;
mod permission;
mod plugin;
mod plugin_updater;
mod registry;
mod service_locator;
mod task_runner;
mod updater;
mod utils;
mod version_fetcher;
mod workspace;
mod diff_engine;
mod encoding_engine;
mod blame_engine;
mod emmet_engine;
mod session_engine;
mod test_engine;
mod ui_actions;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

pub(crate) fn take_last_error() -> Option<String> {
    use std::cell::RefCell;
    thread_local! {
        static LAST_ERROR: RefCell<Option<String>> = const { RefCell::new(None) };
    }
    LAST_ERROR.with(|e| e.borrow_mut().take())
}

/// Get the last error message from any Rust backend (thread-local).
/// Returns a C string that the caller MUST free with rust_free_string().
/// Returns null if no error is available.
#[no_mangle]
pub extern "C" fn rust_last_error() -> *mut c_char {
    match take_last_error() {
        Some(msg) => CString::new(msg).unwrap_or_default().into_raw(),
        None => std::ptr::null_mut(),
    }
}

/// Free a string returned by Rust (all strings are heap-allocated).
/// Safe to call with null pointer (no-op).
#[no_mangle]
pub extern "C" fn rust_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { let _ = CString::from_raw(s); }
    }
}

pub(crate) unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() { return ""; }
    CStr::from_ptr(ptr).to_str().unwrap_or("")
}

pub(crate) fn str_to_cstring(s: &str) -> *mut c_char {
    CString::new(s).unwrap_or_default().into_raw()
}

// All FFI functions are defined in the `ffi` module.
// Individual modules expose their Rust types and impls only.
mod ffi;
