//! Common utilities for the Rust backend.

use std::fmt;

/// Thread-safe error context for FFI error reporting.
#[derive(Debug)]
pub struct ErrorContext {
    message: String,
}

impl ErrorContext {
    pub fn new(msg: impl Into<String>) -> Self {
        Self {
            message: msg.into(),
        }
    }

    pub fn message(&self) -> &str {
        &self.message
    }
}

impl fmt::Display for ErrorContext {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl From<String> for ErrorContext {
    fn from(msg: String) -> Self {
        ErrorContext::new(msg)
    }
}

/// A generic result type for the backend.
pub type BackendResult<T> = Result<T, ErrorContext>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_context_new() {
        let ctx = ErrorContext::new("something went wrong");
        assert_eq!(ctx.message(), "something went wrong");
    }

    #[test]
    fn test_error_context_display() {
        let ctx = ErrorContext::new("error message");
        assert_eq!(format!("{}", ctx), "error message");
    }

    #[test]
    fn test_error_context_from_string() {
        let ctx: ErrorContext = String::from("converted").into();
        assert_eq!(ctx.message(), "converted");
    }

    #[test]
    fn test_error_context_empty_message() {
        let ctx = ErrorContext::new("");
        assert_eq!(ctx.message(), "");
    }

    #[test]
    fn test_backend_result_type() {
        let ok: BackendResult<i32> = Ok(42);
        assert_eq!(ok.unwrap(), 42);

        let err: BackendResult<i32> = Err(ErrorContext::new("fail"));
        assert!(err.is_err());
    }
}
