//! ## Plugin Crash Handler
//!
//! Handles plugin crashes and reports them to the UI layer.
//! Replaces `PluginCrashHandler` from the original C++ codebase.

use std::ffi::c_void;

type CbPluginEvent = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);

pub struct PluginCrashHandler {
    on_crash: Option<CbPluginEvent>,
    on_crash_data: *mut c_void,
}

impl PluginCrashHandler {
    pub fn new() -> Self {
        Self {
            on_crash: None,
            on_crash_data: std::ptr::null_mut(),
        }
    }

    pub fn report_crash(&self, plugin_id: &str, error: &str) {
        if let Some(cb) = self.on_crash {
            let c_id = std::ffi::CString::new(plugin_id).unwrap_or_default();
            let c_err = std::ffi::CString::new(error).unwrap_or_default();
            cb(c_id.as_ptr(), c_err.as_ptr(), self.on_crash_data);
        }
    }

    pub fn set_on_crash(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_crash = Some(cb);
        self.on_crash_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_crash_cb(
        _id: *const c_char, _err: *const c_char, user_data: *mut c_void,
    ) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_handler() {
        let h = PluginCrashHandler::new();
        // Should not panic with no callback set
        h.report_crash("plugin_a", "test error");
    }

    #[test]
    fn test_report_crash_with_callback() {
        let mut h = PluginCrashHandler::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        h.set_on_crash(test_crash_cb, ptr);

        h.report_crash("my_plugin", "Something went wrong");
        assert!(flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_report_crash_with_empty_strings() {
        let mut h = PluginCrashHandler::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        h.set_on_crash(test_crash_cb, ptr);

        h.report_crash("", "");
        assert!(flag.load(Ordering::SeqCst));
    }
}
