//! ## Plugin Manager
//!
//! Manages the lifecycle of all plugins including discovery, loading,
//! dependency resolution, initialization, and unloading.
//!
//! Replaces `PluginManager` (QObject-based) from the original C++ codebase.
//! Since Qt's QPluginLoader is Qt-specific, the Rust implementation
//! manages plugin metadata and coordination, while the actual library
//! loading still happens through the C++ adapter side (via dlopen/libloading
//! or Qt's QPluginLoader).

use std::collections::{HashMap, HashSet};
use std::ffi::c_void;

use serde_json::Value;

use crate::dependency_resolver::DependencyResolver;

type CbPluginEvent = extern "C" fn(*const std::os::raw::c_char, *const std::os::raw::c_char, *mut c_void);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PluginState {
    NotLoaded,
    Loaded,
    #[allow(dead_code)]
    Failed,
    #[allow(dead_code)]
    Disabled,
}

pub struct PluginManager {
    plugins: HashMap<String, PluginInfo>,
    dependency_resolver: DependencyResolver,
    #[allow(dead_code)]
    event_handlers: HashMap<String, Vec<Subscription>>,
    #[allow(dead_code)]
    disabled_plugins: HashSet<String>,
    #[allow(dead_code)]
    next_sub_id: u64,

    // Callbacks
    on_loaded: Option<CbPluginEvent>,
    on_loaded_data: *mut c_void,
    on_unloaded: Option<CbPluginEvent>,
    on_unloaded_data: *mut c_void,
    on_error: Option<CbPluginEvent>,
    on_error_data: *mut c_void,
}

#[derive(Debug, Clone)]
pub struct PluginInfo {
    #[allow(dead_code)]
    pub id: String,
    #[allow(dead_code)]
    pub file_path: String,
    #[allow(dead_code)]
    pub metadata: String,
    pub state: PluginState,
    pub initialized: bool,
    #[allow(dead_code)]
    pub dependencies: Vec<String>,
    pub version: String,
}

#[allow(dead_code)]
struct Subscription {
    id: u64,
    callback: CbPluginEvent,
    user_data: *mut c_void,
}

unsafe impl Send for PluginManager {}
unsafe impl Sync for PluginManager {}

impl PluginManager {
    pub fn new() -> Self {
        Self {
            plugins: HashMap::new(),
            dependency_resolver: DependencyResolver::new(),
            event_handlers: HashMap::new(),
            disabled_plugins: HashSet::new(),
            next_sub_id: 1,
            on_loaded: None,
            on_loaded_data: std::ptr::null_mut(),
            on_unloaded: None,
            on_unloaded_data: std::ptr::null_mut(),
            on_error: None,
            on_error_data: std::ptr::null_mut(),
        }
    }

    pub fn load_plugins(&mut self, path: &str) -> Result<(), String> {
        let dir = std::path::Path::new(path);
        if !dir.is_dir() {
            return Err(format!("Plugin directory not found: {}", path));
        }

        let mut loaded = 0u32;
        for entry in std::fs::read_dir(dir).map_err(|e| e.to_string())? {
            let entry = entry.map_err(|e| e.to_string())?;
            let file_path = entry.path();

            // Look for shared libraries (.so, .dll, .dylib) or directories with plugin.json
            if file_path.is_dir() {
                let json_path = file_path.join("plugin.json");
                if json_path.exists() {
                    if let Ok(metadata_str) = std::fs::read_to_string(&json_path) {
                        if let Ok(metadata) = serde_json::from_str::<Value>(&metadata_str) {
                            if let Some(id) = metadata.get("id").and_then(|v| v.as_str()) {
                                self.plugins.insert(id.to_string(), PluginInfo {
                                    id: id.to_string(),
                                    file_path: file_path.to_string_lossy().to_string(),
                                    metadata: metadata_str.clone(),
                                    state: PluginState::Loaded,
                                    initialized: false,
                                    dependencies: metadata
                                        .get("dependencies")
                                        .and_then(|d| d.as_array())
                                        .map(|arr| {
                                            arr.iter()
                                                .filter_map(|v| v.as_str().map(String::from))
                                                .collect()
                                        })
                                        .unwrap_or_default(),
                                    version: metadata
                                        .get("version")
                                        .and_then(|v| v.as_str())
                                        .unwrap_or("0.0.0")
                                        .to_string(),
                                });

                                loaded += 1;

                                if let Some(cb) = self.on_loaded {
                                    let c_id = std::ffi::CString::new(id).unwrap_or_default();
                                    let c_data = std::ffi::CString::new(&metadata_str[..]).unwrap_or_default();
                                    cb(c_id.as_ptr(), c_data.as_ptr(), self.on_loaded_data);
                                }
                            }
                        }
                    }
                }
            }
        }

        log::info!("Loaded {} plugin(s) from {}", loaded, path);
        Ok(())
    }

    pub fn load_plugin(&mut self, file_path: &str) -> Result<(), String> {
        let path = std::path::Path::new(file_path);
        let json_path = if path.is_dir() {
            path.join("plugin.json")
        } else if path.extension().and_then(|e| e.to_str()) == Some("json") {
            path.to_path_buf()
        } else {
            // It's a library file; look for plugin.json in parent directory
            path.parent()
                .map(|p| p.join("plugin.json"))
                .ok_or_else(|| "Invalid plugin path".to_string())?
        };

        let metadata_str = std::fs::read_to_string(&json_path)
            .map_err(|e| format!("Cannot read plugin metadata: {}", e))?;
        let metadata: Value = serde_json::from_str(&metadata_str)
            .map_err(|e| format!("Invalid metadata JSON: {}", e))?;

        let id = metadata.get("id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| "Plugin metadata missing 'id'".to_string())?;

        let plugin_dir = json_path.parent()
            .map(|p| p.to_string_lossy().to_string())
            .unwrap_or_default();

        self.plugins.insert(id.to_string(), PluginInfo {
            id: id.to_string(),
            file_path: plugin_dir,
            metadata: metadata_str.clone(),
            state: PluginState::Loaded,
            initialized: false,
            dependencies: metadata
                .get("dependencies")
                .and_then(|d| d.as_array())
                .map(|arr| arr.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default(),
            version: metadata
                .get("version")
                .and_then(|v| v.as_str())
                .unwrap_or("0.0.0")
                .to_string(),
        });

        if let Some(cb) = self.on_loaded {
            let c_id = std::ffi::CString::new(id).unwrap_or_default();
            let c_data = std::ffi::CString::new(&metadata_str[..]).unwrap_or_default();
            cb(c_id.as_ptr(), c_data.as_ptr(), self.on_loaded_data);
        }

        Ok(())
    }

    pub fn unload_plugin(&mut self, id: &str) {
        if let Some(info) = self.plugins.get_mut(id) {
            info.state = PluginState::NotLoaded;
            info.initialized = false;
        }
        if let Some(cb) = self.on_unloaded {
            let c_id = std::ffi::CString::new(id).unwrap_or_default();
            cb(c_id.as_ptr(), std::ptr::null(), self.on_unloaded_data);
        }
    }

    pub fn unload_all(&mut self) {
        let ids: Vec<String> = self.plugins.keys().cloned().collect();
        for id in &ids {
            self.unload_plugin(id);
        }
    }

    pub fn is_loaded(&self, id: &str) -> bool {
        self.plugins.get(id).is_some_and(|p| p.state == PluginState::Loaded)
    }

    pub fn plugin_version(&self, id: &str) -> Option<&str> {
        self.plugins.get(id).map(|p| p.version.as_str())
    }

    pub fn list_loaded(&self) -> Vec<String> {
        self.plugins
            .iter()
            .filter(|(_, p)| p.state == PluginState::Loaded)
            .map(|(id, _)| id.clone())
            .collect()
    }

    pub fn build_dependency_graph(&mut self, metadata_jsons: &[&str]) -> Result<(), String> {
        for meta_json in metadata_jsons {
            let meta: Value = serde_json::from_str(meta_json)
                .map_err(|e| format!("Invalid metadata JSON: {}", e))?;
            let id = meta.get("id")
                .and_then(|v| v.as_str())
                .ok_or_else(|| "Missing plugin id in metadata".to_string())?;
            self.dependency_resolver.add_plugin(id, meta_json)?;
        }
        Ok(())
    }

    pub fn topological_sort(&self) -> Vec<String> {
        self.dependency_resolver.resolve_order()
    }

    // ── Event system ───────────────────────────────────────────────

    #[allow(dead_code)]
    pub fn publish_event(&self, event: &str, json_data: &str) {
        if let Some(handlers) = self.event_handlers.get(event) {
            let c_event = std::ffi::CString::new(event).unwrap_or_default();
            let c_data = std::ffi::CString::new(json_data).unwrap_or_default();
            let ev_ptr = c_event.as_ptr();
            let dt_ptr = c_data.as_ptr();
            for handler in handlers {
                (handler.callback)(ev_ptr, dt_ptr, handler.user_data);
            }
        }
    }

    #[allow(dead_code)]
    pub fn subscribe_to_event(
        &mut self,
        event: &str,
        callback: CbPluginEvent,
        user_data: *mut c_void,
    ) -> u64 {
        let id = self.next_sub_id;
        self.next_sub_id += 1;
        self.event_handlers
            .entry(event.to_string())
            .or_default()
            .push(Subscription { id, callback, user_data });
        id
    }

    // ── Callback setters ───────────────────────────────────────────

    pub fn set_on_loaded(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_loaded = Some(cb);
        self.on_loaded_data = data;
    }

    pub fn set_on_unloaded(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_unloaded = Some(cb);
        self.on_unloaded_data = data;
    }

    pub fn set_on_error(&mut self, cb: CbPluginEvent, data: *mut c_void) {
        self.on_error = Some(cb);
        self.on_error_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_manager() {
        let pm = PluginManager::new();
        assert!(!pm.is_loaded("any"));
        assert!(pm.list_loaded().is_empty());
    }

    #[test]
    fn test_is_loaded_nonexistent() {
        let pm = PluginManager::new();
        assert!(!pm.is_loaded("nonexistent"));
    }

    #[test]
    fn test_plugin_version_nonexistent() {
        let pm = PluginManager::new();
        assert!(pm.plugin_version("nonexistent").is_none());
    }

    #[test]
    fn test_unload_plugin_nonexistent() {
        let mut pm = PluginManager::new();
        pm.unload_plugin("nonexistent"); // Should not panic
    }

    #[test]
    fn test_unload_all_empty() {
        let mut pm = PluginManager::new();
        pm.unload_all(); // Should not panic
    }

    #[test]
    fn test_load_plugins_nonexistent_dir() {
        let mut pm = PluginManager::new();
        let result = pm.load_plugins("/nonexistent/plugin/dir");
        assert!(result.is_err());
    }

    #[test]
    fn test_load_plugin_invalid_path() {
        let mut pm = PluginManager::new();
        let result = pm.load_plugin("/nonexistent/file.json");
        assert!(result.is_err());
    }

    #[test]
    fn test_build_dependency_graph_invalid_json() {
        let mut pm = PluginManager::new();
        let result = pm.build_dependency_graph(&["invalid json"]);
        assert!(result.is_err());
    }

    #[test]
    fn test_build_dependency_graph_missing_id() {
        let mut pm = PluginManager::new();
        let result = pm.build_dependency_graph(&[r#"{"name": "no-id"}"#]);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Missing plugin id"));
    }

    #[test]
    fn test_build_dependency_graph_valid() {
        let mut pm = PluginManager::new();
        let result = pm.build_dependency_graph(&[r#"{"id": "plugin_a"}"#]);
        assert!(result.is_ok());
    }

    #[test]
    fn test_topological_sort_empty() {
        let pm = PluginManager::new();
        assert!(pm.topological_sort().is_empty());
    }

    #[test]
    fn test_topological_sort_with_deps() {
        let mut pm = PluginManager::new();
        pm.build_dependency_graph(&[
            r#"{"id": "a"}"#,
            r#"{"id": "b", "dependencies": ["a"]}"#,
        ]).unwrap();
        let order = pm.topological_sort();
        assert_eq!(order.len(), 2);
        assert_eq!(order[0], "a");
        assert_eq!(order[1], "b");
    }

    #[test]
    fn test_set_callbacks() {
        let mut pm = PluginManager::new();
        extern "C" fn dummy_cb(_: *const std::os::raw::c_char, _: *const std::os::raw::c_char, _: *mut c_void) {}
        pm.set_on_loaded(dummy_cb, std::ptr::null_mut());
        pm.set_on_unloaded(dummy_cb, std::ptr::null_mut());
        pm.set_on_error(dummy_cb, std::ptr::null_mut());
    }

    #[test]
    fn test_plugin_state_enum() {
        assert_eq!(PluginState::NotLoaded as isize, 0isize);
        assert_eq!(PluginState::Loaded as isize, 1isize);
    }
}
