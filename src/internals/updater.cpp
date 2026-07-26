#include "updater.h"
#include "rust_backend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QDebug>
#include <QTimerEvent>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_updateCheckEnabled(true)
    , m_updateCheckInterval(7) // Check weekly by default
    , m_lastCheckedType(Stable)
{
    // Load settings
    QSettings settings;
    m_updateCheckEnabled = settings.value("updater/checkEnabled", true).toBool();
    m_updateCheckInterval = settings.value("updater/checkInterval", 7).toInt();

    // Setup periodic check — delegates to Rust backend for version fetching
    connect(m_timer, &QTimer::timeout, this, [this]() { checkForUpdates(Stable); });
    m_timer->start(m_updateCheckInterval * 24 * 60 * 60 * 1000); // Convert days to milliseconds
}

Updater::~Updater()
{
    // Save settings
    QSettings settings;
    settings.setValue("updater/checkEnabled", m_updateCheckEnabled);
    settings.setValue("updater/checkInterval", m_updateCheckInterval);
}

void Updater::checkForUpdates()
{
    checkForUpdates(Stable);
}

void Updater::checkForUpdates(ReleaseType type)
{
    m_lastCheckedType = type;

    // Delegate to Rust backend for version checking in a background thread.
    // The Rust updater handles HTTP fetch + JSON parsing + semver comparison.
    // Qt plumbing (timers, settings, signals) stays in C++.
    
    QSettings settings;
    QString currentVersion = settings.value("updater/currentVersion", "0.0.0").toString();
    QString apiUrl = getGitHubApiUrl(type);
    
    // Create a Rust updater instance for this check
    RustUpdater *rustUpdater = rust_updater_new();
    
    // Run check on background thread using QtConcurrent
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, rustUpdater]() {
        // Results are stored in the Rust Updater struct after background thread completes
        bool available = rust_updater_is_update_available(rustUpdater);
        if (available) {
            char *ver = rust_updater_latest_version(rustUpdater);
            m_latestVersion = QString::fromUtf8(ver);
            rust_free_string(ver);
            if (!m_latestVersion.isEmpty())
                emit updateAvailable(m_latestVersion, m_downloadUrl);
        } else {
            // No update available (or network error — Rust logs the error internally)
            emit noUpdateAvailable();
        }
        rust_updater_free(rustUpdater);
        watcher->deleteLater();
    });
    
    QFuture<void> future = QtConcurrent::run([rustUpdater, currentVersion, apiUrl]() {
        // Blocking Rust call on background thread — handles HTTP fetch + JSON parsing + semver comparison
        QByteArray ver = currentVersion.toUtf8();
        QByteArray url = apiUrl.toUtf8();
        rust_updater_check(rustUpdater, ver.constData(), url.constData());
    });
    watcher->setFuture(future);
}

void Updater::setUpdateCheckEnabled(bool enabled)
{
    m_updateCheckEnabled = enabled;
}

bool Updater::isUpdateCheckEnabled() const
{
    return m_updateCheckEnabled;
}

void Updater::setUpdateCheckInterval(int days)
{
    m_updateCheckInterval = days;
    m_timer->start(m_updateCheckInterval * 24 * 60 * 60 * 1000);
}

QString Updater::latestVersion() const
{
    return m_latestVersion;
}

QString Updater::downloadUrl() const
{
    return m_downloadUrl;
}

Updater::ReleaseType Updater::lastCheckedType() const
{
    return m_lastCheckedType;
}

QString Updater::getLatestReleaseUrl() const
{
    return "https://github.com/jasonblanchard/scriptura/releases/latest";
}

QString Updater::getGitHubApiUrl(ReleaseType type) const
{
    if (type == PreRelease) {
        return "https://api.github.com/repos/jasonblanchard/scriptura/releases?per_page=10";
    }
    return "https://api.github.com/repos/jasonblanchard/scriptura/releases/latest";
}
