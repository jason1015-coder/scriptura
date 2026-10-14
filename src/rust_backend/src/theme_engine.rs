// ── Theme Engine — Rust Backend ──────────────────────────────────────────────
// Scans theme JSON files, validates the schema, resolves + merges colors onto
// the built-in family/mode base, and stores resolved ThemeDefinitions.
// This is the core of the Phase 1 plugin system (themes-only).

use std::collections::HashMap;
use std::fs;
use std::path::Path;
use std::sync::Mutex;

// ── Types ───────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ThemeManifest {
    pub id: String,
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub kind: String, // "theme" | "tab"
    #[serde(default)]
    pub theme: Option<ThemeConfig>,
    #[serde(default)]
    pub themes: Option<Vec<ThemeEntry>>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ThemeEntry {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub theme: Option<ThemeConfig>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ThemeConfig {
    #[serde(default)]
    pub family: Option<String>,
    #[serde(default)]
    pub mode: Option<String>, // "light" | "dark"
    #[serde(default)]
    pub features: Option<serde_json::Value>,
    #[serde(default)]
    pub colors: Option<serde_json::Value>,
}

/// Resolved theme envelope returned by `resolve()` and consumed by C++.
/// `resolved` is a COMPLETE ThemeDefinition JSON (built-in base merged with
/// any subset of the 24 color keys the manifest supplies).
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ThemeLoadResult {
    pub id: String,
    pub name: String,
    pub version: String,
    pub resolved: serde_json::Value,
}

/// Multiple resolved themes from a single plugin.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct MultiThemeLoadResult {
    pub plugin_id: String,
    pub plugin_name: String,
    pub plugin_version: String,
    pub themes: Vec<ThemeLoadResult>,
}

/// C++-facing resolved-theme record used by the registry list.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct PluginRecord {
    pub id: String,
    pub name: String,
    pub version: String,
    pub kind: String,
    pub enabled: bool,
    pub state: String, // "loaded", "failed-load", "disabled"
}

// ── Merge ───────────────────────────────────────────────────────────────────

/// Built-in fallback colors for the 8 families × 2 modes.
pub fn builtin_colors(family: &str, mode: &str) -> serde_json::Value {
    // This is intentionally lightweight: the C++ ThemeManager::buildDefinition
    // holds the real palette. Here we return a minimal JSON object so that the
    // Rust-only unit tests can prove the merge is correct, and so that the
    // resolved JSON always contains every color key.
    let dark = mode == "dark";
    let base = match family {
        "blue" => if dark { "#192332" } else { "#f0f8ff" },
        "green" => if dark { "#192d1e" } else { "#f0fff0" },
        "red" => if dark { "#2d1919" } else { "#fff5f5" },
        "yellow" => if dark { "#2d2d19" } else { "#fffff0" },
        "brown" => if dark { "#281e14" } else { "#fffaf0" },
        "cyan" => if dark { "#192d2d" } else { "#f0ffff" },
        "violet" => if dark { "#231932" } else { "#f5f0ff" },
        _ => if dark { "#2d2d30" } else { "#f5f5f7" },
    };

    serde_json::json!({
        "bgColor": base,
        "textColor": if dark { "#afafb6" } else { "#696970" },
        "svgColor": if dark { "#8c8c94" } else { "#7d7d87" },
        "buttonColor": if dark { "#444449" } else { "#eaedef" },
        "highlightColor": if dark { "#6496ff" } else { "#007aff" },
        "baseColor": if dark { "#2a2a2e" } else { "#fafafa" },
        "borderColor": if dark { "#4c4c51" } else { "#dcdcdf" },
        "lightColor": if dark { "#525257" } else { "#e8e8ec" },
        "midColor": if dark { "#424247" } else { "#ceceD2" },
        "darkColor": if dark { "#343439" } else { "#bebeb2" },
        "buttonTextColor": if dark { "#afafb6" } else { "#696970" },
        "highlightedText": if dark { "#ffffff" } else { "#ffffff" },
        "synKeyword": if dark { "#93c5fd" } else { "#1d4ed8" },
        "synString": if dark { "#86efac" } else { "#15803d" },
        "synComment": if dark { "#94a3b8" } else { "#64748b" },
        "synNumber": if dark { "#c084fc" } else { "#9333ea" },
        "synPreprocessor": if dark { "#a855f7" } else { "#7e22ce" },
        "synTag": if dark { "#60a5fa" } else { "#2563eb" },
        "synAttribute": if dark { "#fbbf24" } else { "#a16207" },
        "synCssProperty": if dark { "#2dd4bf" } else { "#0f766e" },
        "synVariable": if dark { "#38bdf8" } else { "#0369a1" },
        "synFunction": if dark { "#f97316" } else { "#d97706" },
        "synEscape": if dark { "#22d3ee" } else { "#0e7490" }
    })
}

/// Resolve a single ThemeConfig onto the built-in base.
/// Unknown keys are ignored; missing keys fall back to the base.
pub fn resolve_colors(manifest: &ThemeManifest) -> serde_json::Value {
    let base = builtin_colors(
        manifest.theme.as_ref().and_then(|t| t.family.as_deref()).unwrap_or("default"),
        manifest.theme.as_ref().and_then(|t| t.mode.as_deref()).unwrap_or("light"),
    );

    let base_map = base.as_object().expect("builtin_colors is always an object");
    let mut merged: serde_json::Map<String, serde_json::Value> = base_map.clone();

    if let Some(colors) = &manifest.theme.as_ref().and_then(|t| t.colors.as_ref()) {
        if let Some(colors_map) = colors.as_object() {
            for (key, val) in colors_map {
                if base_map.contains_key(key) {
                    merged.insert(key.clone(), val.clone());
                }
            }
        }
    }

    serde_json::Value::Object(merged)
}

/// Resolve a ThemeEntry onto the built-in base.
pub fn resolve_theme_entry(entry: &ThemeEntry, default_family: &str, default_mode: &str) -> serde_json::Value {
    let base = builtin_colors(
        entry.theme.as_ref().and_then(|t| t.family.as_deref()).unwrap_or(default_family),
        entry.theme.as_ref().and_then(|t| t.mode.as_deref()).unwrap_or(default_mode),
    );

    let base_map = base.as_object().expect("builtin_colors is always an object");
    let mut merged: serde_json::Map<String, serde_json::Value> = base_map.clone();

    if let Some(colors) = &entry.theme.as_ref().and_then(|t| t.colors.as_ref()) {
        if let Some(colors_map) = colors.as_object() {
            for (key, val) in colors_map {
                if base_map.contains_key(key) {
                    merged.insert(key.clone(), val.clone());
                }
            }
        }
    }

    serde_json::Value::Object(merged)
}

/// Resolve multiple themes from a manifest (supports both legacy single theme and new multiple themes).
pub fn resolve_all_themes(manifest: &ThemeManifest) -> Vec<ThemeLoadResult> {
    let mut results = Vec::new();
    
    // Default family/mode from the plugin-level theme config (if any)
    let default_family = manifest.theme.as_ref().and_then(|t| t.family.as_deref()).unwrap_or("default");
    let default_mode = manifest.theme.as_ref().and_then(|t| t.mode.as_deref()).unwrap_or("light");
    
    // If there are multiple themes, use those
    if let Some(themes) = &manifest.themes {
        for entry in themes {
            let resolved = resolve_theme_entry(entry, default_family, default_mode);
            results.push(ThemeLoadResult {
                id: entry.id.clone(),
                name: entry.name.clone(),
                version: manifest.version.clone(),
                resolved,
            });
        }
    } 
    // Otherwise fall back to single theme (legacy)
    else if manifest.theme.is_some() {
        let resolved = resolve_colors(manifest);
        results.push(ThemeLoadResult {
            id: manifest.id.clone(),
            name: manifest.name.clone(),
            version: manifest.version.clone(),
            resolved,
        });
    }
    
    results
}


// ── Public scanner API ───────────────────────────────────────────────────────

/// Load all themes from a plugin file (supports multiple themes per file).
pub fn load_themes(themes_dir: &Path, id: &str) -> Result<Vec<ThemeLoadResult>, String> {
    let theme_file = themes_dir.join(format!("{}.json", id));
    if !theme_file.exists() {
        return Err(format!("Theme file not found: {}", theme_file.display()));
    }

    let raw = fs::read_to_string(&theme_file).map_err(|e| format!("Failed to read theme file: {}", e))?;
    let manifest: ThemeManifest = serde_json::from_str(&raw)
        .map_err(|e| format!("Invalid theme JSON: {}", e))?;

    // Validate it's a theme
    if manifest.kind != "" && manifest.kind != "theme" {
        return Err(format!(
            "Invalid theme kind '{}' for id '{}'; expected 'theme' or omitted",
            manifest.kind, manifest.id
        ));
    }

    let themes = resolve_all_themes(&manifest);
    if themes.is_empty() {
        return Err(format!("No valid themes found in plugin '{}'", id));
    }

    Ok(themes)
}

/// Load a single theme by plugin ID and theme ID (for backwards compatibility).
pub fn load_theme(themes_dir: &Path, plugin_id: &str) -> Result<ThemeLoadResult, String> {
    let themes = load_themes(themes_dir, plugin_id)?;
    // Return the first theme for backwards compatibility
    themes.into_iter().next().ok_or_else(|| "No themes found".to_string())
}

/// Scan all theme files in the directory and return a list of loaded themes.
pub fn scan_themes(themes_dir: &Path) -> Vec<ThemeLoadResult> {
    let mut results = Vec::new();
    let dir = match fs::read_dir(themes_dir) {
        Ok(d) => d,
        Err(_) => return results,
    };
    for entry in dir.flatten() {
        let path = entry.path();
        if path.is_file() && path.extension().and_then(|s| s.to_str()) == Some("json") {
            let id = path.file_stem().and_then(|s| s.to_str()).unwrap_or_default();
            match load_themes(themes_dir, id) {
                Ok(themes) => results.extend(themes),
                Err(e) => eprintln!("Failed to load theme {}: {}", path.display(), e),
            }
        }
    }
    results
}

// ── Theme Engine State ──────────────────────────────────────────────────────

pub struct ThemeEngine {
    themes: Mutex<HashMap<String, ThemeLoadResult>>,
    disabled: Mutex<HashMap<String, bool>>,
}

impl ThemeEngine {
    pub fn new() -> Self {
        Self {
            themes: Mutex::new(HashMap::new()),
            disabled: Mutex::new(HashMap::new()),
        }
    }

    pub fn scan(&self, dir: &str) {
        let path = Path::new(dir);
        let results = scan_themes(path);
        let mut themes = self.themes.lock().unwrap();
        themes.clear();
        for r in results {
            themes.insert(r.id.clone(), r);
        }
    }

    pub fn list(&self) -> Vec<ThemeLoadResult> {
        let themes = self.themes.lock().unwrap();
        let mut v: Vec<_> = themes.values().cloned().collect();
        v.sort_by(|a, b| a.id.cmp(&b.id));
        v
    }

    pub fn get(&self, id: &str) -> Option<ThemeLoadResult> {
        let themes = self.themes.lock().unwrap();
        themes.get(id).cloned()
    }

    pub fn validate(&self, json_manifest: &str) -> Result<String, String> {
        serde_json::from_str::<ThemeManifest>(json_manifest)
            .map(|_| String::new())
            .map_err(|e| format!("Invalid theme JSON: {}", e))
    }

    pub fn set_disabled(&self, id: &str, disabled: bool) {
        let mut d = self.disabled.lock().unwrap();
        d.insert(id.to_string(), disabled);
    }

    pub fn is_disabled(&self, id: &str) -> bool {
        let d = self.disabled.lock().unwrap();
        d.get(id).copied().unwrap_or(false)
    }
}

impl Default for ThemeEngine {
    fn default() -> Self {
        Self::new()
    }
}

// ── FFI Helpers ──────────────────────────────────────────────────────────────
pub(crate) unsafe fn cstr_to_str<'a>(ptr: *const std::os::raw::c_char) -> &'a str {
    if ptr.is_null() {
        return "";
    }
    std::ffi::CStr::from_ptr(ptr).to_str().unwrap_or("")
}

pub(crate) fn str_to_cstring(s: &str) -> *mut std::os::raw::c_char {
    std::ffi::CString::new(s).unwrap_or_default().into_raw()
}

// Export the public API for the C++ bindings.

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_valid_theme() {
        let te = ThemeEngine::new();
        let manifest = "{\"id\":\"t1\",\"name\":\"T1\",\"version\":\"1.0.0\",\"kind\":\"theme\",\"theme\":{\"colors\":{\"bgColor\":\"#111111\"}}}";
        let res = te.validate(manifest);
        assert!(res.is_ok());
    }

    #[test]
    fn test_validate_invalid() {
        let te = ThemeEngine::new();
        let res = te.validate("{not json");
        assert!(res.is_err());
    }

    #[test]
    fn test_resolve_colors_subset() {
        let manifest = ThemeManifest {
            id: "my-theme".into(),
            name: "My Theme".into(),
            version: "1.0.0".into(),
            kind: "theme".into(),
            theme: Some(ThemeConfig {
                family: Some("blue".into()),
                mode: Some("dark".into()),
                features: None,
                colors: Some(serde_json::json!({
                    "bgColor": "#1a1b1e",
                    "highlightColor": "#3b82f6",
                    "synKeyword": "#93c5fd"
                })),
            }),
            themes: None,
        };

        let resolved = resolve_colors(&manifest);
        let obj = resolved.as_object().expect("resolved is an object");

        // Only the three provided keys should differ from the built-in blue/dark base.
        assert_eq!(obj["bgColor"], "#1a1b1e");
        assert_eq!(obj["highlightColor"], "#3b82f6");
        assert_eq!(obj["synKeyword"], "#93c5fd");

        // Keys not supplied in the manifest must still be present from the base.
        assert!(obj.contains_key("textColor"));
        assert!(obj.contains_key("buttonColor"));
        assert!(obj.contains_key("synString"));
    }

    #[test]
    fn test_resolve_colors_empty_theme_block() {
        let manifest = ThemeManifest {
            id: "fallback-theme".into(),
            name: "Fallback Theme".into(),
            version: "1.0.0".into(),
            kind: "theme".into(),
            theme: Some(ThemeConfig {
                family: None,
                mode: None,
                features: None,
                colors: None,
            }),
            themes: None,
        };

        let resolved = resolve_colors(&manifest);
        let obj = resolved.as_object().expect("resolved is an object");

        // With no family/mode/colors the manifest falls back to default/light.
        assert_eq!(obj["bgColor"], "#f5f5f7");
        assert_eq!(obj["highlightColor"], "#007aff");
    }

    #[test]
    fn test_resolve_colors_unknown_keys_ignored() {
        let manifest = ThemeManifest {
            id: "forward-compat".into(),
            name: "Forward Compat".into(),
            version: "1.0.0".into(),
            kind: "theme".into(),
            theme: Some(ThemeConfig {
                family: None,
                mode: None,
                features: None,
                colors: Some(serde_json::json!({
                    "bgColor": "#000000",
                    "unknownFutureKey": "#ffffff"
                })),
            }),
            themes: None,
        };

        let resolved = resolve_colors(&manifest);
        let obj = resolved.as_object().expect("resolved is an object");

        assert_eq!(obj["bgColor"], "#000000");
        // unknownFutureKey must not appear in the resolved theme.
        assert!(!obj.contains_key("unknownFutureKey"));
        // But the full base set must still exist.
        assert!(obj.contains_key("textColor"));
    }

    // ── Rusty heritability lint ──────────────────────────────────────────
    // The plugin system guarantee (PLUGINS.md §2): a custom theme is a COMPLETE
    // ThemeDefinition; C++ never fills in missing colors. The macro below is
    // intended to make that compile-time-lintable in the full plugin registry
    // (Phase 2). For the theme-only increment it is a no-op placeholder so the
    // trait/derive scaffolding is in place and the doc tests can reference it.
    #[macro_export]
    macro_rules! rust_theme_heritage {
        // Phase 1: a pass-through placeholder so the trait/derive scaffolding and
        // doc tests can reference it. When the generic KindHandler trait lands,
        // this macro will assert that every resolved theme contains the full
        // 24-color key set before handing it to the C++ adapter.
        ($($tokens:tt)*) => {};
    }

    /// Heritability guarantee (plugin system §2 / §5.2): a resolved theme is a
    /// COMPLETE ThemeDefinition. The merge in `resolve_colors` carries every one
    /// of the 24 color keys, either from the manifest or from the built-in
    /// family/mode base. Unknown manifest keys are ignored; missing keys never
    /// propagate.
    //
    /// ```rust
    /// use scriptura_backend::theme_engine::{self, ThemeManifest, ThemeConfig};
    /// use serde_json::json;
    ///
    /// let manifest = ThemeManifest {
    ///     id: "minimal".into(),
    ///     name: "Minimal".into(),
    ///     version: "1.0.0".into(),
    ///     kind: "theme".into(),
    ///     theme: Some(ThemeConfig {
    ///         family: Some("blue".into()),
    ///         mode: Some("dark".into()),
    ///         features: None,
    ///         colors: Some(json!({
    ///             "bgColor": "#1a1b1e",
    ///             "highlightColor": "#3b82f6",
    ///             "synKeyword": "#93c5fd"
    ///         })),
    ///     }),
    /// };
    ///
    /// let resolved = theme_engine::resolve_colors(&manifest);
    /// let obj = resolved.as_object().unwrap();
    ///
    /// // Provided keys are honored.
    /// assert_eq!(obj["bgColor"], "#1a1b1e");
    /// assert_eq!(obj["highlightColor"], "#3b82f6");
    /// assert_eq!(obj["synKeyword"], "#93c5fd");
    ///
    /// // Unspecified keys are still present from the built-in base.
    /// assert!(obj.contains_key("textColor"));
    /// assert!(obj.contains_key("synString"));
    /// assert!(obj["textColor"].is_string());
    ///
    /// // The resolved object is always the full 23-color set.
    /// assert_eq!(obj.len(), 23);
    /// ```
    #[test]
    fn doc_resolve_colors_heritage() {
        use serde_json::json;

        let manifest = ThemeManifest {
            id: "minimal".into(),
            name: "Minimal".into(),
            version: "1.0.0".into(),
            kind: "theme".into(),
            theme: Some(ThemeConfig {
                family: Some("blue".into()),
                mode: Some("dark".into()),
                features: None,
                colors: Some(json!({
                    "bgColor": "#1a1b1e",
                    "highlightColor": "#3b82f6",
                    "synKeyword": "#93c5fd"
                })),
            }),
            themes: None,
        };

        let resolved = resolve_colors(&manifest);
        let obj = resolved.as_object().expect("resolved is an object");

        assert_eq!(obj["bgColor"], "#1a1b1e");
        assert_eq!(obj["highlightColor"], "#3b82f6");
        assert_eq!(obj["synKeyword"], "#93c5fd");

        assert!(obj.contains_key("textColor"));
        assert!(obj.contains_key("synString"));
        assert!(obj["textColor"].is_string());

        assert_eq!(obj.len(), 23);
    }

}
