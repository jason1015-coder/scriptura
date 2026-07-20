#include "helloplugin.h"
#include <QMenuBar>
#include <QMessageBox>
#include <QApplication>
#include <QIcon>
#include <QColor>
#include <QTimer>

HelloPlugin::HelloPlugin(QObject* parent)
    : ScripturaPlugin(parent)
{
}

HelloPlugin::~HelloPlugin()
{
    shutdown();
}

bool HelloPlugin::initialize(PluginContext* context)
{
    m_context = context;
    if (!m_context) {
        qWarning() << "HelloPlugin: Invalid plugin context";
        return false;
    }

    // ═══════════════════════════════════════════════════════════
    // 1. Menu API — add an action to the "Tools" menu
    // ═══════════════════════════════════════════════════════════
    if (PluginUIApi *ui = m_context->ui()) {
        m_menuAction = ui->addMenuAction(
            QStringLiteral("Tools"),
            tr("Say Hello via API"),
            [this]() { sayHello(); },
            QKeySequence(tr("Ctrl+Alt+H"))
        );

        // Also add a toolbar button
        m_toolbarAction = ui->addToolbarAction(
            QStringLiteral("Plugins"),
            tr("Hello Plugin"),
            [this]() { sayHello(); },
            QIcon()  // default icon
        );

        // Add a status bar widget
        m_statusLabel = ui->addStatusBarWidget(tr("HelloPlugin loaded"), true);

        // Register a custom panel in the bottom area
        m_panel = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(m_panel);
        QPushButton *decorateBtn = new QPushButton(tr("Toggle Decoration"), m_panel);
        QPushButton *notifyBtn = new QPushButton(tr("Show Notification"), m_panel);
        lay->addWidget(decorateBtn);
        lay->addWidget(notifyBtn);
        lay->addStretch();

        connect(decorateBtn, &QPushButton::clicked, this, &HelloPlugin::onDecorateClicked);
        connect(notifyBtn, &QPushButton::clicked, this, &HelloPlugin::onNotifyClicked);

        ui->registerPanel(
            QStringLiteral("com.scriptura.hello.panel"),
            tr("Hello Controls"),
            m_panel,
            PluginUIApi::PanelLocation::BottomPanel
        );

        // Add a sidebar button
        m_sidebarBtn = ui->addSidebarButton(
            QStringLiteral("com.scriptura.hello.sidebar"),
            QIcon(),
            tr("Hello Plugin")
        );
        if (m_sidebarBtn) {
            connect(m_sidebarBtn, &QToolButton::clicked, this, &HelloPlugin::sayHello);
        }
    }

    qDebug() << "HelloPlugin initialized successfully — full developer API";
    return true;
}

void HelloPlugin::cleanupApi()
{
    // Clean up all UI resources the plugin added
    if (PluginUIApi *ui = m_context ? m_context->ui() : nullptr) {
        if (m_sidebarBtn) {
            ui->removeSidebarButton(
                QStringLiteral("com.scriptura.hello.sidebar"));
            m_sidebarBtn = nullptr;
        }
        if (m_panel) {
            ui->unregisterPanel(
                QStringLiteral("com.scriptura.hello.panel"));
            m_panel = nullptr;
        }
    }

    // Qt will clean up QActions when their parent is destroyed, but
    // explicitly deleting ensures immediate removal on shutdown().
    delete m_toolbarAction;  m_toolbarAction = nullptr;
    delete m_menuAction;     m_menuAction = nullptr;
    delete m_statusLabel;    m_statusLabel = nullptr;
    delete m_panel;          m_panel = nullptr;
}

void HelloPlugin::shutdown()
{
    cleanupApi();
    m_context = nullptr;
    qDebug() << "HelloPlugin shutdown";
}

bool HelloPlugin::hasFeature(PluginFeature feature) const
{
    switch (feature) {
        case PluginFeature::MenuAction:
        case PluginFeature::ToolbarButton:
        case PluginFeature::StatusBarWidget:
        case PluginFeature::BottomPanel:
        case PluginFeature::SidebarButton:
        case PluginFeature::EditorDecoration:
        case PluginFeature::Notification:
            return true;
        default:
            return false;
    }
}

void HelloPlugin::sayHello()
{
    // Show a notification via the Notification API
    if (m_context && m_context->notifications()) {
        m_context->notifications()->showInfo(
            tr("Hello"),
            tr("Hello from HelloPlugin! This uses the developer API."),
            4000
        );
    }

    // Also update status bar
    if (m_context && m_context->notifications()) {
        m_context->notifications()->showStatusMessage(
            tr("HelloPlugin: Hello clicked!"), 2000);
    }
}

void HelloPlugin::onDecorateClicked()
{
    if (!m_context || !m_context->editorApi())
        return;

    m_decorationsActive = !m_decorationsActive;
    if (m_decorationsActive) {
        // Highlight the current line with a green decoration
        int line = m_context->editorApi()->cursorLine();
        if (line >= 0) {
            m_context->editorApi()->addLineDecoration(
                QStringLiteral("hello.deco"),
                line,
                QColor(0, 180, 0, 40),
                tr("HelloPlugin decoration")
            );
        }
    } else {
        m_context->editorApi()->removeDecoration(
            QStringLiteral("hello.deco"));
    }
}

void HelloPlugin::onNotifyClicked()
{
    if (!m_context || !m_context->notifications())
        return;

    // Show a warning toast
    m_context->notifications()->showWarning(
        tr("Plugin Demo"),
        tr("This is a warning notification from HelloPlugin."),
        5000
    );

    // Show progress for 3 seconds
    m_context->notifications()->showProgress(
        QStringLiteral("hello.demo"),
        tr("Demo task..."),
        0, 100
    );
    for (int i = 0; i <= 100; i += 25) {
        QTimer::singleShot(i * 30, this, [this, i]() {
            m_context->notifications()->updateProgress(
                QStringLiteral("hello.demo"), i);
        });
    }
    QTimer::singleShot(3000, this, [this]() {
        m_context->notifications()->hideProgress(
            QStringLiteral("hello.demo"));
    });
}
