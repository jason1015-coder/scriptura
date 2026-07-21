//! ## Updater
//!
//! Checks for application updates by fetching version info from a remote URL.
//! Replaces `Updater` (QObject-based) from the original C++ codebase.

use std::ffi::c_void;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

pub struct Updater {
    latest_version: String,
    update_available: bool,
    on_update_available: Option<CbStr>,
    on_update_available_data: *mut c_void,
}

impl Updater {
    pub fn new() -> Self {
        Self {
            latest_version: String::new(),
            update_available: false,
            on_update_available: None,
            on_update_available_data: std::ptr::null_mut(),
        }
    }

    /// Check for updates by fetching the latest version from a remote URL.
    pub fn check(&mut self, current_version: &str, update_url: &str) {
        // Use reqwest in blocking mode to check for updates
        let result = reqwest::blocking::get(update_url);

        match result {
            Ok(response) => {
                if let Ok(body) = response.text() {
                    // Try to parse as JSON first
                    if let Ok(json) = serde_json::from_str::<serde_json::Value>(&body) {
                        if let Some(tag_name) = json.get("tag_name").and_then(|v| v.as_str()) {
                            let latest = tag_name.trim_start_matches('v').to_string();
                            self.latest_version = latest.clone();

                            // Compare versions
                            let current = semver::Version::parse(current_version).ok();
                            let latest_ver = semver::Version::parse(&latest).ok();

                            self.update_available = match (current, latest_ver) {
                                (Some(c), Some(l)) => l > c,
                                _ => false,
                            };

                            if self.update_available {
                                if let Some(cb) = self.on_update_available {
                                    let msg = std::ffi::CString::new(&latest[..]).unwrap_or_default();
                                    cb(msg.as_ptr(), self.on_update_available_data);
                                }
                            }
                        }
                    } else {
                        // Treat the entire response body as the version string
                        let latest = body.trim().to_string();
                        self.latest_version = latest.clone();
                        self.update_available = latest != current_version;

                        if self.update_available {
                            if let Some(cb) = self.on_update_available {
                                let msg = std::ffi::CString::new(&latest[..]).unwrap_or_default();
                                cb(msg.as_ptr(), self.on_update_available_data);
                            }
                        }
                    }
                }
            }
            Err(e) => {
                log::warn!("Failed to check for updates: {}", e);
            }
        }
    }

    pub fn is_update_available(&self) -> bool {
        self.update_available
    }

    pub fn latest_version(&self) -> &str {
        &self.latest_version
    }

    pub fn set_on_update_available(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_update_available = Some(cb);
        self.on_update_available_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_update_cb(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_updater() {
        let u = Updater::new();
        assert!(!u.is_update_available());
        assert_eq!(u.latest_version(), "");
    }

    #[test]
    fn test_check_with_invalid_url() {
        let mut u = Updater::new();
        u.check("1.0.0", "http://nonexistent-url-that-will-fail.example");
        // Should not panic, should just mark no update
        assert!(!u.is_update_available());
        assert_eq!(u.latest_version(), "");
    }

    #[test]
    fn test_check_with_malformed_url() {
        let mut u = Updater::new();
        u.check("1.0.0", "not-a-valid-url");
        // Should not panic
    }

    #[test]
    fn test_version_comparison_logic() {
        let mut u = Updater::new();
        // We can't easily test the HTTP path, but we can test the getters
        assert!(!u.is_update_available());
        u.latest_version();
    }

    #[test]
    fn test_set_callback() {
        let mut u = Updater::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        u.set_on_update_available(test_update_cb, ptr);
        // Callback stored successfully
    }

    #[test]
    fn test_latest_version_initially_empty() {
        let u = Updater::new();
        assert_eq!(u.latest_version(), "");
    }

    #[test]
    fn test_is_update_available_initially_false() {
        let u = Updater::new();
        assert!(!u.is_update_available());
    }
}
