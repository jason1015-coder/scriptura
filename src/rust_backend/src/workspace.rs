//! ## Workspace
//!
//! Manages workspace state including open folders, settings, and recent files.
//! Replaces `Workspace` (QObject-based) from the original C++ codebase.

use serde_json::{json, Value};
use std::fs;

pub struct Workspace {
    path: String,
    folders: Vec<String>,
    settings: Value,
    recent_files: Vec<String>,
    loaded: bool,
}

impl Workspace {
    pub fn new() -> Self {
        Self {
            path: String::new(),
            folders: Vec::new(),
            settings: json!({}),
            recent_files: Vec::new(),
            loaded: false,
        }
    }

    /// Load workspace from a file.
    pub fn load(&mut self, path: &str) -> Result<(), String> {
        let content = fs::read_to_string(path)
            .map_err(|e| format!("Cannot read workspace file: {}", e))?;
        let parsed: Value = serde_json::from_str(&content)
            .map_err(|e| format!("Invalid workspace JSON: {}", e))?;

        self.path = path.to_string();

        if let Some(folders) = parsed.get("folders").and_then(|f| f.as_array()) {
            self.folders = folders
                .iter()
                .filter_map(|f| f.as_str().map(String::from))
                .collect();
        }

        if let Some(settings) = parsed.get("settings") {
            self.settings = settings.clone();
        }

        if let Some(files) = parsed.get("recentFiles").and_then(|f| f.as_array()) {
            self.recent_files = files
                .iter()
                .filter_map(|f| f.as_str().map(String::from))
                .collect();
        }

        self.loaded = true;
        Ok(())
    }

    /// Save workspace state to the current path.
    pub fn save(&self) -> Result<(), String> {
        let content = self.to_json();
        fs::write(&self.path, &content)
            .map_err(|e| format!("Cannot save workspace: {}", e))
    }

    /// Save workspace state to a different path.
    pub fn save_as(&mut self, path: &str) -> Result<(), String> {
        self.path = path.to_string();
        self.save()
    }

    fn to_json(&self) -> String {
        let folders: Vec<Value> = self.folders.iter().map(|f| json!(f)).collect();
        let recent: Vec<Value> = self.recent_files.iter().map(|f| json!(f)).collect();
        let ws = json!({
            "folders": folders,
            "settings": self.settings,
            "recentFiles": recent
        });
        serde_json::to_string_pretty(&ws).unwrap_or_default()
    }

    // ── Getters / Setters ──────────────────────────────────────────

    pub fn folders(&self) -> &[String] {
        &self.folders
    }

    pub fn set_folders(&mut self, folders: Vec<String>) {
        self.folders = folders;
    }

    pub fn settings(&self) -> String {
        serde_json::to_string(&self.settings).unwrap_or_default()
    }

    pub fn set_settings(&mut self, json_settings: &str) {
        if let Ok(val) = serde_json::from_str::<Value>(json_settings) {
            self.settings = val;
        }
    }

    pub fn recent_files(&self) -> &[String] {
        &self.recent_files
    }

    pub fn add_recent_file(&mut self, file: &str) {
        // Remove if already present, then add to front
        self.recent_files.retain(|f| f != file);
        self.recent_files.insert(0, file.to_string());

        // Limit to 20 recent files
        const MAX_RECENT: usize = 20;
        if self.recent_files.len() > MAX_RECENT {
            self.recent_files.truncate(MAX_RECENT);
        }
    }

    pub fn path(&self) -> &str {
        &self.path
    }

    pub fn is_loaded(&self) -> bool {
        self.loaded
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_new_workspace() {
        let ws = Workspace::new();
        assert!(!ws.is_loaded());
        assert!(ws.folders().is_empty());
        assert!(ws.recent_files().is_empty());
        assert_eq!(ws.settings(), "{}");
        assert_eq!(ws.path(), "");
    }

    #[test]
    fn test_set_folders() {
        let mut ws = Workspace::new();
        ws.set_folders(vec!["/a".into(), "/b".into()]);
        assert_eq!(ws.folders(), &["/a".to_string(), "/b".to_string()]);
    }

    #[test]
    fn test_set_settings() {
        let mut ws = Workspace::new();
        ws.set_settings(r#"{"theme": "dark"}"#);
        assert_eq!(ws.settings(), r#"{"theme":"dark"}"#);
    }

    #[test]
    fn test_set_settings_invalid() {
        let mut ws = Workspace::new();
        ws.set_settings("not valid json");
        // Should remain unchanged
        assert_eq!(ws.settings(), "{}");
    }

    #[test]
    fn test_add_recent_file() {
        let mut ws = Workspace::new();
        ws.add_recent_file("/file1.txt");
        ws.add_recent_file("/file2.txt");
        assert_eq!(ws.recent_files(), &["/file2.txt", "/file1.txt"]);
    }

    #[test]
    fn test_add_recent_file_duplicate_moves_to_front() {
        let mut ws = Workspace::new();
        ws.add_recent_file("/a");
        ws.add_recent_file("/b");
        ws.add_recent_file("/a"); // should move to front
        assert_eq!(ws.recent_files(), &["/a", "/b"]);
    }

    #[test]
    fn test_add_recent_file_max_limit() {
        let mut ws = Workspace::new();
        for i in 0..25 {
            ws.add_recent_file(&format!("/file{}", i));
        }
        assert_eq!(ws.recent_files().len(), 20);
        assert_eq!(ws.recent_files()[0], "/file24");
    }

    #[test]
    fn test_load_invalid_json() {
        let mut ws = Workspace::new();
        let result = ws.load("/nonexistent/path.json");
        assert!(result.is_err());
        assert!(!ws.is_loaded());
    }

    #[test]
    fn test_save_without_path() {
        let ws = Workspace::new();
        let result = ws.save();
        assert!(result.is_err());
    }

    #[test]
    fn test_save_as_empty_path() {
        let mut ws = Workspace::new();
        let result = ws.save_as("");
        assert!(result.is_err());
    }

    #[test]
    fn test_is_loaded_after_new() {
        let ws = Workspace::new();
        assert!(!ws.is_loaded());
    }

    #[test]
    fn test_empty_folders() {
        let ws = Workspace::new();
        assert!(ws.folders().is_empty());
    }

    #[test]
    fn test_empty_recent_files() {
        let ws = Workspace::new();
        assert!(ws.recent_files().is_empty());
    }
}
