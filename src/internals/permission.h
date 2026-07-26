#ifndef PERMISSION_H
#define PERMISSION_H

#include <Qt>
#include <QMetaType>
#include <QString>
#include <QList>
#include <QHash>

struct RustPermissionManager;

/**
 * @file permission.h
 * @brief 定義插件權限系統
 * 
 * 權限系統用於控制插件對系統資源的存取權限，
 * 包括檔案系統、網路、進程執行等。
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

/**
 * @class PermissionManager
 * @brief 管理插件權限的授予和檢查
 * 
 * 這個類別負責：
 * - 檢查插件是否有特定權限
 * - 處理權限請求
 * - 授予和撤銷權限
 */
class PermissionManager
{
public:
    PermissionManager();
    ~PermissionManager();
    PermissionManager(const PermissionManager&) = delete;
    PermissionManager& operator=(const PermissionManager&) = delete;
    PermissionManager(PermissionManager&& other) noexcept;
    PermissionManager& operator=(PermissionManager&& other) noexcept;

    /**
     * @brief 檢查插件是否有權限
     * @param pluginId 插件 ID
     * @param permission 要檢查的權限
     * @return 有權限返回 true
     */
    bool checkPermission(const QString& pluginId, Permission permission);
    
    /**
     * @brief 請求權限
     * @param pluginId 插件 ID
     * @param permission 要請求的權限
     */
    void requestPermission(const QString& pluginId, Permission permission);
    
    /**
     * @brief 授予權限
     * @param pluginId 插件 ID
     * @param permission 要授予的權限
     */
    void grantPermission(const QString& pluginId, Permission permission);
    
    /**
     * @brief 撤銷權限
     * @param pluginId 插件 ID
     * @param permission 要撤銷的權限
     */
    void revokePermission(const QString& pluginId, Permission permission);
    
    /**
     * @brief 獲取插件已授予的權限列表
     * @param pluginId 插件 ID
     * @return 權限列表
     */
    QList<Permission> grantedPermissions(const QString& pluginId) const;
    
    /**
     * @brief 設定插件宣告的權限
     * @param pluginId 插件 ID
     * @param permissions 權限列表
     */
    void setDeclaredPermissions(const QString& pluginId, const QList<Permission>& permissions);
    
    /**
     * @brief 獲取插件宣告的權限
     * @param pluginId 插件 ID
     * @return 權限列表
     */
    QList<Permission> declaredPermissions(const QString& pluginId) const;

private:
    RustPermissionManager* m_rustPm = nullptr;
    QHash<QString, QList<Permission>> m_declaredPermissions;  ///< 宣告權限 (Rust doesn't expose this via FFI)
};

#endif // PERMISSION_H