#ifndef PLUGINCONTEXT_H
#define PLUGINCONTEXT_H

#include <QObject>
#include <QVariant>
#include <QSettings>
#include <functional>
#include "plugininterface.h"
#include "eventbus.h"
#include "permission.h"

class PermissionManager;
class MainWindow;
class CodeEditor;
class LspClient;
class ProblemPanel;
class TerminalPanel;
class GitPanel;
class PluginUIApi;
class PluginEditorApi;
class PluginNotificationApi;

class PluginContext : public QObject
{
    Q_OBJECT

public:
    struct Subscription {
        quint64 id;
        std::function<void(const QVariant&)> callback;
    };

    explicit PluginContext(MainWindow* mainWindow, QObject* parent = nullptr);
    ~PluginContext() override;

    MainWindow* mainWindow() const;
    QSettings* settings() const;
    CodeEditor* currentEditor() const;
    LspClient* lspClient() const;
    ProblemPanel* problemPanel() const;
    TerminalPanel* terminalPanel() const;
    GitPanel* gitPanel() const;
    QString currentProjectPath() const;
    QObject* getPlugin(const QString& id) const;

    template<typename T>
    T getPlugin(const QString& id) const;

    void notify(const QString& event, const QVariant& data = QVariant());
    EventBus::SubscriptionId subscribe(const QString& event, std::function<void(const QVariant&)> callback, QObject* owner = nullptr);

    void setPermissionManager(PermissionManager* manager);
    QString currentPluginId() const;
    void setCurrentPluginId(const QString& pluginId);
    bool hasPermission(const QString& pluginId, Permission permission) const;
    void requestPermission(const QString& pluginId, Permission permission);

    // ── Developer API (UI, Editor, Notifications) ──────────────
    PluginUIApi*            ui() const;
    PluginEditorApi*        editorApi() const;
    PluginNotificationApi*  notifications() const;

private:
    MainWindow* m_mainWindow;
    QSettings* m_settings;
    PermissionManager* m_permissionManager = nullptr;
    QString m_currentPluginId;
    mutable PluginUIApi*            m_uiApi = nullptr;
    mutable PluginEditorApi*        m_editorApi = nullptr;
    mutable PluginNotificationApi*  m_notificationApi = nullptr;

    EventBus::SubscriptionId subscribe(const QString& event, std::function<void(const QVariant&)> callback);

    QHash<QString, QList<Subscription>> m_eventHandlers;
};

template<typename T>
T PluginContext::getPlugin(const QString& id) const
{
    QObject* plugin = getPlugin(id);
    return plugin ? qobject_cast<T>(plugin) : nullptr;
}

#endif // PLUGINCONTEXT_H
