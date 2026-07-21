//! ## Service Locator
//!
//! A thread-safe service locator implementing the service locator pattern.
//! Replaces `ServiceLocator` (QObject-based singleton) from the original C++ codebase.
//!
//! Services are identified by string IDs and stored as raw pointers.
//! The C++ adapter side handles type-safe access via qobject_cast.

use std::collections::HashMap;
use std::ffi::c_void;
use std::sync::Mutex;

pub struct ServiceLocator {
    services: Mutex<HashMap<String, *mut c_void>>,
}

unsafe impl Send for ServiceLocator {}
unsafe impl Sync for ServiceLocator {}

impl ServiceLocator {
    pub fn new() -> Self {
        Self {
            services: Mutex::new(HashMap::new()),
        }
    }

    /// Register a service by ID.
    pub fn register(&self, id: &str, service: *mut c_void) {
        if let Ok(mut services) = self.services.lock() {
            services.insert(id.to_string(), service);
        }
    }

    /// Get a service by ID.
    pub fn get(&self, id: &str) -> Option<*mut c_void> {
        self.services.lock().ok().and_then(|services| services.get(id).copied())
    }

    /// Unregister a service.
    pub fn unregister(&self, id: &str) {
        if let Ok(mut services) = self.services.lock() {
            services.remove(id);
        }
    }

    /// Check if a service is registered.
    pub fn has(&self, id: &str) -> bool {
        self.services.lock().ok().is_some_and(|services| services.contains_key(id))
    }

    /// List all registered service IDs.
    pub fn list(&self) -> Vec<String> {
        self.services
            .lock()
            .ok()
            .map(|services| services.keys().cloned().collect())
            .unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_locator() {
        let sl = ServiceLocator::new();
        assert!(!sl.has("any"));
        assert!(sl.list().is_empty());
    }

    #[test]
    fn test_register_and_get() {
        let sl = ServiceLocator::new();
        let service_ptr = &42 as *const i32 as *mut c_void;
        sl.register("my_service", service_ptr);
        assert!(sl.has("my_service"));
        assert_eq!(sl.get("my_service"), Some(service_ptr));
    }

    #[test]
    fn test_get_nonexistent() {
        let sl = ServiceLocator::new();
        assert_eq!(sl.get("nonexistent"), None);
    }

    #[test]
    fn test_unregister() {
        let sl = ServiceLocator::new();
        let service_ptr = &42 as *const i32 as *mut c_void;
        sl.register("svc", service_ptr);
        assert!(sl.has("svc"));
        sl.unregister("svc");
        assert!(!sl.has("svc"));
        assert_eq!(sl.get("svc"), None);
    }

    #[test]
    fn test_unregister_nonexistent() {
        let sl = ServiceLocator::new();
        sl.unregister("nonexistent");
    }

    #[test]
    fn test_register_overwrite() {
        let sl = ServiceLocator::new();
        let ptr1 = &1 as *const i32 as *mut c_void;
        let ptr2 = &2 as *const i32 as *mut c_void;
        sl.register("svc", ptr1);
        sl.register("svc", ptr2);
        assert_eq!(sl.get("svc"), Some(ptr2));
    }

    #[test]
    fn test_list() {
        let sl = ServiceLocator::new();
        let ptr = &0 as *const i32 as *mut c_void;
        sl.register("a", ptr);
        sl.register("b", ptr);
        let mut list = sl.list();
        list.sort();
        assert_eq!(list, vec!["a", "b"]);
    }

    #[test]
    fn test_list_empty_after_unregister() {
        let sl = ServiceLocator::new();
        let ptr = &0 as *const i32 as *mut c_void;
        sl.register("x", ptr);
        sl.unregister("x");
        assert!(sl.list().is_empty());
    }
}
