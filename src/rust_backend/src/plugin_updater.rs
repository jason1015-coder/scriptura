//! ## Plugin Updater
//!
//! Checks for plugin updates from a remote registry.
//! Replaces `PluginUpdater` (QObject-based) from the original C++ codebase.

use std::ffi::c_void;

type CbPluginEvent = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);
type CbProgress = extern "C" fn(*const std::os::raw::c_char, i32, i32, *mut c_void);

pub struct PluginUpdater {
    on_update: Option<CbPluginEvent>,
    on_update_data: *mut c_void,
    on_progress: Option<CbProgress>,
    on_progress_data: *mut c_void,
}

impl PluginUpdater {
    pub fn new() -> Self {
        Self {
            on_update: None,
            on_update_data: std::ptr::null_mut(),
            on_progress: None,
            on_progress_data: std::ptr::null_mut(),
        }
    }

    /// Check for updates for a specific plugin.
    pub fn check(&self, plugin_id: &str, current_version: &str) {
        // TODO: Query the plugin registry for available updates
        let _ = (plugin_id, current_version);
    }

    pub fn set_on_update(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_update = Some(cb);
        self.on_update_data = data;
    }

    pub fn set_on_progress(&mut self, cb: CbProgress, data: *mut c_void) {
        self.on_progress = Some(cb);
        self.on_progress_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_update_cb(
        _id: *const c_char, _data: *const c_char, user_data: *mut c_void,
    ) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    extern "C" fn test_progress_cb(
        _task: *const c_char, _cur: i32, _total: i32, user_data: *mut c_void,
    ) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_updater() {
        let pu = PluginUpdater::new();
        pu.check("plugin", "1.0.0"); // Should not panic
    }

    #[test]
    fn test_set_on_update_callback() {
        let mut pu = PluginUpdater::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        pu.set_on_update(test_update_cb, ptr);
        // Callback would fire after an actual check; just verify it compiles
    }

    #[test]
    fn test_set_on_progress_callback() {
        let mut pu = PluginUpdater::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        pu.set_on_progress(test_progress_cb, ptr);
    }

    #[test]
    fn test_check_no_panic() {
        let pu = PluginUpdater::new();
        pu.check("test_plugin", "1.0.0");
        pu.check("", "");
        pu.check("plugin", "");
        pu.check("", "version");
    }
}
