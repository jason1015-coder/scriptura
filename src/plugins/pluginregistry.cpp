#include "pluginregistry.h"
#include "rust_backend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

PluginRegistry::PluginRegistry(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_checkIntervalDays(7)
    , m_rustRegistry(rust_plugin_registry_new())
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &PluginRegistry::checkForUpdates);
    
    // Set up Rust callback for registry updates
    rust_plugin_registry_on_update(m_rustRegistry, &PluginRegistry::onRustRegistryUpdated, this);
    rust_plugin_registry_on_install_failed(m_rustRegistry, &PluginRegistry::onRustInstallFailed, this);
}

PluginRegistry::~PluginRegistry()
{
    if (m_rustRegistry)
        rust_plugin_registry_free(m_rustRegistry);
}

void PluginRegistry::setRegistryUrl(const QUrl &url)
{
    m_registryUrl = url;
    // Also set on Rust backend
    QByteArray u = url.toString().toUtf8();
    rust_plugin_registry_set_url(m_rustRegistry, u.constData());
}

QUrl PluginRegistry::registryUrl() const
{
    return m_registryUrl;
}

void PluginRegistry::setCheckInterval(int days)
{
    m_checkIntervalDays = days;
    if (m_timer->isActive())
        m_timer->start(m_checkIntervalDays * 24 * 60 * 60 * 1000);
}

void PluginRegistry::startPeriodicCheck()
{
    m_timer->start(m_checkIntervalDays * 24 * 60 * 60 * 1000);
}

void PluginRegistry::checkForUpdates()
{
    // Re-arm the periodic timer for the next interval.
    m_timer->start(m_checkIntervalDays * 24 * 60 * 60 * 1000);

    if (m_registryUrl.isEmpty())
        return;

    // Delegate to Rust backend for registry fetch + parse on background thread.
    // Rust handles HTTP fetch and JSON parsing; result fires back via callback.
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
    });
    
    QFuture<void> future = QtConcurrent::run([this]() {
        rust_plugin_registry_check_updates(m_rustRegistry);
    });
    watcher->setFuture(future);
}

void PluginRegistry::installPlugin(const QString &pluginId, const QUrl &downloadUrl)
{
    if (downloadUrl.isEmpty())
        return;

    QNetworkRequest req(downloadUrl);
    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pluginId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            emit pluginDownloaded(pluginId, data);
        } else {
            emit installFailed(pluginId, reply->errorString());
        }
        reply->deleteLater();
    });
}

bool PluginRegistry::upgradeAvailable(const QString &pluginId, const QString &currentVersion) const
{
    // Delegate to Rust backend for semver comparison
    QByteArray id = pluginId.toUtf8();
    QByteArray ver = currentVersion.toUtf8();
    return rust_plugin_registry_upgrade_available(m_rustRegistry, id.constData(), ver.constData());
}

// ── Static Rust Callbacks ─────────────────────────────────────────────

void PluginRegistry::onRustRegistryUpdated(const char *data, void *userData)
{
    auto *self = static_cast<PluginRegistry*>(userData);
    QString jsonStr = QString::fromUtf8(data);
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        self->m_manifest = obj;
        QMetaObject::invokeMethod(self, [self, obj]() {
            emit self->registryUpdated(obj);
        }, Qt::QueuedConnection);
    }
}

void PluginRegistry::onRustInstallFailed(const char *id, const char *error, void *userData)
{
    auto *self = static_cast<PluginRegistry*>(userData);
    QString pluginId = QString::fromUtf8(id);
    QString err = QString::fromUtf8(error);
    QMetaObject::invokeMethod(self, [self, pluginId, err]() {
        emit self->installFailed(pluginId, err);
    }, Qt::QueuedConnection);
}
