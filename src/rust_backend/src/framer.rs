//! ## Length-Prefixed Framer
//!
//! Handles the Content-Length based framing protocol used by LSP and DAP.
//! Messages are framed as:
//!   Content-Length: <number>\r\n\r\n<payload>
//!
//! The framer buffers incoming data and emits complete messages.
//! This is a pure Rust replacement for `LengthPrefixedFramer` (QObject-based).

use std::collections::VecDeque;
use std::ffi::c_void;

type MessageCallback = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

pub struct LengthPrefixedFramer {
    buffer: Vec<u8>,
    messages: VecDeque<Vec<u8>>,
    content_length: Option<usize>,
    callback: Option<MessageCallback>,
    user_data: *mut c_void,
}

// SAFETY: `*mut c_void` user_data is owned/managed by the C++ side and
// is always accessed behind a Mutex in the LspClient reader thread.
unsafe impl Send for LengthPrefixedFramer {}
unsafe impl Sync for LengthPrefixedFramer {}

impl LengthPrefixedFramer {
    pub fn new() -> Self {
        Self {
            buffer: Vec::with_capacity(4096),
            messages: VecDeque::new(),
            content_length: None,
            callback: None,
            user_data: std::ptr::null_mut(),
        }
    }

    pub fn set_callback(&mut self, callback: MessageCallback, user_data: *mut c_void) {
        self.callback = Some(callback);
        self.user_data = user_data;
    }

    /// Feed raw bytes into the framer. Will try to parse complete messages.
    pub fn feed(&mut self, data: &[u8]) {
        self.buffer.extend_from_slice(data);
        self.try_parse();
    }

    /// Try to extract the next complete message (without callback).
    pub fn next_message(&mut self) -> Option<Vec<u8>> {
        self.messages.pop_front()
    }

    /// Returns the number of bytes buffered but not yet parsed.
    pub fn buffered_len(&self) -> usize {
        self.buffer.len()
    }

    fn try_parse(&mut self) {
        loop {
            // We need to find the headers first
            if self.content_length.is_none() {
                // Look for "\r\n\r\n" which separates headers from body
                if let Some(pos) = self.find_header_end() {
                    let header_part = &self.buffer[..pos];
                    let header_str = String::from_utf8_lossy(header_part);
                    self.content_length = self.parse_content_length(&header_str);
                    // Advance buffer past the header
                    self.buffer.drain(..pos + 4);
                } else {
                    // Not enough data yet
                    break;
                }
            }

            // Now check if we have enough body data
            if let Some(content_length) = self.content_length {
                if self.buffer.len() >= content_length {
                    // Extract the message body
                    let body: Vec<u8> = self.buffer.drain(..content_length).collect();
                    let body_str = String::from_utf8_lossy(&body).to_string();

                    // Store for polling
                    self.messages.push_back(body.clone());

                    // Fire callback if set — use scoped CString
                    if let Some(cb) = self.callback {
                        let c_body = std::ffi::CString::new(&body_str[..]).unwrap_or_default();
                        cb(c_body.as_ptr(), self.user_data);
                    }

                    self.content_length = None;
                    // Continue to try parsing more messages
                    continue;
                }
            }
            break;
        }
    }

    fn find_header_end(&self) -> Option<usize> {
        if self.buffer.len() < 4 {
            return None;
        }
        self.buffer
            .windows(4)
            .position(|w| w == b"\r\n\r\n")
    }

    fn parse_content_length(&self, header: &str) -> Option<usize> {
        for line in header.lines() {
            let line = line.trim();
            if let Some(value) = line.strip_prefix("Content-Length:") {
                let len_str = value.trim();
                if let Ok(len) = len_str.parse::<usize>() {
                    return Some(len);
                }
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_msg_callback(data: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    fn make_lsp_message(payload: &str) -> Vec<u8> {
        format!("Content-Length: {}\r\n\r\n{}", payload.len(), payload).into_bytes()
    }

    #[test]
    fn test_new_framer() {
        let mut framer = LengthPrefixedFramer::new();
        assert_eq!(framer.buffered_len(), 0);
        assert!(framer.next_message().is_none());
    }

    #[test]
    fn test_single_message() {
        let mut framer = LengthPrefixedFramer::new();
        framer.feed(&make_lsp_message(r#"{"jsonrpc":"2.0","method":"test"}"#));
        let msg = framer.next_message();
        assert!(msg.is_some());
        let body = String::from_utf8(msg.unwrap()).unwrap();
        assert_eq!(body, r#"{"jsonrpc":"2.0","method":"test"}"#);
    }

    #[test]
    fn test_multiple_messages() {
        let mut framer = LengthPrefixedFramer::new();
        framer.feed(&make_lsp_message("msg1"));
        framer.feed(&make_lsp_message("msg2"));
        assert_eq!(
            String::from_utf8(framer.next_message().unwrap()).unwrap(),
            "msg1"
        );
        assert_eq!(
            String::from_utf8(framer.next_message().unwrap()).unwrap(),
            "msg2"
        );
        assert!(framer.next_message().is_none());
    }

    #[test]
    fn test_partial_header() {
        let mut framer = LengthPrefixedFramer::new();
        framer.feed(b"Content-Length: 5\r");
        assert!(framer.next_message().is_none());
        framer.feed(b"\n\r\nhello");
        let msg = framer.next_message();
        assert!(msg.is_some());
        assert_eq!(String::from_utf8(msg.unwrap()).unwrap(), "hello");
    }

    #[test]
    fn test_partial_body() {
        let mut framer = LengthPrefixedFramer::new();
        let data = b"Content-Length: 11\r\n\r\nabc";
        framer.feed(data);
        assert!(framer.next_message().is_none());
        framer.feed(b"defghijk");
        let msg = framer.next_message();
        assert!(msg.is_some());
        assert_eq!(String::from_utf8(msg.unwrap()).unwrap(), "abcdefghijk");
    }

    #[test]
    fn test_no_content_length() {
        let mut framer = LengthPrefixedFramer::new();
        framer.feed(b"Invalid header\r\n\r\nbody");
        // Content-Length not found, message should remain buffered but not parsed
        assert!(framer.next_message().is_none());
    }

    #[test]
    fn test_callback_invoked() {
        let mut framer = LengthPrefixedFramer::new();
        let flag = AtomicBool::new(false);
        let flag_ptr = &flag as *const AtomicBool as *mut c_void;
        framer.set_callback(test_msg_callback, flag_ptr);
        framer.feed(&make_lsp_message("callback_test"));
        assert!(flag.load(Ordering::SeqCst));
        // Message should also be available via next_message
        assert!(framer.next_message().is_some());
    }

    #[test]
    fn test_empty_payload() {
        let mut framer = LengthPrefixedFramer::new();
        framer.feed(&make_lsp_message(""));
        let msg = framer.next_message();
        assert!(msg.is_some());
        assert_eq!(String::from_utf8(msg.unwrap()).unwrap(), "");
    }

    #[test]
    fn test_buffered_len_tracking() {
        let mut framer = LengthPrefixedFramer::new();
        assert_eq!(framer.buffered_len(), 0);
        framer.feed(b"Content-Length: 5\r\n\r\nhello");
        assert_eq!(framer.buffered_len(), 0); // fully parsed
        framer.feed(b"Content-Length: 5\r\n\r\nhel");
        // header drained, only partial body 'hel' remains
        assert_eq!(framer.buffered_len(), 3);
    }

    #[test]
    fn test_no_callback_no_panic() {
        let mut framer = LengthPrefixedFramer::new();
        // No callback set, should not panic
        framer.feed(&make_lsp_message("some data"));
        assert!(framer.next_message().is_some());
    }
}
