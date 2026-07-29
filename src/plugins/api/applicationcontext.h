#ifndef APPLICATIONCONTEXT_H
#define APPLICATIONCONTEXT_H

#include <QObject>
#include <QSettings>
#include <QString>

class MainWindow;
class PluginUIApi;
class PluginNotificationApi;

/**
 * @file applicationcontext.h
 * @brief Simplified context provided to ScripturaApplication instances
 *
 * Applications get a lighter-weight context than full plugins,
 * with just enough to integrate with the core UI.
 */
class ApplicationContext : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationContext(MainWindow* mainWindow, QObject* parent = nullptr);
    ~ApplicationContext() override;

    // ── Core Access ──────────────────────────────────────────────

    MainWindow* mainWindow() const { return m_mainWindow; }

    /**
     * @brief Current project root path
     */
    QString currentProjectPath() const;

    /**
     * @brief Settings access for application preferences
     */
    QSettings* settings() const { return m_settings; }

    /**
     * @brief UI API for adding menus, toolbar actions, etc.
     * (More limited than full Plugin UI API access)
     */
    PluginUIApi* ui() const { return m_uiApi; }

    /**
     * @brief Notification API for showing toasts and progress
     */
    PluginNotificationApi* notifications() const { return m_notificationApi; }

    // ── Event Bus ────────────────────────────────────────────────

    void notify(const QString& event, const QVariant& data = QVariant());
    quint64 subscribe(const QString& event, std::function<void(const QVariant&)> callback);
    void unsubscribe(const QString& event, quint64 subscriptionId);

private:
    MainWindow* m_mainWindow;
    QSettings* m_settings;
    PluginUIApi* m_uiApi;
    PluginNotificationApi* m_notificationApi;
};

#endif // APPLICATIONCONTEXT_H
