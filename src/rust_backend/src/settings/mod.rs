//! # Settings Storage Module
//!
//! Secure settings storage with OS-native keychain integration and encryption.
//!
//! ## Architecture
//!
//! - **Non-sensitive settings**: stored in an encrypted JSON file at the
//!   platform-appropriate location (`settings.enc`, AES-256-GCM, `0600`).
//! - **Sensitive settings**: `secret/*`, `*/token`, `*/password`, `*/apiKey`, …
//!   are routed to the OS keychain (macOS Keychain, Windows Credential Manager,
//!   freedesktop Secret Service / kernel keyring on Linux) and only fall back to
//!   the encrypted file when no backend is usable.
//! - **Master key**: kept in the OS keychain when the platform backend is
//!   durable, otherwise in a `0600` permission-restricted file.
//! - **Legacy Qt data**: a one-time import from the old `QSettings` file
//!   (`migrations`), existing values always win.
//!
//! ## Usage
//!
//! ```rust,no_run
//! use scriptura_backend::settings::SettingsStorage;
//!
//! let storage = SettingsStorage::new("Scriptura", "Scriptura")?;
//! storage.set("editor/font", "monospace")?;
//! let font = storage.get("editor/font")?;
//!
//! // Secrets never touch the settings file:
//! storage.set_secret("ai/apiKey", "sk-…")?;
//! # Ok::<(), scriptura_backend::settings::storage::StorageError>(())
//! ```
//!
//! ## Environment overrides
//!
//! | Variable | Effect |
//! |----------|--------|
//! | `SCRIPTURA_SETTINGS_DIR` | Data directory for `settings.enc` / `encryption.key` |
//! | `SCRIPTURA_KEYCHAIN_SERVICE` | Keychain service name (tests, portable installs) |
//! | `SCRIPTURA_KEYCHAIN_MASTER_KEY` | Force keychain master-key storage on Linux |

pub mod encrypt;
pub mod ffi;
pub mod keychain;
pub mod migrations;
pub mod storage;
pub mod types;

// Re-exports exist for readability of internal paths; the crate is consumed by
// the Qt front-end through C FFI, so "unused" here is expected.
#[allow(unused_imports)]
pub use keychain::{
    backend_is_persistent, backend_name, KeyPolicy, KeyStorage, KeychainError, MasterKeyStore,
    OsKeychain,
};
#[allow(unused_imports)]
pub use storage::{SettingsStorage, StorageError};
#[allow(unused_imports)]
pub use types::{is_sensitive_key, SettingCategory, SettingKey, SettingsData};
