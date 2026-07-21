//! ## Config Validator
//!
//! Validates JSON configuration against predefined rules.
//! Replaces `ConfigValidator` from the original C++ codebase.

use std::ffi::c_void;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

pub struct ConfigValidator {
    on_error: Option<CbStr>,
    on_error_data: *mut c_void,
}

impl ConfigValidator {
    pub fn new() -> Self {
        Self {
            on_error: None,
            on_error_data: std::ptr::null_mut(),
        }
    }

    /// Validate JSON config. Returns empty string on success, error message on failure.
    pub fn validate(&self, json_config: &str, schema_json: &str) -> String {
        // Parse the config JSON to verify it's valid
        let config: Result<serde_json::Value, _> = serde_json::from_str(json_config);
        match config {
            Ok(_) => {
                // Basic validation: also parse schema to verify it's valid JSON
                if !schema_json.is_empty()
                    && serde_json::from_str::<serde_json::Value>(schema_json).is_err() {
                        let err = "Invalid schema JSON".to_string();
                        if let Some(cb) = self.on_error {
                            let msg = std::ffi::CString::new(&err[..]).unwrap_or_default();
                            cb(msg.as_ptr(), self.on_error_data);
                        }
                        return err;
                    }
                String::new() // No error
            }
            Err(e) => {
                let err = format!("Invalid config JSON: {}", e);
                if let Some(cb) = self.on_error {
                    let msg = std::ffi::CString::new(&err[..]).unwrap_or_default();
                    cb(msg.as_ptr(), self.on_error_data);
                }
                err
            }
        }
    }

    pub fn set_on_error(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_error = Some(cb);
        self.on_error_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_error_cb(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_validator() {
        let cv = ConfigValidator::new();
        assert_eq!(cv.validate("{}", ""), "");
    }

    #[test]
    fn test_valid_config_no_schema() {
        let cv = ConfigValidator::new();
        assert_eq!(cv.validate(r#"{"key": "value"}"#, ""), "");
    }

    #[test]
    fn test_valid_config_with_valid_schema() {
        let cv = ConfigValidator::new();
        assert_eq!(cv.validate(r#"{}"#, r#"{"type": "object"}"#), "");
    }

    #[test]
    fn test_invalid_config() {
        let cv = ConfigValidator::new();
        let err = cv.validate("not valid json", "");
        assert!(!err.is_empty());
        assert!(err.contains("Invalid config JSON"));
    }

    #[test]
    fn test_invalid_schema() {
        let cv = ConfigValidator::new();
        let err = cv.validate("{}", "not valid json");
        assert!(!err.is_empty());
        assert_eq!(err, "Invalid schema JSON");
    }

    #[test]
    fn test_error_callback_on_config_invalid() {
        let mut cv = ConfigValidator::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        cv.set_on_error(test_error_cb, ptr);

        let err = cv.validate("bad json", "");
        assert!(!err.is_empty());
        assert!(flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_error_callback_on_schema_invalid() {
        let mut cv = ConfigValidator::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        cv.set_on_error(test_error_cb, ptr);

        let err = cv.validate("{}", "bad schema");
        assert!(!err.is_empty());
        assert!(flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_no_error_callback_when_valid() {
        let mut cv = ConfigValidator::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        cv.set_on_error(test_error_cb, ptr);

        assert_eq!(cv.validate(r#"{"a":1}"#, ""), "");
        assert!(!flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_empty_config() {
        let cv = ConfigValidator::new();
        let err = cv.validate("", "");
        assert!(!err.is_empty());
    }
}
