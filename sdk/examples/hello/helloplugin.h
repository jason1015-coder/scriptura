#ifndef HELLOPLUGIN_H
#define HELLOPLUGIN_H

#ifndef HELLOPLUGIN_H
#define HELLOPLUGIN_H

#include <QObject>
#include <QPushButton>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QAction>
#include <QToolButton>
#include "../include/scriptura/plugininterface.h"
#include "../include/scriptura/plugincontext.h"
#include "../include/scriptura/pluginapi/uiapi.h"
#include "../include/scriptura/pluginapi/notificationapi.h"
#include "../include/scriptura/pluginapi/editorapi.h"

/**
 * @file helloplugin.h
 * @brief Expanded Hello World plugin demonstrating the full plugin developer API
 *
 * Shows how to use:
 *   - PluginUIApi (menus, toolbar, status bar, panels)
 *   - PluginEditorApi (line decorations)
 *   - PluginNotificationApi (toasts, status messages)
 */

class HelloPlugin : public ScripturaPlugin
{
     Q_OBJECT
     Q_PLUGIN_METADATA(IID "com.scriptura.plugin/1.0" FILE "plugin.json")
     Q_INTERFACES(ScripturaPlugin)

public:
    explicit HelloPlugin(QObject* parent = nullptr);
    ~HelloPlugin() override;

    // ScripturaPlugin interface
    bool initialize(PluginContext* context) override;
    void shutdown() override;
    QString id() const override { return "com.scriptura.hello"; }
    QString name() const override { return "Hello World"; }
    QString version() const override { return "1.0.0"; }
    QString author() const override { return "Scriptura"; }
    QString description() const override {
        return "Demonstrates the full plugin developer API: menus, toolbar, status bar, notifications, editor decorations";
    }
    QStringList dependencies() const override { return {}; }
    bool hasFeature(PluginFeature feature) const override;

public slots:
    void sayHello();
    void onDecorateClicked();
    void onNotifyClicked();

private:
    void cleanupApi();

    PluginContext* m_context = nullptr;

    // UI API resources
    QAction*    m_menuAction = nullptr;
    QAction*    m_toolbarAction = nullptr;
    QLabel*     m_statusLabel = nullptr;
    QToolButton* m_sidebarBtn = nullptr;
    QWidget*    m_panel = nullptr;

    // Editor API
    bool m_decorationsActive = false;
};

#endif // HELLOPLUGIN_H
