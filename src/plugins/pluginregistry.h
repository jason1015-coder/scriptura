#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QTimer>

#include "rust_backend.h"

class PluginRegistry : public QObject
{
    Q_OBJECT
public:
    explicit PluginRegistry(QObject *parent = nullptr);
    ~PluginRegistry() override;

    void setRegistryUrl(const QUrl &url);
    QUrl registryUrl() const;
    void setCheckInterval(int days);
    void startPeriodicCheck();
    void checkForUpdates();

    void installPlugin(const QString &pluginId, const QUrl &downloadUrl);
    bool upgradeAvailable(const QString &pluginId, const QString &currentVersion) const;

    QJsonObject manifest() const { return m_manifest; }

signals:
    void registryUpdated(const QJsonObject &manifest);
    void pluginDownloaded(const QString &pluginId, const QByteArray &data);
    void installFailed(const QString &pluginId, const QString &error);
    void updateAvailable(const QString &pluginId, const QString &latestVersion);

private:
    // Rust FFI callbacks
    static void onRustRegistryUpdated(const char *data, void *userData);
    static void onRustInstallFailed(const char *id, const char *error, void *userData);

    QUrl m_registryUrl;
    QJsonObject m_manifest;
    QNetworkAccessManager *m_network;
    QTimer *m_timer;
    int m_checkIntervalDays;
    RustPluginRegistry *m_rustRegistry = nullptr;  // Rust backend for registry fetch + JSON parsing
};

#endif // PLUGINREGISTRY_H
