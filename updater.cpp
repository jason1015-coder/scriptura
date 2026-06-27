#include "updater.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>
#include <QTimerEvent>

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_updateCheckEnabled(true)
    , m_updateCheckInterval(7) // Check weekly by default
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &Updater::onNetworkReply);

    // Load settings
    QSettings settings;
    m_updateCheckEnabled = settings.value("updater/checkEnabled", true).toBool();
    m_updateCheckInterval = settings.value("updater/checkInterval", 7).toInt();

    // Setup periodic check
    connect(m_timer, &QTimer::timeout, this, &Updater::checkForUpdates);
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
    if (!m_updateCheckEnabled)
        return;

    QNetworkRequest request;
    request.setUrl(QUrl(getGitHubApiUrl()));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Scriptura-Updater");

    m_networkManager->get(request);
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

QString Updater::getLatestReleaseUrl() const
{
    return "https://github.com/jasonblanchard/scriptura/releases/latest";
}

QString Updater::getGitHubApiUrl() const
{
    return "https://api.github.com/repos/jasonblanchard/scriptura/releases/latest";
}

void Updater::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Update check failed:" << reply->errorString();
        emit updateCheckFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse update response:" << error.errorString();
        emit updateCheckFailed(error.errorString());
        return;
    }

    if (!doc.isObject()) {
        emit updateCheckFailed("Invalid response format");
        return;
    }

    QJsonObject obj = doc.object();
    m_latestVersion = obj["tag_name"].toString();
    m_downloadUrl = obj["html_url"].toString();

    // Get current version from settings (or use a default)
    QSettings settings;
    QString currentVersion = settings.value("updater/currentVersion", "0.0.0").toString();

    // Compare versions (simple string comparison, works for semver)
    if (m_latestVersion != currentVersion && !m_latestVersion.isEmpty()) {
        emit updateAvailable(m_latestVersion, m_downloadUrl);
    } else {
        emit noUpdateAvailable();
    }
}