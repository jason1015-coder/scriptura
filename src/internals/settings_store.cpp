#include "settings_store.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFont>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMetaType>
#include <QSet>

// ── Rust FFI ──────────────────────────────────────────────────────────
// Implemented in src/rust_backend/src/settings/ffi.rs (staticlib).
extern "C" {
int settings_initialize();
int settings_initialize_with_names(const char *app, const char *org);
char *settings_migration_status();
char *settings_get(const char *key);
int settings_set(const char *key, const char *value);
int settings_remove(const char *key);
int settings_contains(const char *key);
char *settings_get_all_json();
int settings_set_all_json(const char *json);
int settings_set_secret(const char *key, const char *value);
char *settings_get_secret(const char *key);
int settings_delete_secret(const char *key);
int settings_has_secret(const char *key);
int settings_is_keychain_available();
char *settings_keychain_backend();
int settings_keychain_is_persistent();
char *settings_key_storage();
char *settings_warnings_json();
int settings_clear_all();
int settings_factory_reset();
char *settings_get_path();
char *settings_get_dir();
void settings_free_string(char *s);
}

namespace {

QString takeRustString(char *s)
{
    if (!s)
        return QString();
    QString out = QString::fromUtf8(s);
    settings_free_string(s);
    return out;
}

QByteArray takeRustBytes(char *s)
{
    if (!s)
        return QByteArray();
    QByteArray out(s);
    settings_free_string(s);
    return out;
}

void rustSet(const QString &key, const QString &value)
{
    QByteArray k = key.toUtf8();
    QByteArray v = value.toUtf8();
    settings_set(k.constData(), v.constData());
}

QString rustGet(const QString &key)
{
    QByteArray k = key.toUtf8();
    return takeRustString(settings_get(k.constData()));
}

// ── QSettings::registerFormat bridge ──────────────────────────────────
// The custom format routes legacy QSettings access through the Rust store
// so plugin code keeps compiling without touching plaintext files.

bool rustStoreRead(QIODevice &device, QSettings::SettingsMap &map)
{
    Q_UNUSED(device);
    char *json = settings_get_all_json();
    if (!json)
        return true; // uninitialized or empty: present an empty map
    QString all = takeRustString(json);
    if (all.isEmpty())
        return true;
    QJsonDocument doc = QJsonDocument::fromJson(all.toUtf8());
    if (!doc.isObject())
        return false;
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map.insert(it.key(), SettingsStore::decodeVariantFromStorage(it.value().toString(), QVariant()));
    return true;
}

bool rustStoreWrite(QIODevice &device, const QSettings::SettingsMap &map)
{
    Q_UNUSED(device);
    // Load current snapshot so deletions (keys absent from map) are honored.
    QSet<QString> wanted;
    QJsonObject obj;
    for (auto it = map.begin(); it != map.end(); ++it) {
        wanted.insert(it.key());
        obj.insert(it.key(), SettingsStore::encodeVariantForStorage(it.value(), true));
    }
    char *currentJson = settings_get_all_json();
    QStringList toRemove;
    if (currentJson) {
        QString all = takeRustString(currentJson);
        QJsonDocument doc = QJsonDocument::fromJson(all.toUtf8());
        if (doc.isObject()) {
            QJsonObject cur = doc.object();
            for (auto it = cur.begin(); it != cur.end(); ++it) {
                if (!wanted.contains(it.key()))
                    toRemove.append(it.key());
            }
        }
    }
    for (const QString &key : toRemove) {
        QByteArray k = key.toUtf8();
        settings_remove(k.constData());
    }
    QJsonDocument out(obj);
    QByteArray payload = out.toJson(QJsonDocument::Compact);
    return settings_set_all_json(payload.constData()) != 0;
}

QSettings::Format rustStoreFormat()
{
    static const QSettings::Format fmt =
        QSettings::registerFormat(QStringLiteral("ruststore"), rustStoreRead, rustStoreWrite);
    return fmt;
}

} // namespace

// ── Singleton ─────────────────────────────────────────────────────────

SettingsStore &SettingsStore::instance()
{
    static SettingsStore s;
    return s;
}

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
}

SettingsStore::~SettingsStore() = default;

bool SettingsStore::initialize()
{
    QString app = QCoreApplication::instance()
        ? QCoreApplication::instance()->applicationName()
        : QString();
    QString org = QCoreApplication::instance()
        ? QCoreApplication::instance()->organizationName()
        : QString();
    if (app.isEmpty())
        app = QStringLiteral("Scriptura");
    if (org.isEmpty())
        org = QStringLiteral("Scriptura");
    return initialize(app, org);
}

bool SettingsStore::initialize(const QString &applicationName, const QString &organizationName)
{
    if (m_initialized)
        return true;
    QByteArray app = (applicationName.isEmpty() ? QStringLiteral("Scriptura") : applicationName).toUtf8();
    QByteArray org = (organizationName.isEmpty() ? QStringLiteral("Scriptura") : organizationName).toUtf8();
    m_initialized = settings_initialize_with_names(app.constData(), org.constData()) != 0;
    return m_initialized;
}

// ── Plain string settings ─────────────────────────────────────────────

QString SettingsStore::get(const QString &key, const QString &defaultValue) const
{
    QString v = rustGet(key);
    return v.isNull() ? defaultValue : v;
}

void SettingsStore::set(const QString &key, const QString &value)
{
    rustSet(key, value);
}

void SettingsStore::set(const QString &key, const QVariant &value)
{
    setValue(key, value);
}

void SettingsStore::remove(const QString &key)
{
    QByteArray k = key.toUtf8();
    settings_remove(k.constData());
}

bool SettingsStore::contains(const QString &key) const
{
    QByteArray k = key.toUtf8();
    return settings_contains(k.constData()) != 0;
}

// ── Typed (QSettings-compatible) access ───────────────────────────────

QVariant SettingsStore::value(const QString &key, const QVariant &defaultValue) const
{
    QString raw = rustGet(key);
    if (raw.isNull())
        return defaultValue;
    return decodeVariantFromStorage(raw, defaultValue);
}

void SettingsStore::setValue(const QString &key, const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        remove(key);
        return;
    }
    rustSet(key, encodeVariantForStorage(value, true));
}

void SettingsStore::clear()
{
    settings_clear_all();
}

void SettingsStore::factoryReset()
{
    settings_factory_reset();
}

// ── Secrets ───────────────────────────────────────────────────────────

QString SettingsStore::secret(const QString &key, const QString &defaultValue) const
{
    QByteArray k = key.toUtf8();
    QString v = takeRustString(settings_get_secret(k.constData()));
    return v.isNull() ? defaultValue : v;
}

void SettingsStore::setSecret(const QString &key, const QString &value)
{
    QByteArray k = key.toUtf8();
    QByteArray v = value.toUtf8();
    settings_set_secret(k.constData(), v.constData());
}

void SettingsStore::removeSecret(const QString &key)
{
    QByteArray k = key.toUtf8();
    settings_delete_secret(k.constData());
}

bool SettingsStore::hasSecret(const QString &key) const
{
    QByteArray k = key.toUtf8();
    return settings_has_secret(k.constData()) != 0;
}

// ── Bulk ──────────────────────────────────────────────────────────────

QString SettingsStore::getAllAsJson() const
{
    return takeRustString(settings_get_all_json());
}

bool SettingsStore::setAllFromJson(const QString &json)
{
    QByteArray j = json.toUtf8();
    return settings_set_all_json(j.constData()) != 0;
}

// ── Diagnostics ───────────────────────────────────────────────────────

QString SettingsStore::storagePath() const
{
    return takeRustString(settings_get_path());
}

QString SettingsStore::storageDirectory() const
{
    return takeRustString(settings_get_dir());
}

bool SettingsStore::isKeychainAvailable() const
{
    return settings_is_keychain_available() != 0;
}

QString SettingsStore::keychainBackend() const
{
    return takeRustString(settings_keychain_backend());
}

bool SettingsStore::isKeychainPersistent() const
{
    return settings_keychain_is_persistent() != 0;
}

QString SettingsStore::keyStorageLocation() const
{
    return takeRustString(settings_key_storage());
}

QStringList SettingsStore::warnings() const
{
    QString json = takeRustString(settings_warnings_json());
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray())
        return QStringList();
    QStringList out;
    for (const QJsonValue &v : doc.array())
        out.append(v.toString());
    return out;
}

QString SettingsStore::migrationStatus() const
{
    QString s = takeRustString(settings_migration_status());
    return s.isNull() ? QString() : s;
}

QSettings *SettingsStore::createLegacySettings(QObject *parent)
{
    // Custom-format QSettings routed through the Rust store. The file name
    // is never backed by a plaintext file; reads/writes go through the
    // registerFormat callbacks above.
    return new QSettings(rustStoreFormat(), QSettings::UserScope,
                         QCoreApplication::organizationName().isEmpty()
                             ? QStringLiteral("Scriptura")
                             : QCoreApplication::organizationName(),
                         QCoreApplication::applicationName().isEmpty()
                             ? QStringLiteral("Scriptura")
                             : QCoreApplication::applicationName(),
                         parent);
}

// ── Variant encoding ──────────────────────────────────────────────────
//
// Tagged form (used by setValue and the legacy QSettings bridge) prefixes
// every value with its type so reads without a typed default still
// round-trip: "b:1", "i:42", "d:3.14", "s:…", "sl:[…]", "ba:<base64>",
// "font:<QFont::toString>". Untagged values (plain set() callers and data
// imported from the old QSettings file) are decoded using the caller
// supplied default value's type.

QString SettingsStore::encodeVariantForStorage(const QVariant &value, bool tagged)
{
    const int type = value.userType();
    if (type == QMetaType::Bool) {
        QString body = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        return tagged ? QStringLiteral("b:") + body : body;
    }
    if (type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::LongLong ||
        type == QMetaType::ULongLong || type == QMetaType::Short || type == QMetaType::UShort ||
        type == QMetaType::Long || type == QMetaType::ULong) {
        QString body = QString::number(value.toLongLong());
        return tagged ? QStringLiteral("i:") + body : body;
    }
    if (type == QMetaType::Double || type == QMetaType::Float) {
        QString body = QString::number(value.toDouble(), 'g', 17);
        return tagged ? QStringLiteral("d:") + body : body;
    }
    if (type == QMetaType::QStringList || type == QMetaType::QString) {
        if (type == QMetaType::QStringList) {
            QJsonArray arr;
            for (const QString &s : value.toStringList())
                arr.append(s);
            QString body = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
            return tagged ? QStringLiteral("sl:") + body : body;
        }
        return tagged ? QStringLiteral("s:") + value.toString() : value.toString();
    }
    if (type == QMetaType::QByteArray) {
        QString body = QString::fromLatin1(value.toByteArray().toBase64());
        return tagged ? QStringLiteral("ba:") + body : body;
    }
    if (type == QMetaType::QFont) {
        QString body = value.value<QFont>().toString();
        return tagged ? QStringLiteral("font:") + body : body;
    }
    if (type == QMetaType::QKeySequence) {
        QString body = value.value<QKeySequence>().toString();
        return tagged ? QStringLiteral("s:") + body : body;
    }
    // Fallback: store the display string.
    return tagged ? QStringLiteral("s:") + value.toString() : value.toString();
}

QVariant SettingsStore::decodeVariantFromStorage(const QString &raw, const QVariant &defaultValue)
{
    // Tagged values decode by prefix regardless of the default.
    if (raw.startsWith(QStringLiteral("b:")))
        return QVariant(raw.mid(2).trimmed() == QStringLiteral("true") || raw.mid(2).trimmed() == QStringLiteral("1"));
    if (raw.startsWith(QStringLiteral("i:"))) {
        bool ok = false;
        long long v = raw.mid(2).trimmed().toLongLong(&ok);
        if (ok)
            return QVariant(v);
        return defaultValue;
    }
    if (raw.startsWith(QStringLiteral("d:"))) {
        bool ok = false;
        double v = raw.mid(2).trimmed().toDouble(&ok);
        if (ok)
            return QVariant(v);
        return defaultValue;
    }
    if (raw.startsWith(QStringLiteral("sl:"))) {
        QJsonDocument doc = QJsonDocument::fromJson(raw.mid(3).toUtf8());
        if (doc.isArray()) {
            QStringList out;
            for (const QJsonValue &v : doc.array())
                out.append(v.toString());
            return QVariant(out);
        }
        return defaultValue;
    }
    if (raw.startsWith(QStringLiteral("s:")))
        return QVariant(raw.mid(2));
    if (raw.startsWith(QStringLiteral("ba:")))
        return QVariant(QByteArray::fromBase64(raw.mid(3).toLatin1()));
    if (raw.startsWith(QStringLiteral("font:"))) {
        QFont f;
        f.fromString(raw.mid(5));
        return QVariant(f);
    }

    // Untagged: interpret according to the caller's default type.
    const int type = defaultValue.userType();
    if (!defaultValue.isValid() || defaultValue.isNull())
        return QVariant(raw);
    switch (type) {
    case QMetaType::Bool:
        return QVariant(raw.trimmed() == QStringLiteral("true") || raw.trimmed() == QStringLiteral("1"));
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Long:
    case QMetaType::ULong: {
        bool ok = false;
        long long v = raw.trimmed().toLongLong(&ok);
        if (!ok)
            return defaultValue;
        // Callers convert via toInt()/toLongLong(), so a long long round-trips.
        return QVariant(v);
    }
    case QMetaType::Double:
    case QMetaType::Float: {
        bool ok = false;
        double v = raw.trimmed().toDouble(&ok);
        return ok ? QVariant(v) : defaultValue;
    }
    case QMetaType::QStringList: {
        // New writes are JSON arrays; accept legacy newline-joined lists too.
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isArray()) {
            QStringList out;
            for (const QJsonValue &v : doc.array())
                out.append(v.toString());
            return QVariant(out);
        }
        if (raw.isEmpty())
            return QVariant(QStringList());
        return QVariant(raw.split(QLatin1Char('\n')));
    }
    case QMetaType::QByteArray:
        // Rust migration writes @ByteArray payloads as base64; plain values
        // may already be base64 or raw bytes.
        if (!raw.isEmpty()) {
            QByteArray decoded = QByteArray::fromBase64(raw.toLatin1());
            if (!decoded.isEmpty() || raw.isEmpty())
                return QVariant(decoded);
        }
        return QVariant(raw.toLatin1());
    case QMetaType::QFont: {
        QFont f;
        f.fromString(raw);
        return QVariant(f);
    }
    default:
        break;
    }
    if (defaultValue.userType() == QMetaType::QString)
        return QVariant(raw);
    return QVariant(raw);
}
