//! # FFI Module for Settings
//!
//! C-compatible interface used by the Qt/C++ `SettingsStore` wrapper
//! (`src/internals/settings_store.cpp`). Every `*mut c_char` result is owned by
//! Rust and must be released with [`settings_free_string`].

use crate::settings::keychain;
use crate::settings::migrations::{self, MigrationStatus};
use crate::settings::storage::{self, SettingsStorage};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::Mutex;

static SETTINGS: Mutex<Option<SettingsStorage>> = Mutex::new(None);
static MIGRATION_STATUS: Mutex<Option<String>> = Mutex::new(None);

// ── Internal helpers ────────────────────────────────────────────────

fn cstr(ptr: *const c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned()
}

/// Move a Rust string across the FFI boundary (caller frees it).
fn to_c_string(value: impl Into<String>) -> *mut c_char {
    CString::new(value.into()).unwrap_or_default().into_raw()
}

fn with_storage<T>(f: impl FnOnce(&SettingsStorage) -> T, fallback: T) -> T {
    match SETTINGS.lock().unwrap().as_ref() {
        Some(storage) => f(storage),
        None => {
            crate::set_last_error("settings: backend not initialized");
            fallback
        }
    }
}

/// Install a storage instance and run the one-time Qt import.
fn install(storage: SettingsStorage) -> i32 {
    let status = migrations::migrate_from_qsettings(&storage);
    *MIGRATION_STATUS.lock().unwrap() = Some(status.as_str());
    if let MigrationStatus::Failed(reason) = status {
        crate::set_last_error(format!("settings migration: {reason}"));
    }
    for warning in storage.warnings() {
        log::warn!("settings: {warning}");
    }
    *SETTINGS.lock().unwrap() = Some(storage);
    1
}

// ── Lifecycle ───────────────────────────────────────────────────────

/// Initialize the store with the default application identity.
#[no_mangle]
pub extern "C" fn settings_initialize() -> i32 {
    match storage::create_default() {
        Ok(storage) => install(storage),
        Err(err) => {
            crate::set_last_error(format!("settings init failed: {err}"));
            0
        }
    }
}

/// Initialize the store with explicit application / organization names.
#[no_mangle]
pub extern "C" fn settings_initialize_with_names(
    app: *const c_char,
    org: *const c_char,
) -> i32 {
    let app_name = cstr(app);
    let org_name = cstr(org);
    if app_name.is_empty() {
        crate::set_last_error("settings init failed: empty application name");
        return 0;
    }
    match SettingsStorage::new(&app_name, &org_name) {
        Ok(storage) => install(storage),
        Err(err) => {
            crate::set_last_error(format!("settings init failed: {err}"));
            0
        }
    }
}

/// Result of the one-time Qt `QSettings` import ("nothing-to-import", …).
#[no_mangle]
pub extern "C" fn settings_migration_status() -> *mut c_char {
    let status = MIGRATION_STATUS.lock().unwrap().clone();
    match status {
        Some(status) => to_c_string(status),
        None => std::ptr::null_mut(),
    }
}

// ── Settings CRUD ───────────────────────────────────────────────────

#[no_mangle]
pub extern "C" fn settings_get(key: *const c_char) -> *mut c_char {
    let key = cstr(key);
    with_storage(
        |storage| match storage.get(&key) {
            Ok(Some(value)) => to_c_string(value),
            Ok(None) => std::ptr::null_mut(),
            Err(err) => {
                crate::set_last_error(format!("settings get '{key}' failed: {err}"));
                std::ptr::null_mut()
            }
        },
        std::ptr::null_mut(),
    )
}

#[no_mangle]
pub extern "C" fn settings_set(key: *const c_char, value: *const c_char) -> i32 {
    let key = cstr(key);
    let value = cstr(value);
    with_storage(
        |storage| match storage.set(&key, &value) {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings set '{key}' failed: {err}"));
                0
            }
        },
        0,
    )
}

#[no_mangle]
pub extern "C" fn settings_remove(key: *const c_char) -> i32 {
    let key = cstr(key);
    with_storage(
        |storage| match storage.remove(&key) {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings remove '{key}' failed: {err}"));
                0
            }
        },
        0,
    )
}

#[no_mangle]
pub extern "C" fn settings_contains(key: *const c_char) -> i32 {
    let key = cstr(key);
    with_storage(|storage| i32::from(storage.contains(&key)), 0)
}

#[no_mangle]
pub extern "C" fn settings_get_all_json() -> *mut c_char {
    with_storage(
        |storage| match storage.get_all_json() {
            Ok(json) => to_c_string(json),
            Err(err) => {
                crate::set_last_error(format!("settings dump failed: {err}"));
                std::ptr::null_mut()
            }
        },
        std::ptr::null_mut(),
    )
}

#[no_mangle]
pub extern "C" fn settings_set_all_json(json: *const c_char) -> i32 {
    let json = cstr(json);
    with_storage(
        |storage| match storage.set_all_json(&json) {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings import failed: {err}"));
                0
            }
        },
        0,
    )
}




// ── Secrets / keychain ──────────────────────────────────────────────

/// Store a secret in the OS keychain (falls back to the encrypted file only
/// when no keychain backend is available).
#[no_mangle]
pub extern "C" fn settings_set_secret(key: *const c_char, value: *const c_char) -> i32 {
    let key = cstr(key);
    let value = cstr(value);
    with_storage(
        |storage| match storage.set_secret(&key, &value) {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings secret '{key}' write failed: {err}"));
                0
            }
        },
        0,
    )
}

/// Read a secret. Returns null when the secret does not exist.
#[no_mangle]
pub extern "C" fn settings_get_secret(key: *const c_char) -> *mut c_char {
    let key = cstr(key);
    with_storage(
        |storage| match storage.get_secret(&key) {
            Ok(Some(value)) => to_c_string(value),
            Ok(None) => std::ptr::null_mut(),
            Err(err) => {
                crate::set_last_error(format!("settings secret '{key}' read failed: {err}"));
                std::ptr::null_mut()
            }
        },
        std::ptr::null_mut(),
    )
}

#[no_mangle]
pub extern "C" fn settings_delete_secret(key: *const c_char) -> i32 {
    let key = cstr(key);
    with_storage(
        |storage| match storage.delete_secret(&key) {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings secret '{key}' delete failed: {err}"));
                0
            }
        },
        0,
    )
}

#[no_mangle]
pub extern "C" fn settings_has_secret(key: *const c_char) -> i32 {
    let key = cstr(key);
    with_storage(|storage| i32::from(storage.has_secret(&key)), 0)
}

/// 1 when a working OS keychain backend is available.
#[no_mangle]
pub extern "C" fn settings_is_keychain_available() -> i32 {
    with_storage(|storage| i32::from(storage.keychain_available()), 0)
}

/// Human readable name of the compiled-in keychain backend.
#[no_mangle]
pub extern "C" fn settings_keychain_backend() -> *mut c_char {
    to_c_string(keychain::backend_name())
}

/// Whether the compiled-in keychain backend survives a logout/reboot.
#[no_mangle]
pub extern "C" fn settings_keychain_is_persistent() -> i32 {
    i32::from(keychain::backend_is_persistent())
}

/// Where the master encryption key is stored ("keychain" or "file").
#[no_mangle]
pub extern "C" fn settings_key_storage() -> *mut c_char {
    let location = with_storage(|storage| storage.key_storage().as_str(), "file");
    to_c_string(location)
}

/// Non-fatal degradation messages recorded since startup (JSON array).
#[no_mangle]
pub extern "C" fn settings_warnings_json() -> *mut c_char {
    with_storage(
        |storage| match serde_json::to_string(&storage.warnings()) {
            Ok(json) => to_c_string(json),
            Err(_) => to_c_string("[]"),
        },
        to_c_string("[]"),
    )
}

// ── Reset ───────────────────────────────────────────────────────────

/// Wipe every stored setting (master key preserved).
#[no_mangle]
pub extern "C" fn settings_clear_all() -> i32 {
    with_storage(
        |storage| match storage.clear_all() {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings clear failed: {err}"));
                0
            }
        },
        0,
    )
}

/// Wipe every setting *and* destroy the master key.
#[no_mangle]
pub extern "C" fn settings_factory_reset() -> i32 {
    with_storage(
        |storage| match storage.factory_reset() {
            Ok(()) => 1,
            Err(err) => {
                crate::set_last_error(format!("settings factory reset failed: {err}"));
                0
            }
        },
        0,
    )
}

// ── Introspection ───────────────────────────────────────────────────

/// Path of the encrypted settings file ("" when uninitialized).
#[no_mangle]
pub extern "C" fn settings_get_path() -> *mut c_char {
    with_storage(
        |storage| to_c_string(storage.settings_path().to_string_lossy().to_string()),
        std::ptr::null_mut(),
    )
}

/// Directory holding the store ("", "encryption.key", …).
#[no_mangle]
pub extern "C" fn settings_get_dir() -> *mut c_char {
    with_storage(
        |storage| to_c_string(storage.data_dir().to_string_lossy().to_string()),
        std::ptr::null_mut(),
    )
}

/// Free any string returned by this module.
#[no_mangle]
pub extern "C" fn settings_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}

