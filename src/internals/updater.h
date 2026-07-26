#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QTimer>
#include <QString>

/**
 * @brief 應用程式更新器
 *
 * 負責檢查 Scriptura 的 GitHub Releases 並提供更新功能。
 * 支援檢查穩定版 (Latest Release) 和預覽版 (Latest Pre-release)。
 * Backend logic (HTTP fetch, JSON parsing, semver comparison) delegated
 * to Rust FFI (rust_backend.h) via QtConcurrent background thread.
 */
class Updater : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 發行類型列舉
     */
    enum ReleaseType {
        Stable,       ///< 穩定版 (Latest Release)
        PreRelease    ///< 預覽版 (Latest Pre-release)
    };

    explicit Updater(QObject *parent = nullptr);
    ~Updater();

    /**
     * @brief 檢查更新（使用預設的穩定版）
     */
    void checkForUpdates();

    /**
     * @brief 檢查指定類型的更新
     * @param type 發行類型 (Stable 或 PreRelease)
     */
    void checkForUpdates(ReleaseType type);

    void setUpdateCheckEnabled(bool enabled);
    bool isUpdateCheckEnabled() const;
    void setUpdateCheckInterval(int days); // Days between update checks
    QString latestVersion() const;
    QString downloadUrl() const;
    ReleaseType lastCheckedType() const;

signals:
    void updateAvailable(const QString &version, const QString &downloadUrl);
    void updateCheckFailed(const QString &error);
    void noUpdateAvailable();

private:
    QTimer *m_timer;
    bool m_updateCheckEnabled;
    int m_updateCheckInterval; // Days
    QString m_latestVersion;
    QString m_downloadUrl;
    ReleaseType m_lastCheckedType;
    QString getLatestReleaseUrl() const;
    QString getGitHubApiUrl(ReleaseType type) const;
};

#endif // UPDATER_H