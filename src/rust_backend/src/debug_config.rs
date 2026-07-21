//! ## Debug Configuration Manager
//!
//! Manages debug configurations (launch.json entries).
//! Replaces `DebugConfiguration` from the original C++ codebase.

use std::collections::HashMap;

pub struct DebugConfigurationManager {
    configs: HashMap<String, String>, // name -> JSON string
}

impl DebugConfigurationManager {
    pub fn new() -> Self {
        Self {
            configs: HashMap::new(),
        }
    }

    /// Load configurations from a JSON array string (e.g., from launch.json).
    pub fn load(&mut self, json_config: &str) -> Result<(), String> {
        let parsed: serde_json::Value = serde_json::from_str(json_config)
            .map_err(|e| format!("Invalid debug config JSON: {}", e))?;

        let configs = parsed
            .get("configurations")
            .and_then(|c| c.as_array())
            .ok_or_else(|| "Missing 'configurations' array".to_string())?;

        for config in configs {
            let name = config
                .get("name")
                .and_then(|n| n.as_str())
                .ok_or_else(|| "Debug config missing 'name'".to_string())?;

            self.configs
                .insert(name.to_string(), serde_json::to_string(config).unwrap_or_default());
        }

        Ok(())
    }

    /// List all configuration names.
    pub fn list(&self) -> Vec<String> {
        self.configs.keys().cloned().collect()
    }

    /// Get a configuration by name.
    pub fn get(&self, name: &str) -> Option<&str> {
        self.configs.get(name).map(|s| s.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_manager() {
        let m = DebugConfigurationManager::new();
        assert!(m.list().is_empty());
        assert!(m.get("anything").is_none());
    }

    #[test]
    fn test_load_single_config() {
        let mut m = DebugConfigurationManager::new();
        let json = r#"{
            "configurations": [
                {"name": "Launch Program", "type": "cppdbg", "request": "launch"}
            ]
        }"#;
        m.load(json).unwrap();
        assert_eq!(m.list(), vec!["Launch Program"]);
        let config = m.get("Launch Program").unwrap();
        assert!(config.contains("cppdbg"));
    }

    #[test]
    fn test_load_multiple_configs() {
        let mut m = DebugConfigurationManager::new();
        let json = r#"{
            "configurations": [
                {"name": "Debug", "type": "gdb"},
                {"name": "Release", "type": "lldb"}
            ]
        }"#;
        m.load(json).unwrap();
        let mut list = m.list();
        list.sort();
        assert_eq!(list, vec!["Debug", "Release"]);
    }

    #[test]
    fn test_load_invalid_json() {
        let mut m = DebugConfigurationManager::new();
        let result = m.load("not valid json");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Invalid debug config JSON"));
    }

    #[test]
    fn test_load_missing_configurations() {
        let mut m = DebugConfigurationManager::new();
        let result = m.load(r#"{"type": "something"}"#);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Missing 'configurations' array"));
    }

    #[test]
    fn test_load_config_missing_name() {
        let mut m = DebugConfigurationManager::new();
        let json = r#"{
            "configurations": [
                {"type": "cppdbg"}
            ]
        }"#;
        let result = m.load(json);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("missing 'name'"));
    }

    #[test]
    fn test_get_nonexistent() {
        let m = DebugConfigurationManager::new();
        assert!(m.get("nonexistent").is_none());
    }

    #[test]
    fn test_load_twice_accumulates() {
        let mut m = DebugConfigurationManager::new();
        m.load(r#"{"configurations": [{"name": "A"}]}"#).unwrap();
        m.load(r#"{"configurations": [{"name": "B"}]}"#).unwrap();
        let mut list = m.list();
        list.sort();
        assert_eq!(list, vec!["A", "B"]);
    }

    #[test]
    fn test_overwrite_config() {
        let mut m = DebugConfigurationManager::new();
        m.load(r#"{"configurations": [{"name": "X", "type": "old"}]}"#).unwrap();
        m.load(r#"{"configurations": [{"name": "X", "type": "new"}]}"#).unwrap();
        let config = m.get("X").unwrap();
        assert!(config.contains("new"));
    }
}
