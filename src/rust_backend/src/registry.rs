//! ## Plugin Registry
//!
//! Manages the remote plugin registry for plugin discovery, download,
//! and installation. Handles checking for plugin updates.
//!
//! Replaces `PluginRegistry` from the original C++ codebase.

use std::ffi::c_void;
use std::sync::Mutex;

use serde_json::Value;

type CbStr = extern "C" fn(*const std::os::raw::c_char, *mut c_void);
type CbPluginEvent = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);

pub struct PluginRegistry {
    registry_url: Mutex<String>,
    manifest: Mutex<Option<Value>>,
    on_update: Option<CbStr>,
    on_update_data: *mut c_void,
    on_install_failed: Option<CbPluginEvent>,
    on_install_failed_data: *mut c_void,
}

impl PluginRegistry {
    pub fn new() -> Self {
        Self {
            registry_url: Mutex::new(String::new()),
            manifest: Mutex::new(None),
            on_update: None,
            on_update_data: std::ptr::null_mut(),
            on_install_failed: None,
            on_install_failed_data: std::ptr::null_mut(),
        }
    }

    /// Set the registry URL.
    pub fn set_url(&self, url: &str) {
        if let Ok(mut registry_url) = self.registry_url.lock() {
            *registry_url = url.to_string();
        }
    }

    /// Get the registry URL.
    pub fn get_url(&self) -> String {
        self.registry_url.lock()
            .map(|u| u.clone())
            .unwrap_or_default()
    }

    /// Check for updates in the registry.
    pub fn check_updates(&self) {
        let url = self.get_url();
        if url.is_empty() {
            return;
        }

        let result = reqwest::blocking::get(&url);
        match result {
            Ok(response) => {
                if let Ok(body) = response.text() {
                    if let Ok(manifest) = serde_json::from_str::<Value>(&body) {
                        if let Ok(mut m) = self.manifest.lock() {
                            *m = Some(manifest.clone());
                        }
                        if let Some(cb) = self.on_update {
                            let result = serde_json::to_string(&manifest).unwrap_or_default();
                            let msg = std::ffi::CString::new(&result[..]).unwrap_or_default();
                            cb(msg.as_ptr(), self.on_update_data);
                        }
                    }
                }
            }
            Err(e) => {
                log::warn!("Failed to check plugin registry: {}", e);
            }
        }
    }

    /// Check if an upgrade is available for a plugin.
    pub fn upgrade_available(&self, plugin_id: &str, current_version: &str) -> bool {
        if let Ok(manifest) = self.manifest.lock() {
            if let Some(ref m) = *manifest {
                if let Some(plugins) = m.get("plugins").and_then(|p| p.as_array()) {
                    for plugin in plugins {
                        if plugin.get("id").and_then(|i| i.as_str()) == Some(plugin_id) {
                            let latest = plugin.get("version")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            let cur = semver::Version::parse(current_version).ok();
                            let lat = semver::Version::parse(latest).ok();
                            return match (cur, lat) {
                                (Some(c), Some(l)) => l > c,
                                _ => latest > current_version,
                            };
                        }
                    }
                }
            }
        }
        false
    }

    pub fn set_on_update(&mut self, cb: CbStr, data: *mut c_void) {
        self.on_update = Some(cb);
        self.on_update_data = data;
    }

    pub fn set_on_install_failed(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_install_failed = Some(cb);
        self.on_install_failed_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_char;
    use std::sync::atomic::{AtomicBool, Ordering};

    #[test]
    fn test_new_registry() {
        let reg = PluginRegistry::new();
        assert_eq!(reg.get_url(), "");
    }

    #[test]
    fn test_set_and_get_url() {
        let reg = PluginRegistry::new();
        reg.set_url("https://example.com/registry.json");
        assert_eq!(reg.get_url(), "https://example.com/registry.json");
    }

    #[test]
    fn test_set_url_empty() {
        let reg = PluginRegistry::new();
        reg.set_url("");
        assert_eq!(reg.get_url(), "");
    }

    #[test]
    fn test_check_updates_with_empty_url_does_nothing() {
        let reg = PluginRegistry::new();
        reg.check_updates(); // Should not panic
    }

    #[test]
    fn test_upgrade_available_with_no_manifest() {
        let reg = PluginRegistry::new();
        assert!(!reg.upgrade_available("any_plugin", "1.0.0"));
    }

    #[test]
    fn test_upgrade_available_with_manifest_content() {
        let reg = PluginRegistry::new();
        // Set manifest manually
        let manifest = serde_json::json!({
            "plugins": [
                {"id": "plugin_a", "version": "2.0.0"},
                {"id": "plugin_b", "version": "1.0.0"}
            ]
        });
        if let Ok(mut m) = reg.manifest.lock() {
            *m = Some(manifest);
        }

        assert!(reg.upgrade_available("plugin_a", "1.0.0"));
        assert!(!reg.upgrade_available("plugin_b", "1.0.0"));
        assert!(!reg.upgrade_available("plugin_b", "2.0.0"));
    }

    #[test]
    fn test_upgrade_available_unknown_plugin() {
        let reg = PluginRegistry::new();
        let manifest = serde_json::json!({
            "plugins": [{"id": "plugin_a", "version": "2.0.0"}]
        });
        if let Ok(mut m) = reg.manifest.lock() {
            *m = Some(manifest);
        }
        assert!(!reg.upgrade_available("unknown", "1.0.0"));
    }

    #[test]
    fn test_manifest_without_plugins_key() {
        let reg = PluginRegistry::new();
        let manifest = serde_json::json!({"other": "data"});
        if let Ok(mut m) = reg.manifest.lock() {
            *m = Some(manifest);
        }
        assert!(!reg.upgrade_available("plugin_a", "1.0.0"));
    }

    #[test]
    fn test_callback_setters() {
        let mut reg = PluginRegistry::new();
        extern "C" fn dummy_cb(_: *const c_char, _: *mut c_void) {}
        extern "C" fn dummy_plugin_cb(_: *const c_char, _: *const c_char, _: *mut c_void) {}

        reg.set_on_update(dummy_cb, std::ptr::null_mut());
        reg.set_on_install_failed(dummy_plugin_cb, std::ptr::null_mut());
    }
}
