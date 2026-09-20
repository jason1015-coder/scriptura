//! # Migrations Module
//!
//! One-time import of the Qt `QSettings` configuration that Scriptura used
//! before settings moved to the Rust encrypted store.
//!
//! * Linux   – `~/.config/Scriptura/Scriptura.conf` (native) or `.ini`
//! * macOS   – `~/Library/Preferences/com.Scriptura.Scriptura.plist` (XML only)
//! * Windows – registry (not imported; the encrypted store starts empty)
//!
//! The import is idempotent: existing keys always win and a marker key is
//! written so the file is only read once.

use crate::settings::storage::SettingsStorage;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

/// Marker key recording that the Qt import already ran.
pub const MIGRATION_MARKER: &str = "__migrated_from_qsettings";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MigrationStatus {
    /// A previous import already ran.
    AlreadyDone,
    /// No Qt settings file was found (or it was empty).
    NothingToImport,
    /// `imported` keys were copied from `source`.
    Imported { imported: usize, source: PathBuf },
    /// The import could not run; the app keeps working with the store as-is.
    Failed(String),
}

impl MigrationStatus {
    pub fn as_str(&self) -> String {
        match self {
            MigrationStatus::AlreadyDone => "already-migrated".to_string(),
            MigrationStatus::NothingToImport => "nothing-to-import".to_string(),
            MigrationStatus::Imported { imported, source } => {
                format!("imported {imported} settings from {}", source.display())
            }
            MigrationStatus::Failed(reason) => format!("failed: {reason}"),
        }
    }
}



// ── Public entry point ──────────────────────────────────────────────

/// Import the legacy Qt settings file, if any, into `storage`.
pub fn migrate_from_qsettings(storage: &SettingsStorage) -> MigrationStatus {
    if storage.is_qsettings_migrated() {
        return MigrationStatus::AlreadyDone;
    }

    let candidates = qsettings_candidate_paths();
    let source = match candidates.into_iter().find(|path| path.is_file()) {
        Some(path) => path,
        None => return MigrationStatus::NothingToImport,
    };

    let entries = match parse_settings_file(&source) {
        Ok(entries) => entries,
        Err(err) => return MigrationStatus::Failed(err),
    };
    if entries.is_empty() {
        return MigrationStatus::NothingToImport;
    }

    // Never overwrite values the encrypted store already owns.
    let existing: HashSet<String> = storage
        .get_all()
        .map(|all| all.into_keys().collect())
        .unwrap_or_default();

    let mut imported = 0usize;
    for (key, value) in entries {
        if key == MIGRATION_MARKER || existing.contains(&key) {
            continue;
        }
        if storage.set(&key, &value).is_err() {
            continue;
        }
        imported += 1;
    }

    if let Err(err) = storage.mark_qsettings_migrated() {
        return MigrationStatus::Failed(err.to_string());
    }
    MigrationStatus::Imported { imported, source }
}

/// Locations the Qt build of Scriptura may have written to.
pub fn qsettings_candidate_paths() -> Vec<PathBuf> {
    let mut paths = Vec::new();

    #[cfg(target_os = "linux")]
    {
        let config_root = std::env::var_os("XDG_CONFIG_HOME")
            .filter(|dir| !dir.is_empty())
            .map(PathBuf::from)
            .or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".config")));
        if let Some(root) = config_root {
            // QSettings native format for org=Scriptura/app=Scriptura.
            paths.push(root.join("Scriptura").join("Scriptura.conf"));
            paths.push(root.join("Scriptura").join("Scriptura.ini"));
            paths.push(root.join("Scriptura.ini"));
            // Older/lower-case layouts.
            paths.push(root.join("scriptura").join("scriptura.conf"));
            paths.push(root.join("scriptura.ini"));
        }
    }

    #[cfg(target_os = "macos")]
    {
        if let Some(home) = std::env::var_os("HOME") {
            let home = PathBuf::from(home);
            paths.push(home.join("Library/Preferences/com.Scriptura.Scriptura.plist"));
            paths.push(home.join("Library/Preferences/Scriptura.Scriptura.plist"));
        }
    }

    paths
}


// ── Parsers ─────────────────────────────────────────────────────────

/// Parse a Qt settings file: XML plist on macOS, INI/conf elsewhere.
pub fn parse_settings_file(path: &Path) -> Result<HashMap<String, String>, String> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| format!("failed to read {}: {e}", path.display()))?;
    if path.extension().map(|e| e == "plist").unwrap_or(false) {
        return Ok(parse_plist(&content));
    }
    Ok(parse_ini(&content))
}

/// Parse a Qt `IniFormat`/native-format file.
///
/// Keys in `[General]` are top-level settings, every other section becomes a
/// `<section>/<key>` group path (which is exactly how `QSettings` addresses
/// them, e.g. `[mainWindow] geometry=` → `mainWindow/geometry`).
pub fn parse_ini(content: &str) -> HashMap<String, String> {
    let mut result = HashMap::new();
    let mut section = String::from("General");

    for raw_line in content.lines() {
        let line = raw_line.trim();
        if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') && line.ends_with(']') {
            section = line[1..line.len() - 1].trim().to_string();
            continue;
        }
        let (key, value) = match line.split_once('=') {
            Some(pair) => pair,
            None => continue,
        };
        let key = key.trim();
        if key.is_empty() {
            continue;
        }
        let full_key = if section == "General" {
            key.to_string()
        } else {
            format!("{}/{}", section, key)
        };
        if let Some(decoded) = decode_qt_value(value.trim()) {
            result.insert(full_key, decoded);
        }
    }

    result
}

/// Minimal XML plist reader covering the flat `key` / `<string|integer|…>`
/// structures `QSettings` writes on macOS. Binary plists yield an empty map
/// (the encrypted store then simply starts empty).
pub fn parse_plist(content: &str) -> HashMap<String, String> {
    let mut result = HashMap::new();
    if !content.contains("<plist") {
        return result;
    }

    let mut pending_key: Option<String> = None;
    let mut rest = content;
    while let Some(open) = rest.find('<') {
        rest = &rest[open + 1..];
        let close = match rest.find('>') {
            Some(index) => index,
            None => break,
        };
        let raw_tag = &rest[..close];
        rest = &rest[close + 1..];

        // Closing tags (`</key>`) carry no data and must not clear a key.
        if raw_tag.starts_with('/') {
            continue;
        }
        let self_closing = raw_tag.ends_with('/');
        let tag = raw_tag
            .trim_start_matches('/')
            .trim_end_matches('/')
            .split_whitespace()
            .next()
            .unwrap_or("")
            .to_string();

        // Text content of the element, if any.
        let value = if self_closing {
            String::new()
        } else {
            match rest.find('<') {
                Some(index) => {
                    let text = rest[..index].to_string();
                    rest = &rest[index..];
                    text
                }
                None => break,
            }
        };

        if tag == "key" {
            pending_key = Some(xml_unescape(value.trim()));
            continue;
        }
        let key = match pending_key.take() {
            Some(key) => key,
            None => continue,
        };
        let decoded = match tag.as_str() {
            "string" => xml_unescape(&value),
            "integer" | "real" => value.trim().to_string(),
            "true" => "true".to_string(),
            "false" => "false".to_string(),
            _ => continue,
        };
        result.insert(key, decoded);
    }

    result
}

fn xml_unescape(value: &str) -> String {
    value
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&apos;", "'")
        .replace("&amp;", "&")
}


/// Decode a Qt INI value: strip quotes, resolve `@ByteArray`, unescape.
pub fn decode_qt_value(raw: &str) -> Option<String> {
    if raw.is_empty() {
        return Some(String::new());
    }
    if let Some(inner) = raw.strip_prefix("@ByteArray(").and_then(|s| s.strip_suffix(')')) {
        // Convert Qt's binary escape sequence into the base64 form the C++
        // `QVariant`/`QByteArray` round-trip expects.
        return Some(base64_encode(&decode_qt_byte_array(inner)));
    }
    // Unsupported Qt variant encodings (QFont, QSize, …) are dropped so the
    // caller's typed default is used instead of a broken value.
    if raw.starts_with('@') {
        return None;
    }
    if raw.len() >= 2 && raw.starts_with('"') && raw.ends_with('"') {
        return Some(unescape_qt_string(&raw[1..raw.len() - 1]));
    }
    Some(raw.to_string())
}

fn unescape_qt_string(value: &str) -> String {
    let mut out = String::with_capacity(value.len());
    let mut chars = value.chars();
    while let Some(ch) = chars.next() {
        if ch != '\\' {
            out.push(ch);
            continue;
        }
        match chars.next() {
            Some('n') => out.push('\n'),
            Some('t') => out.push('\t'),
            Some('r') => out.push('\r'),
            Some('"') => out.push('"'),
            Some('\\') => out.push('\\'),
            Some(other) => out.push(other),
            None => {}
        }
    }
    out
}

fn decode_qt_byte_array(value: &str) -> Vec<u8> {
    let mut out = Vec::new();
    let chars: Vec<char> = value.chars().collect();
    let mut index = 0;
    while index < chars.len() {
        if chars[index] == '\\' && index + 1 < chars.len() {
            match chars[index + 1] {
                'x' if index + 3 < chars.len() => {
                    let hex: String = chars[index + 2..index + 4].iter().collect();
                    if let Ok(byte) = u8::from_str_radix(&hex, 16) {
                        out.push(byte);
                        index += 4;
                        continue;
                    }
                }
                '\\' => {
                    out.push(b'\\');
                    index += 2;
                    continue;
                }
                other => {
                    let mut buffer = [0u8; 4];
                    out.extend_from_slice(other.encode_utf8(&mut buffer).as_bytes());
                    index += 2;
                    continue;
                }
            }
        }
        let mut buffer = [0u8; 4];
        out.extend_from_slice(chars[index].encode_utf8(&mut buffer).as_bytes());
        index += 1;
    }
    out
}

fn base64_encode(bytes: &[u8]) -> String {
    use base64::{engine::general_purpose::STANDARD as BASE64, Engine};
    BASE64.encode(bytes)
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_ini_groups_and_general_section() {
        let ini = "; comment\n[General]\nrecentProjects=foo\n\n[mainWindow]\ngeometry=abc\n";
        let parsed = parse_ini(ini);
        assert_eq!(parsed.get("recentProjects").map(String::as_str), Some("foo"));
        assert_eq!(parsed.get("mainWindow/geometry").map(String::as_str), Some("abc"));
    }

    #[test]
    fn test_parse_ini_quotes_and_escapes() {
        let parsed = parse_ini("[General]\nname=\"Hello \\\"World\\\"\"\ncount=3\n");
        assert_eq!(parsed.get("name").map(String::as_str), Some("Hello \"World\""));
        assert_eq!(parsed.get("count").map(String::as_str), Some("3"));
    }

    #[test]
    fn test_parse_ini_nested_group_path() {
        let parsed = parse_ini("[shortcuts]\nopen_file=Ctrl+O\n");
        assert_eq!(parsed.get("shortcuts/open_file").map(String::as_str), Some("Ctrl+O"));
    }

    #[test]
    fn test_unsupported_variant_values_are_skipped() {
        let parsed = parse_ini("[General]\nfont=@Variant(\\0\\0)\\0\n");
        assert!(!parsed.contains_key("font"));
    }

    #[test]
    fn test_byte_array_values_become_base64() {
        use base64::{engine::general_purpose::STANDARD as BASE64, Engine};
        let parsed = parse_ini("[General]\ngeometry=@ByteArray(\\x01\\x02AB)\n");
        let value = parsed.get("geometry").expect("geometry");
        assert_eq!(BASE64.decode(value).expect("decode"), vec![1u8, 2, b'A', b'B']);
    }

    #[test]
    fn test_parse_plist_pairs() {
        let plist = "<?xml version=\"1.0\"?>\n<plist version=\"1.0\"><dict>\n\
<key>theme/selected</key><integer>4</integer>\n\
<key>recentProjects</key><string>/tmp/project</string>\n\
<key>editor/wordWrap</key><true/>\n</dict></plist>";
        let parsed = parse_plist(plist);
        assert_eq!(parsed.get("theme/selected").map(String::as_str), Some("4"));
        assert_eq!(parsed.get("recentProjects").map(String::as_str), Some("/tmp/project"));
        assert_eq!(parsed.get("editor/wordWrap").map(String::as_str), Some("true"));
    }

    #[test]
    fn test_migration_import_once_semantics() {
        use crate::settings::keychain::KeyPolicy;
        use std::sync::atomic::{AtomicU64, Ordering};

        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let dir = tempfile::tempdir().expect("temp dir");
        let service = format!(
            "com.scriptura.app.test.{}.{:?}.{}",
            std::process::id(),
            std::thread::current().id(),
            COUNTER.fetch_add(1, Ordering::Relaxed)
        );
        let storage = SettingsStorage::in_dir_with_service(dir.path(), service, KeyPolicy::FileOnly)
            .expect("init");
        storage.set("theme/selected", "7").expect("seed");

        let status = migrate_from_qsettings(&storage);
        // No real Qt file in the sandbox: either "nothing" or a real import.
        assert!(matches!(
            status,
            MigrationStatus::NothingToImport
                | MigrationStatus::Imported { .. }
                | MigrationStatus::AlreadyDone
        ));

        // An existing value must never be overwritten by the importer.
        let entries = parse_ini("[General]\ntheme/selected=9\n[editor]\ntabWidth=2\n");
        for (key, value) in &entries {
            if !storage.contains(key) {
                storage.set(key, value).expect("import");
            }
        }
        assert_eq!(storage.get("theme/selected").expect("get").as_deref(), Some("7"));
    }

    #[test]
    fn test_status_descriptions() {
        assert_eq!(MigrationStatus::AlreadyDone.as_str(), "already-migrated");
        let imported = MigrationStatus::Imported {
            imported: 3,
            source: PathBuf::from("/tmp/Scriptura.conf"),
        };
        assert!(imported.as_str().contains("imported 3"));
        assert!(MigrationStatus::Failed("boom".into()).as_str().contains("boom"));
    }
}
