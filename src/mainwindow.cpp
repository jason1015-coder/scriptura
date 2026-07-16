#include <QShortcut>
#include <QCloseEvent>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "updater.h"
#include "configvalidator.h"
#include "pluginmanager.h"
#include "plugincontext.h"
#include "pluginmanagerdialog.h"
#include "version.h"
#include "findreplace.h"
#include "projectsearch.h"
#include "commandpalette.h"
#include "dapclient.h"
#include "debugpanel.h"
#include "debugconfiguration.h"
#include "rundialog.h"
#include "workspace.h"
#include "taskrunner.h"
#include "minimap.h"
#include "splitmanager.h"
#include "breadcrumb.h"
#include "aiinlinecompletion.h"
#include "httpclientpanel.h"
#include "codeactionui.h"
#include "sqliteviewer.h"
#include "pluginregistry.h"
#include "customtitlebar.h"
#include "windowanimator.h"
#include "thememanager.h"
#include "themeicons.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileSystemModel>
#include <QDir>
#include <QTextCursor>
#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QProcess>
#include <QInputDialog>
#include <QLineEdit>
#include <QFontDialog>
#include <QSettings>
#include <QFileInfo>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QToolTip>
#include <QUrl>
#include <QPair>
#include <QMouseEvent>
#include <QButtonGroup>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDesktopServices>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include "customtitlebar.h"
#include "windowanimator.h"
#include "thememanager.h"

#ifdef Q_OS_LINUX
#include <QApplication>
#include <QWidget>
#include <QTime>
#include <QTimer>
#include <QPainter>
#include <QDebug>
#include <QMenu>

// Linux blur support - uses KWin's KWindowEffects if available
// This provides a blur behind the window similar to Windows Acrylic
static bool enableLinuxBlur(WId windowId, bool darkMode)
{
    // Try KWindowEffects first (KDE/KWin)
    // This requires linking against KF5WindowEffects or KWindowSystem
    // For now, we set a property that can be used by window rules
    if (QApplication *app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setProperty("kwin_blur", true);
        app->setProperty("kwin_blur_region", QRegion());
        
        // Also try GTK3/GTK4 client-side decorations if available
        app->setProperty("gtk_application_prefer_dark_theme", darkMode);
    }
    
    return true;
}
#endif

MainWindow::MainWindow(const QString &initialProject, const QStringList &initialFiles, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , selectedTheme(ThemeColorFamily::Default, ThemeMode::Light)
    , autoSaveTimer(new QTimer(this))
    , lspDebounceTimer(new QTimer(this))
    , problemPanel(new ProblemPanel(this))
    , todoPanel(new TodoPanel(this))
    , gitPanel(new GitPanel(this))
    , terminalPanel(new TerminalPanel(this))
    , updater(new Updater(this))
    , configValidator(new ConfigValidator(this))
    , lspClient(new LspClient(this))
    , pluginManager(new PluginManager(this))
    , pluginContext(new PluginContext(this, this))
    , pluginManagerDialog(new PluginManagerDialog(pluginManager, pluginContext, this))
    , m_previousEditorStackIndex(0)
    , dapClient(new DapClient(this))
    , debugPanel(new DebugPanel(this))
    , debugConfigManager(std::make_unique<DebugConfigurationManager>())
    , m_isDebugging(false)
    , m_workspace(new Workspace(this))
    , m_minimap(nullptr)
    , m_splitManager(new SplitManager(this))
    , m_breadcrumb(nullptr)
    , m_aiInline(new AiInlineCompletion(this))
    , m_httpClient(new HttpClientPanel(this))
    , m_codeActionCtrl(new CodeActionController(this))
    , m_sqliteViewer(new SqliteViewerPanel(this))
    , m_pluginRegistry(new PluginRegistry(this))
{
    ui->setupUi(this);

    // Initialize modular UI components
    m_themeManager = new ThemeManager(this);
    // 主題切換時自動重新著色所有追蹤中的圖標，確保在各主題下都保持可見
    connect(m_themeManager, &ThemeManager::themeChanged,
            ThemeIcons::instance(), &ThemeIcons::recolorAll);
    // 檔案樹使用主題感知的圖標提供者，切換主題後需重新取得圖標
    connect(m_themeManager, &ThemeManager::themeChanged, this, [this]() {
        if (fileModel) {
            fileModel->layoutChanged();
        }
    });
    m_windowAnimator = new WindowAnimator(this);
    m_titleBar = new CustomTitleBar(this);
    
    // Connect title bar signals
    connect(m_titleBar, &CustomTitleBar::minimizeRequest, this, &MainWindow::showMinimized);
    connect(m_titleBar, &CustomTitleBar::maximizeRequest, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    connect(m_titleBar, &CustomTitleBar::closeRequest, this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::menuRequested, this, [this]() {
        // Create a dropdown menu from the menu bar actions
        QMenu dropdownMenu(this);
        dropdownMenu.setObjectName("titleBarMenu");

        // Add all menu bar actions to the dropdown
        const QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
        for (QMenu *menu : menus) {
            if (!menu->title().isEmpty()) {
                QAction *menuAction = menu->menuAction();
                if (menuAction) {
                    dropdownMenu.addAction(menuAction);
                    dropdownMenu.addSeparator();
                }
            }
        }

        // Show menu below the button
        QPoint menuPos = m_titleBar->menuButton->mapToGlobal(QPoint(0, m_titleBar->menuButton->height()));
        dropdownMenu.exec(menuPos);
    });

    // Connect unified title bar signals
    connect(m_titleBar, &CustomTitleBar::sidebarToggleClicked, this, [this]() {
        setSidebarCollapsed(!ui->sidebarDrawer->isVisible());
    });
    connect(m_titleBar, &CustomTitleBar::inspectorToggleClicked, this, [this]() {
        // Toggle right-side inspector panel (placeholder)
    });
    connect(m_titleBar, &CustomTitleBar::searchRequested, this, [this]() {
        QString query = m_titleBar->searchField->text();
        if (!query.isEmpty()) {
            on_action_project_search_triggered();
        }
    });
    
    // Install title bar as event filter for dragging
    m_titleBar->installEventFilter(this);
    
    // Setup frameless window with custom title bar
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    
    // Add title bar to editor container layout at the top
    ui->editorContainerLayout->insertWidget(0, m_titleBar);
    
    // Apply initial theme through ThemeManager
    ThemeManager::Theme initialTheme(
        static_cast<ThemeManager::ColorFamily>(static_cast<int>(selectedTheme.family)),
        static_cast<ThemeManager::Mode>(static_cast<int>(selectedTheme.mode)),
        ThemeManager::Features(static_cast<ThemeManager::Feature>(static_cast<int>(selectedTheme.features)))
    );
    m_themeManager->applyTheme(initialTheme);

    pluginManager->setContext(pluginContext);
    
    pluginManager->loadPlugins(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins");
    QString builtinPlugins = QCoreApplication::applicationDirPath() + "/plugins";
    pluginManager->loadPlugins(builtinPlugins);

    loadRecentProjects();

    QSettings settings;
    int legacyTheme = settings.value("theme/selected", 0).toInt();
    selectedTheme = themeFromLegacyInt(legacyTheme);
    applyTheme(selectedTheme);

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect available = screen->availableGeometry();
        resize(qMin(available.width() * 7 / 10, 900), 
               qMin(available.height() * 7 / 10, 800));
    }
    
    fileModel = new QFileSystemModel(this);
    // 使用主題感知的圖標提供者，確保檔案樹圖標在淺/深色/高對比下都可見
    m_fileIconProvider = new ThemeFileIconProvider();
    fileModel->setIconProvider(m_fileIconProvider);
    fileModel->setRootPath(QDir::homePath());
    ui->fileTreeView->setModel(fileModel);
    ui->fileTreeView->setHeaderHidden(true);
    ui->fileTreeView->setColumnHidden(1, true);
    ui->fileTreeView->setColumnHidden(2, true);
    ui->fileTreeView->setColumnHidden(3, true);

    editorStack = ui->editorStack;
    bottomPanelStack = ui->bottomPanelStack;
    tabBar = ui->tabBar;
    bottomPanelTabs = ui->bottomPanelTabs;

    welcomeWidget = createWelcomeWidget();
    editorStack->addWidget(welcomeWidget);
    editorStack->addWidget(ui->tabWidget);
    editorStack->addWidget(todoPanel);
    editorStack->addWidget(terminalPanel);

    // Plugin registry - load user-configured URL
    registryUrl = QSettings().value("plugin/registryUrl", "https://raw.githubusercontent.com/jason1015-coder/scriptura/main/plugin-registry.json").toString();

    // Create settings page widgets
    themeSettingsWidget = createThemeSettingsWidget();
    editorSettingsWidget = createEditorSettingsWidget();
    keyboardShortcutsPageWidget = createKeyboardShortcutsPageWidget();
    updaterSettingsWidget = createUpdaterSettingsWidget();

    editorStack->addWidget(themeSettingsWidget);
    editorStack->addWidget(editorSettingsWidget);
    editorStack->addWidget(keyboardShortcutsPageWidget);
    editorStack->addWidget(updaterSettingsWidget);

    // Add settings tabs to tabBar
    int themeSettingsTabIndex = tabBar->addTab(tr("Theme"));
    tabBar->setTabData(themeSettingsTabIndex, static_cast<int>(TabType::ThemeSettings));
    tabBar->setTabButton(themeSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(themeSettingsTabIndex));

    int editorSettingsTabIndex = tabBar->addTab(tr("Settings"));
    tabBar->setTabData(editorSettingsTabIndex, static_cast<int>(TabType::EditorSettings));
    tabBar->setTabButton(editorSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(editorSettingsTabIndex));

    int keyboardShortcutsTabIndex = tabBar->addTab(tr("Keys"));
    tabBar->setTabData(keyboardShortcutsTabIndex, static_cast<int>(TabType::KeyboardShortcuts));
    tabBar->setTabButton(keyboardShortcutsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(keyboardShortcutsTabIndex));

    int updaterSettingsTabIndex = tabBar->addTab(tr("Updates"));
    tabBar->setTabData(updaterSettingsTabIndex, static_cast<int>(TabType::UpdaterSettings));
    tabBar->setTabButton(updaterSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(updaterSettingsTabIndex));

    bottomPanelStack->addWidget(problemPanel);
    bottomPanelStack->addWidget(gitPanel);
    problemPanel->hide();
    gitPanel->hide();

    // Create panels that need to be added to bottom stack
    projectSearchPanel = new ProjectSearchPanel(this);
    debugPanel = new DebugPanel(this);

    // Top toolbar setup — buttons now live in the unified title bar or editor area
    sidebarToggleButton = new QToolButton(this);
    sidebarToggleButton->setIcon(ThemeIcons::instance()->icon(":/icons/sidebar-toggle.svg"));
    sidebarToggleButton->setIconSize(QSize(20, 20));
    sidebarToggleButton->setToolTip(tr("Toggle Sidebar"));
    sidebarToggleButton->setCheckable(true);
    sidebarToggleButton->setChecked(true);
    sidebarToggleButton->setFixedSize(32, 32);
    sidebarToggleButton->hide(); // Managed by unified title bar

    goUpButton = new QToolButton(this);
    goUpButton->setIcon(ThemeIcons::instance()->icon(":/icons/go-up.svg"));
    goUpButton->setIconSize(QSize(18, 18));
    goUpButton->setToolTip(tr("Go Up"));
    goUpButton->setEnabled(false);
    goUpButton->setFixedSize(32, 32);
    goUpButton->hide(); // Can be added to title bar later

    findReplaceBar = new FindReplaceBar(this);
    findReplaceBar->setVisible(false);

    commandPalette = new CommandPalette(this);

    // Add remaining panels to bottom stack
    bottomPanelStack->addWidget(projectSearchPanel);
    bottomPanelStack->addWidget(debugPanel);
    projectSearchPanel->hide();
    debugPanel->hide();

    // Right-side toolbar buttons (settings moved to title bar area)
    settingsButton = new QToolButton(this);
    settingsButton->setIcon(ThemeIcons::instance()->icon(":/icons/settings.svg"));
    settingsButton->setIconSize(QSize(20, 20));
    settingsButton->setToolTip(tr("Editor Settings"));
    settingsButton->setFixedSize(32, 32);
    settingsButton->hide(); // Accessible via title bar menu

    // Sidebar icon buttons (bottom of drawer)
    fileTreeToggleButton = new QToolButton(ui->sidebarDrawer);
    fileTreeToggleButton->setIcon(ThemeIcons::instance()->icon(":/icons/file-tree.svg"));
    fileTreeToggleButton->setIconSize(QSize(20, 20));
    fileTreeToggleButton->setToolTip(tr("File Tree"));
    fileTreeToggleButton->setCheckable(true);
    fileTreeToggleButton->setChecked(true);
    fileTreeToggleButton->setFixedSize(32, 32);
    ui->sidebarDrawerLayout->addWidget(fileTreeToggleButton);

    QWidget *iconBar = new QWidget(ui->sidebarDrawer);
    iconBar->setObjectName("sidebarIconBar");
    QHBoxLayout *iconBarLayout = new QHBoxLayout(iconBar);
    iconBarLayout->setContentsMargins(6, 8, 6, 8);
    iconBarLayout->setSpacing(6);
    iconBarLayout->setAlignment(Qt::AlignCenter);

    placeholderButton = new QToolButton(iconBar);
    placeholderButton->setIcon(ThemeIcons::instance()->icon(":/icons/todo.svg"));
    placeholderButton->setIconSize(QSize(20, 20));
    placeholderButton->setToolTip(tr("Todo"));
    placeholderButton->setCheckable(true);
    placeholderButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(placeholderButton);

    terminalButton = new QToolButton(iconBar);
    terminalButton->setIcon(ThemeIcons::instance()->icon(":/icons/terminal.svg"));
    terminalButton->setIconSize(QSize(20, 20));
    terminalButton->setToolTip(tr("Terminal"));
    terminalButton->setCheckable(true);
    terminalButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(terminalButton);

    problemsButton = new QToolButton(iconBar);
    problemsButton->setIcon(ThemeIcons::instance()->icon(":/icons/problems.svg"));
    problemsButton->setIconSize(QSize(20, 20));
    problemsButton->setToolTip(tr("Problems"));
    problemsButton->setCheckable(true);
    problemsButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(problemsButton);

    gitButton = new QToolButton(iconBar);
    gitButton->setIcon(ThemeIcons::instance()->icon(":/icons/git.svg"));
    gitButton->setIconSize(QSize(20, 20));
    gitButton->setToolTip(tr("Git"));
    gitButton->setCheckable(true);
    gitButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(gitButton);

    ui->sidebarDrawerLayout->addWidget(iconBar);

    // Tab bar styling and connections
    tabBar->setTabsClosable(false);
    tabBar->installEventFilter(this);
    connect(tabBar, &QTabBar::currentChanged, this, &MainWindow::onTopTabChanged);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateStatusBar);

    // Bottom panel tabs
    bottomPanelTabs->addTab(tr("Problems"));
    bottomPanelTabs->addTab(tr("Git"));
    bottomPanelTabs->addTab(tr("Search Results"));
    bottomPanelTabs->addTab(tr("Debug"));
    connect(bottomPanelTabs, &QTabBar::currentChanged, this, &MainWindow::onBottomTabChanged);

    // Sidebar connections
    connect(sidebarToggleButton, &QToolButton::toggled, this, &MainWindow::toggleSidebar);
    connect(goUpButton, &QToolButton::clicked, this, &MainWindow::goUpClicked);
    connect(fileTreeToggleButton, &QToolButton::toggled, this, [this](bool checked) {
        Q_UNUSED(checked);
        if (ui->fileTreeView->isHidden()) {
            ui->fileTreeView->show();
        } else {
            ui->fileTreeView->hide();
        }
    });

    // Panel button connections
    connect(placeholderButton, &QToolButton::toggled, this, &MainWindow::toggleTodoPanel);
    connect(terminalButton, &QToolButton::toggled, this, &MainWindow::toggleTerminalPanel);
    connect(problemsButton, &QToolButton::toggled, this, &MainWindow::toggleProblemPanel);
    connect(gitButton, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            ui->bottomPanelContainer->show();
            bottomPanelTabs->setCurrentIndex(1);
            bottomPanelStack->setCurrentIndex(1);
            gitPanel->show();
            if (placeholderButton->isChecked()) {
                QSignalBlocker blocker(placeholderButton);
                placeholderButton->setChecked(false);
            }
            if (terminalButton->isChecked()) {
                QSignalBlocker blocker(terminalButton);
                terminalButton->setChecked(false);
                if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
                    editorStack->setCurrentIndex(m_previousEditorStackIndex);
                }
            }
            if (problemPanel->isVisible()) {
                problemPanel->hide();
                problemsButton->setChecked(false);
            }
        } else {
            ui->bottomPanelContainer->hide();
            gitPanel->hide();
         }
     });

    connect(gitPanel, &GitPanel::commitRequested, this, &MainWindow::on_action_git_commit_triggered);
    connect(gitPanel, &GitPanel::pushRequested, this, &MainWindow::on_action_git_push_triggered);
    connect(gitPanel, &GitPanel::pullRequested, this, &MainWindow::on_action_git_pull_triggered);
    connect(gitPanel, &GitPanel::fetchRequested, this, &MainWindow::on_action_git_fetch_triggered);

    // Settings button
    connect(settingsButton, &QToolButton::clicked, this, &MainWindow::on_action_editor_settings_triggered);

    // File tree
    connect(ui->fileTreeView, &QTreeView::clicked, this, &MainWindow::on_fileTreeView_clicked);

    // Tab close requests
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::on_tabWidget_tabCloseRequested);

    editorStack->addWidget(welcomeWidget);
    editorStack->addWidget(ui->tabWidget);
    editorStack->addWidget(todoPanel);
    editorStack->addWidget(terminalPanel);

    if (!initialProject.isEmpty()) {
        loadProjectDirectory(initialProject);
        for (const QString &f : initialFiles) {
            if (QFile::exists(f))
                openFileInTab(f);
        }
        showEditorInterface();
    } else {
        showWelcomeScreen();
    }

    // Restore panel layout from previous session
    {
        QSettings settings;
        if (settings.value("mainWindow/bottomPanelVisible", false).toBool()) {
            ui->bottomPanelContainer->show();
            int idx = settings.value("mainWindow/bottomPanelIndex", 0).toInt();
            if (idx >= 0 && idx < bottomPanelTabs->count()) {
                bottomPanelTabs->setCurrentIndex(idx);
                bottomPanelStack->setCurrentIndex(idx);
            }
        }
    }

    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);

    // LSP debounce timer for text changes
    lspDebounceTimer->setSingleShot(true);
    lspDebounceTimer->setInterval(500); // 500ms debounce
    connect(lspDebounceTimer, &QTimer::timeout, this, &MainWindow::onEditorTextChanged);
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(350);
    connect(m_hoverTimer, &QTimer::timeout, this, &MainWindow::requestHover);

    QShortcut *shortcutDialog = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(shortcutDialog, &QShortcut::activated, this, &MainWindow::showKeyboardShortcuts);

    QShortcut *shortcutFind = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(shortcutFind, &QShortcut::activated, this, &MainWindow::on_action_find_triggered);

    QShortcut *shortcutReplace = new QShortcut(QKeySequence("Ctrl+H"), this);
    connect(shortcutReplace, &QShortcut::activated, this, &MainWindow::on_action_replace_triggered);

    QShortcut *shortcutFindNext = new QShortcut(QKeySequence("Ctrl+G"), this);
    connect(shortcutFindNext, &QShortcut::activated, this, [this]() { if (findReplaceBar) findReplaceBar->findNext(); });

    QShortcut *shortcutFindPrev = new QShortcut(QKeySequence("Ctrl+Shift+G"), this);
    connect(shortcutFindPrev, &QShortcut::activated, this, [this]() { if (findReplaceBar) findReplaceBar->findPrev(); });

    QShortcut *shortcutProjectSearch = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    connect(shortcutProjectSearch, &QShortcut::activated, this, &MainWindow::on_action_project_search_triggered);

    QShortcut *shortcutCommandPalette = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(shortcutCommandPalette, &QShortcut::activated, this, &MainWindow::on_action_command_palette_triggered);

    QShortcut *shortcutFormat = new QShortcut(QKeySequence("Ctrl+Shift+I"), this);
    connect(shortcutFormat, &QShortcut::activated, this, &MainWindow::on_action_format_document_triggered);

    QShortcut *shortcutCompletion = new QShortcut(QKeySequence("Ctrl+Space"), this);
    connect(shortcutCompletion, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
            return;
        QTextCursor c = editor->textCursor();
        LspClient::Position pos;
        pos.line = c.blockNumber();
        pos.character = c.positionInBlock();
        lspClient->completion(QUrl::fromLocalFile(currentFile).toString(), pos);
    });

    QShortcut *shortcutSigHelp = new QShortcut(QKeySequence("Ctrl+Shift+Space"), this);
    connect(shortcutSigHelp, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
            return;
        QTextCursor c = editor->textCursor();
        LspClient::Position pos;
        pos.line = c.blockNumber();
        pos.character = c.positionInBlock();
        lspClient->signatureHelp(QUrl::fromLocalFile(currentFile).toString(), pos);
    });

    QShortcut *shortcutDefinition = new QShortcut(QKeySequence("F12"), this);
    connect(shortcutDefinition, &QShortcut::activated, this, &MainWindow::on_action_go_to_definition_triggered);

    // Debug shortcuts
    QShortcut *shortcutRunDebug = new QShortcut(QKeySequence("F5"), this);
    connect(shortcutRunDebug, &QShortcut::activated, this, &MainWindow::on_action_run_debug_triggered);

    QShortcut *shortcutStopDebug = new QShortcut(QKeySequence("Shift+F5"), this);
    connect(shortcutStopDebug, &QShortcut::activated, this, &MainWindow::on_action_stop_debug_triggered);

    QShortcut *shortcutStepOver = new QShortcut(QKeySequence("F10"), this);
    connect(shortcutStepOver, &QShortcut::activated, this, &MainWindow::on_action_step_over_triggered);

    QShortcut *shortcutStepInto = new QShortcut(QKeySequence("F11"), this);
    connect(shortcutStepInto, &QShortcut::activated, this, &MainWindow::on_action_step_into_triggered);

    QShortcut *shortcutStepOut = new QShortcut(QKeySequence("Shift+F11"), this);
    connect(shortcutStepOut, &QShortcut::activated, this, &MainWindow::on_action_step_out_triggered);

    QShortcut *shortcutContinue = new QShortcut(QKeySequence("Ctrl+F5"), this);
    connect(shortcutContinue, &QShortcut::activated, this, &MainWindow::on_action_continue_debug_triggered);

    QShortcut *shortcutToggleBreakpoint = new QShortcut(QKeySequence("F9"), this);
    connect(shortcutToggleBreakpoint, &QShortcut::activated, this, &MainWindow::on_action_toggle_breakpoint_triggered);

    QShortcut *shortcutCloseWindow = new QShortcut(QKeySequence("Ctrl+W"), this);
    connect(shortcutCloseWindow, &QShortcut::activated, this, &QWidget::close);

    QMenu *searchMenu = ui->menubar->addMenu(tr("&Search"));
    QAction *actionFind = searchMenu->addAction(tr("&Find..."));
    connect(actionFind, &QAction::triggered, this, &MainWindow::on_action_find_triggered);
    QAction *actionReplace = searchMenu->addAction(tr("&Replace..."));
    connect(actionReplace, &QAction::triggered, this, &MainWindow::on_action_replace_triggered);
    searchMenu->addSeparator();
    QAction *actionProjectSearch = searchMenu->addAction(tr("Search in &Project..."));
    connect(actionProjectSearch, &QAction::triggered, this, &MainWindow::on_action_project_search_triggered);
    QAction *actionCommandPalette = searchMenu->addAction(tr("&Command Palette..."));
    connect(actionCommandPalette, &QAction::triggered, this, &MainWindow::on_action_command_palette_triggered);

    QMenu *lspMenu = ui->menubar->addMenu(tr("&Code"));
    QAction *actionFormat = lspMenu->addAction(tr("&Format Document"));
    connect(actionFormat, &QAction::triggered, this, &MainWindow::on_action_format_document_triggered);
    QAction *actionSymbols = lspMenu->addAction(tr("Show &Symbols..."));
    connect(actionSymbols, &QAction::triggered, this, &MainWindow::on_action_show_document_symbols_triggered);
    QAction *actionDefinition = lspMenu->addAction(tr("Go to &Definition"));
    connect(actionDefinition, &QAction::triggered, this, &MainWindow::on_action_go_to_definition_triggered);
    QAction *actionDeclaration = lspMenu->addAction(tr("Go to &Declaration"));
    connect(actionDeclaration, &QAction::triggered, this, &MainWindow::on_action_go_to_declaration_triggered);
    QAction *actionTypeDefinition = lspMenu->addAction(tr("Go to &Type Definition"));
    connect(actionTypeDefinition, &QAction::triggered, this, &MainWindow::on_action_go_to_type_definition_triggered);
    QAction *actionImplementation = lspMenu->addAction(tr("Go to &Implementation"));
    connect(actionImplementation, &QAction::triggered, this, &MainWindow::on_action_go_to_implementation_triggered);

    // Debug menu
    QMenu *debugMenu = ui->menubar->addMenu(tr("&Debug"));
    QAction *actionRunDebug = debugMenu->addAction(tr("Run / &Debug..."));
    actionRunDebug->setShortcut(QKeySequence("F5"));
    connect(actionRunDebug, &QAction::triggered, this, &MainWindow::on_action_run_debug_triggered);
    QAction *actionStopDebug = debugMenu->addAction(tr("Stop &Debugging"));
    actionStopDebug->setShortcut(QKeySequence("Shift+F5"));
    connect(actionStopDebug, &QAction::triggered, this, &MainWindow::on_action_stop_debug_triggered);
    debugMenu->addSeparator();
    QAction *actionToggleBreakpoint = debugMenu->addAction(tr("Toggle &Breakpoint"));
    actionToggleBreakpoint->setShortcut(QKeySequence("F9"));
    connect(actionToggleBreakpoint, &QAction::triggered, this, &MainWindow::on_action_toggle_breakpoint_triggered);
    debugMenu->addSeparator();
    QAction *actionContinue = debugMenu->addAction(tr("&Continue"));
    actionContinue->setShortcut(QKeySequence("Ctrl+F5"));
    connect(actionContinue, &QAction::triggered, this, &MainWindow::on_action_continue_debug_triggered);
    QAction *actionStepOver = debugMenu->addAction(tr("Step &Over"));
    actionStepOver->setShortcut(QKeySequence("F10"));
    connect(actionStepOver, &QAction::triggered, this, &MainWindow::on_action_step_over_triggered);
    QAction *actionStepInto = debugMenu->addAction(tr("Step &Into"));
    actionStepInto->setShortcut(QKeySequence("F11"));
    connect(actionStepInto, &QAction::triggered, this, &MainWindow::on_action_step_into_triggered);
    QAction *actionStepOut = debugMenu->addAction(tr("Step &Out"));
    actionStepOut->setShortcut(QKeySequence("Shift+F11"));
    connect(actionStepOut, &QAction::triggered, this, &MainWindow::on_action_step_out_triggered);

    QMenu *toolsMenu = ui->menubar->addMenu(tr("&Tools"));
    QAction *actionHttpClient = toolsMenu->addAction(tr("HTTP &Client"));
    connect(actionHttpClient, &QAction::triggered, this, [this]() {
        bottomPanelTabs->setCurrentIndex(4);
        ui->bottomPanelContainer->show();
    });
    QAction *actionDbViewer = toolsMenu->addAction(tr("SQLite &Viewer"));
    connect(actionDbViewer, &QAction::triggered, this, [this]() {
        bottomPanelTabs->setCurrentIndex(5);
        ui->bottomPanelContainer->show();
    });
    toolsMenu->addSeparator();
    QAction *actionAICompletions = toolsMenu->addAction(tr("AI &Completions"));
    actionAICompletions->setCheckable(true);
    actionAICompletions->setChecked(m_aiInline->isEnabled());
    connect(actionAICompletions, &QAction::triggered, this, [this, actionAICompletions](bool checked) {
        QSettings settings;
        settings.setValue("ai/enabled", checked);
        m_aiInline->setSettings(
            settings.value("ai/provider", "ollama").toString(),
            settings.value("ai/endpoint", "http://localhost:11434/api/chat").toString(),
            settings.value("ai/model", "codellama").toString(),
            checked,
            settings.value("ai/debounceMs", 400).toInt(),
            settings.value("ai/apiKey", {}).toString()
        );
        actionAICompletions->setChecked(checked);
    });

    connect(findReplaceBar, &FindReplaceBar::replaceAllComplete, this, [](int count) {
        qDebug() << "Replace all complete:" << count;
    });

    // Hide the native menu bar — menus are accessible via the unified title bar dropdown
    ui->menubar->hide();

    connect(projectSearchPanel, &ProjectSearchPanel::resultActivated, this, [this](const QString &filePath, int line, int column) {
        QModelIndex index = fileModel->index(filePath);
        if (index.isValid())
            on_fileTreeView_clicked(index);
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor) {
            QTextBlock block = editor->document()->findBlockByNumber(line - 1);
            QTextCursor cursor(block);
            if (column > 0)
                cursor.setPosition(block.position() + column);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        }
    });

    // LSP connections
    connect(lspClient, &LspClient::diagnosticsReceived, this, &MainWindow::onDiagnosticsReceived);
    connect(lspClient, &LspClient::serverStarted, this, []() {
        qDebug() << "LSP server started";
    });
    connect(lspClient, &LspClient::serverFailed, this, [](const QString &err) {
        qDebug() << "LSP server failed:" << err;
    });
    connect(lspClient, &LspClient::logMessage, this, [](const QString &msg) {
        qDebug() << "LSP:" << msg;
    });

    // LSP response handlers (definition / declaration / typeDefinition / implementation / references)
    auto jumpToLocation = [this](const QJsonArray &locations) {
        if (locations.isEmpty())
            return;
        QJsonObject loc = locations.first().toObject();
        QString path = QUrl(loc["uri"].toString()).toLocalFile();
        QJsonObject start = loc["range"].toObject()["start"].toObject();
        int line = start["line"].toInt();
        int character = start["character"].toInt();
        if (!path.isEmpty() && path != currentFile)
            openFileInTab(path);
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor)
            return;
        QTextBlock block = editor->document()->findBlockByNumber(line);
        if (!block.isValid())
            return;
        QTextCursor cursor(editor->document());
        cursor.setPosition(block.position() + character);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    };
    connect(lspClient, &LspClient::definitionReceived, this, jumpToLocation);
    connect(lspClient, &LspClient::declarationReceived, this, jumpToLocation);
    connect(lspClient, &LspClient::typeDefinitionReceived, this, jumpToLocation);
    connect(lspClient, &LspClient::implementationReceived, this, jumpToLocation);
    connect(lspClient, &LspClient::referencesReceived, this, [this, jumpToLocation](const QJsonArray &locations, int) {
        jumpToLocation(locations);
    });

    connect(lspClient, &LspClient::formattingReceived, this, [this](const QString &, const QJsonArray &edits) {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || edits.isEmpty())
            return;
        QTextDocument *doc = editor->document();
        struct Edit { QTextCursor cursor; QString text; };
        QList<Edit> toApply;
        for (const QJsonValue &v : edits) {
            QJsonObject e = v.toObject();
            QJsonObject r = e["range"].toObject();
            int sl = r["start"].toObject()["line"].toInt();
            int sc = r["start"].toObject()["character"].toInt();
            int el = r["end"].toObject()["line"].toInt();
            int ec = r["end"].toObject()["character"].toInt();
            QTextBlock sBlock = doc->findBlockByNumber(sl);
            QTextBlock eBlock = doc->findBlockByNumber(el);
            if (!sBlock.isValid() || !eBlock.isValid())
                continue;
            QTextCursor c(doc);
            c.setPosition(sBlock.position() + sc);
            c.setPosition(eBlock.position() + ec, QTextCursor::KeepAnchor);
            toApply.append({c, e["newText"].toString()});
        }
        for (int i = toApply.size() - 1; i >= 0; --i)
            toApply[i].cursor.insertText(toApply[i].text);
    });

    connect(lspClient, &LspClient::documentSymbolReceived, this,
            [this](const QList<LspClient::SymbolInformation> &symbols) {
        if (symbols.isEmpty())
            return;
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Document Symbols"));
        QVBoxLayout *lay = new QVBoxLayout(&dlg);
        QListWidget *list = new QListWidget(&dlg);
        for (const LspClient::SymbolInformation &s : symbols)
            list->addItem(QString("%1  (%2)").arg(s.name, s.kind));
        lay->addWidget(list);
        connect(list, &QListWidget::itemDoubleClicked, &dlg, [&dlg]() { dlg.accept(); });
        if (dlg.exec() == QDialog::Accepted && list->currentRow() >= 0) {
            const LspClient::SymbolInformation &s = symbols[list->currentRow()];
            CodeEditor *editor = getCurrentCodeEditor();
            if (editor) {
                QTextBlock block = editor->document()->findBlockByNumber(s.range.start.line);
                if (block.isValid()) {
                    QTextCursor cursor(editor->document());
                    cursor.setPosition(block.position() + s.range.start.character);
                    editor->setTextCursor(cursor);
                    editor->centerCursor();
                }
            }
        }
    });

    connect(lspClient, &LspClient::completionReceived, this, &MainWindow::onCompletionReceived);

    connect(lspClient, &LspClient::hoverReceived, this, [this](const QJsonObject &hover, int) {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor)
            return;
        auto extractHover = [](const QJsonValue &v) -> QString {
            if (v.isString()) return v.toString();
            if (v.isObject()) {
                QJsonObject o = v.toObject();
                if (o.contains("value")) return o["value"].toString();
            }
            return QString();
        };
        QString text;
        QJsonValue cval = hover.value("contents");
        if (cval.isUndefined()) cval = QJsonValue(hover);
        if (cval.isArray()) {
            QStringList parts;
            for (const QJsonValue &e : cval.toArray()) {
                QString t = extractHover(e);
                if (!t.isEmpty()) parts << t;
            }
            text = parts.join("\n\n");
        } else {
            text = extractHover(cval);
        }
        text = text.trimmed();
        if (text.isEmpty())
            return;
        QToolTip::showText(editor->mapToGlobal(editor->cursorRect().bottomLeft()), text, editor);
    });

    connect(lspClient, &LspClient::signatureHelpReceived, this, [this](const QJsonObject &help, int) {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor)
            return;
        QJsonArray sigs = help.value("signatures").toArray();
        QString text = sigs.isEmpty() ? QString()
                                       : sigs.first().toObject().value("label").toString();
        if (!text.isEmpty())
            QToolTip::showText(editor->mapToGlobal(editor->cursorRect().bottomLeft()), text, editor);
    });

    // DAP connections
    debugPanel->setClient(dapClient);
    connect(dapClient, &DapClient::initialized, this, &MainWindow::onDapInitialized);
    connect(dapClient, &DapClient::stopped, this, &MainWindow::onDapStopped);
    connect(dapClient, &DapClient::continued, this, &MainWindow::onDapContinued);
    connect(dapClient, &DapClient::stackTraceReceived, this, &MainWindow::onStackTraceReceived);
    connect(dapClient, &DapClient::scopesReceived, this, &MainWindow::onScopesReceived);
    connect(dapClient, &DapClient::variablesReceived, this, &MainWindow::onVariablesReceived);
    connect(dapClient, &DapClient::evaluationReceived, this, [this](const QString &expr, const QString &result, const QString &ctx) {
        if (ctx == "watch")
            debugPanel->showWatchEvaluation(expr, result);
        else
            debugPanel->showEvaluation(expr, result);
    });
    connect(debugPanel, &DebugPanel::frameActivated, this, [this](int frameId) {
        if (frameId >= 0) {
            m_currentFrameId = frameId;
            dapClient->scopes(frameId);
        }
    });
    connect(debugPanel, &DebugPanel::evaluateRequested, this, [this](const QString &expr) {
        if (m_isDebugging) dapClient->evaluate(expr, m_currentFrameId);
    });
    connect(debugPanel, &DebugPanel::watchRequested, this, [this](const QString &expr) {
        if (m_isDebugging) dapClient->evaluate(expr, m_currentFrameId, "watch");
    });
    connect(dapClient, &DapClient::logMessage, this, &MainWindow::onDapLogMessage);
    connect(dapClient, &DapClient::serverFailed, this, [](const QString &err) {
        qDebug() << "DAP server failed:" << err;
    });
    connect(dapClient, &DapClient::serverStopped, this, &MainWindow::stopDebug);

    // Problem panel connections
    connect(problemPanel, &ProblemPanel::problemActivated, this, &MainWindow::onProblemActivated);
    connect(problemPanel, &ProblemPanel::filterChanged, this, &MainWindow::onProblemsFilterChanged);

    // Updater connections
    connect(updater, &Updater::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(updater, &Updater::updateCheckFailed, this, &MainWindow::onUpdateCheckFailed);

    // AI inline completion wiring
    m_aiInline->setSettings(
        QSettings().value("ai/provider", "ollama").toString(),
        QSettings().value("ai/endpoint", "http://localhost:11434/api/chat").toString(),
        QSettings().value("ai/model", "codellama").toString(),
        QSettings().value("ai/enabled", false).toBool(),
        QSettings().value("ai/debounceMs", 400).toInt(),
        QSettings().value("ai/apiKey", {}).toString()
    );

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index >= 0 && index < ui->tabWidget->count()) {
            if (CodeEditor *ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(index))) {
                m_aiInline->setEditor(ce);
                m_codeActionCtrl->attach(ce, lspClient, QUrl::fromLocalFile(currentFile).toString());
                if (m_codeActionCtrl->isVisible())
                    m_codeActionCtrl->showCurrent();
                todoPanel->parseDocument(ce->toPlainText(), currentFile);
            }
        }
    });

    // CodeAction controller
    connect(lspClient, &LspClient::codeActionReceived, m_codeActionCtrl, &CodeActionController::onCodeActionReceived);
    connect(lspClient, &LspClient::diagnosticsReceived, m_codeActionCtrl, &CodeActionController::onDiagnosticsReceived);
    connect(m_codeActionCtrl, &CodeActionController::actionTriggered, this, [](const QString &title, const QString &kind, int) {
        qDebug() << "CodeAction triggered:" << title << kind;
    });

    // Todo panel: jump to TODO comment line
    connect(todoPanel, &TodoPanel::todoActivated, this, [this](int line) {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor)
            return;
        QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cursor(editor->document());
            cursor.setPosition(block.position());
            editor->setTextCursor(cursor);
            editor->centerCursor();
            editor->setFocus();
        }
    });

    // HTTP Client - add bottom panel tab
    int httpIdx = bottomPanelTabs->addTab(tr("HTTP"));
    bottomPanelStack->addWidget(m_httpClient);
    m_httpClient->hide();

    int sqliteIdx = bottomPanelTabs->addTab(tr("Database"));
    bottomPanelStack->addWidget(m_sqliteViewer);
    m_sqliteViewer->hide();
    connect(m_httpClient, &HttpClientPanel::requestSent, this, [](const QString &url, int code) {
        qDebug() << "HTTP request:" << url << "->" << code;
    });

    // Plugin registry
    m_pluginRegistry->setRegistryUrl(QUrl(registryUrl));
    m_pluginRegistry->setCheckInterval(7);
    QTimer::singleShot(5000, m_pluginRegistry, &PluginRegistry::checkForUpdates);
    m_pluginRegistry->startPeriodicCheck();

    connect(m_pluginRegistry, &PluginRegistry::registryUpdated, this, [this](const QJsonObject &manifest) {
        qDebug() << "Plugin registry updated:" << manifest["version"].toString();
        const QJsonArray plugins = manifest["plugins"].toArray();
        for (const QJsonValue &v : plugins) {
            if (!v.isObject())
                continue;
            QJsonObject obj = v.toObject();
            QString id = obj["id"].toString();
            QString latest = obj["version"].toString();
            QString current = pluginManager->pluginVersion(id);
            if (!id.isEmpty() && !latest.isEmpty() && !current.isEmpty() && latest != current)
                emit m_pluginRegistry->updateAvailable(id, latest);
        }
    });
    connect(m_pluginRegistry, &PluginRegistry::pluginDownloaded, this, [this](const QString &pluginId, const QByteArray &data) {
        if (pluginManager->installPluginFromArchive(pluginId, data, this))
            statusBar()->showMessage(tr("Plugin '%1' installed.").arg(pluginId));
        else
            statusBar()->showMessage(tr("Failed to install plugin '%1'.").arg(pluginId));
    });
    connect(m_pluginRegistry, &PluginRegistry::installFailed, this, [this](const QString &pluginId, const QString &error) {
        statusBar()->showMessage(tr("Plugin install failed for '%1': %2").arg(pluginId, error));
    });

    // Config validator - validate settings on startup
    configValidator->resetInvalidSettings();

    setSidebarCollapsed(settings.value("ui/sidebarCollapsed", true).toBool());

    // Restore window geometry and state
    if (settings.contains("mainWindow/geometry")) {
        restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    }
    if (settings.contains("mainWindow/state")) {
        restoreState(settings.value("mainWindow/state").toByteArray());
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

QPlainTextEdit* MainWindow::getCurrentEditor()
{
    return qobject_cast<QPlainTextEdit*>(ui->tabWidget->currentWidget());
}

CodeEditor* MainWindow::getCurrentCodeEditor()
{
    return qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
}

QPushButton* MainWindow::createTabCloseButton(int tabIndex)
{
    QPushButton *closeBtn = new QPushButton();
    closeBtn->setIcon(ThemeIcons::instance()->icon(":/icons/close.svg"));
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this, tabIndex]() {
        on_tabWidget_tabCloseRequested(tabIndex);
    });
    return closeBtn;
}

 QWidget* MainWindow::createWelcomeWidget()
 {
     QWidget *widget = new QWidget(this);

     // App icon
     QLabel *iconLabel = new QLabel(widget);
     iconLabel->setPixmap(QIcon(":/icons/app-icon.svg").pixmap(64, 64));
     iconLabel->setAlignment(Qt::AlignCenter);

     QLabel *titleLabel = new QLabel(tr("Welcome to Scriptura"), widget);
     titleLabel->setObjectName("welcomeTitle");
     titleLabel->setAlignment(Qt::AlignCenter);
     QFont titleFont = titleLabel->font();
     titleFont.setPointSize(28);
     titleFont.setBold(true);
     titleLabel->setFont(titleFont);

     QLabel *descriptionLabel = new QLabel(tr("Open a project or create a new file to start editing."), widget);
     descriptionLabel->setAlignment(Qt::AlignCenter);
     QFont descFont = descriptionLabel->font();
     descFont.setPointSize(11);
     descriptionLabel->setFont(descFont);
     descriptionLabel->setStyleSheet("color: palette(mid);");

     QPushButton *openProjectButton = new QPushButton(ThemeIcons::instance()->icon(":/icons/folder.svg"), tr("Open Project"), widget);
     openProjectButton->setObjectName("primaryButton");
     openProjectButton->setMinimumWidth(160);

     QPushButton *newFileButton = new QPushButton(tr("New File"), widget);
     newFileButton->setObjectName("primaryButton");
     newFileButton->setMinimumWidth(160);

     QHBoxLayout *buttonLayout = new QHBoxLayout();
     buttonLayout->addStretch();
     buttonLayout->addWidget(openProjectButton);
     buttonLayout->addSpacing(12);
     buttonLayout->addWidget(newFileButton);
     buttonLayout->addStretch();

     QFrame *recentProjectsFrame = new QFrame(widget);
     recentProjectsFrame->setObjectName("recentProjectsFrame");
     recentProjectsFrame->setFrameShape(QFrame::StyledPanel);
     recentProjectsFrame->setFrameShadow(QFrame::Sunken);
     recentProjectsLayout = new QVBoxLayout(recentProjectsFrame);
     recentProjectsLayout->setSpacing(6);
     QLabel *recentLabel = new QLabel(tr("<b>Recent Projects:</b>"), recentProjectsFrame);
     recentProjectsLayout->addWidget(recentLabel);

     updateRecentProjectsOnWelcome();

     QVBoxLayout *layout = new QVBoxLayout(widget);
     layout->setSpacing(16);
     layout->addStretch();
     layout->addWidget(iconLabel);
     layout->addWidget(titleLabel);
     layout->addSpacing(8);
     layout->addWidget(descriptionLabel);
     layout->addSpacing(32);
     layout->addLayout(buttonLayout);
     layout->addSpacing(40);
     layout->addWidget(recentProjectsFrame);
     layout->addSpacing(24);
     layout->addWidget(createKeyboardShortcutsWidget());
     layout->addStretch();

    connect(openProjectButton, &QPushButton::clicked, this, &MainWindow::on_action_open_project_triggered);
    connect(newFileButton, &QPushButton::clicked, this, [this]() {
        on_action_add_file_directory_triggered();
    });

    return widget;
}

QWidget* MainWindow::createKeyboardShortcutsWidget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *shortcutsLabel = new QLabel(tr(
        "<b>Keyboard Shortcuts:</b><br>"
        "Ctrl+S: Save<br>"
        "Ctrl+Shift+S: Save As<br>"
        "Ctrl+Z: Undo<br>"
        "Ctrl+Y: Redo<br>"
        "Ctrl+F: Find<br>"
        "Ctrl+H: Replace<br>"
        "Ctrl+G: Find Next<br>"
        "Ctrl+Shift+P: Command Palette<br>"
        "Ctrl+K: Keyboard Shortcuts"
    ), widget);
    shortcutsLabel->setAlignment(Qt::AlignCenter);
    shortcutsLabel->setOpenExternalLinks(false);
    layout->addWidget(shortcutsLabel);

    return widget;
}

QWidget* MainWindow::createThemeSettingsWidget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(widget);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    QGroupBox *themeGroup = new QGroupBox(tr("Theme"), widget);
    QGridLayout *themeGrid = new QGridLayout(themeGroup);
    themeGrid->setSpacing(6);
    QButtonGroup *themeBtnGroup = new QButtonGroup(themeGroup);

    struct FamilyEntry {
        ThemeColorFamily family;
        QString name;
    };

    FamilyEntry families[] = {
        {ThemeColorFamily::Default, "Default"},
        {ThemeColorFamily::Blue,    "Blue"},
        {ThemeColorFamily::Green,   "Green"},
        {ThemeColorFamily::Red,     "Red"},
        {ThemeColorFamily::Yellow,  "Yellow"},
        {ThemeColorFamily::Brown,   "Brown"},
        {ThemeColorFamily::Cyan,    "Cyan"},
        {ThemeColorFamily::Violet,  "Violet"}
    };

    QMap<QPushButton*, Theme> themeMap;

    int btnIdx = 0;
    for (int fi = 0; fi < 8; ++fi) {
        for (int mi = 0; mi < 2; ++mi) {
            for (int hci = 0; hci < 2; ++hci) {
                ThemeMode mode = static_cast<ThemeMode>(mi);
                ThemeFeatures features = hci ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures();
                Theme t(static_cast<ThemeColorFamily>(fi), mode, features);

                QString label = families[fi].name + " – " + (mode == ThemeMode::Light ? tr("Light") : tr("Dark"));
                if (hci)
                    label += " (" + tr("High Contrast") + ")";

                QPushButton *btn = new QPushButton(label, themeGroup);
                btn->setCheckable(true);
                btn->setCursor(Qt::PointingHandCursor);

                QPalette p = buildBasePalette(t.family, t.mode);
                if (features.testFlag(ThemeFeature::HighContrast))
                    applyHighContrastPalette(p, t.family, t.mode);

                QColor bg = p.color(QPalette::Window);
                QColor textColor = p.color(QPalette::WindowText);
                btn->setStyleSheet(QString("background-color: %1; color: %2; padding: 8px; border: 1px solid %3; border-radius: 4px; text-align: left;")
                                   .arg(bg.name()).arg(textColor.name()).arg(textColor.name()));

                themeGrid->addWidget(btn, btnIdx / 4, btnIdx % 4);
                themeBtnGroup->addButton(btn, btnIdx);
                themeMap.insert(btn, t);

                if (t == selectedTheme)
                    btn->setChecked(true);

                ++btnIdx;
            }
        }
    }

    mainLayout->addWidget(themeGroup);
    mainLayout->addStretch();

    connect(themeBtnGroup, static_cast<void (QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, [this, themeMap](QAbstractButton *button) {
        QPushButton *btn = qobject_cast<QPushButton*>(button);
        if (!btn || !themeMap.contains(btn))
            return;
        Theme t = themeMap.value(btn);
        if (t != selectedTheme) {
            selectedTheme = t;
            applyTheme(selectedTheme);
            QSettings settings;
            settings.setValue("theme/selected", themeToLegacyInt(selectedTheme));
        }
    });

    return widget;
}

QWidget* MainWindow::createEditorSettingsWidget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(widget);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());

    // Font settings
    QGroupBox *fontGroup = new QGroupBox(tr("Font"), widget);
    QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);
    
    QHBoxLayout *fontNameLayout = new QHBoxLayout();
    QLabel *fontLabel = new QLabel(tr("Font Family:"), widget);
    QFontComboBox *fontCombo = new QFontComboBox(widget);
    fontCombo->setFontFilters(QFontComboBox::ScalableFonts);
    if (editor) fontCombo->setCurrentFont(editor->font());
    fontNameLayout->addWidget(fontLabel);
    fontNameLayout->addWidget(fontCombo, 1);
    fontLayout->addLayout(fontNameLayout);
    
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel(tr("Size:"), widget);
    QSpinBox *sizeSpin = new QSpinBox(widget);
    sizeSpin->setRange(8, 72);
    if (editor) sizeSpin->setValue(editor->font().pointSize());
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeSpin);
    sizeLayout->addStretch();
    fontLayout->addLayout(sizeLayout);
    
    mainLayout->addWidget(fontGroup);

    // Editor behavior
    QGroupBox *behaviorGroup = new QGroupBox(tr("Behavior"), widget);
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);
    
    QHBoxLayout *tabLayout = new QHBoxLayout();
    QLabel *tabLabel = new QLabel(tr("Tab Width:"), widget);
    QSpinBox *tabSpin = new QSpinBox(widget);
    tabSpin->setRange(1, 16);
    if (editor) tabSpin->setValue(editor->tabWidth());
    tabLayout->addWidget(tabLabel);
    tabLayout->addWidget(tabSpin);
    tabLayout->addStretch();
    behaviorLayout->addLayout(tabLayout);
    
    QCheckBox *wordWrapCheckbox = new QCheckBox(tr("Word Wrap"), widget);
    if (editor) wordWrapCheckbox->setChecked(editor->lineWrapMode() != QPlainTextEdit::NoWrap);
    behaviorLayout->addWidget(wordWrapCheckbox);
    
    QCheckBox *indentGuidesCheckbox = new QCheckBox(tr("Show Indent Guides"), widget);
    if (editor) indentGuidesCheckbox->setChecked(editor->property("showIndentGuides").toBool());
    behaviorLayout->addWidget(indentGuidesCheckbox);
    
    mainLayout->addWidget(behaviorGroup);

    // Display
    QGroupBox *displayGroup = new QGroupBox(tr("Display"), widget);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);
    
    QCheckBox *lineNumbersCheckbox = new QCheckBox(tr("Show Line Numbers"), widget);
    if (editor) lineNumbersCheckbox->setChecked(editor->property("showLineNumbers").toBool());
    displayLayout->addWidget(lineNumbersCheckbox);
    
    QHBoxLayout *widthLayout = new QHBoxLayout();
    QLabel *widthLabel = new QLabel(tr("Editor Width:"), widget);
    QSpinBox *widthSpin = new QSpinBox(widget);
    widthSpin->setRange(200, 2000);
    widthSpin->setSuffix(" px");
    if (editor) widthSpin->setValue(editor->minimumWidth());
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(widthSpin);
    widthLayout->addStretch();
    displayLayout->addLayout(widthLayout);
    
    mainLayout->addWidget(displayGroup);
    mainLayout->addStretch();

    // Apply settings immediately when changed
    auto applySettings = [&]() {
        QSettings settings;

        QFont font = fontCombo->currentFont();
        font.setPointSize(sizeSpin->value());
        settings.setValue("editor/font", font);

        int tabWidth = tabSpin->value();
        settings.setValue("editor/tabWidth", tabWidth);

        int editorWidth = widthSpin->value();
        settings.setValue("editor/width", editorWidth);
        
        bool wordWrap = wordWrapCheckbox->isChecked();
        settings.setValue("editor/wordWrap", wordWrap);
        
        bool showIndentGuides = indentGuidesCheckbox->isChecked();
        settings.setValue("editor/showIndentGuides", showIndentGuides);
        
        bool showLineNumbers = lineNumbersCheckbox->isChecked();
        settings.setValue("editor/showLineNumbers", showLineNumbers);

        for (int i = 0; i < ui->tabWidget->count(); i++) {
            if (CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                ed->setFont(font);
                ed->setTabWidth(tabWidth);
                if (editorWidth > 0)
                    ed->setMinimumWidth(editorWidth);
                ed->setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
                ed->setProperty("showIndentGuides", showIndentGuides);
                ed->setProperty("showLineNumbers", showLineNumbers);
            }
        }
    };

    connect(fontCombo, &QFontComboBox::currentFontChanged, widget, [applySettings](const QFont &) { applySettings(); });
    connect(sizeSpin, &QSpinBox::valueChanged, widget, [applySettings](int) { applySettings(); });
    connect(tabSpin, &QSpinBox::valueChanged, widget, [applySettings](int) { applySettings(); });
    connect(wordWrapCheckbox, &QCheckBox::toggled, widget, [applySettings](bool) { applySettings(); });
    connect(indentGuidesCheckbox, &QCheckBox::toggled, widget, [applySettings](bool) { applySettings(); });
    connect(lineNumbersCheckbox, &QCheckBox::toggled, widget, [applySettings](bool) { applySettings(); });
    connect(widthSpin, &QSpinBox::valueChanged, widget, [applySettings](int) { applySettings(); });

    return widget;
}

QWidget* MainWindow::createKeyboardShortcutsPageWidget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(16);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel *titleLabel = new QLabel(tr("<b>Keyboard Shortcuts</b>"), widget);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel *shortcutsLabel = new QLabel(tr(
        "<b>File Operations:</b><br>"
        "Ctrl+N: New File &nbsp;&nbsp; "
        "Ctrl+S: Save &nbsp;&nbsp; "
        "Ctrl+Shift+S: Save As &nbsp;&nbsp; "
        "Ctrl+O: Open File<br><br>"
        "<b>Edit Operations:</b><br>"
        "Ctrl+Z: Undo &nbsp;&nbsp; "
        "Ctrl+Y: Redo &nbsp;&nbsp; "
        "Ctrl+X: Cut &nbsp;&nbsp; "
        "Ctrl+C: Copy &nbsp;&nbsp; "
        "Ctrl+V: Paste<br><br>"
        "<b>View/Navigation:</b><br>"
        "Ctrl+F: Find &nbsp;&nbsp; "
        "Ctrl+H: Replace &nbsp;&nbsp; "
        "Ctrl+G: Find Next &nbsp;&nbsp; "
        "Ctrl+Shift+G: Find Previous &nbsp;&nbsp; "
        "Ctrl+Shift+F: Project Search &nbsp;&nbsp; "
        "Ctrl+Shift+P: Command Palette &nbsp;&nbsp; "
        "Ctrl+K: Keyboard Shortcuts<br><br>"
        "<b>Project:</b><br>"
        "Ctrl+E: Project Settings &nbsp;&nbsp; "
        "Ctrl+T: Theme Selection"
    ), widget);
    shortcutsLabel->setTextFormat(Qt::RichText);
    shortcutsLabel->setAlignment(Qt::AlignLeft);
    shortcutsLabel->setWordWrap(true);
    layout->addWidget(shortcutsLabel);

    layout->addStretch();

    return widget;
}

QWidget* MainWindow::createUpdaterSettingsWidget()
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(16);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel *titleLabel = new QLabel(tr("<b>Application Updates</b>"), widget);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel *descriptionLabel = new QLabel(tr(
        "Check for Scriptura updates from GitHub Releases.<br>"
        "You can check for the latest stable release or the latest pre-release."
    ), widget);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

     QGroupBox *checkGroup = new QGroupBox(tr("Check for Updates"), widget);
     QVBoxLayout *checkLayout = new QVBoxLayout(checkGroup);

     QPushButton *checkStableButton = new QPushButton(tr("Check for Latest Release (Recommended)"), checkGroup);
     QPushButton *checkPreReleaseButton = new QPushButton(tr("Check for Latest Pre-release"), checkGroup);
     checkPreReleaseButton->setToolTip(tr("Pre-releases may be unstable. Update at your own risk."));

     checkLayout->addWidget(checkStableButton);
     checkLayout->addWidget(checkPreReleaseButton);

     layout->addWidget(checkGroup);

     QGroupBox *registryGroup = new QGroupBox(tr("Plugin Registry"), widget);
     QVBoxLayout *registryLayout = new QVBoxLayout(registryGroup);

     QLabel *registryUrlLabel = new QLabel(tr("Registry URL:"), registryGroup);
     QLineEdit *registryUrlEdit = new QLineEdit(registryGroup);
     registryUrlEdit->setPlaceholderText(tr("https://example.com/plugin-registry.json"));
     registryUrlEdit->setText(registryUrl);
     connect(registryUrlEdit, &QLineEdit::textChanged, this, [this](const QString &url) {
         m_pluginRegistry->setRegistryUrl(QUrl(url));
         QSettings().setValue("plugin/registryUrl", url);
     });
     registryLayout->addWidget(registryUrlLabel);
     registryLayout->addWidget(registryUrlEdit);

     layout->addWidget(registryGroup);

     QGroupBox *infoGroup = new QGroupBox(tr("Current Version"), widget);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);

    QLabel *versionLabel = new QLabel(tr("Version: %1").arg(SCRIPTURA_VERSION), infoGroup);
    infoLayout->addWidget(versionLabel);

    layout->addWidget(infoGroup);
    layout->addStretch();

    // Connect buttons
    connect(checkStableButton, &QPushButton::clicked, this, [this]() {
        updater->checkForUpdates(Updater::Stable);
    });
    connect(checkPreReleaseButton, &QPushButton::clicked, this, [this]() {
        // Show warning for pre-release
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("Pre-release Warning"),
            tr("Pre-releases may be unstable and contain bugs.\n"
               "They are intended for testing new features before the stable release.\n\n"
               "Do you want to continue?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            updater->checkForUpdates(Updater::PreRelease);
        }
    });

    return widget;
}

void MainWindow::on_action_check_updates_triggered()
{
    // Show updater settings tab
    for (int i = 0; i < tabBar->count(); ++i) {
        if (static_cast<TabType>(tabBar->tabData(i).toInt()) == TabType::UpdaterSettings) {
            tabBar->setCurrentIndex(i);
            return;
        }
    }
    // If updater tab doesn't exist yet, add it
    int updaterSettingsTabIndex = tabBar->addTab(tr("Updates"));
    tabBar->setTabData(updaterSettingsTabIndex, static_cast<int>(TabType::UpdaterSettings));
    tabBar->setTabButton(updaterSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(updaterSettingsTabIndex));
    tabBar->setCurrentIndex(updaterSettingsTabIndex);
}

void MainWindow::showWelcomeScreen()
{
    editorStack->setCurrentWidget(welcomeWidget);
    tabBar->hide();
    findReplaceBar->setVisible(false);
}

void MainWindow::showEditorInterface()
{
    editorStack->setCurrentWidget(ui->tabWidget);
    updateTabBarVisibility();
}

void MainWindow::updateCursorPosition()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor) {
        QTextCursor cursor = editor->textCursor();
        int line = cursor.blockNumber() + 1;
        int column = cursor.positionInBlock() + 1;
        ui->statusbar->showMessage(QString("Line %1, Column %2").arg(line).arg(column));
    }
}

void MainWindow::on_action_open_project_triggered()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Open Project"), QString(),
        QFileDialog::DontUseNativeDialog);
    if (dirName.isEmpty())
        return;

    if (!recentProjects.contains(dirName)) {
        recentProjects.prepend(dirName);
        while (recentProjects.size() > maxRecentProjects)
            recentProjects.removeLast();
        saveRecentProjects();
        updateRecentProjectsOnWelcome();
    }

    loadProjectDirectory(dirName);
}

void MainWindow::loadProjectDirectory(const QString &dirName)
{
    projectDir = dirName;
    rootIndex = fileModel->index(projectDir);
    ui->fileTreeView->setRootIndex(rootIndex);
    ui->fileTreeView->hideColumn(1);
    ui->fileTreeView->hideColumn(2);
    ui->fileTreeView->hideColumn(3);
    // Disable goUpButton to restrict access to other directories when project is opened
    goUpButton->setEnabled(false);

    autoSaveTimer->start(30000);

    setWindowTitle(QFileInfo(projectDir).fileName() + " - Scriptura");

    // Initialize LSP for the project directory
    QString rootUri = QUrl::fromLocalFile(projectDir).toString();
    if (!lspClient->isRunning()) {
        // Try to start a language server for the project
        startLanguageServerForProject(projectDir);
    }
    problemPanel->clearAll();
    problemPanel->setCurrentFile(QString()); // Show all problems
    problemPanel->show();
    problemsButton->setChecked(true);

    // Update terminal working directory
    if (terminalPanel) {
        terminalPanel->setWorkingDirectory(projectDir);
    }

    // Update git panel project path (enables staging/diff/branches)
    gitPanel->setProjectPath(projectDir);
}

void MainWindow::on_action_save_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;
        
    if (currentFile.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), 
            projectDir.isEmpty() ? QString() : projectDir,
            tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
        if (fileName.isEmpty())
            return;
        currentFile = fileName;
    }

    QFile file(currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString errorMsg;
        QString errorStr = file.errorString();
        if (errorStr.contains("Permission", Qt::CaseInsensitive)) {
            errorMsg = tr("Permission denied. Please check file permissions.");
        } else if (errorStr.contains("disk", Qt::CaseInsensitive) || errorStr.contains("space", Qt::CaseInsensitive)) {
            errorMsg = tr("Disk full. Cannot save file.");
        } else if (currentFile.contains("://")) {
            errorMsg = tr("Network path unavailable. Please check connection.");
        } else {
            errorMsg = tr("Cannot open file for writing: %1").arg(errorStr);
        }
        QMessageBox::warning(this, tr("Error"), errorMsg);
        return;
    }

    // Check disk space before writing
    QStorageInfo storage(QFileInfo(currentFile).absolutePath());
    qint64 contentSize = editor->toPlainText().toUtf8().size();
    if (storage.bytesAvailable() < contentSize * 2) {
        QMessageBox::warning(this, tr("Warning"), 
            tr("Low disk space. Available: %1 MB").arg(storage.bytesAvailable() / (1024 * 1024)));
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();
    
    for (OpenFile &f : openFiles) {
        if (f.filePath == currentFile) {
            f.modified = false;
            break;
        }
    }
    
    setWindowTitle(QFileInfo(currentFile).fileName() + " - Scriptura");
}

void MainWindow::on_action_save_as_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;
        
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File As"), 
        currentFile.isEmpty() ? (projectDir.isEmpty() ? QString() : projectDir) : currentFile,
        tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file for writing: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();
    
    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].filePath == currentFile) {
            openFiles[i].filePath = fileName;
            openFiles[i].fileName = QFileInfo(fileName).fileName();
            openFiles[i].modified = false;
            ui->tabWidget->setTabText(i, openFiles[i].fileName);
            break;
        }
    }
    
    currentFile = fileName;
    setWindowTitle(QFileInfo(fileName).fileName() + " - Scriptura");
}

void MainWindow::on_action_open_file_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
        projectDir.isEmpty() ? QDir::homePath() : projectDir,
        tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    openFileInTab(fileName);
}

void MainWindow::openFileInTab(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString errorMsg;
        QString errorStr = file.errorString();
        if (errorStr.contains("Permission", Qt::CaseInsensitive)) {
            errorMsg = tr("Permission denied. Please check file permissions.");
        } else if (fileName.contains("://")) {
            errorMsg = tr("Network path unavailable. Please check connection.");
        } else {
            errorMsg = tr("Cannot open file: %1").arg(errorStr);
        }
        QMessageBox::warning(this, tr("Error"), errorMsg);
        return;
    }

    // Check file size and warn for large files
    qint64 fileSize = file.size();
    if (fileSize > 10 * 1024 * 1024) { // 10MB
        QMessageBox::warning(this, tr("Large File"),
            tr("This file is %1 MB. Opening large files may impact performance.").arg(fileSize / (1024 * 1024)));
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].filePath == fileName) {
            showEditorInterface();
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    CodeEditor *editor = new CodeEditor(this);
    editor->setLanguageForFile(fileName);
    editor->installEventFilter(this);
    connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
    QSettings settings;
    QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
    editor->setFont(savedFont);
    editor->setTabWidth(settings.value("editor/tabWidth", editor->tabWidth()).toInt());
    int w = settings.value("editor/width", 0).toInt();
    if (w > 0) editor->setMinimumWidth(w);
    editor->setPlainText(content);

    // Create minimap for this editor
    Minimap *minimap = new Minimap(editor, this);
    minimap->setDocument(editor->document());
    connect(minimap, &Minimap::viewportRequested, editor, [editor](int position) {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, position);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    });

    // Create breadcrumb for this editor
    Breadcrumb *breadcrumb = new Breadcrumb(editor, this);
    breadcrumb->setFilePath(fileName);
    connect(breadcrumb, &Breadcrumb::breadcrumbClicked, this, [this](const QString &path) {
        // Handle breadcrumb navigation
    });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, breadcrumb, &Breadcrumb::updateFromCursor);

    int tabIndex = openFiles.size();
    connect(editor, &QPlainTextEdit::modificationChanged, this,
            [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (lspClient->isRunning())
            m_hoverTimer->start();
    });

    openFiles.append({fileName, QFileInfo(fileName).fileName(), false});
    showEditorInterface();
    ui->tabWidget->addTab(editor, QFileInfo(fileName).fileName());
    int tabBarIndex = tabBar->addTab(QFileInfo(fileName).fileName());
    tabBar->setTabData(tabBarIndex, fileName);
    tabBar->setTabToolTip(tabBarIndex, fileName);
    tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createTabCloseButton(tabBarIndex));
    ui->tabWidget->setCurrentWidget(editor);
    tabBar->setCurrentIndex(tabBarIndex);
    currentFile = fileName;
    setWindowTitle(QFileInfo(fileName).fileName() + " - Scriptura");
    showSearchBar(true);
    addRecentFile(fileName);
}

void MainWindow::on_actionCu_t_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor)
        editor->cut();
}

void MainWindow::on_action_copy_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor)
        editor->copy();
}

void MainWindow::on_action_Paste_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor)
        editor->paste();
}

void MainWindow::on_action_Undo_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor)
        editor->undo();
}

void MainWindow::on_action_Redo_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor)
        editor->redo();
}

void MainWindow::on_action_add_file_directory_triggered()
{
    if (projectDir.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please open a project first."));
        return;
    }
    
    QMenu menu(this);
    QAction *addFileAction = menu.addAction(tr("Add File..."));
    QAction *addDirAction = menu.addAction(tr("Add Directory..."));
    
    QPoint pos = mapFromGlobal(QCursor::pos());
    QAction *selected = menu.exec(mapToGlobal(pos));
    
    if (selected == addFileAction) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Create New File"), projectDir, tr("All Files (*)"));
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.close();
                QMessageBox::information(this, tr("Success"), tr("File created: %1").arg(fileName));
            }
        }
    } else if (selected == addDirAction) {
        QString dirName = QInputDialog::getText(this, tr("Create Directory"), tr("Directory name:"));
        if (!dirName.isEmpty()) {
            QDir dir(projectDir);
            QString fullPath = projectDir + QDir::separator() + dirName;
            if (dir.mkdir(dirName)) {
                QMessageBox::information(this, tr("Success"), tr("Directory created: %1").arg(fullPath));
            } else {
                QMessageBox::warning(this, tr("Error"), tr("Failed to create directory."));
            }
        }
    }
}

void MainWindow::on_action_delete_file_directory_triggered()
{
    QModelIndex index = ui->fileTreeView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, tr("Error"), tr("No file selected for deletion."));
        return;
    }
    
    QString path = fileModel->filePath(index);
    QFileInfo fileInfo(path);
    
    QMessageBox::StandardButton reply;
    if (fileInfo.isDir()) {
        reply = QMessageBox::question(this, tr("Delete Directory"), 
            tr("Are you sure you want to delete directory: %1?").arg(path),
            QMessageBox::Yes | QMessageBox::No);
    } else {
        reply = QMessageBox::question(this, tr("Delete File"), 
            tr("Are you sure you want to delete file: %1?").arg(path),
            QMessageBox::Yes | QMessageBox::No);
    }
    
    if (reply == QMessageBox::Yes) {
        QFile file(path);
        if (fileInfo.isDir()) {
            QDir dir(path);
            if (dir.removeRecursively()) {
                QMessageBox::information(this, tr("Success"), tr("Deleted successfully."));
            } else {
                QMessageBox::warning(this, tr("Error"), tr("Failed to delete: %1").arg(file.errorString()));
            }
        } else {
            if (file.remove()) {
                QMessageBox::information(this, tr("Success"), tr("Deleted successfully."));
            } else {
                QMessageBox::warning(this, tr("Error"), tr("Failed to delete: %1").arg(file.errorString()));
            }
        }
    }
}

void MainWindow::on_fileTreeView_clicked(const QModelIndex &index)
{
    QString path = fileModel->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir()) {
        rootIndex = index;
        ui->fileTreeView->setRootIndex(index);
        goUpButton->setEnabled(rootIndex.parent().isValid());
        // Update terminal working directory when folder is clicked
        if (terminalPanel && terminalPanel->isRunning()) {
            terminalPanel->setWorkingDirectory(path);
        }
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot open file for reading: %1").arg(file.errorString()));
            return;
        }

        QTextStream in(&file);
        QString content = in.readAll();
        file.close();
        
        for (int i = 0; i < openFiles.size(); i++) {
            if (openFiles[i].filePath == path) {
                showEditorInterface();
                ui->tabWidget->setCurrentIndex(i);
                return;
            }
        }
        
        CodeEditor *editor = new CodeEditor(this);
        editor->setLanguageForFile(path);
        connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
        QSettings settings;
        QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
        editor->setFont(savedFont);
        int savedTabWidth = settings.value("editor/tabWidth", editor->tabWidth()).toInt();
        editor->setTabWidth(savedTabWidth);
        int savedEditorWidth = settings.value("editor/width", 0).toInt();
        if (savedEditorWidth > 0)
            editor->setMinimumWidth(savedEditorWidth);
        editor->setPlainText(content);

        // Create minimap for this editor
        Minimap *minimap = new Minimap(editor, this);
        minimap->setDocument(editor->document());
        connect(minimap, &Minimap::viewportRequested, editor, [editor](int position) {
            QTextCursor cursor(editor->document());
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, position);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        });

        // Create breadcrumb for this editor
        Breadcrumb *breadcrumb = new Breadcrumb(editor, this);
        breadcrumb->setFilePath(path);
        connect(breadcrumb, &Breadcrumb::breadcrumbClicked, this, [this](const QString &path) {
            // Handle breadcrumb navigation
        });
        connect(editor, &QPlainTextEdit::cursorPositionChanged, breadcrumb, &Breadcrumb::updateFromCursor);

        int tabIndex = openFiles.size();
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
        connect(editor, &QPlainTextEdit::modificationChanged, this,
                [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
        connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
            lspDebounceTimer->start();
        });

        OpenFile openFile;
        openFile.filePath = path;
        openFile.fileName = fileInfo.fileName();
        openFiles.append(openFile);

        showEditorInterface();
        ui->tabWidget->addTab(editor, openFile.fileName);
        int tabBarIndex = tabBar->addTab(openFile.fileName);
        tabBar->setTabData(tabBarIndex, path);
        tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createTabCloseButton(tabBarIndex));
        ui->tabWidget->setCurrentWidget(editor);
        tabBar->setCurrentIndex(tabBarIndex);

        currentFile = path;
        setWindowTitle(openFile.fileName + " - Scriptura");
        showSearchBar(true);

        // LSP: Open file in language server
        startLanguageServer(path);
        QString uri = QUrl::fromLocalFile(path).toString();
        QString langId = QFileInfo(path).suffix().toLower();
        lspClient->didOpen(uri, langId, content);
        problemPanel->setCurrentFile(uri);
    }
}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    if (openFiles[index].modified) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("%1 has unsaved changes. Save before closing?").arg(openFiles[index].fileName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Save) {
            ui->tabWidget->setCurrentIndex(index);
            currentFile = openFiles[index].filePath;
            on_action_save_triggered();
            if (openFiles[index].modified) {
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QString closedPath = openFiles[index].filePath;
    if (openFiles[index].filePath == currentFile) {
        currentFile = "";
    }
    // LSP: Close file in language server
    QString closedUri = QUrl::fromLocalFile(closedPath).toString();
    lspClient->didClose(closedUri);
    openFiles.removeAt(index);

    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    
    // Find and remove the corresponding tabBar tab by file path
    for (int i = 0; i < tabBar->count(); ++i) {
        QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString && data.toString() == closedPath) {
            tabBar->removeTab(i);
            break;
        }
    }
    delete widget;

    if (ui->tabWidget->count() > 0) {
        showEditorInterface();
        updateStatusBar();
        // Update problem panel to show current file's problems
        QString newCurrent = QUrl::fromLocalFile(currentFile).toString();
        problemPanel->setCurrentFile(newCurrent);
    } else {
        showWelcomeScreen();
        setWindowTitle(projectDir.isEmpty() ? "Scriptura" : QFileInfo(projectDir).fileName() + " - Scriptura");
        showSearchBar(false);
        problemPanel->hide();
        problemsButton->setChecked(false);
    }
}

void MainWindow::goUpClicked()
{
    if (!rootIndex.isValid())
        return;
    
    // When a project is opened, restrict navigation to project directory only
    if (!projectDir.isEmpty()) {
        // Already at project root, do not go up
        return;
    }
    
    QModelIndex parentIndex = rootIndex.parent();
    if (parentIndex.isValid()) {
        rootIndex = parentIndex;
        ui->fileTreeView->setRootIndex(parentIndex);
        goUpButton->setEnabled(parentIndex.parent().isValid());
    } else {
        goUpButton->setEnabled(false);
    }
}

void MainWindow::on_action_new_window_triggered()
{
    QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
}

void MainWindow::on_action_clone_window_triggered()
{
    QStringList args;
    if (!projectDir.isEmpty())
        args << "--project" << projectDir;
    for (const OpenFile &f : openFiles)
        args << f.filePath;
    QProcess::startDetached(QApplication::applicationFilePath(), args);
}

void MainWindow::toggleTerminalPanel()
{
    if (editorStack->currentWidget() == terminalPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
        terminalButton->setChecked(false);
    } else {
        m_previousEditorStackIndex = editorStack->currentIndex();
        editorStack->setCurrentWidget(terminalPanel);
        terminalButton->setChecked(true);
        if (placeholderButton->isChecked()) {
            QSignalBlocker blocker(placeholderButton);
            placeholderButton->setChecked(false);
        }
        if (ui->bottomPanelContainer->isVisible()) {
            ui->bottomPanelContainer->hide();
            problemsButton->setChecked(false);
            gitButton->setChecked(false);
        }
        if (!terminalPanel->isRunning()) {
            terminalPanel->startShell(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        } else {
            terminalPanel->setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        }
    }
}

void MainWindow::toggleSidebar()
{
    if (sidebarToggleButton->isChecked()) {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->sidebarDrawer, true, 200);
        } else {
            ui->sidebarDrawer->setMaximumWidth(240);
            ui->sidebarDrawer->setMinimumWidth(48);
        }
    } else {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->sidebarDrawer, false, 200);
        } else {
            ui->sidebarDrawer->setMaximumWidth(0);
            ui->sidebarDrawer->setMinimumWidth(0);
        }
    }
}

void MainWindow::onTopTabChanged(int index)
{
    if (index < 0 || index >= tabBar->count())
        return;

    QVariant data = tabBar->tabData(index);
    if (data.typeId() == QMetaType::Int) {
        // Settings tab
        TabType type = static_cast<TabType>(data.toInt());
        switch (type) {
            case TabType::ThemeSettings:
                editorStack->setCurrentWidget(themeSettingsWidget);
                break;
            case TabType::EditorSettings:
                editorStack->setCurrentWidget(editorSettingsWidget);
                break;
            case TabType::KeyboardShortcuts:
                editorStack->setCurrentWidget(keyboardShortcutsPageWidget);
                break;
            case TabType::UpdaterSettings:
                editorStack->setCurrentWidget(updaterSettingsWidget);
                break;
        }
    } else if (data.typeId() == QMetaType::QString) {
        // File tab - find by file path
        QString filePath = data.toString();
        for (int i = 0; i < openFiles.size(); ++i) {
            if (openFiles[i].filePath == filePath) {
                ui->tabWidget->setCurrentIndex(i);
                editorStack->setCurrentWidget(ui->tabWidget);
                break;
            }
        }
    }
    updateTabBarVisibility();
}

void MainWindow::onSettingsTabCloseRequested(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= tabBar->count())
        return;

    TabType type = static_cast<TabType>(tabBar->tabData(tabIndex).toInt());
    tabBar->removeTab(tabIndex);

    // If the closed tab was the current one, switch to appropriate view
    if (tabBar->currentIndex() == -1) {
        if (ui->tabWidget->count() > 0) {
            showEditorInterface();
        } else {
            showWelcomeScreen();
        }
    }

    updateTabBarVisibility();
}

void MainWindow::updateTabBarVisibility()
{
    if (ui->tabWidget->count() > 0 || tabBar->count() > 0) {
        tabBar->show();
    } else {
        tabBar->hide();
    }
}

QPushButton* MainWindow::createSettingsTabCloseButton(int tabIndex)
{
    QPushButton *closeBtn = new QPushButton();
    closeBtn->setIcon(ThemeIcons::instance()->icon(":/icons/close.svg"));
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this, tabIndex]() {
        onSettingsTabCloseRequested(tabIndex);
    });
    return closeBtn;
}

void MainWindow::onBottomTabChanged(int index)
{
    bottomPanelStack->setCurrentIndex(index);
    if (index == 0) {
        problemPanel->show();
        gitPanel->hide();
        projectSearchPanel->hide();
        m_httpClient->hide();
        m_sqliteViewer->hide();
        debugPanel->hide();
        problemsButton->setChecked(true);
        gitButton->setChecked(false);
    } else if (index == 1) {
        problemPanel->hide();
        gitPanel->show();
        projectSearchPanel->hide();
        m_httpClient->hide();
        m_sqliteViewer->hide();
        debugPanel->hide();
        problemsButton->setChecked(false);
        gitButton->setChecked(true);
    } else if (index == 2) {
        problemPanel->hide();
        gitPanel->hide();
        projectSearchPanel->show();
        m_httpClient->hide();
        m_sqliteViewer->hide();
        debugPanel->hide();
        problemsButton->setChecked(false);
        gitButton->setChecked(false);
    } else if (index == 3) {
        problemPanel->hide();
        gitPanel->hide();
        projectSearchPanel->hide();
        m_httpClient->hide();
        m_sqliteViewer->hide();
        debugPanel->show();
        problemsButton->setChecked(false);
        gitButton->setChecked(false);
    } else if (index == 4) {
        problemPanel->hide();
        gitPanel->hide();
        projectSearchPanel->hide();
        m_httpClient->show();
        m_sqliteViewer->hide();
        debugPanel->hide();
        problemsButton->setChecked(false);
        gitButton->setChecked(false);
    } else if (index == 5) {
        problemPanel->hide();
        gitPanel->hide();
        projectSearchPanel->hide();
        m_httpClient->hide();
        m_sqliteViewer->show();
        debugPanel->hide();
        problemsButton->setChecked(false);
        gitButton->setChecked(false);
    }
}

void MainWindow::toggleTodoPanel()
{
    if (placeholderButton->isChecked()) {
        m_previousEditorStackIndex = editorStack->currentIndex();
        editorStack->setCurrentWidget(todoPanel);
        if (terminalButton->isChecked()) {
            QSignalBlocker blocker(terminalButton);
            terminalButton->setChecked(false);
        }
        if (ui->bottomPanelContainer->isVisible()) {
            ui->bottomPanelContainer->hide();
            problemsButton->setChecked(false);
            gitButton->setChecked(false);
        }
    } else {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
}

void MainWindow::toggleProblemPanel()
{
    if (problemsButton->isChecked()) {
        bottomPanelTabs->setCurrentIndex(0);
        bottomPanelStack->setCurrentIndex(0);
        problemPanel->show();
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, true, 200);
        } else {
            ui->bottomPanelContainer->show();
        }
        if (placeholderButton->isChecked()) {
            QSignalBlocker blocker(placeholderButton);
            placeholderButton->setChecked(false);
        }
        if (terminalButton->isChecked()) {
            QSignalBlocker blocker(terminalButton);
            terminalButton->setChecked(false);
            if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
                editorStack->setCurrentIndex(m_previousEditorStackIndex);
            }
        }
        if (gitButton->isChecked()) {
            QSignalBlocker blocker(gitButton);
            gitButton->setChecked(false);
            gitPanel->hide();
        }
    } else {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, false, 200);
        } else {
            ui->bottomPanelContainer->hide();
        }
        problemPanel->hide();
    }
}

void MainWindow::showGitPanel()
{
    bottomPanelTabs->setCurrentIndex(1);
    bottomPanelStack->setCurrentIndex(1);
    gitPanel->show();
    if (m_windowAnimator) {
        m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, true, 200);
    } else {
        ui->bottomPanelContainer->show();
    }
    QSignalBlocker blocker1(placeholderButton);
    placeholderButton->setChecked(false);
    if (terminalButton->isChecked()) {
        QSignalBlocker blocker(terminalButton);
        terminalButton->setChecked(false);
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count())
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
        problemsButton->setChecked(false);
    }
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, tr("About Scriptura"),
        tr("Scriptura\nA simple Qt-based text editor with project file browsing.\n\n"
           "Version: %1\n\n"
           "Built with C++17 and Qt Widgets.\n\n"
           "License: MIT").arg(SCRIPTURA_VERSION));
}

void MainWindow::on_action_editor_settings_triggered()
{
    // Show editor settings as a page in the editor area
    for (int i = 0; i < tabBar->count(); ++i) {
        if (static_cast<TabType>(tabBar->tabData(i).toInt()) == TabType::EditorSettings) {
            tabBar->setCurrentIndex(i);
            return;
        }
    }
    // If settings tab doesn't exist yet, add it
    int editorSettingsTabIndex = tabBar->addTab(tr("Settings"));
    tabBar->setTabData(editorSettingsTabIndex, static_cast<int>(TabType::EditorSettings));
    tabBar->setTabButton(editorSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(editorSettingsTabIndex));
    tabBar->setCurrentIndex(editorSettingsTabIndex);
}

void MainWindow::applyTheme(const Theme &theme)
{
    // Delegate base palette and global stylesheet to ThemeManager
    ThemeManager::Theme newTheme(
        static_cast<ThemeManager::ColorFamily>(static_cast<int>(theme.family)),
        static_cast<ThemeManager::Mode>(static_cast<int>(theme.mode)),
        ThemeManager::Features(static_cast<ThemeManager::Feature>(static_cast<int>(theme.features)))
    );
    if (m_themeManager) {
        m_themeManager->setCurrentTheme(newTheme);
    }
    
    QPalette palette = buildBasePalette(theme.family, theme.mode);
    bool isDark = theme.isDark();
    auto syntax = baseSyntaxColors(theme.mode);

    if (theme.features.testFlag(ThemeFeature::HighContrast)) {
        applyHighContrastPalette(palette, theme.family, theme.mode);
        applyHighContrastSyntax(syntax, theme.family, theme.mode);
    }

    QApplication::setStyle("Fusion");
    QApplication::setPalette(palette);

    QColor windowColor = palette.color(QPalette::Window);
    QColor baseColor = palette.color(QPalette::Base);
    QColor buttonColor = palette.color(QPalette::Button);
    QColor textColor = palette.color(QPalette::WindowText);
    QColor accentColor = palette.color(QPalette::Highlight);
    QColor midColor = palette.color(QPalette::Mid);
    QColor lightColor = palette.color(QPalette::Light);

    // Compute glassmorphism translucents
    QColor glassSidebar = isDark ? QColor(42, 44, 47, 217) : QColor(242, 242, 247, 217);
    QColor glassPanel = isDark ? QColor(38, 40, 43, 217) : QColor(232, 232, 232, 217);
    QColor glassToolbar = isDark ? QColor(56, 58, 61, 230) : QColor(246, 246, 246, 230);
    QColor glassStatus = isDark ? QColor(28, 31, 33, 242) : QColor(240, 240, 240, 242);
    QColor glassBorder = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 25);
    QColor neumorphicLight = isDark ? QColor(255, 255, 255, 12) : QColor(255, 255, 255, 180);
    QColor neumorphicDark = isDark ? QColor(0, 0, 0, 40) : QColor(0, 0, 0, 25);

    QString modernSheet = QString(R"(
        /* Xcode-Style Layout — Glassmorphism & Neumorphism */

        /* Base widget styling */
        QMainWindow, QDialog {
            background-color: %1;
        }

        /* Group boxes and frames — neumorphic */
        QGroupBox, QFrame#recentProjectsFrame {
            background-color: rgba(%8, %9, %10, 0.6);
            border: 1px solid %3;
            border-radius: 12px;
            margin-top: 16px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top center;
            padding: 0 8px;
            color: %4;
            font-weight: 600;
        }

        /* Tool buttons — neumorphic soft press */
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
            min-width: 24px;
            min-height: 24px;
            margin: 0px;
            color: %4;
        }
        QToolButton:hover {
            background-color: rgba(%11, %12, %13, 0.10);
        }
        QToolButton:checked {
            background-color: rgba(%14, %15, %16, 0.15);
            color: %4;
        }
        QToolButton:pressed {
            background-color: rgba(%11, %12, %13, 0.18);
        }
        QToolButton:disabled {
            color: %3;
        }

        /* Push buttons — neumorphic raised */
        QPushButton {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.08);
            border-radius: 8px;
            padding: 6px 14px;
            color: %4;
            font-weight: 500;
            min-height: 28px;
            min-width: 64px;
        }
        QPushButton:hover {
            background-color: rgba(%8, %9, %10, 0.7);
            border-color: rgba(%14, %15, %16, 0.3);
        }
        QPushButton:pressed {
            background-color: rgba(%8, %9, %10, 0.4);
        }
        QPushButton:disabled {
            background-color: rgba(%8, %9, %10, 0.3);
            color: %3;
            border-color: transparent;
        }

        /* Input fields — neumorphic inset */
        QLineEdit, QSpinBox, QFontComboBox {
            background-color: rgba(%20, %21, %22, 0.4);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 6px;
            padding: 6px 10px;
            color: %4;
            min-height: 28px;
        }
        QLineEdit:focus, QSpinBox:focus, QFontComboBox:focus {
            border: 1px solid rgba(%14, %15, %16, 0.4);
            background-color: rgba(%20, %21, %22, 0.5);
        }
        QLineEdit:disabled, QSpinBox:disabled, QFontComboBox:disabled {
            border-color: transparent;
            color: %3;
            background-color: rgba(%8, %9, %10, 0.3);
        }

        /* Text edit areas */
        QTextEdit, QPlainTextEdit {
            background-color: %7;
            border: 1px solid rgba(%17, %18, %19, 0.08);
            border-radius: 6px;
            padding: 8px;
            color: %4;
            selection-background-color: %6;
            selection-color: %1;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid rgba(%14, %15, %16, 0.3);
        }

        /* Tree view — glassmorphism */
        QTreeView {
            background-color: transparent;
            border: none;
            selection-background-color: rgba(%14, %15, %16, 0.20);
            selection-color: %4;
            outline: none;
        }
        QTreeView::item {
            padding: 4px 8px;
            border-radius: 6px;
            margin: 1px 4px;
            color: %4;
        }
        QTreeView::item:hover {
            background-color: rgba(%11, %12, %13, 0.08);
        }
        QTreeView::item:selected {
            background-color: rgba(%14, %15, %16, 0.20);
            color: %4;
        }

        /* Xcode-style tabs — active merges with editor */
        QTabBar::tab {
            background-color: transparent;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 8px 16px;
            margin-right: 1px;
            color: %3;
            min-width: 80px;
            font-size: 12px;
        }
        QTabBar::tab:hover:!selected {
            background-color: rgba(%11, %12, %13, 0.08);
            color: %4;
        }
        QTabBar::tab:selected {
            background-color: %7;
            border-bottom: 2px solid %6;
            color: %4;
        }
        QTabBar::tab:disabled {
            color: %3;
        }
        QTabWidget::pane {
            border: none;
            top: -1px;
        }

        /* Status bar — darkest glassmorphism layer */
        QStatusBar {
            background-color: rgba(%23, %24, %25, 0.95);
            border-top: 1px solid %3;
            color: %3;
            padding: 2px 10px;
            font-size: 12px;
        }

        /* Menu bar — hidden but styled for dropdown */
        QMenuBar {
            background-color: transparent;
            border: none;
            padding: 2px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 12px;
            border-radius: 6px;
            color: %4;
        }
        QMenuBar::item:selected {
            background-color: rgba(%11, %12, %13, 0.10);
        }

        /* Menu dropdown — glassmorphism */
        QMenu {
            background-color: rgba(%8, %9, %10, 0.92);
            border: 1px solid rgba(%17, %18, %19, 0.12);
            border-radius: 10px;
            padding: 6px;
            margin: 2px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 6px 24px 6px 10px;
            border-radius: 6px;
            color: %4;
        }
        QMenu::item:selected {
            background-color: rgba(%14, %15, %16, 0.20);
            color: %4;
        }
        QMenu::item:disabled {
            color: %3;
        }
        QMenu::separator {
            height: 1px;
            background-color: rgba(%17, %18, %19, 0.10);
            margin: 4px 8px;
        }

        /* Scrollbars — thin overlay */
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0px;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background-color: rgba(%11, %12, %13, 0.25);
            border-radius: 5px;
            min-height: 30px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background-color: rgba(%11, %12, %13, 0.25);
            border-radius: 5px;
            min-width: 30px;
            margin: 2px;
        }
        QScrollBar::handle:hover {
            background-color: rgba(%11, %12, %13, 0.45);
        }
        QScrollBar::add-line, QScrollBar::sub-line,
        QScrollBar::add-page, QScrollBar::sub-page {
            height: 0px;
            width: 0px;
            border: none;
            background: transparent;
        }

        /* Checkbox and radio — neumorphic */
        QCheckBox, QRadioButton {
            spacing: 8px;
            color: %4;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid rgba(%17, %18, %19, 0.15);
            background-color: rgba(%20, %21, %22, 0.3);
        }
        QRadioButton::indicator {
            border-radius: 8px;
        }
        QCheckBox::indicator:hover, QRadioButton::indicator:hover {
            border-color: rgba(%14, %15, %16, 0.4);
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background-color: %6;
            border: 1px solid %6;
        }
        QCheckBox:disabled, QRadioButton:disabled {
            color: %3;
        }
        QCheckBox::indicator:disabled, QRadioButton::indicator:disabled {
            border-color: transparent;
            background-color: rgba(%8, %9, %10, 0.3);
        }

        /* Tooltip — glassmorphism */
        QToolTip {
            background-color: rgba(%8, %9, %10, 0.92);
            color: %4;
            border: 1px solid rgba(%17, %18, %19, 0.12);
            border-radius: 8px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* Toolbar */
        QToolBar {
            background-color: transparent;
            border: none;
            spacing: 4px;
            padding: 2px;
        }
        QTabBar {
            background-color: transparent;
            border: none;
        }

        /* Container widgets — glassmorphism layers */
        QWidget#bottomPanelContainer {
            background-color: rgba(%26, %27, %28, 0.85);
            border-top: 1px solid rgba(%17, %18, %19, 0.10);
        }
        QWidget#sidebarDrawer {
            background-color: rgba(%29, %30, %31, 0.85);
            border-right: 1px solid rgba(%17, %18, %19, 0.10);
        }
        QWidget#editorContainer {
            background-color: %7;
        }
        QWidget#unifiedTitleBar {
            background-color: rgba(%32, %33, %34, 0.90);
            border-bottom: 1px solid rgba(%17, %18, %19, 0.10);
        }

        QFrame#recentProjectsFrame {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 12px;
            padding: 16px;
        }

        /* Special buttons */
        QPushButton#projectButton {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 8px;
            padding: 8px;
            text-align: left;
        }
        QPushButton#projectButton:hover {
            background-color: rgba(%8, %9, %10, 0.7);
        }

        /* Primary buttons — neumorphic accent */
        QPushButton#primaryButton {
            background-color: %6;
            border: none;
            border-radius: 10px;
            padding: 12px 24px;
            color: %1;
            font-weight: 600;
            font-size: 14px;
            min-width: 140px;
        }
        QPushButton#primaryButton:hover {
            background-color: %6;
        }
        QPushButton#primaryButton:pressed {
            background-color: %6;
        }

        /* Welcome screen title */
        QLabel#welcomeTitle {
            color: %4;
        }

        /* Side bar icon bar — glassmorphism */
        QWidget#sidebarIconBar {
            background-color: transparent;
            border-top: 1px solid rgba(%17, %18, %19, 0.10);
            padding: 8px;
        }

        /* Focus styling */
        :focus {
            outline: none;
        }
        ::selection {
            background-color: %6;
            color: %1;
        }
    )")
        .arg(windowColor.name())          // %1
        .arg(buttonColor.name())          // %2
        .arg(midColor.name())             // %3
        .arg(textColor.name())            // %4
        .arg(lightColor.name())           // %5
        .arg(accentColor.name())          // %6
        .arg(baseColor.name())            // %7
        .arg(QString::number(glassSidebar.red()))    // %8
        .arg(QString::number(glassSidebar.green()))  // %9
        .arg(QString::number(glassSidebar.blue()))   // %10
        .arg(QString::number(windowColor.red()))     // %11
        .arg(QString::number(windowColor.green()))   // %12
        .arg(QString::number(windowColor.blue()))    // %13
        .arg(QString::number(accentColor.red()))     // %14
        .arg(QString::number(accentColor.green()))   // %15
        .arg(QString::number(accentColor.blue()))    // %16
        .arg(QString::number(midColor.red()))        // %17
        .arg(QString::number(midColor.green()))      // %18
        .arg(QString::number(midColor.blue()))       // %19
        .arg(QString::number(baseColor.red()))       // %20
        .arg(QString::number(baseColor.green()))     // %21
        .arg(QString::number(baseColor.blue()))      // %22
        .arg(QString::number(glassStatus.red()))     // %23
        .arg(QString::number(glassStatus.green()))   // %24
        .arg(QString::number(glassStatus.blue()))    // %25
        .arg(QString::number(glassPanel.red()))      // %26
        .arg(QString::number(glassPanel.green()))    // %27
        .arg(QString::number(glassPanel.blue()))     // %28
        .arg(QString::number(glassSidebar.red()))    // %29
        .arg(QString::number(glassSidebar.green()))  // %30
        .arg(QString::number(glassSidebar.blue()))   // %31
        .arg(QString::number(glassToolbar.red()))    // %32
        .arg(QString::number(glassToolbar.green()))  // %33
        .arg(QString::number(glassToolbar.blue()));  // %34

    for (QWidget *widget : QApplication::allWidgets()) {
        widget->setPalette(palette);
        widget->setAutoFillBackground(true);
        if (CodeEditor *editor = qobject_cast<CodeEditor*>(widget)) {
            editor->viewport()->setPalette(palette);
            editor->viewport()->setAutoFillBackground(true);
            editor->setDarkMode(isDark);
            QColor trailingBg = isDark ? QColor("#7f1d1d") : QColor("#fecaca");
            editor->setThemeColors(syntax.keyword, syntax.string, syntax.comment, syntax.number,
                                   syntax.preprocessor, syntax.tag, syntax.attribute,
                                   syntax.cssProperty, syntax.variable, syntax.function,
                                   syntax.escape, trailingBg);
        }
    }

    QSettings settings;
    settings.setValue("theme/selected", themeToLegacyInt(theme));
}

void MainWindow::on_action_theme_triggered()
{
    // Show theme settings as a page in the editor area
    for (int i = 0; i < tabBar->count(); ++i) {
        if (static_cast<TabType>(tabBar->tabData(i).toInt()) == TabType::ThemeSettings) {
            tabBar->setCurrentIndex(i);
            return;
        }
    }
    // If theme tab doesn't exist yet, add it
    int themeSettingsTabIndex = tabBar->addTab(tr("Theme"));
    tabBar->setTabData(themeSettingsTabIndex, static_cast<int>(TabType::ThemeSettings));
    tabBar->setTabButton(themeSettingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(themeSettingsTabIndex));
    tabBar->setCurrentIndex(themeSettingsTabIndex);
}

void MainWindow::on_action_license_triggered()
{
    QMessageBox::about(this, tr("License"), tr("MIT License\n\n"
        "Copyright (c) 2026 Scriptura\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE."));
  }

  void MainWindow::on_action_manage_plugins_triggered()
  {
      if (pluginManagerDialog) {
          pluginManagerDialog->refresh();
          pluginManagerDialog->show();
      }
  }

  void MainWindow::setSidebarCollapsed(bool collapsed)
{
    QSettings settings;
    settings.setValue("ui/sidebarCollapsed", collapsed);

    if (collapsed) {
        if (sidebarToggleButton)
            sidebarToggleButton->setChecked(false);
        QPropertyAnimation *animation = new QPropertyAnimation(ui->sidebarDrawer, "maximumWidth");
        animation->setDuration(200);
        animation->setStartValue(ui->sidebarDrawer->width());
        animation->setEndValue(0);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this]() {
            ui->sidebarDrawer->setMinimumWidth(0);
            ui->sidebarDrawer->setMaximumWidth(0);
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        if (sidebarToggleButton)
            sidebarToggleButton->setChecked(true);
        QPropertyAnimation *animation = new QPropertyAnimation(ui->sidebarDrawer, "maximumWidth");
        animation->setDuration(200);
        animation->setStartValue(ui->sidebarDrawer->width());
        animation->setEndValue(240);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this]() {
            ui->sidebarDrawer->setMinimumWidth(48);
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);

        QPropertyAnimation *minAnim = new QPropertyAnimation(ui->sidebarDrawer, "minimumWidth");
        minAnim->setDuration(200);
        minAnim->setStartValue(0);
        minAnim->setEndValue(48);
        minAnim->setEasingCurve(QEasingCurve::InOutCubic);
        minAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void MainWindow::showSearchBar(bool show)
{
    findReplaceBar->setVisible(show);
    if (show) {
        findReplaceBar->setEditor(qobject_cast<QPlainTextEdit*>(ui->tabWidget->currentWidget()));
    }
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor)
        editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
}

QString MainWindow::findTerminal()
{
#ifdef Q_OS_WIN
    return "cmd.exe";
#elif defined(Q_OS_MAC)
    return "open";
#else
    QStringList candidates = {"konsole", "gnome-terminal", "xfce4-terminal",
                              "mate-terminal", "alacritty", "kitty", "xterm"};
    for (const QString &term : candidates) {
        if (!QStandardPaths::findExecutable(term).isEmpty())
            return term;
    }
    return "xterm";
#endif
}

void MainWindow::updateFamilyButtonPreview(QPushButton *btn, ThemeColorFamily family, ThemeMode mode, ThemeFeatures features)
{
    QPalette p = buildBasePalette(family, mode);
    if (features.testFlag(ThemeFeature::HighContrast))
        applyHighContrastPalette(p, family, mode);

    QColor bg = p.color(QPalette::Window);
    QColor accent = highContrastAccentColor(family, mode);
    QColor textColor = p.color(QPalette::WindowText);

    btn->setStyleSheet(QString("background-color: %1; color: %2; padding: 10px; border: 1px solid %2; border-radius: 4px;")
                       .arg(bg.name()).arg(textColor.name()));
}

QPalette MainWindow::buildBasePalette(ThemeColorFamily family, ThemeMode mode)
{
    QPalette p;
    bool isDark = (mode == ThemeMode::Dark);

    auto s = [&](QPalette::ColorRole role, const QColor &c) { p.setColor(role, c); };

    switch (family) {
    case ThemeColorFamily::Default:
        if (isDark) {
            s(QPalette::Window, QColor(45, 45, 48));
            s(QPalette::WindowText, Qt::white);
            s(QPalette::Base, QColor(30, 30, 32));
            s(QPalette::AlternateBase, QColor(55, 55, 60));
            s(QPalette::ToolTipBase, Qt::white);
            s(QPalette::ToolTipText, Qt::white);
            s(QPalette::Text, Qt::white);
            s(QPalette::Button, QColor(60, 60, 65));
            s(QPalette::ButtonText, Qt::white);
            s(QPalette::BrightText, Qt::red);
            s(QPalette::Link, QColor(100, 150, 255));
            s(QPalette::Highlight, QColor(100, 150, 255));
            s(QPalette::HighlightedText, Qt::white);
            s(QPalette::Light, QColor(80, 80, 85));
            s(QPalette::Mid, QColor(50, 50, 55));
            s(QPalette::Dark, QColor(35, 35, 38));
            s(QPalette::Midlight, QColor(70, 70, 75));
        } else {
            s(QPalette::Window, QColor(245, 245, 247));
            s(QPalette::WindowText, QColor(30, 30, 32));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(235, 235, 238));
            s(QPalette::ToolTipBase, Qt::white);
            s(QPalette::ToolTipText, QColor(30, 30, 32));
            s(QPalette::Text, QColor(30, 30, 32));
            s(QPalette::Button, QColor(240, 240, 242));
            s(QPalette::ButtonText, QColor(30, 30, 32));
            s(QPalette::BrightText, Qt::red);
            s(QPalette::Link, QColor(0, 122, 255));
            s(QPalette::Highlight, QColor(0, 122, 255));
            s(QPalette::HighlightedText, Qt::white);
            s(QPalette::Light, QColor(250, 250, 252));
            s(QPalette::Mid, QColor(200, 200, 205));
            s(QPalette::Dark, QColor(180, 180, 185));
            s(QPalette::Midlight, QColor(245, 245, 247));
        }
        return p;

    case ThemeColorFamily::Blue:
        if (isDark) {
            s(QPalette::Window, QColor(35, 42, 55));
            s(QPalette::Base, QColor(25, 30, 42));
            s(QPalette::AlternateBase, QColor(45, 52, 65));
            s(QPalette::Button, QColor(50, 58, 72));
            s(QPalette::Highlight, QColor(80, 140, 255));
        } else {
            s(QPalette::Window, QColor(235, 243, 255));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(220, 230, 245));
            s(QPalette::Button, QColor(220, 230, 245));
            s(QPalette::Highlight, QColor(0, 122, 255));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 72, 85) : QColor(240, 245, 255));
        s(QPalette::Mid, isDark ? QColor(45, 52, 60) : QColor(175, 190, 210));
        s(QPalette::Dark, isDark ? QColor(30, 35, 45) : QColor(150, 165, 185));
        s(QPalette::Midlight, isDark ? QColor(55, 62, 75) : QColor(230, 240, 250));
        break;

    case ThemeColorFamily::Green:
        if (isDark) {
            s(QPalette::Window, QColor(35, 48, 35));
            s(QPalette::Base, QColor(25, 38, 25));
            s(QPalette::AlternateBase, QColor(45, 58, 45));
            s(QPalette::Button, QColor(50, 65, 50));
            s(QPalette::Highlight, QColor(80, 200, 120));
        } else {
            s(QPalette::Window, QColor(235, 248, 235));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(220, 240, 220));
            s(QPalette::Button, QColor(220, 240, 220));
            s(QPalette::Highlight, QColor(0, 180, 80));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 78, 65) : QColor(240, 250, 240));
        s(QPalette::Mid, isDark ? QColor(45, 58, 48) : QColor(170, 200, 170));
        s(QPalette::Dark, isDark ? QColor(30, 42, 30) : QColor(140, 170, 140));
        s(QPalette::Midlight, isDark ? QColor(55, 68, 55) : QColor(230, 245, 230));
        break;

    case ThemeColorFamily::Red:
        if (isDark) {
            s(QPalette::Window, QColor(48, 35, 35));
            s(QPalette::Base, QColor(38, 25, 25));
            s(QPalette::AlternateBase, QColor(58, 45, 45));
            s(QPalette::Button, QColor(65, 50, 50));
            s(QPalette::Highlight, QColor(255, 100, 100));
        } else {
            s(QPalette::Window, QColor(248, 238, 238));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(245, 225, 225));
            s(QPalette::Button, QColor(245, 225, 225));
            s(QPalette::Highlight, QColor(255, 55, 55));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::Text, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(78, 65, 65) : QColor(252, 240, 240));
        s(QPalette::Mid, isDark ? QColor(58, 45, 45) : QColor(210, 185, 185));
        s(QPalette::Dark, isDark ? QColor(42, 30, 30) : QColor(190, 160, 160));
        s(QPalette::Midlight, isDark ? QColor(68, 55, 55) : QColor(248, 235, 235));
        break;

    case ThemeColorFamily::Yellow:
        if (isDark) {
            s(QPalette::Window, QColor(48, 45, 32));
            s(QPalette::Base, QColor(38, 35, 22));
            s(QPalette::AlternateBase, QColor(58, 55, 42));
            s(QPalette::Button, QColor(65, 62, 48));
            s(QPalette::Highlight, QColor(220, 200, 80));
        } else {
            s(QPalette::Window, QColor(248, 248, 235));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(245, 242, 215));
            s(QPalette::Button, QColor(245, 242, 215));
            s(QPalette::Highlight, QColor(210, 180, 0));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::Text, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(78, 75, 62) : QColor(252, 250, 240));
        s(QPalette::Mid, isDark ? QColor(58, 55, 42) : QColor(210, 200, 150));
        s(QPalette::Dark, isDark ? QColor(42, 40, 25) : QColor(190, 180, 130));
        s(QPalette::Midlight, isDark ? QColor(68, 65, 52) : QColor(248, 245, 225));
        break;

    case ThemeColorFamily::Brown:
        if (isDark) {
            s(QPalette::Window, QColor(42, 35, 28));
            s(QPalette::Base, QColor(32, 25, 18));
            s(QPalette::AlternateBase, QColor(52, 45, 38));
            s(QPalette::Button, QColor(58, 50, 42));
            s(QPalette::Highlight, QColor(190, 140, 90));
        } else {
            s(QPalette::Window, QColor(248, 245, 238));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(240, 232, 218));
            s(QPalette::Button, QColor(240, 232, 218));
            s(QPalette::Highlight, QColor(180, 130, 70));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::Text, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(72, 65, 58) : QColor(250, 247, 240));
        s(QPalette::Mid, isDark ? QColor(52, 45, 38) : QColor(200, 185, 160));
        s(QPalette::Dark, isDark ? QColor(35, 30, 25) : QColor(180, 165, 140));
        s(QPalette::Midlight, isDark ? QColor(62, 55, 48) : QColor(245, 240, 228));
        break;

    case ThemeColorFamily::Cyan:
        if (isDark) {
            s(QPalette::Window, QColor(35, 48, 50));
            s(QPalette::Base, QColor(25, 38, 40));
            s(QPalette::AlternateBase, QColor(45, 58, 60));
            s(QPalette::Button, QColor(50, 65, 68));
            s(QPalette::Highlight, QColor(60, 190, 210));
        } else {
            s(QPalette::Window, QColor(235, 248, 250));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(215, 238, 242));
            s(QPalette::Button, QColor(215, 238, 242));
            s(QPalette::Highlight, QColor(0, 180, 200));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 78, 82) : QColor(238, 248, 250));
        s(QPalette::Mid, isDark ? QColor(45, 58, 62) : QColor(165, 200, 210));
        s(QPalette::Dark, isDark ? QColor(30, 42, 45) : QColor(140, 180, 190));
        s(QPalette::Midlight, isDark ? QColor(55, 68, 72) : QColor(228, 242, 245));
        break;

    case ThemeColorFamily::Violet:
        if (isDark) {
            s(QPalette::Window, QColor(40, 35, 55));
            s(QPalette::Base, QColor(30, 25, 42));
            s(QPalette::AlternateBase, QColor(50, 45, 65));
            s(QPalette::Button, QColor(55, 48, 70));
            s(QPalette::Highlight, QColor(140, 100, 220));
        } else {
            s(QPalette::Window, QColor(240, 238, 252));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(228, 222, 242));
            s(QPalette::Button, QColor(228, 222, 242));
            s(QPalette::Highlight, QColor(140, 80, 230));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::Text, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(72, 65, 85) : QColor(245, 242, 252));
        s(QPalette::Mid, isDark ? QColor(52, 45, 62) : QColor(190, 178, 215));
        s(QPalette::Dark, isDark ? QColor(35, 30, 48) : QColor(165, 150, 195));
        s(QPalette::Midlight, isDark ? QColor(62, 55, 75) : QColor(238, 232, 248));
        break;
    }

    return p;
}

QColor MainWindow::highContrastAccentColor(ThemeColorFamily family, ThemeMode mode)
{
    bool isDark = (mode == ThemeMode::Dark);
    switch (family) {
    case ThemeColorFamily::Default:
        return isDark ? QColor("#4d94ff") : QColor("#0055dd");
    case ThemeColorFamily::Blue:
        return isDark ? QColor("#80bfff") : QColor("#0044aa");
    case ThemeColorFamily::Green:
        return isDark ? QColor("#33ff99") : QColor("#006633");
    case ThemeColorFamily::Red:
        return isDark ? QColor("#ff5555") : QColor("#cc0000");
    case ThemeColorFamily::Yellow:
        return isDark ? QColor("#ffdd00") : QColor("#997a00");
    case ThemeColorFamily::Brown:
        return isDark ? QColor("#ddaa55") : QColor("#885522");
    case ThemeColorFamily::Cyan:
        return isDark ? QColor("#00eeff") : QColor("#008899");
    case ThemeColorFamily::Violet:
        return isDark ? QColor("#bb88ff") : QColor("#6633cc");
    }
    return isDark ? QColor("#4d94ff") : QColor("#0055dd");
}

void MainWindow::applyHighContrastPalette(QPalette &p, ThemeColorFamily family, ThemeMode mode)
{
    bool isDark = (mode == ThemeMode::Dark);
    QColor accent = highContrastAccentColor(family, mode);

    if (isDark) {
        p.setColor(QPalette::Window, Qt::black);
        p.setColor(QPalette::WindowText, accent);
        p.setColor(QPalette::Base, QColor(20, 20, 20));
        p.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
        p.setColor(QPalette::Text, accent);
        p.setColor(QPalette::Button, Qt::black);
        p.setColor(QPalette::ButtonText, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::Link, accent);
    } else {
        p.setColor(QPalette::Window, Qt::white);
        p.setColor(QPalette::WindowText, accent);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        p.setColor(QPalette::Text, accent);
        p.setColor(QPalette::Button, Qt::white);
        p.setColor(QPalette::ButtonText, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Link, accent);
    }
}

MainWindow::SyntaxColors MainWindow::baseSyntaxColors(ThemeMode mode)
{
    SyntaxColors c;
    if (mode == ThemeMode::Dark) {
        c.keyword = QColor("#93c5fd");
        c.string = QColor("#86efac");
        c.comment = QColor("#94a3b8");
        c.number = QColor("#c084fc");
        c.preprocessor = QColor("#a855f7");
        c.tag = QColor("#60a5fa");
        c.attribute = QColor("#fbbf24");
        c.cssProperty = QColor("#2dd4bf");
        c.variable = QColor("#38bdf8");
        c.function = QColor("#f97316");
        c.escape = QColor("#22d3ee");
    } else {
        c.keyword = QColor("#1d4ed8");
        c.string = QColor("#15803d");
        c.comment = QColor("#64748b");
        c.number = QColor("#9333ea");
        c.preprocessor = QColor("#7e22ce");
        c.tag = QColor("#2563eb");
        c.attribute = QColor("#a16207");
        c.cssProperty = QColor("#0f766e");
        c.variable = QColor("#0369a1");
        c.function = QColor("#d97706");
        c.escape = QColor("#0e7490");
    }
    return c;
}

MainWindow::SyntaxColors MainWindow::highContrastSyntaxColor(ThemeColorFamily family, ThemeMode mode, const SyntaxColors &base)
{
    QColor accent = highContrastAccentColor(family, mode);
    bool isDark = (mode == ThemeMode::Dark);

    // Blend accent with base colors to create vibrant, family-tinted syntax colors
    // while maintaining strong contrast against the high contrast background
    auto blend = [&](const QColor &baseColor, float accentWeight) -> QColor {
        if (isDark) {
            // In dark mode, boost saturation and lightness for visibility on near-black
            int r = qMin(255, baseColor.red() + static_cast<int>(accent.red() * accentWeight));
            int g = qMin(255, baseColor.green() + static_cast<int>(accent.green() * accentWeight));
            int b = qMin(255, baseColor.blue() + static_cast<int>(accent.blue() * accentWeight));
            return QColor(r, g, b);
        } else {
            // In light mode, deepen colors for contrast against white
            int r = qMax(0, baseColor.red() - static_cast<int>((255 - accent.red()) * accentWeight * 0.3f));
            int g = qMax(0, baseColor.green() - static_cast<int>((255 - accent.green()) * accentWeight * 0.3f));
            int b = qMax(0, baseColor.blue() - static_cast<int>((255 - accent.blue()) * accentWeight * 0.3f));
            return QColor(r, g, b);
        }
    };

    SyntaxColors result;
    result.keyword = blend(base.keyword, 0.4f);
    result.string = blend(base.string, 0.35f);
    result.comment = isDark ? QColor("#b0b0b0") : QColor("#555555");
    result.number = blend(base.number, 0.45f);
    result.preprocessor = blend(base.preprocessor, 0.5f);
    result.tag = blend(base.tag, 0.4f);
    result.attribute = blend(base.attribute, 0.45f);
    result.cssProperty = blend(base.cssProperty, 0.35f);
    result.variable = blend(base.variable, 0.4f);
    result.function = blend(base.function, 0.5f);
    result.escape = blend(base.escape, 0.4f);
    return result;
}

void MainWindow::applyHighContrastSyntax(SyntaxColors &c, ThemeColorFamily family, ThemeMode mode)
{
    SyntaxColors base = baseSyntaxColors(mode);
    c = highContrastSyntaxColor(family, mode, base);
}

void MainWindow::updateStatusBar()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor) {
        int line = editor->textCursor().blockNumber() + 1;
        int column = editor->textCursor().positionInBlock() + 1;
        QString fileInfo = currentFile.isEmpty() ? tr("No file") : QFileInfo(currentFile).fileName();
        QString lang = tr("Plain Text");
        if (!currentFile.isEmpty()) {
            QString ext = QFileInfo(currentFile).suffix().toLower();
            if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp") lang = "C++";
            else if (ext == "py") lang = "Python";
            else if (ext == "js") lang = "JavaScript";
            else if (ext == "ts") lang = "TypeScript";
            else if (ext == "java") lang = "Java";
            else if (ext == "rs") lang = "Rust";
            else if (ext == "go") lang = "Go";
            else if (ext == "html" || ext == "htm") lang = "HTML";
            else if (ext == "css") lang = "CSS";
            else if (ext == "md") lang = "Markdown";
            else if (ext == "json") lang = "JSON";
        }
        QString lspStatus = lspClient->isRunning() ? tr("LSP: Connected") : tr("LSP: Disconnected");
        ui->statusbar->showMessage(QString("%1 | %2 | %3 | Line %4, Column %5")
            .arg(fileInfo, lang, lspStatus).arg(line).arg(column));
    }
}

void MainWindow::updateTabModified(int index, bool modified)
{
    if (index < 0 || index >= openFiles.size())
        return;
    openFiles[index].modified = modified;
    QString title = openFiles[index].fileName;
    if (modified)
        title = "*" + title;
    if (index < ui->tabWidget->count())
        ui->tabWidget->setTabText(index, title);
    if (index < tabBar->count())
        tabBar->setTabText(index, title);
}

void MainWindow::updateTopTabBar()
{
    int currentFileIndex = ui->tabWidget->currentIndex();
    if (currentFileIndex >= 0 && currentFileIndex < openFiles.size()) {
        QString currentFilePath = openFiles[currentFileIndex].filePath;
        for (int i = 0; i < tabBar->count(); ++i) {
            QVariant data = tabBar->tabData(i);
            if (data.typeId() == QMetaType::QString && data.toString() == currentFilePath) {
                QSignalBlocker blocker(tabBar);
                tabBar->setCurrentIndex(i);
                return;
            }
        }
    }
    QSignalBlocker blocker(tabBar);
    tabBar->setCurrentIndex(-1);
}

void MainWindow::updateBottomTabBar()
{
    bottomPanelTabs->setCurrentIndex(bottomPanelStack->currentIndex());
}

void MainWindow::loadRecentProjects()
{
    QSettings settings;
    recentProjects = settings.value("recentProjects").toStringList();
    recentFiles = settings.value("recentFiles").toStringList();
}

void MainWindow::saveRecentProjects()
{
    QSettings settings;
    settings.setValue("recentProjects", recentProjects);
    settings.setValue("recentFiles", recentFiles);
}

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    recentFiles.removeAll(path);
    recentFiles.prepend(path);
    while (recentFiles.size() > 20)
        recentFiles.removeLast();
    saveRecentProjects();
}

void MainWindow::updateRecentProjectsOnWelcome()
{
    if (!recentProjectsLayout) return;

    while (recentProjectsLayout->count() > 1) {
        QLayoutItem *item = recentProjectsLayout->takeAt(recentProjectsLayout->count() - 1);
        delete item->widget();
        delete item;
    }

    for (const QString &project : recentProjects) {
        QPushButton *btn = new QPushButton(QFileInfo(project).fileName(), this);
        btn->setToolTip(project);
        btn->setProperty("projectPath", project);
        btn->setStyleSheet("QPushButton { text-align: left; padding: 6px 12px; }"
                          "QPushButton:hover { background-color: palette(highlight); color: palette(highlighted-text); }");
        connect(btn, &QPushButton::clicked, this, [this, project]() {
            QDir dir(project);
            if (dir.exists()) {
                projectDir = project;
                rootIndex = fileModel->index(project);
                ui->fileTreeView->setRootIndex(rootIndex);
                ui->fileTreeView->hideColumn(1);
                ui->fileTreeView->hideColumn(2);
                ui->fileTreeView->hideColumn(3);
                goUpButton->setEnabled(rootIndex.parent().isValid());
                setWindowTitle(QFileInfo(project).fileName() + " - Scriptura");
            }
        });
        recentProjectsLayout->addWidget(btn);
    }
}

void MainWindow::autoSave()
{
    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].modified) {
            QFileInfo fileInfo(openFiles[i].filePath);
            // Skip if file no longer exists or path is empty
            if (openFiles[i].filePath.isEmpty() || !fileInfo.exists()) {
                continue;
            }
            // Skip if file is not writable
            if (!fileInfo.isWritable()) {
                qDebug() << "Auto-save skipped (not writable):" << openFiles[i].filePath;
                continue;
            }

            QFile file(openFiles[i].filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
                if (editor)
                    out << editor->toPlainText();
                file.close();
                openFiles[i].modified = false;
                qDebug() << "Auto-saved:" << openFiles[i].filePath;
            }
        }
    }
}

bool MainWindow::checkUnsavedChanges()
{
    for (const OpenFile &f : openFiles) {
        if (f.modified) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, tr("Unsaved Changes"),
                tr("%1 has unsaved changes. Save before closing?").arg(f.fileName),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (reply == QMessageBox::Save) {
                // Find and save the modified file
                for (int i = 0; i < openFiles.size(); ++i) {
                    if (openFiles[i].filePath == f.filePath && openFiles[i].modified) {
                        ui->tabWidget->setCurrentIndex(i);
                        currentFile = openFiles[i].filePath;
                        on_action_save_triggered();
                        if (openFiles[i].modified) {
                            // Save failed or cancelled
                            return false;
                        }
                        break;
                    }
                }
            } else if (reply == QMessageBox::Cancel) {
                return false;
            }
        }
    }
    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton) {
            int idx = tabBar->tabAt(me->pos());
            if (idx >= 0 && idx < openFiles.size())
                on_tabWidget_tabCloseRequested(idx);
        }
        return QMainWindow::eventFilter(watched, event);
    }

    // Handle title bar dragging via CustomTitleBar
    if (watched == m_titleBar && m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_titleBar->handleMousePress(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            m_titleBar->handleMouseMove(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_titleBar->stopDrag();
            return true;
        }
    }

    // Forward navigation keys to the completion popup when visible
    if (m_completionPopup && m_completionPopup->isVisible() && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
            case Qt::Key_Down:
                m_completionPopup->setCurrentRow(qMin(m_completionPopup->currentRow() + 1, m_completionPopup->count() - 1));
                return true;
            case Qt::Key_Up:
                m_completionPopup->setCurrentRow(qMax(m_completionPopup->currentRow() - 1, 0));
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (m_completionPopup->currentItem())
                    emit m_completionPopup->itemActivated(m_completionPopup->currentItem());
                return true;
            case Qt::Key_Escape:
                hideCompletion();
                return true;
            default:
                break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (checkUnsavedChanges()) {
        autoSaveTimer->stop();
        
        // Save window geometry and state
        QSettings settings;
        settings.setValue("mainWindow/geometry", saveGeometry());
        settings.setValue("mainWindow/state", saveState());
        settings.setValue("mainWindow/bottomPanelVisible", ui->bottomPanelContainer->isVisible());
        settings.setValue("mainWindow/bottomPanelIndex", bottomPanelTabs->currentIndex());
        settings.setValue("ui/sidebarCollapsed", ui->sidebarDrawer->isHidden());
        
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // Apply platform-specific backdrop effects after window is shown
#ifdef Q_OS_WIN
    if (windowHandle()) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        enableMicaEffect(hwnd, isDarkModeEnabled());
    }
#endif
#ifdef Q_OS_MACOS
    // macOS vibrancy - requires Objective-C++ for full NSVisualEffectView support
    // Currently falls back to translucent background
    setAttribute(Qt::WA_TranslucentBackground, false);
#endif
#ifdef Q_OS_LINUX
    // Linux blur support for KDE/KWin compositor
    enableLinuxBlur(winId(), isDarkModeEnabled());
#endif
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif

void MainWindow::enableMicaEffect(HWND hwnd, bool darkMode)
{
    // Try Mica first (Windows 11 22H2+)
    BOOL useMica = TRUE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &useMica, sizeof(useMica));
    
    if (FAILED(hr)) {
        // Mica not available - just enable dark mode title bar
        BOOL darkModeEnabled = darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkModeEnabled, sizeof(darkModeEnabled));
    }
    
    // Enable rounded corners (Windows 11)
    int cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
    
    // Enable dark mode for title bar
    BOOL darkModeEnabled = darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkModeEnabled, sizeof(darkModeEnabled));
}
#endif

#ifdef Q_OS_WIN

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            if (m_titleBar && m_titleBar->isVisible()) {
                QPoint globalPos(msg->lParam & 0xFFFF, (msg->lParam >> 16) & 0xFFFF);
                QPoint localPos = m_titleBar->mapFromGlobal(globalPos);
                if (m_titleBar->rect().contains(localPos)) {
                    *result = HTCAPTION;
                    return true;
                }
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif


void MainWindow::showKeyboardShortcuts()
{
    // Show keyboard shortcuts as a page in the editor area
    for (int i = 0; i < tabBar->count(); ++i) {
        if (static_cast<TabType>(tabBar->tabData(i).toInt()) == TabType::KeyboardShortcuts) {
            tabBar->setCurrentIndex(i);
            return;
        }
    }
    // If keyboard shortcuts tab doesn't exist yet, add it
    int keyboardShortcutsTabIndex = tabBar->addTab(tr("Keys"));
    tabBar->setTabData(keyboardShortcutsTabIndex, static_cast<int>(TabType::KeyboardShortcuts));
    tabBar->setTabButton(keyboardShortcutsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(keyboardShortcutsTabIndex));
    tabBar->setCurrentIndex(keyboardShortcutsTabIndex);
}

Theme MainWindow::themeFromLegacyInt(int legacy) const
{
    if (legacy < 2) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy));
    }
    if (legacy >= 30) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy - 30),
                     ThemeFeatures(ThemeFeature::HighContrast));
    }
    int familyIndex = (legacy - 2) / 4 + 1;
    int remainder = (legacy - 2) % 4;
    ThemeMode mode = static_cast<ThemeMode>(remainder / 2);
    bool hc = (remainder % 2) != 0;
    return Theme(static_cast<ThemeColorFamily>(familyIndex), mode,
                 hc ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures());
}

int MainWindow::themeToLegacyInt(const Theme &theme) const
{
    int family = static_cast<int>(theme.family);
    int mode = static_cast<int>(theme.mode);
    bool hc = theme.features.testFlag(ThemeFeature::HighContrast);

    if (family == 0) {
        return hc ? 30 + mode : mode;
    }
    return 2 + (family - 1) * 4 + mode * 2 + (hc ? 1 : 0);
}

void MainWindow::startLanguageServer(const QString &filePath)
{
    Q_UNUSED(filePath)

    // Language server configuration based on file extension
    static QMap<QString, QPair<QString, QStringList>> serverConfigs = {
        {"cpp",  {"/usr/bin/clangd", QStringList()}},
        {"c",    {"/usr/bin/clangd", QStringList()}},
        {"py",   {"/usr/bin/pyright-langserver", QStringList("--stdio")}},
        {"js",   {"/usr/bin/typescript-language-server", QStringList("--stdio")}},
        {"ts",   {"/usr/bin/typescript-language-server", QStringList("--stdio")}},
        {"java", {"/usr/bin/jdtls", QStringList()}},
        {"rs",   {"/usr/bin/rust-analyzer", QStringList()}},
        {"go",   {"/usr/bin/gopls", QStringList()}},
    };

    QString ext = QFileInfo(filePath).suffix().toLower();
    auto it = serverConfigs.find(ext);
    if (it == serverConfigs.end())
        return;

    QString command = it.value().first;
    QStringList args = it.value().second;

    if (!lspClient->isRunning()) {
        QString rootUri = QUrl::fromLocalFile(projectDir.isEmpty() ? QDir::homePath() : projectDir).toString();
        if (lspClient->startServer(command, args, rootUri)) {
            // Wait for server to be ready, then initialize
            QTimer::singleShot(500, this, [this, filePath]() {
                QString uri = QUrl::fromLocalFile(filePath).toString();
                QString langId = QFileInfo(filePath).suffix().toLower();
                lspClient->initialize(QUrl::fromLocalFile(projectDir.isEmpty() ? QDir::homePath() : projectDir).toString(), langId);
            });
        }
    }
}

void MainWindow::startLanguageServerForProject(const QString &projectPath)
{
    // Determine the best language server for the project based on file types found
    QDir dir(projectPath);
    QStringList filters = {"*.cpp", "*.c", "*.h", "*.hpp", "*.py", "*.js", "*.ts", "*.java", "*.rs", "*.go"};
    QSet<QString> foundExtensions;

    // Scan project directory for source files (limit to root and one level deep)
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setNameFilters(filters);
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &fi : files) {
        foundExtensions.insert(fi.suffix().toLower());
    }

    // Also check subdirectories (one level deep)
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &subdir : subdirs) {
        QDir sub(subdir.absoluteFilePath());
        sub.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        sub.setNameFilters(filters);
        QFileInfoList subFiles = sub.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : subFiles) {
            foundExtensions.insert(fi.suffix().toLower());
        }
    }

    // Pick the most appropriate language server
    QString serverCommand;
    QStringList serverArgs;

    if (foundExtensions.contains("cpp") || foundExtensions.contains("c") || foundExtensions.contains("h")) {
        serverCommand = "/usr/bin/clangd";
    } else if (foundExtensions.contains("py")) {
        serverCommand = "/usr/bin/pyright-langserver";
        serverArgs = QStringList("--stdio");
    } else if (foundExtensions.contains("js") || foundExtensions.contains("ts")) {
        serverCommand = "/usr/bin/typescript-language-server";
        serverArgs = QStringList("--stdio");
    } else if (foundExtensions.contains("java")) {
        serverCommand = "/usr/bin/jdtls";
    } else if (foundExtensions.contains("rs")) {
        serverCommand = "/usr/bin/rust-analyzer";
    } else if (foundExtensions.contains("go")) {
        serverCommand = "/usr/bin/gopls";
    } else {
        // No recognized source files, don't start a server
        return;
    }

    QString rootUri = QUrl::fromLocalFile(projectPath).toString();
    if (lspClient->startServer(serverCommand, serverArgs, rootUri)) {
        QTimer::singleShot(500, this, [this, projectPath, rootUri, filters]() {
            // Determine language ID from first found file
            QString langId = "plaintext";
            QDir dir(projectPath);
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            dir.setNameFilters(filters);
            QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
            if (!files.isEmpty()) {
                langId = files.first().suffix().toLower();
            }
            lspClient->initialize(rootUri, langId);

            // Open only a limited number of source files for diagnostics
            // to avoid overwhelming the language server
            const int maxFilesToOpen = 50;
            int openedCount = 0;
            for (const QFileInfo &fi : files) {
                if (openedCount >= maxFilesToOpen)
                    break;
                QFile file(fi.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    QString content = in.readAll();
                    file.close();
                    QString uri = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
                    lspClient->didOpen(uri, fi.suffix().toLower(), content);
                    openedCount++;
                }
            }
        });
    }
}

void MainWindow::stopLanguageServer()
{
    if (lspClient->isRunning()) {
        lspClient->shutdown();
        lspClient->exit();
    }
}

void MainWindow::onDiagnosticsReceived(const QString &uri, const QList<LspClient::Diagnostic> &diagnostics)
{
    // Update problem panel
    problemPanel->setProblems(uri, diagnostics);

    // Update squiggly underlines in editor
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!editor)
            continue;

        QString tabUri = QUrl::fromLocalFile(openFiles[i].filePath).toString();
        if (tabUri == uri) {
            QList<QTextEdit::ExtraSelection> extraSelections;
            QList<QPair<QTextCursor, QString>> diagnosticTooltips;

            for (const LspClient::Diagnostic &diag : diagnostics) {
                QList<QTextEdit::ExtraSelection> diagSelections =
                    lspClient->createDiagnosticSelections(diag, editor->document());
                extraSelections.append(diagSelections);

                if (!diag.message.isEmpty() && !diagSelections.isEmpty()) {
                    diagnosticTooltips.append({diagSelections.first().cursor, diag.message});
                }
            }
            editor->setDiagnosticTooltips(diagnosticTooltips);
            editor->setDiagnostics(extraSelections);
            break;
        }
    }
}

void MainWindow::onProblemActivated(const QString &fileUri, int line, int column)
{
    // Find the file in open tabs or open it
    QString localPath = QUrl(fileUri).toLocalFile();
    for (int i = 0; i < openFiles.size(); ++i) {
        if (openFiles[i].filePath == localPath) {
            ui->tabWidget->setCurrentIndex(i);
            CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (editor) {
                QTextCursor cursor = editor->textCursor();
                QTextBlock block = editor->document()->findBlockByNumber(line);
                if (block.isValid()) {
                    cursor.setPosition(block.position() + column);
                    editor->setTextCursor(cursor);
                    editor->setFocus();
                }
            }
            return;
        }
    }

    // File not open, open it
    QModelIndex index = fileModel->index(localPath);
    if (index.isValid()) {
        on_fileTreeView_clicked(index);
        // After opening, navigate to the line
        QTimer::singleShot(100, this, [this, line, column]() {
            CodeEditor *editor = getCurrentCodeEditor();
            if (editor) {
                QTextCursor cursor = editor->textCursor();
                QTextBlock block = editor->document()->findBlockByNumber(line);
                if (block.isValid()) {
                    cursor.setPosition(block.position() + column);
                    editor->setTextCursor(cursor);
                    editor->setFocus();
                }
            }
        });
    }
}

void MainWindow::onProblemsFilterChanged(ProblemPanel::Filter filter)
{
    Q_UNUSED(filter)
    // Could update status bar or other UI elements based on filter
}

void MainWindow::on_action_git_commit_triggered()
{
    // Check if git is available
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }

    bool ok;
    QString message = QInputDialog::getText(this, tr("Git Commit"), tr("Commit message:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !message.isEmpty()) {
        QProcess gitProcess(this);
        gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        gitProcess.start("git", {"commit", "-m", message});
        if (gitProcess.waitForFinished(10000)) {
            QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
            QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
            gitPanel->setOutput(output + error);
        } else {
            gitPanel->setOutput(tr("Failed to run git commit. The operation may have timed out."));
        }
        // Close other panels that occupy the same area (block signals to prevent recursion)
        if (terminalButton->isChecked()) {
            QSignalBlocker blocker(terminalButton);
            terminalButton->setChecked(false);
            if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
                editorStack->setCurrentIndex(m_previousEditorStackIndex);
            }
        }
        if (placeholderButton->isChecked()) {
            QSignalBlocker blocker(placeholderButton);
            placeholderButton->setChecked(false);
        }
        if (problemPanel->isVisible()) {
            problemPanel->hide();
            problemsButton->setChecked(false);
        }
        showGitPanel();
    }
}

void MainWindow::on_action_git_push_triggered()
{
    // Check if git is available
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }

    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"push"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
    } else {
        gitPanel->setOutput(tr("Failed to run git push. The operation may have timed out."));
    }
    // Close other panels that occupy the same area (block signals to prevent recursion)
    if (terminalButton->isChecked()) {
        QSignalBlocker blocker(terminalButton);
        terminalButton->setChecked(false);
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
    if (placeholderButton->isChecked()) {
        QSignalBlocker blocker(placeholderButton);
        placeholderButton->setChecked(false);
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
        problemsButton->setChecked(false);
    }
    showGitPanel();
}

void MainWindow::on_placeholderButton_clicked(bool checked)
{
    if (checked) {
        // Show todo panel, replacing the editor
        m_previousEditorStackIndex = editorStack->currentIndex();
        // Close other panels that occupy the same area (block signals to prevent recursion)
        if (editorStack->currentWidget() == terminalPanel) {
            QSignalBlocker blocker(terminalButton);
            terminalButton->setChecked(false);
        }
        if (problemPanel->isVisible()) {
            problemPanel->hide();
            problemsButton->setChecked(false);
        }
        if (gitPanel->isVisible()) {
            gitPanel->hide();
        }
        editorStack->setCurrentWidget(todoPanel);
    } else {
        // Todo panel was unchecked, restore previous view
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
}

void MainWindow::onEditorTextChanged()
{
    // Debounced: send text change to LSP server
    if (currentFile.isEmpty() || !lspClient->isRunning())
        return;

    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;

    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->didChange(uri, editor->toPlainText());

    if (placeholderButton->isChecked()) {
        todoPanel->parseDocument(editor->toPlainText(), currentFile);
    }
}

void MainWindow::requestHover()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
        return;
    QTextCursor cursor = editor->textCursor();
    LspClient::Position pos;
    pos.line = cursor.blockNumber();
    pos.character = cursor.positionInBlock();
    lspClient->hover(QUrl::fromLocalFile(currentFile).toString(), pos);
}

void MainWindow::onUpdateAvailable(const QString &version, const QString &downloadUrl)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Update Available"),
        tr("Version %1 is available. Would you like to download it?\n\n%2")
            .arg(version).arg(downloadUrl),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(downloadUrl));
    }
}

void MainWindow::onUpdateCheckFailed(const QString &error)
{
    qDebug() << "Update check failed:" << error;
    // Silent fail - don't show error to user for background update check
}

void MainWindow::on_action_git_pull_triggered()
{
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }
    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"pull"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
        gitPanel->detectMergeConflicts();
    } else {
        gitPanel->setOutput(tr("Failed to run git pull. The operation may have timed out."));
    }
    // Close other panels that occupy the same area (block signals to prevent recursion)
    if (terminalButton->isChecked()) {
        QSignalBlocker blocker(terminalButton);
        terminalButton->setChecked(false);
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
    if (placeholderButton->isChecked()) {
        QSignalBlocker blocker(placeholderButton);
        placeholderButton->setChecked(false);
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
        problemsButton->setChecked(false);
    }
    showGitPanel();
}

void MainWindow::on_action_git_fetch_triggered()
{
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }
    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"fetch"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
    } else {
        gitPanel->setOutput(tr("Failed to run git fetch. The operation may have timed out."));
    }
    // Close other panels that occupy the same area (block signals to prevent recursion)
    if (terminalButton->isChecked()) {
        QSignalBlocker blocker(terminalButton);
        terminalButton->setChecked(false);
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
    if (placeholderButton->isChecked()) {
        QSignalBlocker blocker(placeholderButton);
        placeholderButton->setChecked(false);
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
        problemsButton->setChecked(false);
    }
    showGitPanel();
}

void MainWindow::on_action_find_triggered()
{
    showEditorInterface();
    showSearchBar(true);
    findReplaceBar->setReplaceVisible(false);
    findReplaceBar->setEditor(getCurrentCodeEditor());
    findReplaceBar->findNext();
}

void MainWindow::on_action_replace_triggered()
{
    showEditorInterface();
    showSearchBar(true);
    findReplaceBar->setReplaceVisible(true);
    findReplaceBar->setEditor(getCurrentCodeEditor());
    findReplaceBar->findNext();
}

void MainWindow::on_action_project_search_triggered()
{
    QString root = projectDir.isEmpty() ? QDir::homePath() : projectDir;
    findReplaceBar->setVisible(false);
    bottomPanelTabs->setCurrentIndex(2);
    bottomPanelStack->setCurrentIndex(2);
    projectSearchPanel->show();
    ui->bottomPanelContainer->show();
    projectSearchPanel->setRootPath(root);
    projectSearchPanel->search(QString(), root);
}

void MainWindow::on_action_command_palette_triggered()
{
    if (!commandPalette) return;
    commandPalette->registerCommand({"open-project", tr("Open Project..."), "Ctrl+Shift+O", [this]() { on_action_open_project_triggered(); }});
    commandPalette->registerCommand({"open-file", tr("Open File..."), "Ctrl+O", [this]() { on_action_open_file_triggered(); }});
    commandPalette->registerCommand({"save", tr("Save"), "Ctrl+S", [this]() { on_action_save_triggered(); }});
    commandPalette->registerCommand({"save-as", tr("Save As..."), "Ctrl+Shift+S", [this]() { on_action_save_as_triggered(); }});
    commandPalette->registerCommand({"new-file", tr("New File..."), "Ctrl+N", [this]() { on_action_add_file_directory_triggered(); }});
    commandPalette->registerCommand({"undo", tr("Undo"), "Ctrl+Z", [this]() { on_action_Undo_triggered(); }});
    commandPalette->registerCommand({"redo", tr("Redo"), "Ctrl+Y", [this]() { on_action_Redo_triggered(); }});
    commandPalette->registerCommand({"cut", tr("Cut"), "Ctrl+X", [this]() { on_actionCu_t_triggered(); }});
    commandPalette->registerCommand({"copy", tr("Copy"), "Ctrl+C", [this]() { on_action_copy_triggered(); }});
    commandPalette->registerCommand({"paste", tr("Paste"), "Ctrl+V", [this]() { on_action_Paste_triggered(); }});
    commandPalette->registerCommand({"find", tr("Find in File"), "Ctrl+F", [this]() { on_action_find_triggered(); }});
    commandPalette->registerCommand({"replace", tr("Find and Replace"), "Ctrl+H", [this]() { on_action_replace_triggered(); }});
    commandPalette->registerCommand({"project-search", tr("Project Search"), "Ctrl+Shift+F", [this]() { on_action_project_search_triggered(); }});
    commandPalette->registerCommand({"git-commit", tr("Git Commit..."), "", [this]() { on_action_git_commit_triggered(); }});
    commandPalette->registerCommand({"git-push", tr("Git Push..."), "", [this]() { on_action_git_push_triggered(); }});
    commandPalette->registerCommand({"git-pull", tr("Git Pull..."), "", [this]() { on_action_git_pull_triggered(); }});
    commandPalette->registerCommand({"git-fetch", tr("Git Fetch"), "", [this]() { on_action_git_fetch_triggered(); }});
    commandPalette->registerCommand({"find-references", tr("Find References"), "", [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
            return;
        QTextCursor c = editor->textCursor();
        lspClient->references(QUrl::fromLocalFile(currentFile).toString(),
                              LspClient::Position{c.blockNumber(), c.positionInBlock()});
    }});
    commandPalette->registerCommand({"theme", tr("Theme"), "Ctrl+T", [this]() { on_action_theme_triggered(); }});
    commandPalette->registerCommand({"editor-settings", tr("Editor Settings"), "", [this]() { on_action_editor_settings_triggered(); }});
    commandPalette->registerCommand({"shortcuts", tr("Keyboard Shortcuts"), "Ctrl+K", [this]() { showKeyboardShortcuts(); }});
    commandPalette->registerCommand({"updates", tr("Check for Updates..."), "", [this]() { on_action_check_updates_triggered(); }});
    commandPalette->registerCommand({"plugins", tr("Manage Plugins..."), "", [this]() { on_action_manage_plugins_triggered(); }});
    commandPalette->execute();
}

void MainWindow::on_action_format_document_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->formatting(uri);
    qDebug() << "Formatting requested for" << currentFile;
}

void MainWindow::on_action_go_to_definition_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    LspClient::Position pos;
    pos.line = cursor.blockNumber();
    pos.character = cursor.positionInBlock();
    lspClient->definition(uri, pos);
    qDebug() << "Go to definition requested for" << currentFile;
}

void MainWindow::on_action_go_to_declaration_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    LspClient::Position pos;
    pos.line = cursor.blockNumber();
    pos.character = cursor.positionInBlock();
    lspClient->declaration(uri, pos);
}

void MainWindow::on_action_go_to_type_definition_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    LspClient::Position pos;
    pos.line = cursor.blockNumber();
    pos.character = cursor.positionInBlock();
    lspClient->typeDefinition(uri, pos);
}

void MainWindow::on_action_go_to_implementation_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    LspClient::Position pos;
    pos.line = cursor.blockNumber();
    pos.character = cursor.positionInBlock();
    lspClient->implementation(uri, pos);
}

void MainWindow::on_action_show_document_symbols_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->documentSymbol(uri);
    qDebug() << "Document symbols requested for" << currentFile;
}

// Debug methods
void MainWindow::on_action_run_debug_triggered()
{
    loadDebugConfigurations();
    
    RunDialog dialog(this);
    dialog.setConfigurations(debugConfigManager->configurations());
    
    if (dialog.exec() == QDialog::Accepted) {
        QList<DebugConfiguration> configs = dialog.configurations();
        DebugConfiguration selected = dialog.selectedConfiguration();
        QString mode = dialog.selectedMode();

        // Persist any Add/Edit/Delete changes to launch.json
        if (!projectDir.isEmpty()) {
            QString configPath = projectDir + "/.vscode/launch.json";
            QDir().mkpath(projectDir + "/.vscode");
            debugConfigManager->setConfigurations(configs);
            debugConfigManager->saveToFile(configPath);
        }

        if (mode == "debug") {
            startDebug(selected.name);
        } else {
            // Run without debugging: launch the program directly
            if (selected.program.isEmpty()) {
                QMessageBox::warning(this, tr("Run"),
                    tr("No program specified for configuration \"%1\".").arg(selected.name));
                return;
            }
            bool started = QProcess::startDetached(
                selected.program, selected.args, selected.cwd.isEmpty() ? QDir::currentPath() : selected.cwd);
            if (!started)
                QMessageBox::warning(this, tr("Run"),
                    tr("Failed to start program: %1").arg(selected.program));
        }
    }
}

void MainWindow::on_action_stop_debug_triggered()
{
    if (m_isDebugging) {
        stopDebug();
    }
}

void MainWindow::on_action_step_over_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->next();
    }
}

void MainWindow::on_action_step_into_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->stepIn();
    }
}

void MainWindow::on_action_step_out_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->stepOut();
    }
}

void MainWindow::on_action_continue_debug_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->continueDebug();
    }
}

void MainWindow::on_action_toggle_breakpoint_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;
    
    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber() + 1;
    QString key = currentFile + ":" + QString::number(line);
    
    bool enabled = editor->breakpointLines().contains(line);
    if (enabled) {
        editor->setBreakpointLine(line, false);
        m_breakpointConditions.remove(key);
    } else {
        bool ok = false;
        QString condition = QInputDialog::getText(this, tr("Conditional Breakpoint"),
            tr("Breakpoint condition (leave empty to break always):"), QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return; // cancelled
        editor->setBreakpointLine(line, true);
        if (!condition.isEmpty())
            m_breakpointConditions[key] = condition;
    }
    
    // If debugging, update breakpoints in DAP
    if (m_isDebugging && dapClient->isRunning()) {
        updateDapBreakpoints();
    }
}

void MainWindow::onCompletionReceived(const QJsonArray &items, int)
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || items.isEmpty()) {
        hideCompletion();
        return;
    }
    if (!m_completionPopup) {
        m_completionPopup = new QListWidget(this);
        m_completionPopup->setWindowFlags(Qt::Popup);
        connect(m_completionPopup, &QListWidget::itemActivated, this, [this, editor](QListWidgetItem *item) {
            QString text = item->data(Qt::UserRole).toString();
            QTextCursor c = editor->textCursor();
            c.select(QTextCursor::WordUnderCursor);
            c.insertText(text);
            editor->setTextCursor(c);
            hideCompletion();
            editor->setFocus();
        });
    }
    m_completionPopup->clear();
    for (const QJsonValue &v : items) {
        QJsonObject it = v.toObject();
        QString label = it["label"].toString();
        QString detail = it["detail"].toString();
        QString insert = it.contains("insertText") ? it["insertText"].toString() : label;
        QListWidgetItem *item = new QListWidgetItem(detail.isEmpty() ? label : label + "  — " + detail);
        item->setData(Qt::UserRole, insert);
        m_completionPopup->addItem(item);
    }
    QPoint pos = editor->mapToGlobal(editor->cursorRect().bottomLeft());
    m_completionPopup->move(pos);
    m_completionPopup->setCurrentRow(0);
    m_completionPopup->show();
}

void MainWindow::hideCompletion()
{
    if (m_completionPopup)
        m_completionPopup->hide();
}

void MainWindow::updateDapBreakpoints()
{
    QMap<QString, QList<DapClient::Breakpoint>> perFile;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!editor)
            continue;
        QString filePath = (i < openFiles.size()) ? openFiles[i].filePath : QString();
        if (filePath.isEmpty())
            continue;
        for (int line : editor->breakpointLines().values()) {
            DapClient::Breakpoint bp;
            bp.line = line;
            QString key = filePath + ":" + QString::number(line);
            if (m_breakpointConditions.contains(key))
                bp.condition = m_breakpointConditions.value(key);
            perFile[filePath].append(bp);
        }
    }
    for (auto it = perFile.constBegin(); it != perFile.constEnd(); ++it) {
        dapClient->setBreakpointsWithConditions(it.key(), it.value());
    }
}

void MainWindow::startDebug(const QString &configName)
{
    DebugConfiguration config = debugConfigManager->configuration(configName);
    if (config.name.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Debug configuration not found: %1").arg(configName));
        return;
    }
    
    // Start the debugger adapter (e.g., lldb, gdb, or a DAP server)
    QString debuggerPath = config.debuggerPath;
    if (debuggerPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No debugger path specified in configuration."));
        return;
    }
    
    if (!dapClient->startServer(debuggerPath, QStringList())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to start debugger adapter."));
        return;
    }
    
    // Initialize DAP
    dapClient->initialize(config.program, config.args, config.cwd);
    
    m_isDebugging = true;
    
    // Show debug panel
    ui->bottomPanelContainer->show();
    bottomPanelTabs->setCurrentIndex(3);
    bottomPanelStack->setCurrentIndex(3);
    debugPanel->show();
}

void MainWindow::stopDebug()
{
    if (dapClient->isRunning()) {
        dapClient->disconnect();
        dapClient->stopServer();
    }
    
    m_isDebugging = false;
    
    // Clear debug highlights
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor) {
        editor->clearBreakpoints();
    }
    
    debugPanel->hide();
    ui->bottomPanelContainer->hide();
}

void MainWindow::loadDebugConfigurations()
{
    QString configPath = projectDir + "/.vscode/launch.json";
    if (QFile::exists(configPath)) {
        debugConfigManager->loadFromFile(configPath);
    } else {
        // Create a default configuration
        DebugConfiguration defaultConfig;
        defaultConfig.name = "Default";
        defaultConfig.type = "cppdbg";
        defaultConfig.request = "launch";
        defaultConfig.program = "";
        defaultConfig.args = QStringList();
        defaultConfig.cwd = projectDir;
        debugConfigManager->addConfiguration(defaultConfig);
    }
}

void MainWindow::onBreakpointToggled(int line, bool enabled)
{
    Q_UNUSED(line)
    Q_UNUSED(enabled)
    // Breakpoint toggled in editor - handled in on_action_toggle_breakpoint_triggered
}

void MainWindow::onDapInitialized()
{
    qDebug() << "DAP initialized";
    // Send breakpoints after initialization
    updateDapBreakpoints();
    
    // Launch the program
    dapClient->launch();
}

void MainWindow::onDapStopped(const QString &reason)
{
    qDebug() << "DAP stopped:" << reason;
    
    // Request stack trace
    dapClient->stackTrace(dapClient->currentThreadId());

    // Clear previous debug state in the panel
    debugPanel->clearVariables();
    
    // Show debug panel if not visible
    if (!debugPanel->isVisible()) {
        ui->bottomPanelContainer->show();
        bottomPanelTabs->setCurrentIndex(3);
        bottomPanelStack->setCurrentIndex(3);
        debugPanel->show();
    }
}

void MainWindow::onDapContinued()
{
    qDebug() << "DAP continued";
    // Clear current line highlight
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor) {
        editor->highlightCurrentLine(-1);
    }
}

void MainWindow::onStackTraceReceived(int threadId, const QList<DapClient::StackFrame> &frames)
{
    Q_UNUSED(threadId)
    qDebug() << "Stack trace received with" << frames.size() << "frames";
    
    debugPanel->setStack(frames);
    
    // Highlight the current line from the top frame
    if (!frames.isEmpty()) {
        m_currentFrameId = frames[0].id;
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && frames[0].source.path == currentFile) {
            editor->highlightCurrentLine(frames[0].line);
        }

        // Request scopes for the top frame
        dapClient->scopes(frames[0].id);
    }
}

void MainWindow::onScopesReceived(int frameId, const QList<DapClient::Scope> &scopes)
{
    Q_UNUSED(frameId)
    qDebug() << "Scopes received:" << scopes.size();
    
    // Request variables for each scope
    for (const DapClient::Scope &scope : scopes) {
        if (scope.variablesReference > 0) {
            dapClient->variables(scope.variablesReference);
        }
    }
}

void MainWindow::onVariablesReceived(int variablesReference, const QList<DapClient::Variable> &variables)
{
    Q_UNUSED(variablesReference)
    debugPanel->addVariables(variables);
}

void MainWindow::onDapLogMessage(const QString &msg)
{
    qDebug() << "DAP:" << msg;
}
