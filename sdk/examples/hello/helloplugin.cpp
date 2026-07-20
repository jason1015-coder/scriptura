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

        // Theme demo button
        QPushButton *themeBtn = new QPushButton(tr("Apply Custom Theme"), m_panel);
        lay->addWidget(themeBtn);
        connect(themeBtn, &QPushButton::clicked, this, &HelloPlugin::onThemeDemoClicked);

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
    // Clean up theme registration
    if (PluginThemeApi *theme = m_context ? m_context->theme() : nullptr) {
        theme->unregisterTheme(QStringLiteral("com.scriptura.hello.theme"));
    }

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
        case PluginFeature::ThemeProvider:
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

void HelloPlugin::onThemeDemoClicked()
{
    if (!m_context || !m_context->theme())
        return;

    PluginThemeApi *themeApi = m_context->theme();

    // Check if our theme is already registered
    if (themeApi->isThemeRegistered(QStringLiteral("com.scriptura.hello.theme"))) {
        // Toggle: apply the theme
        if (themeApi->currentThemeId() == QStringLiteral("com.scriptura.hello.theme")) {
            // Already active - show info
            m_context->notifications()->showInfo(
                tr("Theme Active"),
                tr("HelloPlugin custom theme is already active!"),
                2000
            );
        } else {
            themeApi->applyTheme(QStringLiteral("com.scriptura.hello.theme"));
            m_context->notifications()->showInfo(
                tr("Theme Applied"),
                tr("HelloPlugin custom theme applied!"),
                2000
            );
        }
    } else {
        // Register a custom "Ocean" theme
        PluginThemeApi::ThemeDefinition def;
        def.id = QStringLiteral("com.scriptura.hello.theme");
        def.displayName = tr("HelloPlugin Ocean Theme");
        def.author = QStringLiteral("HelloPlugin");
        def.description = tr("A calm ocean-inspired dark theme with teal accents");
        def.isDark = true;

        // Ocean color palette
        def.colors.window       = QColor(15, 25, 35);      // Deep navy
        def.colors.windowText   = QColor(200, 220, 230);    // Light blue-gray
        def.colors.base         = QColor(10, 20, 30);       // Darker navy
        def.colors.alternateBase = QColor(20, 35, 50);      // Slightly lighter
        def.colors.text         = QColor(200, 220, 230);    // Light blue-gray
        def.colors.button       = QColor(25, 50, 70);       // Medium navy
        def.colors.buttonText   = QColor(200, 230, 240);    // Light blue
        def.colors.accent       = QColor(0, 180, 200);      // Teal accent
        def.colors.accentHover  = QColor(0, 210, 230);      // Lighter teal
        def.colors.accentPressed = QColor(0, 150, 170);     // Darker teal
        def.colors.highlight    = QColor(0, 150, 180);      // Teal highlight
        def.colors.highlightedText = QColor(255, 255, 255);
        def.colors.border       = QColor(40, 70, 100);      // Blue-gray border
        def.colors.mid          = QColor(30, 55, 75);       // Mid blue-gray
        def.colors.success      = QColor(0, 200, 120);      // Sea green
        def.colors.warning      = QColor(255, 200, 50);     // Sandy yellow
        def.colors.error        = QColor(220, 80, 80);      // Coral red
        def.colors.info         = QColor(0, 160, 220);      // Ocean blue
        def.colors.link         = QColor(0, 180, 200);      // Teal
        def.colors.linkVisited  = QColor(100, 140, 180);    // Muted blue
        def.colors.diffAdded    = QColor(0, 200, 120);      // Sea green
        def.colors.diffModified = QColor(200, 200, 80);     // Sandy yellow
        def.colors.diffDeleted  = QColor(220, 80, 80);      // Coral red
        def.colors.diffConflict = QColor(255, 140, 60);     // Orange

        // Syntax colors
        def.syntaxColors.keyword      = QColor(0, 200, 220);   // Bright teal
        def.syntaxColors.string       = QColor(120, 200, 140); // Seafoam
        def.syntaxColors.comment      = QColor(100, 120, 140); // Muted blue-gray
        def.syntaxColors.number       = QColor(220, 160, 80);  // Sandy gold
        def.syntaxColors.preprocessor = QColor(200, 100, 100); // Coral
        def.syntaxColors.function     = QColor(80, 180, 240);  // Sky blue
        def.syntaxColors.type         = QColor(0, 180, 200);   // Teal
        def.syntaxColors.variable     = QColor(180, 220, 200); // Mint

        // Custom stylesheet for nicer borders and selection
        def.customStylesheet = QStringLiteral(
            "QTabBar::tab:selected { border-bottom: 2px solid #00B4C8; }\n"
            "QPushButton:hover { border-color: #00B4C8; }\n"
        );

        themeApi->registerTheme(def);
        themeApi->applyTheme(def.id);

        m_context->notifications()->showInfo(
            tr("Theme Created"),
            tr("HelloPlugin registered and applied 'Ocean Theme'!\nCheck the theme selector in Settings."),
            4000
        );
    }
}
