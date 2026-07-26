#include "plugincontext.h"
#include "rust_adapter.h"
#include "pluginmanager.h"
#include "mainwindow.h"
#include "plugins/api/uiapi.h"
#include "plugins/api/editorapi.h"
#include "plugins/api/notificationapi.h"
#include "plugins/api/themeapi.h"
#include <QDebug>
#include <QApplication>

#define RUST_BACKEND RustBackend::instance()

PluginContext::PluginContext(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_settings(nullptr)
    , m_uiApi(nullptr)
    , m_editorApi(nullptr)
    , m_notificationApi(nullptr)
    , m_themeApi(nullptr)
{
    if (mainWindow) {
        m_settings = new QSettings(this);
        m_uiApi = new PluginUIApi(mainWindow, this);
        m_editorApi = new PluginEditorApi(mainWindow, this);
        m_notificationApi = new PluginNotificationApi(mainWindow, this);
        m_themeApi = new PluginThemeApi(mainWindow, this);
    }
}

PluginContext::~PluginContext()
{
}

// ── Developer API ────────────────────────────────────────────────

PluginUIApi* PluginContext::ui() const
{
    return m_uiApi;
}

PluginEditorApi* PluginContext::editorApi() const
{
    return m_editorApi;
}

PluginNotificationApi* PluginContext::notifications() const
{
    return m_notificationApi;
}

PluginThemeApi* PluginContext::theme() const
{
    return m_themeApi;
}

MainWindow* PluginContext::mainWindow() const
{
    if (!m_currentPluginId.isEmpty()) {
        QString pid = m_currentPluginId;
        if (!RUST_BACKEND->permissionManager()->checkPermission(pid, Permission::SystemSettings)) {
            qWarning() << "Plugin" << pid << "denied access to mainWindow (SystemSettings)";
            return nullptr;
        }
    }
    return m_mainWindow;
}

QSettings* PluginContext::settings() const
{
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::SystemSettings)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to settings (SystemSettings)";
            return nullptr;
        }
    }
    return m_settings;
}

CodeEditor* PluginContext::currentEditor() const
{
    if (!m_mainWindow) return nullptr;
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::FileRead)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to currentEditor (FileRead)";
            return nullptr;
        }
    }
    return m_mainWindow->getCurrentCodeEditor();
}

LspClient* PluginContext::lspClient() const
{
    if (!m_mainWindow) return nullptr;
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::ProcessExecution)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to lspClient (ProcessExecution)";
            return nullptr;
        }
    }
    return m_mainWindow->getLspClient();
}

ProblemPanel* PluginContext::problemPanel() const
{
    if (!m_mainWindow) return nullptr;
    return m_mainWindow->getProblemPanel();
}

TerminalPanel* PluginContext::terminalPanel() const
{
    if (!m_mainWindow) return nullptr;
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::ProcessExecution)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to terminalPanel (ProcessExecution)";
            return nullptr;
        }
    }
    return m_mainWindow->getTerminalPanel();
}

GitPanel* PluginContext::gitPanel() const
{
    if (!m_mainWindow) return nullptr;
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::ProcessExecution)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to gitPanel (ProcessExecution)";
            return nullptr;
        }
    }
    return m_mainWindow->getGitPanel();
}

QString PluginContext::currentProjectPath() const
{
    if (!m_mainWindow) return QString();
    if (!m_currentPluginId.isEmpty()) {
        if (!RUST_BACKEND->permissionManager()->checkPermission(m_currentPluginId, Permission::FileRead)) {
            qWarning() << "Plugin" << m_currentPluginId << "denied access to currentProjectPath (FileRead)";
            return QString();
        }
    }
    return m_mainWindow->currentProjectPath();
}

QObject* PluginContext::getPlugin(const QString& id) const
{
    PluginManager* manager = qApp->findChild<PluginManager*>();
    if (manager) {
        ScripturaPlugin* plugin = manager->getPlugin(id);
        if (plugin) {
            return qobject_cast<QObject*>(plugin);
        }
    }
    return nullptr;
}

void PluginContext::notify(const QString& event, const QVariant& data)
{
    RUST_BACKEND->eventBus()->publish(event, data);
}

quint64 PluginContext::subscribe(const QString& event, std::function<void(const QVariant&)> callback, QObject* owner)
{
    quint64 id = RUST_BACKEND->eventBus()->subscribe(event, owner, callback);
    m_eventHandlers[event].append({id, callback});
    return id;
}

quint64 PluginContext::subscribe(const QString& event, std::function<void(const QVariant&)> callback)
{
    quint64 id = RUST_BACKEND->eventBus()->subscribe(event, nullptr, callback);
    m_eventHandlers[event].append({id, callback});
    return id;
}

void PluginContext::unsubscribe(const QString& event, quint64 subscriptionId)
{
    RUST_BACKEND->eventBus()->unsubscribe(event, subscriptionId);
    
    auto& subscriptions = m_eventHandlers[event];
    for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it) {
        if (it->id == subscriptionId) {
            subscriptions.erase(it);
            break;
        }
    }
    
    if (subscriptions.isEmpty()) {
        m_eventHandlers.remove(event);
    }
}

QString PluginContext::currentPluginId() const
{
    return m_currentPluginId;
}

void PluginContext::setCurrentPluginId(const QString& pluginId)
{
    m_currentPluginId = pluginId;
}

bool PluginContext::hasPermission(const QString& pluginId, Permission permission) const
{
    return RUST_BACKEND->permissionManager()->checkPermission(pluginId, permission);
}

void PluginContext::requestPermission(const QString& pluginId, Permission permission)
{
    RUST_BACKEND->permissionManager()->requestPermission(pluginId, permission);
}
