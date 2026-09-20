#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief C++ front-end for the Rust encrypted settings store.
 *
 * Replaces `QSettings` for the whole application:
 *
 *  - **Settings** are written to `<app data>/settings.enc`, an AES-256-GCM
 *    encrypted file owned by the Rust backend, mode `0600` on Unix.
 *  - **Secrets** (`secret/*`, `*/token`, `*/password`, `*/apiKey`, …) are not
 *    written to disk at all: they go to the OS keychain (macOS Keychain,
 *    Windows Credential Manager, Linux Secret Service / kernel keyring) and
 *    only fall back to the encrypted file when no backend is available.
 *  - **Legacy data**: the Rust side imports the old `QSettings` file once at
 *    startup; existing values always win.
 *
 * Every write is persisted immediately, so `sync()` exists only for
 * `QSettings` source compatibility.
 */
class SettingsStore : public QObject
{
    Q_OBJECT
public:
    static SettingsStore &instance();

    // ── Lifecycle ────────────────────────────────────────────────────

    /// Initialize with the application/organization names from
    /// `QCoreApplication` (falling back to "Scriptura"). Safe to call twice.
    bool initialize();
    /// Initialize with explicit names (used by the test suite).
    bool initialize(const QString &applicationName, const QString &organizationName);
    bool isInitialized() const { return m_initialized; }

    /// No-op: the Rust store persists on every write (`QSettings` parity).
    void sync() {}

    // ── Settings ─────────────────────────────────────────────────────

    QString get(const QString &key, const QString &defaultValue = QString()) const;
    void set(const QString &key, const QString &value);
    void set(const QString &key, const QVariant &value);
    void remove(const QString &key);
    bool contains(const QString &key) const;

    /// Typed getters with defaults (`QSettings`-compatible).
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    /// Remove every stored setting (master key is preserved).
    void clear();
    /// Remove every setting *and* destroy the master key (factory reset).
    void factoryReset();

    // ── Secrets (OS keychain) ────────────────────────────────────────

    QString secret(const QString &key, const QString &defaultValue = QString()) const;
    void setSecret(const QString &key, const QString &value);
    void removeSecret(const QString &key);
    bool hasSecret(const QString &key) const;

    // ── Bulk operations (migration, debugging) ───────────────────────

    /// Every setting as a JSON object (secrets reported under their own key).
    QString getAllAsJson() const;
    bool setAllFromJson(const QString &json);

    // ── Diagnostics ──────────────────────────────────────────────────

    /// Path of the encrypted settings file.
    QString storagePath() const;
    /// Directory holding the store (`settings.enc`, `encryption.key`).
    QString storageDirectory() const;
    bool isKeychainAvailable() const;
    QString keychainBackend() const;
    bool isKeychainPersistent() const;
    /// "keychain" when the master key lives in the OS keychain, else "file".
    QString keyStorageLocation() const;
    /// Non-fatal degradations reported by the backend (empty when all good).
    QStringList warnings() const;
    /// Result of the one-time Qt -> Rust import ("nothing-to-import", …).
    QString migrationStatus() const;

    // ── Compatibility ────────────────────────────────────────────────

    /**
     * @brief `QSettings`-compatible view of this store.
     *
     * Backed by `QSettings::registerFormat` callbacks that route reads and
     * writes through the Rust store, so plugin/legacy code keeps compiling
     * without touching plaintext configuration files. Deleting keys through
     * this view is supported via a load/write comparison.
     */
    QSettings *createLegacySettings(QObject *parent = nullptr);

    // Encoding helpers (also used by the `QSettings` format callbacks)
    static QString encodeVariantForStorage(const QVariant &value, bool tagged);
    static QVariant decodeVariantFromStorage(const QString &raw, const QVariant &defaultValue);

private:
    explicit SettingsStore(QObject *parent = nullptr);
    ~SettingsStore() override;
    SettingsStore(const SettingsStore &) = delete;
    SettingsStore &operator=(const SettingsStore &) = delete;

    bool m_initialized = false;
};

#endif // SETTINGS_STORE_H

