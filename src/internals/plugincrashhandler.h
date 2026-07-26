#ifndef PLUGINCRASHHANDLER_H
#define PLUGINCRASHHANDLER_H

#include <QDateTime>
#include <QString>

/**
 * @file plugincrashhandler.h
 * @brief 插件崩潰資訊結構
 * 
 * CrashInfo 結構用於傳遞插件崩潰的詳細資訊。
 * 崩潰處理由 RustPluginCrashHandlerAdapter 管理。
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

#endif // PLUGINCRASHHANDLER_H
