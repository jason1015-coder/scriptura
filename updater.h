#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QString>

class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater();

    void checkForUpdates();
    void setUpdateCheckEnabled(bool enabled);
    bool isUpdateCheckEnabled() const;
    void setUpdateCheckInterval(int days); // Days between update checks
    QString latestVersion() const;
    QString downloadUrl() const;

signals:
    void updateAvailable(const QString &version, const QString &downloadUrl);
    void updateCheckFailed(const QString &error);
    void noUpdateAvailable();

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QTimer *m_timer;
    bool m_updateCheckEnabled;
    int m_updateCheckInterval; // Days
    QString m_latestVersion;
    QString m_downloadUrl;
    QString getLatestReleaseUrl() const;
    QString getGitHubApiUrl() const;
};

#endif // UPDATER_H