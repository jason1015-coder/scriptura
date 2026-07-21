//! ## Permission Manager
//!
//! Manages plugin permissions: which plugins are allowed to access
//! which system resources (file system, network, etc.).
//!
//! Replaces `PermissionManager` from the original C++ codebase.

use std::collections::HashMap;
use std::sync::Mutex;

/// Permission types a plugin can request.
#[allow(dead_code)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(i32)]
pub enum Permission {
    None = 0,
    FileSystem = 1,
    Network = 2,
    Process = 4,
    Notifications = 8,
    UiAccess = 16,
    EditorAccess = 32,
    All = 63,
}

pub struct PermissionManager {
    granted: Mutex<HashMap<String, i32>>, // plugin_id -> bitmask of granted permissions
}

impl PermissionManager {
    pub fn new() -> Self {
        Self {
            granted: Mutex::new(HashMap::new()),
        }
    }

    /// Check if a plugin has a specific permission.
    pub fn check(&self, plugin_id: &str, perm: i32) -> bool {
        if let Ok(granted) = self.granted.lock() {
            if let Some(mask) = granted.get(plugin_id) {
                return (*mask & perm) == perm;
            }
        }
        false
    }

    /// Request a permission for a plugin. In the UI, this would prompt the user.
    pub fn request(&self, _plugin_id: &str, _perm: i32) {
        // In production, this would show a permission dialog via callback
    }

    /// Grant a permission to a plugin.
    pub fn grant(&self, plugin_id: &str, perm: i32) {
        if let Ok(mut granted) = self.granted.lock() {
            let entry = granted.entry(plugin_id.to_string()).or_insert(0);
            *entry |= perm;
        }
    }

    /// Revoke a permission from a plugin.
    pub fn revoke(&self, plugin_id: &str, perm: i32) {
        if let Ok(mut granted) = self.granted.lock() {
            if let Some(mask) = granted.get_mut(plugin_id) {
                *mask &= !perm;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_manager() {
        let pm = PermissionManager::new();
        assert!(!pm.check("plugin_a", 1));
    }

    #[test]
    fn test_grant_and_check() {
        let pm = PermissionManager::new();
        pm.grant("my_plugin", Permission::FileSystem as i32);
        assert!(pm.check("my_plugin", Permission::FileSystem as i32));
        assert!(!pm.check("my_plugin", Permission::Network as i32));
    }

    #[test]
    fn test_grant_multiple() {
        let pm = PermissionManager::new();
        pm.grant("p", Permission::FileSystem as i32 | Permission::Network as i32);
        assert!(pm.check("p", Permission::FileSystem as i32));
        assert!(pm.check("p", Permission::Network as i32));
        assert!(!pm.check("p", Permission::UiAccess as i32));
    }

    #[test]
    fn test_revoke() {
        let pm = PermissionManager::new();
        pm.grant("p", Permission::All as i32);
        assert!(pm.check("p", Permission::FileSystem as i32));
        pm.revoke("p", Permission::Network as i32);
        assert!(pm.check("p", Permission::FileSystem as i32));
        assert!(!pm.check("p", Permission::Network as i32));
    }

    #[test]
    fn test_revoke_unknown_plugin() {
        let pm = PermissionManager::new();
        pm.revoke("nonexistent", 1);
    }

    #[test]
    fn test_check_unknown_plugin() {
        let pm = PermissionManager::new();
        assert!(!pm.check("unknown", Permission::FileSystem as i32));
    }

    #[test]
    fn test_request_does_not_panic() {
        let pm = PermissionManager::new();
        pm.request("some_plugin", Permission::Network as i32);
    }

    #[test]
    fn test_permission_enum_values() {
        assert_eq!(Permission::None as i32, 0);
        assert_eq!(Permission::FileSystem as i32, 1);
        assert_eq!(Permission::Network as i32, 2);
        assert_eq!(Permission::Process as i32, 4);
        assert_eq!(Permission::All as i32, 63);
    }
}
