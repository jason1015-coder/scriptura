//! ## Debug Session
//!
//! Manages individual debugging sessions, including state tracking and
//! lifecycle management. Works alongside `DapClient` for protocol communication.
//!
//! Replaces `DebugSession` from the original C++ codebase.

use std::ffi::c_void;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SessionState {
    Idle,
    Initializing,
    Running,
    Paused,
    Stopping,
    Terminated,
}

impl SessionState {
    pub fn to_str(&self) -> &'static str {
        match self {
            Self::Idle => "idle",
            Self::Initializing => "initializing",
            Self::Running => "running",
            Self::Paused => "paused",
            Self::Stopping => "stopping",
            Self::Terminated => "terminated",
        }
    }
}

pub struct DebugSession {
    state: SessionState,
    config_json: String,
    on_state_change: Option<CbStr>,
    on_state_change_data: *mut c_void,
}

impl DebugSession {
    pub fn new() -> Self {
        Self {
            state: SessionState::Idle,
            config_json: String::new(),
            on_state_change: None,
            on_state_change_data: std::ptr::null_mut(),
        }
    }

    pub fn start(&mut self, config_json: &str) {
        self.config_json = config_json.to_string();
        self.state = SessionState::Initializing;
        self.fire_state_change();
    }

    pub fn stop(&mut self) {
        self.state = SessionState::Terminated;
        self.fire_state_change();
    }

    pub fn set_running(&mut self) {
        self.state = SessionState::Running;
        self.fire_state_change();
    }

    pub fn set_paused(&mut self) {
        self.state = SessionState::Paused;
        self.fire_state_change();
    }

    pub fn state(&self) -> SessionState {
        self.state
    }

    pub fn config_json(&self) -> &str {
        &self.config_json
    }

    fn fire_state_change(&self) {
        if let Some(cb) = self.on_state_change {
            let msg = std::ffi::CString::new(self.state.to_str()).unwrap_or_default();
            cb(msg.as_ptr(), self.on_state_change_data);
        }
    }

    pub fn set_on_state_change(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_state_change = Some(cb);
        self.on_state_change_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_state_cb(_msg: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_session() {
        let s = DebugSession::new();
        assert_eq!(s.state(), SessionState::Idle);
        assert_eq!(s.config_json(), "");
    }

    #[test]
    fn test_start_session() {
        let mut s = DebugSession::new();
        s.start(r#"{"program": "test"}"#);
        assert_eq!(s.state(), SessionState::Initializing);
        assert_eq!(s.config_json(), r#"{"program": "test"}"#);
    }

    #[test]
    fn test_start_to_stop() {
        let mut s = DebugSession::new();
        s.start("{}");
        assert_eq!(s.state(), SessionState::Initializing);
        s.stop();
        assert_eq!(s.state(), SessionState::Terminated);
    }

    #[test]
    fn test_set_running() {
        let mut s = DebugSession::new();
        s.set_running();
        assert_eq!(s.state(), SessionState::Running);
    }

    #[test]
    fn test_set_paused() {
        let mut s = DebugSession::new();
        s.set_paused();
        assert_eq!(s.state(), SessionState::Paused);
    }

    #[test]
    fn test_state_change_callback() {
        let mut s = DebugSession::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        s.set_on_state_change(test_state_cb, ptr);

        s.start("{}");
        assert!(flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_session_state_to_str() {
        assert_eq!(SessionState::Idle.to_str(), "idle");
        assert_eq!(SessionState::Initializing.to_str(), "initializing");
        assert_eq!(SessionState::Running.to_str(), "running");
        assert_eq!(SessionState::Paused.to_str(), "paused");
        assert_eq!(SessionState::Stopping.to_str(), "stopping");
        assert_eq!(SessionState::Terminated.to_str(), "terminated");
    }

    #[test]
    fn test_full_lifecycle() {
        let mut s = DebugSession::new();
        assert_eq!(s.state(), SessionState::Idle);

        s.start("{}");
        assert_eq!(s.state(), SessionState::Initializing);

        s.set_running();
        assert_eq!(s.state(), SessionState::Running);

        s.set_paused();
        assert_eq!(s.state(), SessionState::Paused);

        s.stop();
        assert_eq!(s.state(), SessionState::Terminated);
    }

    #[test]
    fn test_session_state_eq() {
        assert_eq!(SessionState::Idle, SessionState::Idle);
        assert_ne!(SessionState::Idle, SessionState::Running);
    }
}
