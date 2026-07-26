#include "permission.h"
#include "rust_backend.h"
#include <QDebug>
#include <QMessageBox>

namespace {
QString permissionName(Permission p)
{
    switch (p) {
        case Permission::FileRead: return QObject::tr("File Read");
        case Permission::FileWrite: return QObject::tr("File Write");
        case Permission::NetworkAccess: return QObject::tr("Network Access");
        case Permission::ProcessExecution: return QObject::tr("Process Execution");
        case Permission::SystemSettings: return QObject::tr("System Settings");
        case Permission::ClipboardAccess: return QObject::tr("Clipboard Access");
        case Permission::Notification: return QObject::tr("Notification");
    }
    return QObject::tr("Unknown");
}

int permissionToInt(Permission p)
{
    return static_cast<int>(p);
}
}

PermissionManager::PermissionManager()
    : m_rustPm(rust_permission_manager_new())
{
}

PermissionManager::~PermissionManager()
{
    if (m_rustPm) {
        rust_permission_manager_free(m_rustPm);
    }
}

PermissionManager::PermissionManager(PermissionManager&& other) noexcept
    : m_rustPm(other.m_rustPm)
    , m_declaredPermissions(std::move(other.m_declaredPermissions))
{
    other.m_rustPm = nullptr;
}

PermissionManager& PermissionManager::operator=(PermissionManager&& other) noexcept
{
    if (this != &other) {
        if (m_rustPm) rust_permission_manager_free(m_rustPm);
        m_rustPm = other.m_rustPm;
        m_declaredPermissions = std::move(other.m_declaredPermissions);
        other.m_rustPm = nullptr;
    }
    return *this;
}

bool PermissionManager::checkPermission(const QString& pluginId, Permission permission)
{
    if (!m_rustPm) return false;
    QByteArray idBytes = pluginId.toUtf8();
    return rust_permission_manager_check(m_rustPm, idBytes.constData(), permissionToInt(permission));
}

void PermissionManager::requestPermission(const QString& pluginId, Permission permission)
{
    if (!m_rustPm) return;

    // Check if declared
    if (!m_declaredPermissions.contains(pluginId) || 
        !m_declaredPermissions[pluginId].contains(permission)) {
        qWarning() << "Plugin" << pluginId << "requested undeclared permission:" << static_cast<int>(permission);
        return;
    }

    // Already granted via Rust side, check if Rust has it
    QByteArray idBytes = pluginId.toUtf8();
    if (rust_permission_manager_check(m_rustPm, idBytes.constData(), permissionToInt(permission))) {
        return;
    }

    // Trigger permission dialog, get user approval
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        QObject::tr("Permission Request"),
        QObject::tr("Plugin \"%1\" is requesting permission: %2.\n\nGrant this permission?")
            .arg(pluginId, permissionName(permission)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
        grantPermission(pluginId, permission);
}

void PermissionManager::grantPermission(const QString& pluginId, Permission permission)
{
    if (!m_rustPm) return;
    QByteArray idBytes = pluginId.toUtf8();
    rust_permission_manager_grant(m_rustPm, idBytes.constData(), permissionToInt(permission));
}

void PermissionManager::revokePermission(const QString& pluginId, Permission permission)
{
    if (!m_rustPm) return;
    QByteArray idBytes = pluginId.toUtf8();
    rust_permission_manager_revoke(m_rustPm, idBytes.constData(), permissionToInt(permission));
}

QList<Permission> PermissionManager::grantedPermissions(const QString& pluginId) const
{
    // Rust FFI doesn't expose grantedPermissions list; keep local tracking via declared
    if (!m_declaredPermissions.contains(pluginId)) {
        return QList<Permission>();
    }
    // We don't have a full list from Rust, so return declared permissions as best-effort
    // (the Rust side tracks granted/revoked internally)
    return m_declaredPermissions[pluginId];
}

void PermissionManager::setDeclaredPermissions(const QString& pluginId, const QList<Permission>& permissions)
{
    m_declaredPermissions[pluginId] = permissions;
}

QList<Permission> PermissionManager::declaredPermissions(const QString& pluginId) const
{
    if (!m_declaredPermissions.contains(pluginId)) {
        return QList<Permission>();
    }
    return m_declaredPermissions[pluginId];
}
