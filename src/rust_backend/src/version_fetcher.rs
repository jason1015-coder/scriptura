//! ## Version Fetcher
//!
//! Fetches version information from remote URLs (e.g., GitHub releases API).
//! Replaces `VersionFetcher` from the original C++ codebase.

use std::ffi::c_void;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

pub struct VersionFetcher {
    latest_version: String,
    on_fetched: Option<CbStr>,
    on_fetched_data: *mut c_void,
}

impl VersionFetcher {
    pub fn new() -> Self {
        Self {
            latest_version: String::new(),
            on_fetched: None,
            on_fetched_data: std::ptr::null_mut(),
        }
    }

    /// Fetch the latest version from a URL.
    pub fn fetch(&mut self, url: &str) {
        let result = reqwest::blocking::get(url);
        match result {
            Ok(response) => {
                if let Ok(body) = response.text() {
                    self.latest_version = body.trim().to_string();
                    if let Some(cb) = self.on_fetched {
                        let msg = std::ffi::CString::new(&self.latest_version[..]).unwrap_or_default();
                        cb(msg.as_ptr(), self.on_fetched_data);
                    }
                }
            }
            Err(e) => {
                log::warn!("Version fetch failed: {}", e);
            }
        }
    }

    pub fn latest(&self) -> &str {
        &self.latest_version
    }

    pub fn set_on_fetched(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_fetched = Some(cb);
        self.on_fetched_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_fetch_cb(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_fetcher() {
        let vf = VersionFetcher::new();
        assert_eq!(vf.latest(), "");
    }

    #[test]
    fn test_fetch_invalid_url() {
        let mut vf = VersionFetcher::new();
        vf.fetch("http://nonexistent-url.example/version.txt");
        // Should not panic
        assert_eq!(vf.latest(), "");
    }

    #[test]
    fn test_set_on_fetched_callback() {
        let mut vf = VersionFetcher::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        vf.set_on_fetched(test_fetch_cb, ptr);
    }

    #[test]
    fn test_fetch_malformed_url() {
        let mut vf = VersionFetcher::new();
        vf.fetch("not-a-url");
        assert_eq!(vf.latest(), "");
    }
}
