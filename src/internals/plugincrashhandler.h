#ifndef PLUGINCRASHHANDLER_H
#define PLUGINCRASHHANDLER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QList>
#include <QHash>

struct RustPluginCrashHandler;

/**
 * @file plugincrashhandler.h
 * @brief 插件崩潰處理器，負責監控和處理插件崩潰
 */

/**
 * @struct CrashInfo
 * @brief 崩潰資訊結構
 */
struct CrashInfo {
    QString pluginId;          ///< 插件 ID
    QDateTime timestamp;       ///< 崩潰時間
    QString errorType;         ///< 錯誤類型
    QString stackTrace;        ///< 堆疊追蹤
    bool autoDisabled;         ///< 是否自動禁用
};

/**
 * @class PluginCrashHandler
 * @brief 處理插件崩潰並執行恢復策略
 */
class PluginCrashHandler : public QObject
{
    Q_OBJECT
public:
    explicit PluginCrashHandler(QObject* parent = nullptr);
    ~PluginCrashHandler();

    void registerPluginProcess(const QString& pluginId, class QProcess* process);
    void handleCrash(const QString& pluginId);
    void disablePlugin(const QString& pluginId);
    QList<CrashInfo> recentCrashes(int limit = 10);
    bool isPluginDisabled(const QString& pluginId) const;
    void enablePlugin(const QString& pluginId);

signals:
    void pluginCrashed(const QString& pluginId, const CrashInfo& info);

private:
    static void onCrashCb(const char* pluginId, const char* error, void* userData);

    RustPluginCrashHandler* m_rustH = nullptr;
    QList<CrashInfo> m_crashHistory;
    QHash<QString, QProcess*> m_pluginProcesses;
    QHash<QString, bool> m_disabledPlugins;
    QString m_crashLogPath;
};

#endif // PLUGINCRASHHANDLER_H
