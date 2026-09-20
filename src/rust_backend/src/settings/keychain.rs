//! # Keychain Module
//!
//! OS-native secure storage access, backed by the cross-platform `keyring`
//! crate (macOS Keychain, Windows Credential Manager, freedesktop Secret
//! Service / in-kernel keyring on Linux).
//!
//! ## What lives where
//!
//! * **Secrets** (API tokens, passwords, …) → OS keychain, never on disk.
//!   When no keychain backend is available (headless Linux, BSD, CI sandboxes)
//!   [`SettingsStorage`](crate::settings::storage::SettingsStorage) falls back
//!   to keeping them inside the AES-256-GCM encrypted settings file.
//! * **Master encryption key** → OS keychain on platforms whose backend is
//!   durable (macOS, Windows, Linux + `linux-secret-service` feature).
//!   On Linux without the Secret Service feature the in-kernel keyring does not
//!   survive a logout, so the key is kept in a `0600` file next to the
//!   encrypted settings file instead of risking undecryptable user data.
//!
//! ## Backend selection
//!
//! `keyring` v3 has no default features, so `Cargo.toml` enables one backend
//! per target OS. Everything here degrades gracefully: an unavailable backend
//! is reported through [`KeychainError::Unavailable`] instead of panicking.

use crate::settings::encrypt::EncryptionKey;
use std::fmt;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

/// Service name used for every Scriptura credential.
pub const DEFAULT_SERVICE_NAME: &str = "com.scriptura.app";

/// Account name of the master encryption key inside the keychain.
const MASTER_KEY_ACCOUNT: &str = "master-encryption-key";

/// Account used to probe whether a keychain backend actually works.
const PROBE_ACCOUNT: &str = "__keychain-probe__";

/// File name of the fallback (and legacy) master key file.
pub const MASTER_KEY_FILENAME: &str = "encryption.key";
/// Environment override for the keychain service name (used by tests so they
/// never read or write the developer's real credentials).
pub const SERVICE_ENV_VAR: &str = "SCRIPTURA_KEYCHAIN_SERVICE";

/// Environment flag that forces keychain-backed master key storage on
/// platforms whose default keychain backend is not durable (Linux/keyutils).
pub const MASTER_KEY_KEYCHAIN_ENV_VAR: &str = "SCRIPTURA_KEYCHAIN_MASTER_KEY";


// ── Errors ──────────────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub enum KeychainError {
    /// No usable credential store on this machine (headless Linux, locked
    /// keychain, missing Secret Service, …).
    Unavailable(String),
    /// The requested credential does not exist.
    NotFound(String),
    /// The credential store exists but refused access (locked / denied).
    AccessDenied(String),
    /// Any other backend failure.
    Operation(String),
}

impl fmt::Display for KeychainError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            KeychainError::Unavailable(m) => write!(f, "keychain unavailable: {m}"),
            KeychainError::NotFound(m) => write!(f, "keychain entry not found: {m}"),
            KeychainError::AccessDenied(m) => write!(f, "keychain access denied: {m}"),
            KeychainError::Operation(m) => write!(f, "keychain error: {m}"),
        }
    }
}

impl std::error::Error for KeychainError {}

impl KeychainError {
    /// True when the caller should fall back to non-keychain storage.
    pub fn is_unavailable(&self) -> bool {
        matches!(self, KeychainError::Unavailable(_) | KeychainError::AccessDenied(_))
    }

    pub fn is_not_found(&self) -> bool {
        matches!(self, KeychainError::NotFound(_))
    }
}

impl From<keyring::Error> for KeychainError {
    fn from(err: keyring::Error) -> Self {
        match err {
            keyring::Error::NoEntry => KeychainError::NotFound(err.to_string()),
            keyring::Error::NoStorageAccess(e) => KeychainError::Unavailable(e.to_string()),
            keyring::Error::PlatformFailure(e) => KeychainError::Operation(e.to_string()),
            other => KeychainError::Operation(other.to_string()),
        }
    }
}

impl From<io::Error> for KeychainError {
    fn from(err: io::Error) -> Self {
        KeychainError::Operation(err.to_string())
    }
}

// ── OS keychain ─────────────────────────────────────────────────────

/// Human readable name of the compiled-in credential store.
pub fn backend_name() -> &'static str {
    #[cfg(all(target_os = "linux", feature = "linux-secret-service"))]
    {
        "Secret Service (libsecret/D-Bus)"
    }
    #[cfg(all(target_os = "linux", not(feature = "linux-secret-service")))]
    {
        "Linux kernel keyring (keyutils)"
    }
    #[cfg(target_os = "macos")]
    {
        "macOS Keychain"
    }
    #[cfg(target_os = "windows")]
    {
        "Windows Credential Manager"
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    {
        "unsupported platform"
    }
}

/// True when the compiled-in backend persists credentials across reboots and
/// logouts. Only durable backends should hold the master encryption key.
pub fn backend_is_persistent() -> bool {
    cfg!(any(
        target_os = "macos",
        target_os = "windows",
        feature = "linux-secret-service"
    ))
}

/// Resolve the service name: `SCRIPTURA_KEYCHAIN_SERVICE` wins, then
/// [`DEFAULT_SERVICE_NAME`].
pub fn service_name() -> String {
    std::env::var(SERVICE_ENV_VAR)
        .ok()
        .filter(|s| !s.trim().is_empty())
        .unwrap_or_else(|| DEFAULT_SERVICE_NAME.to_string())
}

// ── OS keychain wrapper ─────────────────────────────────────────────

/// Thin wrapper around a single keychain service.
pub struct OsKeychain {
    service: String,
    /// Cached probe result so the (write+read+delete) probe only runs once.
    available: OnceLock<bool>,
}

impl OsKeychain {
    pub fn new(service: impl Into<String>) -> Self {
        Self { service: service.into(), available: OnceLock::new() }
    }

    /// Keychain for the current application (honours the service env var).
    pub fn for_app() -> Self {
        Self::new(service_name())
    }

    pub fn service(&self) -> &str {
        &self.service
    }

    fn entry(&self, account: &str) -> Result<keyring::Entry, KeychainError> {
        keyring::Entry::new(&self.service, account).map_err(KeychainError::from)
    }

    /// Store (or overwrite) a secret.
    pub fn set_secret(&self, account: &str, secret: &str) -> Result<(), KeychainError> {
        self.entry(account)?.set_password(secret).map_err(KeychainError::from)
    }

    /// Read a secret. `Ok(None)` means "no entry" (not an error).
    pub fn get_secret(&self, account: &str) -> Result<Option<String>, KeychainError> {
        match self.entry(account)?.get_password() {
            Ok(value) => Ok(Some(value)),
            Err(keyring::Error::NoEntry) => Ok(None),
            Err(err) => Err(KeychainError::from(err)),
        }
    }

    /// Delete a secret. Missing entries are treated as success.
    pub fn delete_secret(&self, account: &str) -> Result<(), KeychainError> {
        match self.entry(account)?.delete_credential() {
            Ok(()) => Ok(()),
            Err(keyring::Error::NoEntry) => Ok(()),
            Err(err) => Err(KeychainError::from(err)),
        }
    }

    pub fn has_secret(&self, account: &str) -> bool {
        matches!(self.get_secret(account), Ok(Some(_)))
    }

    /// Whether a *working* credential store is present. The result is cached and
    /// probed with a real write/read/delete cycle, so "backend present but
    /// locked" is detected as well.
    pub fn is_available(&self) -> bool {
        *self.available.get_or_init(|| self.probe())
    }

    fn probe(&self) -> bool {
        let entry = match self.entry(PROBE_ACCOUNT) {
            Ok(entry) => entry,
            Err(_) => return false,
        };
        let token = format!("probe-{}", std::process::id());
        if entry.set_password(&token).is_err() {
            return false;
        }
        let readable = matches!(entry.get_password(), Ok(value) if value == token);
        let _ = entry.delete_credential();
        readable
    }
}

impl Default for OsKeychain {
    fn default() -> Self {
        Self::for_app()
    }
}




// ── Master key storage policy ───────────────────────────────────────

/// Where the master encryption key currently lives.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyStorage {
    /// Stored as a keychain entry.
    Keychain,
    /// Stored in a permission-restricted file next to the settings file.
    File,
}

impl KeyStorage {
    pub fn as_str(self) -> &'static str {
        match self {
            KeyStorage::Keychain => "keychain",
            KeyStorage::File => "file",
        }
    }
}

/// Which storage the master key should prefer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyPolicy {
    /// Keychain when it is durable, otherwise a `0600` file.
    Auto,
    /// Always use the key file (ignores the keychain completely).
    FileOnly,
    /// Always use the keychain.
    KeychainOnly,
}

/// Master encryption key store: OS keychain first, restricted file as fallback.
pub struct MasterKeyStore {
    keychain: OsKeychain,
    file_path: PathBuf,
    policy: KeyPolicy,
    location: Mutex<KeyStorage>,
}

impl MasterKeyStore {
    pub fn new(service: impl Into<String>, data_dir: &Path, policy: KeyPolicy) -> Self {
        Self {
            keychain: OsKeychain::new(service),
            file_path: data_dir.join(MASTER_KEY_FILENAME),
            policy,
            location: Mutex::new(KeyStorage::File),
        }
    }

    /// Store attached to the default app service and the given data directory.
    pub fn for_app(data_dir: &Path) -> Self {
        Self::new(service_name(), data_dir, KeyPolicy::Auto)
    }

    /// Where the key was last read from / written to.
    pub fn location(&self) -> KeyStorage {
        *self.location.lock().unwrap()
    }

    pub fn file_path(&self) -> &Path {
        &self.file_path
    }

    /// True when the keychain is usable *and* durable enough to hold the key.
    fn prefer_keychain(&self) -> bool {
        match self.policy {
            KeyPolicy::FileOnly => false,
            KeyPolicy::KeychainOnly => true,
            KeyPolicy::Auto => {
                let forced = std::env::var(MASTER_KEY_KEYCHAIN_ENV_VAR)
                    .map(|v| matches!(v.trim().to_ascii_lowercase().as_str(), "1" | "true" | "yes"))
                    .unwrap_or(false);
                self.keychain.is_available() && (backend_is_persistent() || forced)
            }
        }
    }


    /// Load the master key from the keychain, importing (and then removing) a
    /// legacy key file when the keychain takes over. `Ok(None)` means "no key
    /// stored yet", so the caller should generate one.
    pub fn load(&self) -> Result<Option<EncryptionKey>, KeychainError> {
        if self.prefer_keychain() {
            match self.keychain.get_secret(MASTER_KEY_ACCOUNT) {
                Ok(Some(encoded)) => {
                    let key = EncryptionKey::from_base64(encoded.trim()).map_err(|err| {
                        KeychainError::Operation(format!("stored master key is invalid: {err}"))
                    })?;
                    *self.location.lock().unwrap() = KeyStorage::Keychain;
                    // The keychain now owns the key: drop any stale file.
                    let _ = fs::remove_file(&self.file_path);
                    return Ok(Some(key));
                }
                Ok(None) => {}
                Err(err) if err.is_not_found() => {}
                Err(err) => return Err(err),
            }
        }

        if let Some(key) = self.read_key_file()? {
            if self.prefer_keychain() {
                // Migration: file → keychain. Only unlink the file once the
                // keychain write really landed, otherwise keep using the file.
                if self.keychain.set_secret(MASTER_KEY_ACCOUNT, &key.to_base64()).is_ok()
                    && self.keychain.has_secret(MASTER_KEY_ACCOUNT)
                {
                    let _ = fs::remove_file(&self.file_path);
                    *self.location.lock().unwrap() = KeyStorage::Keychain;
                    return Ok(Some(key));
                }
            }
            *self.location.lock().unwrap() = KeyStorage::File;
            return Ok(Some(key));
        }

        Ok(None)
    }

    fn read_key_file(&self) -> Result<Option<EncryptionKey>, KeychainError> {
        if !self.file_path.exists() {
            return Ok(None);
        }
        let raw = fs::read_to_string(&self.file_path)?;
        let raw = raw.trim();
        if raw.is_empty() {
            return Ok(None);
        }
        let key = EncryptionKey::from_base64(raw)
            .map_err(|e| KeychainError::Operation(format!("invalid key file: {e}")))?;
        Ok(Some(key))
    }

    /// Persist the master key (keychain when preferred, `0600` file otherwise).
    pub fn store(&self, key: &EncryptionKey) -> Result<KeyStorage, KeychainError> {
        if self.prefer_keychain() {
            match self.keychain.set_secret(MASTER_KEY_ACCOUNT, &key.to_base64()) {
                Ok(()) => {
                    let _ = fs::remove_file(&self.file_path);
                    *self.location.lock().unwrap() = KeyStorage::Keychain;
                    return Ok(KeyStorage::Keychain);
                }
                // Locked keychain: fall through to the file so the user never
                // ends up with data they cannot decrypt.
                Err(err) if err.is_unavailable() => {}
                Err(err) => return Err(err),
            }
        }
        write_private_file(&self.file_path, key.to_base64().as_bytes())?;
        *self.location.lock().unwrap() = KeyStorage::File;
        Ok(KeyStorage::File)
    }

    /// Forget the master key everywhere (used by the factory-reset path).
    pub fn delete(&self) -> Result<(), KeychainError> {
        let keychain_result = self.keychain.delete_secret(MASTER_KEY_ACCOUNT);
        if self.file_path.exists() {
            fs::remove_file(&self.file_path)?;
        }
        match keychain_result {
            Ok(()) => Ok(()),
            Err(err) if err.is_not_found() || err.is_unavailable() => Ok(()),
            Err(err) => Err(err),
        }
    }
}



// ── Private file helpers ────────────────────────────────────────────

/// Write `contents` to `path`, restricting access to the owner on Unix.
pub fn write_private_file(path: &Path, contents: &[u8]) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
        restrict_to_owner(parent, 0o700)?;
    }
    let mut options = fs::OpenOptions::new();
    options.write(true).create(true).truncate(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::OpenOptionsExt;
        options.mode(0o600);
    }
    let mut file = options.open(path)?;
    file.write_all(contents)?;
    file.flush()?;
    // `mode(0o600)` only applies when the file is created: fix up leftovers.
    restrict_to_owner(path, 0o600)?;
    Ok(())
}

/// Tighten the permissions of an existing path (no-op on Windows).
#[cfg(unix)]
pub fn restrict_to_owner(path: &Path, mode: u32) -> io::Result<()> {
    use std::os::unix::fs::PermissionsExt;
    let current = fs::metadata(path)?.permissions().mode() & 0o777;
    if current != mode {
        fs::set_permissions(path, fs::Permissions::from_mode(mode))?;
    }
    Ok(())
}

#[cfg(not(unix))]
pub fn restrict_to_owner(_path: &Path, _mode: u32) -> io::Result<()> {
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    fn temp_dir() -> tempfile::TempDir {
        tempfile::tempdir().expect("temp dir")
    }

    /// Unique service per test (process id + thread + counter) so parallel
    /// tests never share keychain entries with each other or touch real
    /// credentials. The kernel keyring is process-global, so `process::id`
    /// alone is not enough when tests run on multiple threads.
    fn test_service() -> String {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        format!(
            "com.scriptura.app.test.{}.{:?}.{}",
            std::process::id(),
            std::thread::current().id(),
            COUNTER.fetch_add(1, Ordering::Relaxed)
        )
    }

    #[test]
    fn test_backend_metadata_is_populated() {
        assert!(!backend_name().is_empty());
        assert!(!backend_name().contains("unsupported"));
        assert_eq!(DEFAULT_SERVICE_NAME, "com.scriptura.app");
    }

    #[test]
    fn test_file_policy_roundtrip() {
        let dir = temp_dir();
        let store = MasterKeyStore::new(test_service(), dir.path(), KeyPolicy::FileOnly);
        assert!(store.load().expect("load").is_none());

        let key = EncryptionKey::generate();
        assert_eq!(store.store(&key).expect("store"), KeyStorage::File);
        assert_eq!(store.location(), KeyStorage::File);

        let loaded = store.load().expect("load").expect("key");
        assert_eq!(loaded.to_base64(), key.to_base64());

        store.delete().expect("delete");
        assert!(store.load().expect("load").is_none());
    }

    #[cfg(unix)]
    #[test]
    fn test_key_file_permissions_are_owner_only() {
        use std::os::unix::fs::PermissionsExt;
        let dir = temp_dir();
        let store = MasterKeyStore::new(test_service(), dir.path(), KeyPolicy::FileOnly);
        store.store(&EncryptionKey::generate()).expect("store");

        let mode = fs::metadata(store.file_path()).expect("metadata").permissions().mode() & 0o777;
        assert_eq!(mode, 0o600, "master key file must not be group/world readable");
    }

    #[test]
    fn test_keychain_secret_roundtrip_when_available() {
        let keychain = OsKeychain::new(test_service());
        if !keychain.is_available() {
            // Headless CI / locked keychain: the storage layer falls back to the
            // encrypted settings file, nothing to assert here.
            return;
        }
        assert!(!keychain.has_secret("token"));

        keychain.set_secret("token", "s3cret").expect("set");
        assert_eq!(keychain.get_secret("token").expect("get").as_deref(), Some("s3cret"));
        assert!(keychain.has_secret("token"));

        keychain.delete_secret("token").expect("delete");
        assert_eq!(keychain.get_secret("token").expect("get"), None);
        assert!(!keychain.has_secret("token"));
        // Deleting a missing entry must stay a no-op.
        keychain.delete_secret("token").expect("delete again");
    }

    #[test]
    fn test_keychain_policy_roundtrip_when_available() {
        let keychain = OsKeychain::new(test_service());
        if !keychain.is_available() {
            return;
        }
        let dir = temp_dir();
        let store = MasterKeyStore::new(test_service(), dir.path(), KeyPolicy::KeychainOnly);
        let key = EncryptionKey::generate();
        assert_eq!(store.store(&key).expect("store"), KeyStorage::Keychain);
        assert!(!store.file_path().exists(), "key must not be duplicated on disk");
        assert_eq!(store.location(), KeyStorage::Keychain);

        let loaded = store.load().expect("load").expect("key");
        assert_eq!(loaded.to_base64(), key.to_base64());

        store.delete().expect("delete");
        assert!(store.load().expect("load").is_none());
    }

    #[test]
    fn test_key_file_to_keychain_migration_when_available() {
        let keychain = OsKeychain::new(test_service());
        if !keychain.is_available() {
            return;
        }
        let dir = temp_dir();
        let file_store = MasterKeyStore::new(test_service(), dir.path(), KeyPolicy::FileOnly);
        let key = EncryptionKey::generate();
        file_store.store(&key).expect("store file");

        // A keychain-preferring store must import the file and unlink it.
        let keychain_store = MasterKeyStore::new(test_service(), dir.path(), KeyPolicy::KeychainOnly);
        let loaded = keychain_store.load().expect("load").expect("key");
        assert_eq!(loaded.to_base64(), key.to_base64());
        assert_eq!(keychain_store.location(), KeyStorage::Keychain);
        assert!(!keychain_store.file_path().exists(), "imported key file must be removed");
        keychain_store.delete().expect("cleanup");
    }

    #[test]
    fn test_missing_keychain_account_is_none() {
        let keychain = OsKeychain::new(test_service());
        if !keychain.is_available() {
            return;
        }
        assert!(keychain.get_secret("definitely-not-set").expect("get").is_none());
    }
}

