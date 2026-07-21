//! ## EventBus
//!
//! A thread-safe publish-subscribe event system.
//! Replaces the Qt `EventBus` (QObject-based singleton).
//!
//! Events are published as string names with optional JSON payloads.
//! Subscribers register callbacks that fire on `publish()`.

use std::collections::HashMap;
use std::ffi::c_void;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

type Callback = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);

struct Subscription {
    id: u64,
    callback: Callback,
    user_data: *mut c_void,
}

// SAFETY: Subscription is safe to send across threads because callbacks are
// C function pointers and user_data is managed by the C++ side.
unsafe impl Send for Subscription {}
unsafe impl Sync for Subscription {}

pub struct EventBus {
    next_id: AtomicU64,
    subscribers: Mutex<HashMap<String, Vec<Subscription>>>,
}

impl EventBus {
    pub fn new() -> Self {
        Self {
            next_id: AtomicU64::new(1),
            subscribers: Mutex::new(HashMap::new()),
        }
    }

    pub fn subscribe(
        &self,
        event: &str,
        callback: Callback,
        user_data: *mut c_void,
    ) -> u64 {
        let id = self.next_id.fetch_add(1, Ordering::Relaxed);
        if let Ok(mut subs) = self.subscribers.lock() {
            subs.entry(event.to_string())
                .or_default()
                .push(Subscription { id, callback, user_data });
        }
        id
    }

    pub fn unsubscribe(&self, event: &str, sub_id: u64) {
        if let Ok(mut subs) = self.subscribers.lock() {
            if let Some(entries) = subs.get_mut(event) {
                entries.retain(|s| s.id != sub_id);
                if entries.is_empty() {
                    subs.remove(event);
                }
            }
        }
    }

    pub fn publish(&self, event: &str, json_data: &str) {
        let entries: Vec<(Callback, *mut c_void)> =
            if let Ok(subs) = self.subscribers.lock() {
                subs.get(event)
                    .map(|entries| {
                        entries
                            .iter()
                            .map(|s| (s.callback, s.user_data))
                            .collect()
                    })
                    .unwrap_or_default()
            } else {
                return;
            };

        // Use scoped CStrings — freed on drop after callbacks return
        let c_event = std::ffi::CString::new(event).unwrap_or_default();
        let c_data = std::ffi::CString::new(json_data).unwrap_or_default();
        let event_ptr = c_event.as_ptr();
        let data_ptr = c_data.as_ptr();

        for (cb, ud) in &entries {
            cb(event_ptr, data_ptr, *ud);
        }
    }

    pub fn has_subscribers(&self, event: &str) -> bool {
        if let Ok(subs) = self.subscribers.lock() {
            subs.contains_key(event)
        } else {
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    extern "C" fn test_callback(_event: *const c_char, _data: *const c_char, user_data: *mut c_void) {
        unsafe {
            let flag = &*(user_data as *const AtomicBool);
            flag.store(true, Ordering::SeqCst);
        }
    }

    #[test]
    fn test_new_eventbus() {
        let bus = EventBus::new();
        assert!(!bus.has_subscribers("test"));
    }

    #[test]
    fn test_subscribe_and_publish() {
        let bus = EventBus::new();
        let flag = AtomicBool::new(false);
        let flag_ptr = &flag as *const AtomicBool as *mut c_void;

        let id = bus.subscribe("test.event", test_callback, flag_ptr);
        assert!(id > 0);
        assert!(bus.has_subscribers("test.event"));

        bus.publish("test.event", r#"{"key": "value"}"#);
        assert!(flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_unsubscribe() {
        let bus = EventBus::new();
        let flag = AtomicBool::new(false);
        let flag_ptr = &flag as *const AtomicBool as *mut c_void;

        let id = bus.subscribe("test.event", test_callback, flag_ptr);
        bus.unsubscribe("test.event", id);
        assert!(!bus.has_subscribers("test.event"));

        flag.store(false, Ordering::SeqCst);
        bus.publish("test.event", "data");
        assert!(!flag.load(Ordering::SeqCst));
    }

    #[test]
    fn test_multiple_subscribers() {
        let bus = EventBus::new();
        let flag1 = AtomicBool::new(false);
        let flag2 = AtomicBool::new(false);
        let ptr1 = &flag1 as *const AtomicBool as *mut c_void;
        let ptr2 = &flag2 as *const AtomicBool as *mut c_void;

        bus.subscribe("test.event", test_callback, ptr1);
        bus.subscribe("test.event", test_callback, ptr2);

        bus.publish("test.event", "data");
        assert!(flag1.load(Ordering::SeqCst));
        assert!(flag2.load(Ordering::SeqCst));
    }

    #[test]
    fn test_event_isolation() {
        let bus = EventBus::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;

        bus.subscribe("event.a", test_callback, ptr);
        assert!(bus.has_subscribers("event.a"));
        assert!(!bus.has_subscribers("event.b"));
    }

    #[test]
    fn test_publish_to_nonexistent_event() {
        let bus = EventBus::new();
        bus.publish("nonexistent", "data");
    }

    #[test]
    fn test_unsubscribe_nonexistent() {
        let bus = EventBus::new();
        bus.unsubscribe("nonexistent", 999);
    }
}
