#ifndef PERMISSION_H
#define PERMISSION_H

#include <Qt>
#include <QMetaType>

/**
 * @file permission.h
 * @brief 定義插件權限系統的權限枚舉
 * 
 * 權限系統用於控制插件對系統資源的存取權限，
 * 包括檔案系統、網路、進程執行等。
 * 權限的檢查和授予由 RustPermissionManagerAdapter 處理。
 */

/**
 * @enum Permission
 * @brief 插件權限類型
 */
enum class Permission {
    FileRead,           ///< 讀取檔案
    FileWrite,          ///< 寫入檔案
    NetworkAccess,      ///< 網路存取
    ProcessExecution,   ///< 執行程序
    SystemSettings,     ///< 系統設定
    ClipboardAccess,    ///< 剪貼簿存取
    Notification         ///< 系統通知
};

// 註冊為 QMetaType 以支援 QVariant 封裝
Q_DECLARE_METATYPE(Permission)

#endif // PERMISSION_H
