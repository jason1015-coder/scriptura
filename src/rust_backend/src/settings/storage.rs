//! # Settings Storage Module
//!
//! Encrypted settings storage using AES-256-GCM, with OS-native keychain
//! integration for secrets and for the master key when the platform backend is
//! durable (see [`crate::settings::keychain`]).
//!
//! ## Layout on disk
//!
//! ```text
//! <data dir>/                0700   (Linux: ~/.local/share/scriptura)
//! ├── settings.enc           0600   AES-256-GCM(nonce ‖ ciphertext ‖ tag)
//! └── encryption.key         0600   master key fallback (legacy/CI/headless)
//! ```
//!
//! Non-sensitive settings live in `settings.enc`. Values whose key looks
//! sensitive (`ai/apiKey`, `*/token`, `*/password`, `secret/*`, …) are routed to
//! the OS keychain instead, falling back to the encrypted file only when no
//! keychain backend is usable.

use crate::settings::encrypt::{self, EncryptionKey, KeyStore};
use crate::settings::keychain::{
    self, KeyPolicy, KeyStorage, KeychainError, MasterKeyStore, OsKeychain,
};
use crate::settings::types::{is_sensitive_key, SettingsData};
use directories::ProjectDirs;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use thiserror::Error;

const SETTINGS_FILENAME: &str = "settings.enc";
const SETTINGS_TMP_FILENAME: &str = "settings.enc.tmp";
const SECRET_PREFIX: &str = "secret/";
const MIGRATION_MARKER: &str = "__migrated_from_qsettings";

/// Environment override for the settings directory (used by the test suite and
/// portable installs to keep user data out of the real profile).
pub const DATA_DIR_ENV_VAR: &str = "SCRIPTURA_SETTINGS_DIR";

#[derive(Error, Debug)]
pub enum StorageError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    #[error("Encryption error: {0}")]
    Encryption(String),
    #[error("Decryption error: {0}")]
    Decryption(String),
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),
    #[error("Settings not initialized")]
    NotInitialized,
    #[error("Could not determine the application data directory")]
    NoDataDir,
    #[error("Keychain error: {0}")]
    Keychain(#[from] KeychainError),
}

impl From<encrypt::EncryptionError> for StorageError {
    fn from(err: encrypt::EncryptionError) -> Self {
        match err {
            encrypt::EncryptionError::Encryption(e) => StorageError::Encryption(e),
            encrypt::EncryptionError::Decryption(e) => StorageError::Decryption(e),
            encrypt::EncryptionError::KeyUnavailable(_) => StorageError::NotInitialized,
            encrypt::EncryptionError::InvalidFormat(e) => StorageError::Encryption(e),
        }
    }
}



// ── Helpers ─────────────────────────────────────────────────────────

/// Data directory for the encrypted store:
/// `SCRIPTURA_SETTINGS_DIR` wins, otherwise the platform location.
fn resolve_data_dir(application: &str, organization: &str) -> Result<PathBuf, StorageError> {
    if let Some(dir) = std::env::var_os(DATA_DIR_ENV_VAR).filter(|d| !d.is_empty()) {
        return Ok(PathBuf::from(dir));
    }
    let dirs = ProjectDirs::from("com", organization, application).ok_or(StorageError::NoDataDir)?;
    Ok(dirs.data_dir().to_path_buf())
}

/// Create the data directory and make sure only the owner can read it.
fn prepare_data_dir(dir: &Path) -> Result<(), StorageError> {
    fs::create_dir_all(dir)?;
    keychain::restrict_to_owner(dir, 0o700)?;
    Ok(())
}

// ── Storage ─────────────────────────────────────────────────────────

pub struct SettingsStorage {
    data_dir: PathBuf,
    settings_path: PathBuf,
    master_keys: MasterKeyStore,
    key_store: KeyStore,
    secrets: OsKeychain,
    data: Mutex<SettingsData>,
    initialized: Mutex<bool>,
    /// Non-fatal degradations (keychain unavailable, fallback in use, …).
    warnings: Mutex<Vec<String>>,
    /// Keychain accounts this process has seen (the keychain is not enumerable
    /// portably, so bulk operations rely on this cache).
    tracked_secrets: Mutex<Vec<String>>,
}

impl SettingsStorage {
    pub fn new(application: &str, organization: &str) -> Result<Self, StorageError> {
        let data_dir = resolve_data_dir(application, organization)?;
        Self::in_dir_with_service(&data_dir, keychain::service_name(), KeyPolicy::Auto)
    }

    /// Build a store rooted in an explicit directory with an explicit key
    /// policy and keychain service. Used by the test suite and by embedders
    /// that want a self-contained store (portable installs).
    pub fn in_dir_with_service(
        dir: &Path,
        service: impl Into<String>,
        key_policy: KeyPolicy,
    ) -> Result<Self, StorageError> {
        prepare_data_dir(dir)?;
        let service = service.into();
        let storage = Self {
            data_dir: dir.to_path_buf(),
            settings_path: dir.join(SETTINGS_FILENAME),
            master_keys: MasterKeyStore::new(service.clone(), dir, key_policy),
            secrets: OsKeychain::new(service),
            key_store: KeyStore::new(),
            data: Mutex::new(SettingsData::default()),
            initialized: Mutex::new(false),
            warnings: Mutex::new(Vec::new()),
            tracked_secrets: Mutex::new(Vec::new()),
        };
        storage.initialize()?;
        Ok(storage)
    }

    /// Convenience wrapper around [`Self::in_dir_with_service`] using the
    /// default keychain service and [`KeyPolicy::Auto`].
    pub fn in_dir(dir: &Path) -> Result<Self, StorageError> {
        Self::in_dir_with_service(dir, keychain::service_name(), KeyPolicy::Auto)
    }

    fn warn(&self, message: impl Into<String>) {
        let message = message.into();
        let mut warnings = self.warnings.lock().unwrap();
        if !warnings.iter().any(|existing| existing == &message) {
            log::warn!("settings: {}", message);
            warnings.push(message);
        }
    }

    /// Load (or create) the master key and read the encrypted settings file.
    pub fn initialize(&self) -> Result<(), StorageError> {
        let key = match self.master_keys.load()? {
            Some(key) => key,
            None => {
                let key = EncryptionKey::generate();
                self.master_keys.store(&key)?;
                key
            }
        };
        self.key_store.set_key(key);
        if self.master_keys.location() == KeyStorage::File {
            self.warn(format!(
                "master key stored in {} (keychain backend '{}' is not durable)",
                self.master_keys.file_path().display(),
                keychain::backend_name()
            ));
        }
        if self.settings_path.exists() {
            match self.load_settings() {
                Ok(data) => *self.data.lock().unwrap() = data,
                Err(err) => {
                    // Never destroy user data because of a read failure: keep the
                    // file and continue with defaults.
                    self.warn(format!("could not read {}: {}", self.settings_path.display(), err));
                }
            }
        }
        *self.initialized.lock().unwrap() = true;
        Ok(())
    }

    fn save_to_disk(&self) -> Result<(), StorageError> {
        let data = self.data.lock().unwrap().clone();
        let json = serde_json::to_vec(&data).map_err(StorageError::Json)?;
        let key = self.key_store.get_key()?;
        let encrypted = encrypt::encrypt(&json, &key)?;
        // Write-then-rename keeps a crash from truncating the settings file.
        let tmp = self.data_dir.join(SETTINGS_TMP_FILENAME);
        keychain::write_private_file(&tmp, &encrypted)?;
        fs::rename(&tmp, &self.settings_path)?;
        Ok(())
    }

    fn load_settings(&self) -> Result<SettingsData, StorageError> {
        let encrypted = fs::read(&self.settings_path)?;
        let key = self.key_store.get_key()?;
        let json = encrypt::decrypt(&encrypted, &key)?;
        let data: SettingsData = serde_json::from_slice(&json)?;
        Ok(data)
    }

    // ── Settings CRUD ───────────────────────────────────────────────

    /// Read a setting. Keys that look sensitive are served from the keychain.
    pub fn get(&self, key: &str) -> Result<Option<String>, StorageError> {
        if is_sensitive_key(key) {
            if let Some(secret) = self.get_secret(key)? {
                return Ok(Some(secret));
            }
        }
        Ok(self.data.lock().unwrap().data.get(key).cloned())
    }

    /// Write a setting. Sensitive keys are routed to the OS keychain.
    pub fn set(&self, key: &str, value: &str) -> Result<(), StorageError> {
        if is_sensitive_key(key) {
            return self.set_secret(key, value);
        }
        self.data
            .lock()
            .unwrap()
            .data
            .insert(key.to_string(), value.to_string());
        self.save_to_disk()
    }

    pub fn remove(&self, key: &str) -> Result<(), StorageError> {
        if is_sensitive_key(key) || key.starts_with(SECRET_PREFIX) {
            let _ = self.delete_secret(key);
        }
        self.data.lock().unwrap().data.remove(key);
        self.save_to_disk()
    }

    pub fn contains(&self, key: &str) -> bool {
        if is_sensitive_key(key) {
            if let Ok(Some(_)) = self.get_secret(key) {
                return true;
            }
        }
        self.data.lock().unwrap().data.contains_key(key)
    }

    pub fn get_all(&self) -> Result<HashMap<String, String>, StorageError> {
        let mut all = self.data.lock().unwrap().data.clone();
        // Keychain-resident secrets stay out of the file, but callers that ask
        // for "everything" (bulk migration, JSON dump) must still see them —
        // reported under the caller-facing key, not the internal account name.
        for account in self.known_secret_keys() {
            if let Ok(Some(value)) = self.get_secret(&account) {
                let key = account
                    .strip_prefix(SECRET_PREFIX)
                    .unwrap_or(account.as_str())
                    .to_string();
                all.insert(key, value);
            }
        }
        Ok(all)
    }

    pub fn set_all(&self, settings: HashMap<String, String>) -> Result<(), StorageError> {
        for (key, value) in &settings {
            if is_sensitive_key(key) {
                let _ = self.set_secret(key, value);
            }
        }
        // Plain values keep the map in sync; fallback copies of secrets that the
        // keychain could not accept must survive the replacement.
        let mut plain: HashMap<String, String> = settings
            .into_iter()
            .filter(|(key, _)| !is_sensitive_key(key))
            .collect();
        {
            let current = self.data.lock().unwrap();
            for (key, value) in current.data.iter() {
                if key.starts_with(SECRET_PREFIX) {
                    plain.insert(key.clone(), value.clone());
                }
            }
        }
        self.data.lock().unwrap().data = plain;
        self.save_to_disk()
    }

    pub fn get_all_json(&self) -> Result<String, StorageError> {
        let all = self.get_all()?;
        serde_json::to_string(&all).map_err(StorageError::Json)
    }

    pub fn set_all_json(&self, json: &str) -> Result<(), StorageError> {
        let settings: HashMap<String, String> = serde_json::from_str(json)?;
        self.set_all(settings)
    }



    // ── Secrets / keychain ──────────────────────────────────────────

    /// Whether a working OS keychain backend is available.
    pub fn keychain_available(&self) -> bool {
        self.secrets.is_available()
    }

    /// Name of the compiled-in keychain backend.
    pub fn keychain_backend(&self) -> &'static str {
        keychain::backend_name()
    }

    /// Where the master encryption key is stored.
    pub fn key_storage(&self) -> KeyStorage {
        self.master_keys.location()
    }

    /// Non-fatal degradations recorded while running.
    pub fn warnings(&self) -> Vec<String> {
        self.warnings.lock().unwrap().clone()
    }

    /// Store a secret in the OS keychain, falling back to the encrypted file
    /// only when the keychain is unusable (headless/CI).
    pub fn set_secret(&self, key: &str, value: &str) -> Result<(), StorageError> {
        let account = secret_account(key);
        if self.secrets.is_available() {
            match self.secrets.set_secret(&account, value) {
                Ok(()) => {
                    self.track_secret(&account);
                    // A previous fallback copy must not linger.
                    let removed = self.data.lock().unwrap().data.remove(&account).is_some();
                    if removed {
                        self.save_to_disk()?;
                    }
                    return Ok(());
                }
                Err(err) => self.warn(format!("keychain write failed for '{key}': {err}")),
            }
        } else {
            self.warn(format!(
                "no keychain backend ({}) - '{key}' is stored in the encrypted settings file",
                keychain::backend_name()
            ));
        }
        self.data
            .lock()
            .unwrap()
            .data
            .insert(account, value.to_string());
        self.save_to_disk()
    }

    /// Read a secret from the keychain, promoting fallback copies as needed.
    pub fn get_secret(&self, key: &str) -> Result<Option<String>, StorageError> {
        let account = secret_account(key);
        if self.secrets.is_available() {
            match self.secrets.get_secret(&account) {
                Ok(Some(value)) => {
                    self.track_secret(&account);
                    return Ok(Some(value));
                }
                Ok(None) => {}
                Err(err) => self.warn(format!("keychain read failed for '{key}': {err}")),
            }
        }
        // Fallback copy written while no keychain was available.
        let fallback = self.data.lock().unwrap().data.get(&account).cloned();
        if let Some(value) = fallback {
            if self.secrets.is_available() && self.secrets.set_secret(&account, &value).is_ok() {
                // Promote to the keychain and drop the encrypted copy.
                self.data.lock().unwrap().data.remove(&account);
                self.save_to_disk()?;
            }
            return Ok(Some(value));
        }
        Ok(None)
    }

    pub fn delete_secret(&self, key: &str) -> Result<(), StorageError> {
        let account = secret_account(key);
        if self.secrets.is_available() {
            if let Err(err) = self.secrets.delete_secret(&account) {
                self.warn(format!("keychain delete failed for '{key}': {err}"));
            }
        }
        self.tracked_secrets.lock().unwrap().retain(|k| k != &account);
        let removed = self.data.lock().unwrap().data.remove(&account).is_some();
        if removed {
            self.save_to_disk()?;
        }
        Ok(())
    }

    pub fn has_secret(&self, key: &str) -> bool {
        matches!(self.get_secret(key), Ok(Some(_)))
    }

    /// Remember that a keychain account exists (used by bulk operations).
    pub fn track_secret(&self, account: &str) {
        let mut tracked = self.tracked_secrets.lock().unwrap();
        if !tracked.iter().any(|k| k == account) {
            tracked.push(account.to_string());
        }
    }

    /// Every secret key currently kept in the encrypted file (fallback copies).
    /// The keychain itself cannot be enumerated portably, so keychain-only
    /// secrets are tracked by [`SettingsStorage::track_secret`].
    fn known_secret_keys(&self) -> Vec<String> {
        let mut keys: Vec<String> = self
            .data
            .lock()
            .unwrap()
            .data
            .keys()
            .filter(|k| k.starts_with(SECRET_PREFIX))
            .cloned()
            .collect();
        keys.extend(self.tracked_secrets.lock().unwrap().iter().cloned());
        keys.sort();
        keys.dedup();
        keys
    }

    // ── Reset ───────────────────────────────────────────────────────

    /// Wipe every stored setting (the master key is kept, so the store can be
    /// re-created with the same identity).
    pub fn clear_all(&self) -> Result<(), StorageError> {
        for key in self.known_secret_keys() {
            let _ = self.delete_secret(&key);
        }
        self.data.lock().unwrap().data.clear();
        self.tracked_secrets.lock().unwrap().clear();
        let _ = fs::remove_file(&self.settings_path);
        let _ = fs::remove_file(self.data_dir.join(SETTINGS_TMP_FILENAME));
        Ok(())
    }

    /// Clear all settings *and* destroy the master key (factory reset).
    pub fn factory_reset(&self) -> Result<(), StorageError> {
        self.clear_all()?;
        self.master_keys.delete()?;
        *self.initialized.lock().unwrap() = false;
        self.initialize()?;
        Ok(())
    }

    // ── Introspection ───────────────────────────────────────────────

    pub fn settings_path(&self) -> &PathBuf {
        &self.settings_path
    }

    pub fn data_dir(&self) -> &PathBuf {
        &self.data_dir
    }

    pub fn is_initialized(&self) -> bool {
        *self.initialized.lock().unwrap()
    }

    /// Record that the one-time Qt `QSettings` import has been performed.
    pub fn mark_qsettings_migrated(&self) -> Result<(), StorageError> {
        self.set(MIGRATION_MARKER, "true")
    }

    pub fn is_qsettings_migrated(&self) -> bool {
        self.data.lock().unwrap().data.contains_key(MIGRATION_MARKER)
    }
}

/// Account name used for a secret key inside the keychain.
fn secret_account(key: &str) -> String {
    let bare = key.strip_prefix(SECRET_PREFIX).unwrap_or(key);
    format!("{SECRET_PREFIX}{bare}")
}

pub fn create_default() -> Result<SettingsStorage, StorageError> {
    SettingsStorage::new("Scriptura", "Scriptura")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::settings::keychain::KeyPolicy;
    use std::sync::atomic::{AtomicU64, Ordering};

    /// Unique service per test (see keychain tests) so parallel tests never
    /// share keychain entries through the availability probe or secrets.
    fn test_service() -> String {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        format!(
            "com.scriptura.app.test.{}.{:?}.{}",
            std::process::id(),
            std::thread::current().id(),
            COUNTER.fetch_add(1, Ordering::Relaxed)
        )
    }

    /// Storage rooted in a temporary directory, with an isolated keychain
    /// service so tests never touch real credentials.
    fn test_storage() -> (tempfile::TempDir, SettingsStorage) {
        let dir = tempfile::tempdir().expect("temp dir");
        let storage =
            SettingsStorage::in_dir_with_service(dir.path(), test_service(), KeyPolicy::FileOnly)
                .expect("initialize");
        (dir, storage)
    }

    #[test]
    fn test_settings_crud() {
        let (_dir, storage) = test_storage();
        storage.set("test.key", "test value").expect("set");
        assert!(storage.contains("test.key"));
        assert_eq!(storage.get("test.key").expect("get").as_deref(), Some("test value"));

        storage.remove("test.key").expect("remove");
        assert!(!storage.contains("test.key"));
        assert_eq!(storage.get("test.key").expect("get"), None);
    }

    #[test]
    fn test_settings_persist_across_reload() {
        let (dir, storage) = test_storage();
        storage.set("theme/selected", "42").expect("set");
        storage.set("recentProjects", "[\"/tmp/a\"]").expect("set");
        assert!(storage.settings_path().exists());

        // Re-open the same directory: the encrypted file must decrypt again.
        let reopened =
            SettingsStorage::in_dir_with_service(dir.path(), test_service(), KeyPolicy::FileOnly)
                .expect("re-open");
        assert_eq!(reopened.get("theme/selected").expect("get").as_deref(), Some("42"));
        assert_eq!(
            reopened.get("recentProjects").expect("get").as_deref(),
            Some("[\"/tmp/a\"]")
        );
    }

    #[test]
    fn test_encrypted_file_is_not_plaintext() {
        let (_dir, storage) = test_storage();
        storage.set("marker", "super-secret-value").expect("set");
        let raw = fs::read(storage.settings_path()).expect("read");
        let needle = b"super-secret-value";
        assert!(
            !raw.windows(needle.len()).any(|w| w == needle),
            "settings file must not contain plaintext values"
        );
    }

    #[cfg(unix)]
    #[test]
    fn test_settings_file_permissions_are_owner_only() {
        use std::os::unix::fs::PermissionsExt;
        let (_dir, storage) = test_storage();
        storage.set("theme/selected", "1").expect("set");
        let mode = fs::metadata(storage.settings_path())
            .expect("metadata")
            .permissions()
            .mode()
            & 0o777;
        assert_eq!(mode, 0o600, "settings file must not be group/world readable");
    }

    #[test]
    fn test_sensitive_keys_are_routed_to_secrets() {
        let (_dir, storage) = test_storage();
        storage.set("ai/apiKey", "sk-test-token").expect("set");
        assert!(storage.has_secret("ai/apiKey"));

        // The plaintext must not be duplicated inside the encrypted payload.
        let data = storage.data.lock().unwrap().clone();
        assert!(!data.data.contains_key("ai/apiKey"));
        assert_eq!(
            storage.get("ai/apiKey").expect("get").as_deref(),
            Some("sk-test-token")
        );

        storage.remove("ai/apiKey").expect("remove");
        assert!(!storage.has_secret("ai/apiKey"));
    }

    #[test]
    fn test_secret_roundtrip() {
        let (_dir, storage) = test_storage();
        assert!(!storage.has_secret("plugins/token"));
        storage.set_secret("plugins/token", "abc123").expect("set secret");
        assert!(storage.has_secret("plugins/token"));
        assert_eq!(
            storage.get_secret("plugins/token").expect("get secret").as_deref(),
            Some("abc123")
        );
        storage.delete_secret("plugins/token").expect("delete secret");
        assert!(!storage.has_secret("plugins/token"));
    }

    #[test]
    fn test_bulk_json_roundtrip() {
        let (_dir, storage) = test_storage();
        let mut values = HashMap::new();
        values.insert("theme/selected".to_string(), "3".to_string());
        values.insert("ai/apiKey".to_string(), "token-value".to_string());
        storage.set_all(values).expect("set all");

        let json = storage.get_all_json().expect("json");
        assert!(json.contains("theme/selected"));

        let parsed: HashMap<String, String> = serde_json::from_str(&json).expect("parse");
        assert_eq!(parsed.get("theme/selected").map(String::as_str), Some("3"));
        assert_eq!(parsed.get("ai/apiKey").map(String::as_str), Some("token-value"));
    }

    #[test]
    fn test_clear_all_keeps_the_store_usable() {
        let (_dir, storage) = test_storage();
        storage.set("theme/selected", "1").expect("set");
        storage.clear_all().expect("clear");
        assert_eq!(storage.get("theme/selected").expect("get"), None);
        // Writing after a clear must work (fresh file, same master key).
        storage.set("theme/selected", "2").expect("set again");
        assert_eq!(storage.get("theme/selected").expect("get").as_deref(), Some("2"));
    }

    #[test]
    fn test_factory_reset_rotates_the_key() {
        let (_dir, storage) = test_storage();
        storage.set("theme/selected", "1").expect("set");
        storage.factory_reset().expect("reset");
        assert_eq!(storage.get("theme/selected").expect("get"), None);
        assert!(storage.is_initialized());
        storage.set("theme/selected", "5").expect("set after reset");
        assert_eq!(storage.get("theme/selected").expect("get").as_deref(), Some("5"));
    }

    #[test]
    fn test_data_dir_env_override() {
        let dir = tempfile::tempdir().expect("temp dir");
        let custom = dir.path().join("custom-data-dir");
        std::env::set_var(DATA_DIR_ENV_VAR, &custom);
        let resolved = resolve_data_dir("Scriptura", "Scriptura").expect("resolve");
        std::env::remove_var(DATA_DIR_ENV_VAR);
        assert_eq!(resolved, custom);
    }

    #[test]
    fn test_migration_marker_is_tracked() {
        let (_dir, storage) = test_storage();
        assert!(!storage.is_qsettings_migrated());
        storage.mark_qsettings_migrated().expect("mark");
        assert!(storage.is_qsettings_migrated());
    }
}

