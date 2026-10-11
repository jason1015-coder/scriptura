// ── Plugin Registry — the generic kind-registry core (PLUGINS.md §3) ──────────
//
// One registry, many kind handlers. The registry parses the shared manifest
// envelope, validates it (id / filename stem / semver / kind / apiVersion),
// checks the persisted enabled flag, then dispatches on `kind` to a registered
// `KindHandler`. Code-plugin loading (`libloading`, `HostApi`, `PluginVTable`)
// and the `tab` kind are Phase 2 and are deliberately not implemented here; the
// only built-in handler for this theme-only increment is `theme`, which
// delegates the colour merge to `theme_engine`.

use std::collections::HashMap;
use std::ffi::CStr;
use std::fs;
use std::path::Path;
use std::sync::Mutex;

use serde_json::Value;

use crate::theme_engine::{resolve_colors, ThemeManifest};

/// Host API version this build supports. A manifest whose `apiVersion`
/// exceeds this is rejected (PLUGINS.md §2).
pub const HOST_API_VERSION: u32 = 1;

/// Settings key prefix under which per-plugin disabled flags are persisted.
/// The full key is `plugin/disabled/<id>` (PLUGINS.md §3.4).
const DISABLED_KEY_PREFIX: &str = "plugin/disabled/";

// ── Persistence helpers (best-effort; no-op when the store is not ready) ─────
//
// The registry owns the disabled state and mirrors it to the encrypted settings
// store so the flag survives a restart (decision #6: Rust persists-enabled).
// When the store is not initialised (e.g. unit tests) these degrade to an
// in-memory-only cache, which keeps the registry fully testable.

fn settings_get(key: &str) -> Option<String> {
    let ckey = std::ffi::CString::new(key).ok()?;
    let ptr = crate::settings::ffi::settings_get(ckey.as_ptr());
    if ptr.is_null() {
        return None;
    }
    let s = unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned();
    crate::settings::ffi::settings_free_string(ptr);
    Some(s)
}

fn settings_set(key: &str, value: &str) {
    if let (Ok(ckey), Ok(cval)) = (
        std::ffi::CString::new(key),
        std::ffi::CString::new(value),
    ) {
        crate::settings::ffi::settings_set(ckey.as_ptr(), cval.as_ptr());
    }
}

// ── Kind handlers (PLUGINS.md §3.2) ──────────────────────────────────────────

/// Context handed to a kind handler when it activates a concrete plugin.
pub struct ActivateCtx<'a> {
    pub plugins_dir: &'a str,
    pub manifest: &'a Value,
}

/// A kind handler: knows how to validate and activate plugins of one `kind`.
/// `builtin()` distinguishes official compiled-in kinds from plugin-registered
/// ones. Built-in kinds are immutable (decision #5).
pub trait KindHandler: Send + Sync {
    fn kind(&self) -> &str;
    fn validate(&self, manifest: &Value) -> Result<(), String>;
    /// Activate a validated manifest; returns the resolved payload the C++
    /// layer consumes (for `theme`: a complete ThemeDefinition JSON).
    fn activate(&self, ctx: &ActivateCtx) -> Result<Value, String>;
    fn builtin(&self) -> bool {
        true
    }
}

/// The built-in `theme` handler. Delegates the colour merge to `theme_engine`
/// so a partial theme still resolves to a complete ThemeDefinition.
pub struct ThemeKindHandler;

impl KindHandler for ThemeKindHandler {
    fn kind(&self) -> &str {
        "theme"
    }

    fn validate(&self, manifest: &Value) -> Result<(), String> {
        // A theme manifest must deserialize into a ThemeManifest; an empty or
        // "theme" kind is accepted (PLUGINS.md §5.2).
        serde_json::from_value::<ThemeManifest>(manifest.clone())
            .map(|_| ())
            .map_err(|e| format!("invalid theme manifest: {e}"))
    }

    fn activate(&self, ctx: &ActivateCtx) -> Result<Value, String> {
        let manifest: ThemeManifest = serde_json::from_value(ctx.manifest.clone())
            .map_err(|e| format!("invalid theme manifest: {e}"))?;
        Ok(resolve_colors(&manifest))
    }

    fn builtin(&self) -> bool {
        true
    }
}

// ── Plugin record (the C++-facing envelope) ──────────────────────────────────

/// One row of the Plugins page: the shared envelope every kind produces.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct PluginRecord {
    pub id: String,
    pub name: String,
    pub version: String,
    pub kind: String,
    pub enabled: bool,
    pub state: String, // "loaded" | "failed-load" | "disabled"
}

struct KindEntry {
    builtin: bool,
    handler: Box<dyn KindHandler>,
}

// ── The registry ─────────────────────────────────────────────────────────────

pub struct PluginRegistry {
    kinds: Mutex<HashMap<String, KindEntry>>,
    records: Mutex<HashMap<String, PluginRecord>>,
    /// Resolved payload per plugin id (for `theme`: complete ThemeDefinition).
    resolved: Mutex<HashMap<String, Value>>,
    disabled: Mutex<HashMap<String, bool>>,
}

impl PluginRegistry {
    pub fn new() -> Self {
        let reg = Self {
            kinds: Mutex::new(HashMap::new()),
            records: Mutex::new(HashMap::new()),
            resolved: Mutex::new(HashMap::new()),
            disabled: Mutex::new(HashMap::new()),
        };
        // Register the built-in `theme` handler (immutable; decision #5).
        // The `tab` kind is Phase 2 and is not registered here.
        reg.register_kind(Box::new(ThemeKindHandler))
            .expect("built-in theme kind must register");
        reg
    }

    // ── Add-only kind registry (PLUGINS.md §3.3) ────────────────────────────

    /// Register a NEW kind handler. Add-only: an id that already exists
    /// (built-in *or* another plugin) is rejected. Kinds are never removed or
    /// overridden, so the editor's own kinds can't be hijacked.
    pub fn register_kind(&self, handler: Box<dyn KindHandler>) -> Result<(), String> {
        let id = handler.kind().to_string();
        let mut kinds = self.kinds.lock().unwrap();
        if kinds.contains_key(&id) {
            log::warn!("plugin registry: rejecting duplicate kind '{id}' (add-only)");
            return Err(format!("kind '{id}' already exists (add-only registry)"));
        }
        kinds.insert(id, KindEntry { builtin: handler.builtin(), handler });
        Ok(())
    }

    /// Attempt to remove/unregister a kind. Built-in kinds are immutable: this
    /// is a rejected no-op with a warning (decision #5). Only plugin-registered
    /// (non-builtin) kinds may be removed.
    pub fn remove_kind(&self, kind_id: &str) -> Result<(), String> {
        let mut kinds = self.kinds.lock().unwrap();
        match kinds.get(kind_id) {
            None => Err(format!("kind '{kind_id}' is not registered")),
            Some(entry) if entry.builtin => {
                log::warn!("plugin registry: cannot remove built-in kind '{kind_id}'");
                Err(format!("built-in kind '{kind_id}' is immutable"))
            }
            Some(_) => {
                kinds.remove(kind_id);
                Ok(())
            }
        }
    }

    pub fn has_kind(&self, kind_id: &str) -> bool {
        self.kinds.lock().unwrap().contains_key(kind_id)
    }
    // ── Scan (PLUGINS.md §3.1) ──────────────────────────────────────────

    pub fn scan(&self, dir: &str) {
        {
            self.records.lock().unwrap().clear();
            self.resolved.lock().unwrap().clear();
        }
        let path = Path::new(dir);
        let entries = match fs::read_dir(path) {
            Ok(e) => e,
            Err(_) => return,
        };
        for entry in entries.flatten() {
            let p = entry.path();
            if !p.is_file() || p.extension().and_then(|s| s.to_str()) != Some("json") {
                continue;
            }
            let stem = p
                .file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or_default()
                .to_string();
            let raw = match fs::read_to_string(&p) {
                Ok(r) => r,
                Err(_) => continue,
            };
            let manifest: Value = match serde_json::from_str(&raw) {
                Ok(v) => v,
                Err(_) => {
                    log::warn!("plugin registry: skipping malformed {}", p.display());
                    continue;
                }
            };
            self.ingest(&stem, dir, manifest);
        }
    }

    /// Validate + dispatch a single manifest envelope.
    fn ingest(&self, stem: &str, plugins_dir: &str, manifest: Value) {
        // id: non-empty and equal to the filename stem (PLUGINS.md §2).
        let id = manifest
            .get("id")
            .and_then(|v| v.as_str())
            .unwrap_or_default()
            .to_string();
        if id.is_empty() || id != stem {
            log::warn!("plugin registry: rejecting '{stem}' (id/filename mismatch)");
            return;
        }
        let name = manifest
            .get("name")
            .and_then(|v| v.as_str())
            .unwrap_or_default()
            .to_string();

        // version: valid semver (PLUGINS.md §2).
        let version = manifest
            .get("version")
            .and_then(|v| v.as_str())
            .unwrap_or_default()
            .to_string();
        if semver::Version::parse(&version).is_err() {
            log::warn!("plugin registry: rejecting '{id}' (invalid semver '{version}')");
            return;
        }

        // kind: must resolve to a known handler; empty means "theme" for
        // backward compatibility with plain theme files.
        let raw_kind = manifest
            .get("kind")
            .and_then(|v| v.as_str())
            .unwrap_or_default()
            .to_string();
        let kind = if raw_kind.is_empty() { "theme".to_string() } else { raw_kind };

        // apiVersion: must not exceed the host's supported API (PLUGINS.md §2).
        let api_version = manifest
            .get("apiVersion")
            .and_then(|v| v.as_u64())
            .unwrap_or(1);
        if api_version > HOST_API_VERSION as u64 {
            log::warn!(
                "plugin registry: rejecting '{id}' (apiVersion {api_version} > {HOST_API_VERSION})"
            );
            return;
        }

        // enabled? (persisted per plugin id, never per kind — decision #5).
        let enabled = !self.is_disabled(&id);
        if !enabled {
            self.record(PluginRecord {
                id,
                name,
                version,
                kind,
                enabled: false,
                state: "disabled".to_string(),
            });
            return;
        }
        // dispatch by kind: validate + activate via the registered handler.
        let outcome = {
            let kinds = self.kinds.lock().unwrap();
            match kinds.get(&kind) {
                None => None, // unknown kind → rejected below
                Some(entry) => {
                    if entry.handler.validate(&manifest).is_err() {
                        Some(Err("validate failed".to_string()))
                    } else {
                        let ctx = ActivateCtx { plugins_dir, manifest: &manifest };
                        Some(entry.handler.activate(&ctx))
                    }
                }
            }
        };

        match outcome {
            None => {
                log::warn!("plugin registry: rejecting '{id}' (unknown kind '{kind}')");
                // Unknown kind → rejected outright, not recorded as a row.
            }
            Some(Err(e)) => {
                log::warn!("plugin registry: '{id}' failed to activate: {e}");
                self.record(PluginRecord {
                    id,
                    name,
                    version,
                    kind,
                    enabled: true,
                    state: "failed-load".to_string(),
                });
            }
            Some(Ok(resolved)) => {
                self.resolved.lock().unwrap().insert(id.clone(), resolved);
                self.record(PluginRecord {
                    id,
                    name,
                    version,
                    kind,
                    enabled: true,
                    state: "loaded".to_string(),
                });
            }
        }
    }

    fn record(&self, rec: PluginRecord) {
        self.records.lock().unwrap().insert(rec.id.clone(), rec);
    }

    // ── Query ───────────────────────────────────────────────────────────

    pub fn list(&self) -> Vec<PluginRecord> {
        let records = self.records.lock().unwrap();
        let mut v: Vec<_> = records.values().cloned().collect();
        v.sort_by(|a, b| a.id.cmp(&b.id));
        v
    }

    /// Resolved payload for a plugin id (for `theme`, a complete
    /// ThemeDefinition JSON), or `None` if not loaded.
    pub fn get_resolved(&self, id: &str) -> Option<Value> {
        self.resolved.lock().unwrap().get(id).cloned()
    }

    // ── Enable / disable (persisted per plugin id — PLUGINS.md §3.4) ────

    pub fn set_disabled(&self, id: &str, disabled: bool) {
        self.disabled.lock().unwrap().insert(id.to_string(), disabled);
        let key = format!("{DISABLED_KEY_PREFIX}{id}");
        settings_set(&key, if disabled { "true" } else { "false" });

        // Keep the cached record in sync so list() reflects the change
        // immediately. Records snapshot `enabled`/`state` at scan time, but the
        // Plugins UI and its tests read list() right after a toggle (PLUGINS.md
        // §7) without forcing a re-scan.
        if let Some(rec) = self.records.lock().unwrap().get_mut(id) {
            rec.enabled = !disabled;
            if disabled {
                rec.state = "disabled".to_string();
            } else if rec.state == "disabled" {
                rec.state = "loaded".to_string();
            }
        }
    }

    pub fn is_disabled(&self, id: &str) -> bool {
        {
            let d = self.disabled.lock().unwrap();
            if let Some(&v) = d.get(id) {
                return v;
            }
        }
        // Fall back to the persisted flag, then cache it.
        let key = format!("{DISABLED_KEY_PREFIX}{id}");
        let disabled = matches!(settings_get(&key).as_deref(), Some("true"));
        self.disabled.lock().unwrap().insert(id.to_string(), disabled);
        disabled
    }
}

impl Default for PluginRegistry {
    fn default() -> Self {
        Self::new()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use std::fs;
    use std::io::Write;

    /// A stand-in for a plugin-registered (non-builtin) kind.
    struct DummyKind;
    impl KindHandler for DummyKind {
        fn kind(&self) -> &str {
            "panel"
        }
        fn validate(&self, _m: &Value) -> Result<(), String> {
            Ok(())
        }
        fn activate(&self, _ctx: &ActivateCtx) -> Result<Value, String> {
            Ok(json!({"ok": true}))
        }
        fn builtin(&self) -> bool {
            false
        }
    }

    fn write_plugin(dir: &std::path::Path, stem: &str, body: Value) {
        let mut f = fs::File::create(dir.join(format!("{stem}.json"))).unwrap();
        f.write_all(body.to_string().as_bytes()).unwrap();
    }

    fn theme_manifest(id: &str) -> Value {
        json!({
            "id": id,
            "name": "T",
            "version": "1.0.0",
            "kind": "theme",
            "theme": { "family": "blue", "mode": "dark",
                       "colors": { "bgColor": "#1a1b1e" } }
        })
    }

    #[test]
    fn test_builtin_theme_kind_registered() {
        let reg = PluginRegistry::new();
        assert!(reg.has_kind("theme"));
        // tab is Phase 2, not registered in the theme-only increment.
        assert!(!reg.has_kind("tab"));
    }

    #[test]
    fn test_register_kind_conflict_rejected() {
        let reg = PluginRegistry::new();
        // First registration of a brand-new kind is accepted.
        assert!(reg.register_kind(Box::new(DummyKind)).is_ok());
        // A second registration of the same id is rejected (add-only).
        assert!(reg.register_kind(Box::new(DummyKind)).is_err());
    }

    #[test]
    fn test_builtin_kind_cannot_be_overridden_or_removed() {
        let reg = PluginRegistry::new();
        // A plugin claiming the built-in `theme` id is rejected.
        struct HijackTheme;
        impl KindHandler for HijackTheme {
            fn kind(&self) -> &str { "theme" }
            fn validate(&self, _m: &Value) -> Result<(), String> { Ok(()) }
            fn activate(&self, _c: &ActivateCtx) -> Result<Value, String> { Ok(json!({})) }
            fn builtin(&self) -> bool { false }
        }
        assert!(reg.register_kind(Box::new(HijackTheme)).is_err());
        // Removing a built-in kind is an immutable no-op (rejected).
        assert!(reg.remove_kind("theme").is_err());
        assert!(reg.has_kind("theme"));
    }

    #[test]
    fn test_remove_plugin_kind_allowed() {
        let reg = PluginRegistry::new();
        assert!(reg.register_kind(Box::new(DummyKind)).is_ok());
        assert!(reg.remove_kind("panel").is_ok());
        assert!(!reg.has_kind("panel"));
    }
    #[test]
    fn test_scan_dispatch_and_merge() {
        let dir = tempfile::tempdir().unwrap();
        write_plugin(dir.path(), "my-theme", theme_manifest("my-theme"));
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        let list = reg.list();
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].id, "my-theme");
        assert_eq!(list[0].kind, "theme");
        assert!(list[0].enabled);
        assert_eq!(list[0].state, "loaded");
        // Resolved payload is a COMPLETE ThemeDefinition with the override applied.
        let resolved = reg.get_resolved("my-theme").expect("resolved");
        assert_eq!(resolved["bgColor"], "#1a1b1e");
        assert!(resolved.as_object().unwrap().len() >= 23);
        assert!(resolved.get("textColor").is_some());
    }

    #[test]
    fn test_scan_rejects_id_filename_mismatch() {
        let dir = tempfile::tempdir().unwrap();
        write_plugin(dir.path(), "actual-name", theme_manifest("different-id"));
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        assert!(reg.list().is_empty());
    }

    #[test]
    fn test_scan_rejects_invalid_semver() {
        let dir = tempfile::tempdir().unwrap();
        let mut m = theme_manifest("bad-version");
        m["version"] = json!("not-a-version");
        write_plugin(dir.path(), "bad-version", m);
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        assert!(reg.list().is_empty());
    }

    #[test]
    fn test_scan_rejects_unknown_kind() {
        let dir = tempfile::tempdir().unwrap();
        let mut m = theme_manifest("mystery");
        m["kind"] = json!("nonsense");
        write_plugin(dir.path(), "mystery", m);
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        // Unknown kind → rejected outright, not recorded.
        assert!(reg.list().is_empty());
    }

    #[test]
    fn test_scan_rejects_future_api_version() {
        let dir = tempfile::tempdir().unwrap();
        let mut m = theme_manifest("too-new");
        m["apiVersion"] = json!(99);
        write_plugin(dir.path(), "too-new", m);
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        assert!(reg.list().is_empty());
    }

    #[test]
    fn test_scan_skips_disabled_plugin() {
        let dir = tempfile::tempdir().unwrap();
        write_plugin(dir.path(), "off-theme", theme_manifest("off-theme"));
        let reg = PluginRegistry::new();
        reg.set_disabled("off-theme", true);
        reg.scan(dir.path().to_str().unwrap());
        let list = reg.list();
        assert_eq!(list.len(), 1);
        assert!(!list[0].enabled);
        assert_eq!(list[0].state, "disabled");
        // Disabled plugin resolves nothing.
        assert!(reg.get_resolved("off-theme").is_none());
    }

    #[test]
    fn test_scan_skips_malformed_json() {
        let dir = tempfile::tempdir().unwrap();
        fs::write(dir.path().join("broken.json"), b"{ not json").unwrap();
        let reg = PluginRegistry::new();
        reg.scan(dir.path().to_str().unwrap());
        assert!(reg.list().is_empty());
    }

    #[test]
    fn test_disabled_state_is_per_plugin_not_kind() {
        let reg = PluginRegistry::new();
        reg.set_disabled("alpha", true);
        assert!(reg.is_disabled("alpha"));
        assert!(!reg.is_disabled("beta"));
        // Disabling does not affect the kind table at all.
        assert!(reg.has_kind("theme"));
    }
}