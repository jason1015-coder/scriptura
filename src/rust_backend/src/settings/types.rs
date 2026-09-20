//! # Settings Types

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SettingCategory {
    Standard,
    Sensitive,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SettingKey {
    pub name: String,
    pub category: SettingCategory,
}

impl SettingKey {
    pub fn standard(name: impl Into<String>) -> Self {
        Self { name: name.into(), category: SettingCategory::Standard }
    }

    pub fn sensitive(name: impl Into<String>) -> Self {
        Self { name: name.into(), category: SettingCategory::Sensitive }
    }
}

impl From<&str> for SettingKey {
    fn from(s: &str) -> Self {
        Self { name: s.to_string(), category: SettingCategory::Standard }
    }
}

impl std::fmt::Display for SettingKey {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SettingsData {
    pub version: u32,
    pub schema: String,
    pub data: std::collections::HashMap<String, String>,
}

impl Default for SettingsData {
    fn default() -> Self {
        Self {
            version: 1,
            schema: "scriptura-settings-v1".to_string(),
            data: std::collections::HashMap::new(),
        }
    }
}

/// Markers that make a setting sensitive enough to belong in the OS keychain.
const SENSITIVE_MARKERS: [&str; 8] = [
    "apikey",
    "api_key",
    "token",
    "password",
    "passwd",
    "secret",
    "credential",
    "privatekey",
];

/// Decide whether a settings key holds a secret. Matches the `secret/` prefix
/// (used by the explicit secret API) and common token/password names, so the
/// existing `QSettings`-style keys such as `ai/apiKey` move to the keychain
/// without touching the call sites.
pub fn is_sensitive_key(name: &str) -> bool {
    let lowered = name.to_ascii_lowercase();
    if lowered.starts_with("secret/") || lowered.contains("/secret/") {
        return true;
    }
    let leaf = lowered.rsplit('/').next().unwrap_or(lowered.as_str());
    SENSITIVE_MARKERS.iter().any(|marker| leaf.contains(marker))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sensitive_key_detection() {
        for key in [
            "secret/foo",
            "ai/apiKey",
            "ai/api_key",
            "auth/token",
            "git/password",
            "plugins/credential",
            "ssl/privateKey",
            "db/passwd",
        ] {
            assert!(is_sensitive_key(key), "{key} must be treated as sensitive");
        }
        for key in [
            "theme/selected",
            "editor/font",
            "recentProjects",
            "mainWindow/geometry",
            "shortcuts/open_file",
            "snippets",
        ] {
            assert!(!is_sensitive_key(key), "{key} must not be treated as sensitive");
        }
    }

    #[test]
    fn test_default_schema_is_versioned() {
        let data = SettingsData::default();
        assert_eq!(data.version, 1);
        assert_eq!(data.schema, "scriptura-settings-v1");
        assert!(data.data.is_empty());
    }
}
