#include "pluginupdater.h"
#include "archiveextractor.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QApplication>
#include <QCoreApplication>

PluginUpdater::PluginUpdater(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_updateTimer(new QTimer(this))
    , m_registryUrl("https://plugins.scriptura.app/registry.json")
{
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        checkForUpdates();
    });
}

PluginUpdater::~PluginUpdater()
{
}

void PluginUpdater::setRegistryUrl(const QString& url)
{
    m_registryUrl = url;
}

void PluginUpdater::scheduleUpdateCheck(int intervalHours)
{
    m_updateTimer->setInterval(intervalHours * 60 * 60 * 1000);
    m_updateTimer->start();
}

QList<UpdateInfo> PluginUpdater::checkForUpdates()
{
    QList<UpdateInfo> updates;

    QNetworkRequest request(m_registryUrl);
    QNetworkReply* reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject registry = doc.object();
            QJsonObject plugins = registry["plugins"].toObject();

            QString pluginDir = QApplication::applicationDirPath() + "/plugins";
            QDir dir(pluginDir);

            if (dir.exists()) {
                for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    QString metadataFile = info.absoluteFilePath() + "/plugin.json";
                    if (QFile::exists(metadataFile)) {
                        QFile file(metadataFile);
                        if (file.open(QIODevice::ReadOnly)) {
                            QByteArray content = file.readAll();
                            file.close();

                            QJsonParseError pe;
                            QJsonDocument pd = QJsonDocument::fromJson(content, &pe);
                            if (pe.error == QJsonParseError::NoError && pd.isObject()) {
                                QJsonObject plugin = pd.object();
                                QString id = plugin["id"].toString();
                                QString currentVersion = plugin["version"].toString();

                                if (plugins.contains(id)) {
                                    QJsonObject pluginInfo = plugins[id].toObject();
                                    QString latest = pluginInfo["latest"].toString();

                                    if (latest != currentVersion) {
                                        UpdateInfo update;
                                        update.pluginId = id;
                                        update.currentVersion = currentVersion;
                                        update.latestVersion = latest;
                                        update.downloadUrl = pluginInfo["downloadUrl"].toString();
                                        update.changelog = pluginInfo["changelog"].toString();
                                        update.securityUpdate = pluginInfo["securityUpdate"].toBool();
                                        updates.append(update);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            emit updateCheckFailed("Invalid registry format");
            reply->deleteLater();
            return updates;
        }
    } else {
        emit updateCheckFailed(reply->errorString());
        reply->deleteLater();
        return updates;
    }

    reply->deleteLater();

    if (updates.isEmpty()) {
        emit noUpdatesAvailable();
    } else {
        emit updatesAvailable(updates);
    }

    return updates;
}

bool PluginUpdater::downloadUpdate(const UpdateInfo& update)
{
    if (update.downloadUrl.isEmpty()) {
        return false;
    }

    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString downloadPath = downloadDir + "/" + update.pluginId + "-" + update.latestVersion + ".zip";

    QNetworkRequest request(update.downloadUrl);
    QNetworkReply* reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QFile file(downloadPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            reply->deleteLater();
            return true;
        }
    }

    reply->deleteLater();
    return false;
}

bool PluginUpdater::installUpdate(const QString& pluginId)
{
    QString pluginDir = QApplication::applicationDirPath() + "/plugins/" + pluginId;
    QDir dir(pluginDir);

    if (!dir.exists()) {
        return false;
    }

    QString backupDir = pluginDir + ".backup";
    if (QDir(backupDir).exists()) {
        QDir(backupDir).removeRecursively();
    }

    if (dir.rename(pluginDir, backupDir)) {
        QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString archivePath = downloadDir + "/" + pluginId + "-*.zip";

        QDir downloadDirObj(downloadDir);
        QStringList zipFiles = downloadDirObj.entryList(QStringList() << pluginId + "-*.zip", QDir::Files);

        if (!zipFiles.isEmpty()) {
            QString zipPath = downloadDir + "/" + zipFiles.first();

            if (Scriptura::ArchiveExtractor::extract(zipPath, pluginDir)) {
                QDir(backupDir).removeRecursively();
                emit pluginUpdated(pluginId);
                return true;
            }

            dir.rename(backupDir, pluginDir);
        } else {
            dir.rename(backupDir, pluginDir);
        }
    }

    return false;
}

void PluginUpdater::onRegistryReply(QNetworkReply* reply)
{
    Q_UNUSED(reply);
}
