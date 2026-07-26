#ifndef PLUGINCONTEXT_H
#define PLUGINCONTEXT_H

#include <QObject>
#include <QVariant>
#include <QSettings>
#include <functional>
#include "plugininterface.h"
#include "permission.h"
class RustPermissionManagerAdapter;
class RustEventBusAdapter;

class MainWindow;
class CodeEditor;
class LspClient;
class ProblemPanel;
class TerminalPanel;
class GitPanel;
class PluginUIApi;
class PluginEditorApi;
class PluginNotificationApi;
class PluginThemeApi;

/**
 * @class PluginContext
 * @brief 插件上下文，提供對核心服務的訪問
 * 
 * 所有服務都以抽象介面的形式提供，允許插件：
 * - 存取主視窗和 UI 元件
 * - 使用設定系統
 * - 與其他插件通訊
 * - 訂閱和發佈事件
 * - 檢查和請求權限
 * - 控制 UI 佈局、編輯器、通知
 */
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
    bool hasPermission(const QString& pluginId, Permission permission) const;
    void requestPermission(const QString& pluginId, Permission permission);
    QSettings* settings() const;
    QString currentPluginId() const;
    void setCurrentPluginId(const QString& pluginId);
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
    quint64 subscribe(const QString& event, std::function<void(const QVariant&)> callback, QObject* owner = nullptr);
    void unsubscribe(const QString& event, quint64 subscriptionId);

    // ── Developer API (UI, Editor, Notifications, Theme) ───────
    PluginUIApi*            ui() const;
    PluginEditorApi*        editorApi() const;
    PluginNotificationApi*  notifications() const;
    PluginThemeApi*         theme() const;

private:
    MainWindow* m_mainWindow;
    QSettings* m_settings;
    QString m_currentPluginId;
    mutable PluginUIApi*            m_uiApi = nullptr;
    mutable PluginEditorApi*        m_editorApi = nullptr;
    mutable PluginNotificationApi*  m_notificationApi = nullptr;
    mutable PluginThemeApi*         m_themeApi = nullptr;

    quint64 subscribe(const QString& event, std::function<void(const QVariant&)> callback);

    QHash<QString, QList<Subscription>> m_eventHandlers;
};

template<typename T>
T PluginContext::getPlugin(const QString& id) const
{
    QObject* plugin = getPlugin(id);
    return plugin ? qobject_cast<T>(plugin) : nullptr;
}

#endif // PLUGINCONTEXT_H