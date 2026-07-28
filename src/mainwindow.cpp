#include <QShortcut>
#include <QCloseEvent>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "pluginmanagerdialog.h"
#include "version.h"
#include "findreplace.h"
#include "projectsearch.h"
#include "commandpalette.h"
#include "debugpanel.h"
#include "debugconfiguration.h"
#include "rundialog.h"
#include "minimap.h"
#include "splitmanager.h"
#include "breadcrumb.h"
#include "aiinlinecompletion.h"
#include "httpclientpanel.h"
#include "codeactionui.h"
#include "sqliteviewer.h"
#include "plugins/api/uiapi.h"
#include "plugins/api/editorapi.h"
#include "plugins/api/notificationapi.h"
#include "windowanimator.h"
#include "thememanager.h"
#include "themeicons.h"
#include "foldmanager.h"
#include "bookmarkmanager.h"
#include "snippetmanager.h"
#include "filewatcher.h"
#include "outlinepanel.h"
#include "regextester.h"
#include "dataformatter.h"
#include "markdownpreview.h"
#include "globalreplacepreview.h"
#include "rust_adapter.h"

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
#include <QFrame>
#include <QMenu>
#include "windowanimator.h"
#include "thememanager.h"

#ifdef Q_OS_LINUX
#include <QApplication>
#include <QWidget>
#include <QTime>
#include <QTimer>
#include <QPainter>
#include <QDebug>

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
    , updater(RustBackend::instance()->updater())
    , configValidator(RustBackend::instance()->configValidator())
    , lspClient(RustBackend::instance()->lspClient())
    , pluginManager(RustBackend::instance()->pluginManager())
    , pluginManagerDialog(new PluginManagerDialog(pluginManager, nullptr, this))
    , m_previousEditorStackIndex(0)
    , dapClient(RustBackend::instance()->dapClient())
    , debugPanel(new DebugPanel(this))
    , debugConfigManager(std::make_unique<DebugConfigurationManager>())
    , m_isDebugging(false)
    , m_workspace(RustBackend::instance()->workspace())
    , m_minimap(nullptr)
    , m_inspectorDrawer(nullptr)
    , m_universalSearch(nullptr)
    , m_splitManager(new SplitManager(this))
    , m_breadcrumb(nullptr)
    , m_aiInline(new AiInlineCompletion(this))
    , m_httpClient(new HttpClientPanel(this))
    , m_codeActionCtrl(new CodeActionController(this))
    , m_sqliteViewer(new SqliteViewerPanel(this))
    , m_pluginRegistry(RustBackend::instance()->pluginRegistry())
    , m_zenMode(nullptr)
    , m_fileWatcher(nullptr)
    , m_outlinePanel(nullptr)
    , m_regexTester(nullptr)
    , m_dataFormatter(nullptr)
    , m_markdownPreview(nullptr)
    , m_globalReplacePreview(nullptr)
    , m_sessionManager(nullptr)
    , m_refactoringManager(nullptr)
    , m_codeLensManager(nullptr)
    , m_gitBlame(nullptr)
    , m_statusBarWidget(nullptr)
    , m_encodingManager(nullptr)
    , m_diffViewer(nullptr)
    , m_notificationCenter(nullptr)
    , m_gitStash(nullptr)
    , m_gitRebase(nullptr)
    , m_taskRunnerUI(nullptr)
    , m_bookmarkPanel(nullptr)
    , m_pluginMarketplace(nullptr)
    , m_themeMarketplace(nullptr)
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
    
    // Give editor container rounded corners
    ui->editorContainer->setStyleSheet("QWidget#editorContainer { background-color: palette(window); border-radius: 14px; }");
    
    // Add margins to main layout so container rounded corners are visible
    if (QHBoxLayout *hLayout = qobject_cast<QHBoxLayout*>(ui->centralwidget->layout())) {
        hLayout->setContentsMargins(6, 6, 6, 6);
        hLayout->setSpacing(6);
    }
    ui->centralwidget->setStyleSheet("QWidget#centralwidget { background-color: palette(mid); }");

    ui->sidebarDrawer->setStyleSheet(R"(
        QWidget#sidebarDrawer {
            background-color: palette(window);
            border-radius: 14px;
        }
    )");

    // Round bottom panel container corners so they don't appear sharp
    ui->bottomPanelContainer->setStyleSheet(R"(
        QWidget#bottomPanelContainer {
            background-color: palette(window);
            border-top: 1px solid palette(mid);
            border-bottom-left-radius: 14px;
            border-bottom-right-radius: 14px;
        }
    )");
    
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

    // Enable mouse tracking for resize edge detection
    setMouseTracking(true);
    if (centralWidget())
        centralWidget()->setMouseTracking(true);

    // Connect unified title bar signals
    connect(m_titleBar, &CustomTitleBar::sidebarToggleClicked, this, [this]() {
        // sidebarDrawer has zero width when collapsed (not hidden), so check width
        bool isCollapsed = ui->sidebarDrawer->maximumWidth() == 0 || ui->sidebarDrawer->width() < 10;
        setSidebarCollapsed(!isCollapsed);
    });
    connect(m_titleBar, &CustomTitleBar::inspectorToggleClicked, this, [this]() {
        toggleInspector();
    });
    connect(m_titleBar, &CustomTitleBar::settingsClicked, this, [this]() {
        on_action_editor_settings_triggered();
    });
    // ---- Universal Search ----
    m_universalSearch = new UniversalSearchPopup(m_titleBar->searchField, this);

    // Connect file search results to open files
    connect(m_universalSearch, &UniversalSearchPopup::fileOpenRequested, this, [this](const QString &path) {
        openFileInTab(path);
    });

    // Register commands in universal search
    {
        using Cat = SearchResult::Category;
        auto regCmd = [this](const QString &label, const QString &sub, auto fn, Cat cat = Cat::Command) {
            m_universalSearch->registerResult({cat, label, sub, fn});
        };

        // Commands
        regCmd(tr("Open Project..."),         tr("Ctrl+Shift+O"), [this]() { on_action_open_project_triggered(); });
        regCmd(tr("Open File..."),            tr("Ctrl+O"),      [this]() { on_action_open_file_triggered(); });
        regCmd(tr("Save"),                    tr("Ctrl+S"),      [this]() { on_action_save_triggered(); });
        regCmd(tr("Save As..."),              tr("Ctrl+Shift+S"),[this]() { on_action_save_as_triggered(); });
        regCmd(tr("New File"),                tr("Ctrl+N"),      [this]() { on_action_add_file_directory_triggered(); });
        regCmd(tr("Undo"),                    tr("Ctrl+Z"),      [this]() { on_action_Undo_triggered(); });
        regCmd(tr("Redo"),                    tr("Ctrl+Y"),      [this]() { on_action_Redo_triggered(); });
        regCmd(tr("Cut"),                     tr("Ctrl+X"),      [this]() { on_actionCu_t_triggered(); });
        regCmd(tr("Copy"),                    tr("Ctrl+C"),      [this]() { on_action_copy_triggered(); });
        regCmd(tr("Paste"),                   tr("Ctrl+V"),      [this]() { on_action_Paste_triggered(); });
        regCmd(tr("Find in File"),            tr("Ctrl+F"),      [this]() { on_action_find_triggered(); });
        regCmd(tr("Find and Replace"),        tr("Ctrl+H"),      [this]() { on_action_replace_triggered(); });
        regCmd(tr("Project Search"),          tr("Ctrl+Shift+F"),[this]() { on_action_project_search_triggered(); });
        regCmd(tr("Command Palette"),         tr("Ctrl+Shift+P"),[this]() { on_action_command_palette_triggered(); });
        regCmd(tr("Git Commit"),              QString(),          [this]() { on_action_git_commit_triggered(); });
        regCmd(tr("Git Push"),                QString(),          [this]() { on_action_git_push_triggered(); });
        regCmd(tr("Git Pull"),                QString(),          [this]() { on_action_git_pull_triggered(); });
        regCmd(tr("Git Fetch"),               QString(),          [this]() { on_action_git_fetch_triggered(); });
        regCmd(tr("Format Document"),          tr("Ctrl+Shift+I"),[this]() { on_action_format_document_triggered(); });
        regCmd(tr("Go to Definition"),        tr("F12"),         [this]() { on_action_go_to_definition_triggered(); });
        regCmd(tr("Toggle Breakpoint"),       tr("F9"),          [this]() { on_action_toggle_breakpoint_triggered(); });
        regCmd(tr("Run / Debug"),             tr("F5"),          [this]() { on_action_run_debug_triggered(); });
        regCmd(tr("Step Over"),               tr("F10"),         [this]() { on_action_step_over_triggered(); });
        regCmd(tr("Step Into"),               tr("F11"),         [this]() { on_action_step_into_triggered(); });
        regCmd(tr("Step Out"),                tr("Shift+F11"),   [this]() { on_action_step_out_triggered(); });
        regCmd(tr("Continue"),                tr("Ctrl+F5"),     [this]() { on_action_continue_debug_triggered(); });
        regCmd(tr("Manage Plugins..."),       QString(),          [this]() { on_action_manage_plugins_triggered(); });
        regCmd(tr("Check for Updates..."),    QString(),          [this]() { on_action_check_updates_triggered(); });
        regCmd(tr("Keyboard Shortcuts"),      tr("Ctrl+K"),      [this]() { showKeyboardShortcuts(); });
        regCmd(tr("About Scriptura"),         QString(),          [this]() { on_action_about_triggered(); });

        // Settings pages — all unified into one scrollable page
        regCmd(tr("Settings"),                 QString(), [this]() { on_action_editor_settings_triggered(); }, Cat::Setting);
        regCmd(tr("Theme Settings"),           QString(), [this]() { on_action_theme_triggered(); }, Cat::Setting);
        regCmd(tr("Update Settings"),          QString(), [this]() { on_action_editor_settings_triggered(); }, Cat::Setting);

        // Theme quick-switches
        struct ThemeEntry { QString name; ThemeColorFamily family; ThemeMode mode; };
        ThemeEntry themes[] = {
            {tr("Default Light"), ThemeColorFamily::Default, ThemeMode::Light},
            {tr("Default Dark"),  ThemeColorFamily::Default, ThemeMode::Dark},
            {tr("Blue Light"),    ThemeColorFamily::Blue,   ThemeMode::Light},
            {tr("Blue Dark"),     ThemeColorFamily::Blue,   ThemeMode::Dark},
            {tr("Green Light"),   ThemeColorFamily::Green,  ThemeMode::Light},
            {tr("Green Dark"),    ThemeColorFamily::Green,  ThemeMode::Dark},
            {tr("Red Light"),     ThemeColorFamily::Red,    ThemeMode::Light},
            {tr("Red Dark"),      ThemeColorFamily::Red,    ThemeMode::Dark},
            {tr("Cyan Light"),    ThemeColorFamily::Cyan,   ThemeMode::Light},
            {tr("Cyan Dark"),     ThemeColorFamily::Cyan,   ThemeMode::Dark},
            {tr("Violet Light"),  ThemeColorFamily::Violet, ThemeMode::Light},
            {tr("Violet Dark"),   ThemeColorFamily::Violet, ThemeMode::Dark},
        };
        for (const auto &t : themes) {
            Theme targetTheme(t.family, t.mode);
            regCmd(t.name, tr("Theme"), [this, targetTheme]() {
                if (targetTheme != selectedTheme) {
                    selectedTheme = targetTheme;
                    applyTheme(selectedTheme);
                    QSettings s;
                    s.setValue("theme/selected", themeToLegacyInt(selectedTheme));
                }
            }, Cat::Theme);
        }
    }

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

    pluginManager->loadPlugins(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins");
    pluginManager->loadPlugins(QCoreApplication::applicationDirPath() + "/plugins");

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
    // Enable right-click context menu on the file tree
    ui->fileTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->fileTreeView, &QTreeView::customContextMenuRequested,
            this, &MainWindow::on_fileTreeView_contextMenu);

    // Wire file model into universal search for file name searching
    if (m_universalSearch) {
        m_universalSearch->setFileModel(fileModel, QDir::homePath());
    }

    editorStack = ui->editorStack;
    bottomPanelStack = ui->bottomPanelStack;
    tabBar = ui->tabBar;
    bottomPanelTabs = ui->bottomPanelTabs;

    // In-editor welcome widget removed — replaced by standalone WelcomeMenuScreen
    // shown between splash and main window startup.
    editorStack->addWidget(ui->tabWidget);
    editorStack->addWidget(todoPanel);
    editorStack->addWidget(terminalPanel);

    // Plugin registry - load user-configured URL
    registryUrl = QSettings().value("plugin/registryUrl", "https://raw.githubusercontent.com/jason1015-coder/scriptura/main/plugin-registry.json").toString();

    // Create unified scrollable settings page (tab added when user opens settings)
    unifiedSettingsWidget = createUnifiedSettingsWidget();
    editorStack->addWidget(unifiedSettingsWidget);

    bottomPanelStack->addWidget(problemPanel);
    bottomPanelStack->addWidget(gitPanel);
    problemPanel->hide();
    gitPanel->hide();

    // Create panels that need to be added to bottom stack
    projectSearchPanel = new ProjectSearchPanel(this);
    debugPanel = new DebugPanel(this);

    // Top toolbar setup — buttons now live in the unified title bar or editor area
    sidebarToggleButton = new QToolButton(this);
    ThemeIcons::instance()->setIcon(sidebarToggleButton, ":/icons/sidebar-toggle.svg");
    sidebarToggleButton->setIconSize(QSize(20, 20));
    sidebarToggleButton->setToolTip(tr("Toggle Sidebar"));
    sidebarToggleButton->setCheckable(true);
    sidebarToggleButton->setChecked(true);
    sidebarToggleButton->setFixedSize(32, 32);
    sidebarToggleButton->hide(); // Managed by unified title bar

    goUpButton = new QToolButton(this);
    ThemeIcons::instance()->setIcon(goUpButton, ":/icons/go-up.svg");
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
    ThemeIcons::instance()->setIcon(settingsButton, ":/icons/settings.svg");
    settingsButton->setIconSize(QSize(20, 20));
    settingsButton->setToolTip(tr("Editor Settings"));
    settingsButton->setFixedSize(32, 32);
    settingsButton->hide(); // Accessible via title bar menu

    // Sidebar icon buttons (bottom of drawer)
    fileTreeToggleButton = new QToolButton(ui->sidebarDrawer);
    ThemeIcons::instance()->setIcon(fileTreeToggleButton, ":/icons/file-tree.svg");
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
    ThemeIcons::instance()->setIcon(placeholderButton, ":/icons/todo.svg");
    placeholderButton->setIconSize(QSize(20, 20));
    placeholderButton->setToolTip(tr("Todo"));
    placeholderButton->setCheckable(true);
    placeholderButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(placeholderButton);

    terminalButton = new QToolButton(iconBar);
    ThemeIcons::instance()->setIcon(terminalButton, ":/icons/terminal.svg");
    terminalButton->setIconSize(QSize(20, 20));
    terminalButton->setToolTip(tr("Terminal"));
    terminalButton->setCheckable(true);
    terminalButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(terminalButton);

    problemsButton = new QToolButton(iconBar);
    ThemeIcons::instance()->setIcon(problemsButton, ":/icons/problems.svg");
    problemsButton->setIconSize(QSize(20, 20));
    problemsButton->setToolTip(tr("Problems"));
    problemsButton->setCheckable(true);
    problemsButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(problemsButton);

    gitButton = new QToolButton(iconBar);
    ThemeIcons::instance()->setIcon(gitButton, ":/icons/git.svg");
    gitButton->setIconSize(QSize(20, 20));
    gitButton->setToolTip(tr("Git"));
    gitButton->setCheckable(true);
    gitButton->setFixedSize(32, 32);
    iconBarLayout->addWidget(gitButton);

    ui->sidebarDrawerLayout->addWidget(iconBar);

    // --- Right-side Inspector Drawer ---
    m_inspectorDrawer = new QWidget(this);
    m_inspectorDrawer->setObjectName("inspectorDrawer");
    m_inspectorDrawer->setMinimumWidth(0);
    m_inspectorDrawer->setMaximumWidth(0);
    m_inspectorDrawer->setVisible(true);

    QVBoxLayout *inspectorLayout = new QVBoxLayout(m_inspectorDrawer);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(0);

    // Header bar
    QWidget *inspectorHeader = new QWidget(m_inspectorDrawer);
    inspectorHeader->setObjectName("inspectorHeader");
    QHBoxLayout *headerLayout = new QHBoxLayout(inspectorHeader);
    headerLayout->setContentsMargins(12, 8, 8, 8);
    headerLayout->setSpacing(4);

    QLabel *inspectorTitle = new QLabel(tr("Assistant"), inspectorHeader);
    inspectorTitle->setObjectName("inspectorTitle");
    QFont titleFont = inspectorTitle->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    inspectorTitle->setFont(titleFont);

    QPushButton *inspectorCloseBtn = new QPushButton(inspectorHeader);
    inspectorCloseBtn->setObjectName("inspectorCloseBtn");
    inspectorCloseBtn->setFixedSize(24, 24);
    ThemeIcons::instance()->setIcon(inspectorCloseBtn, ":/icons/close.svg");
    inspectorCloseBtn->setFlat(true);
    inspectorCloseBtn->setCursor(Qt::ArrowCursor);
    inspectorCloseBtn->setToolTip(tr("Close inspector"));
    connect(inspectorCloseBtn, &QPushButton::clicked, this, &MainWindow::toggleInspector);

    headerLayout->addWidget(inspectorTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(inspectorCloseBtn);

    inspectorLayout->addWidget(inspectorHeader);

    // Separator line
    QFrame *separator = new QFrame(m_inspectorDrawer);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName("inspectorSeparator");
    inspectorLayout->addWidget(separator);

    // Content area
    QWidget *inspectorContent = new QWidget(m_inspectorDrawer);
    inspectorContent->setObjectName("inspectorContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(inspectorContent);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(12);
    contentLayout->setAlignment(Qt::AlignCenter);

    // Sparkle icon placeholder
    QLabel *iconLabel = new QLabel(inspectorContent);
    iconLabel->setObjectName("assistantIcon");
    iconLabel->setPixmap(QIcon(":/icons/app-icon.svg").pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *aiLabel = new QLabel(tr("AI Assistant is in development"), inspectorContent);
    aiLabel->setObjectName("aiStatusLabel");
    aiLabel->setAlignment(Qt::AlignCenter);
    aiLabel->setWordWrap(true);
    QFont aiFont = aiLabel->font();
    aiFont.setPointSize(12);
    aiLabel->setFont(aiFont);

    QLabel *subtitleLabel = new QLabel(tr("Coming soon — intelligent code assistance, ") +
                                        tr("refactoring suggestions, and more."), inspectorContent);
    subtitleLabel->setObjectName("aiSubtitleLabel");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    QFont subFont = subtitleLabel->font();
    subFont.setPointSize(10);
    subtitleLabel->setFont(subFont);

    contentLayout->addStretch();
    contentLayout->addWidget(iconLabel);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(aiLabel);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(subtitleLabel);
    contentLayout->addStretch();

    inspectorLayout->addWidget(inspectorContent, 1);

    // Insert into the main horizontal layout, after editorContainer (index 1), before tabWidget (index 2)
    QHBoxLayout *mainLayout = qobject_cast<QHBoxLayout*>(ui->centralwidget->layout());
    if (mainLayout) {
        mainLayout->insertWidget(2, m_inspectorDrawer);
    }

    // Apply styles for the inspector drawer
    m_inspectorDrawer->setStyleSheet(R"(
        QWidget#inspectorDrawer {
            background-color: palette(window);
            border-radius: 14px;
            border-left: 1px solid palette(mid);
        }
        QWidget#inspectorHeader {
            background-color: transparent;
            border-bottom: none;
        }
        QLabel#inspectorTitle {
            color: palette(text);
        }
        QPushButton#inspectorCloseBtn {
            border: none;
            border-radius: 8px;
            padding: 2px;
            background-color: transparent;
        }
        QPushButton#inspectorCloseBtn:hover {
            background-color: palette(light);
        }
        QFrame#inspectorSeparator {
            color: palette(mid);
            max-height: 1px;
        }
        QWidget#inspectorContent {
            background-color: transparent;
        }
        QLabel#aiStatusLabel {
            color: palette(text);
            font-weight: bold;
        }
        QLabel#aiSubtitleLabel {
            color: palette(midlight);
        }
    )");

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

    editorStack->addWidget(ui->tabWidget);
    editorStack->addWidget(todoPanel);
    editorStack->addWidget(terminalPanel);

    if (!initialProject.isEmpty()) {
        loadProjectDirectory(initialProject);
        for (const QString &f : initialFiles) {
            if (QFile::exists(f))
                openFileInTab(f);
        }
    }
    showEditorInterface();

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
        lspClient->completion(QUrl::fromLocalFile(currentFile).toString(), c.blockNumber(), c.positionInBlock());
    });

    QShortcut *shortcutSigHelp = new QShortcut(QKeySequence("Ctrl+Shift+Space"), this);
    connect(shortcutSigHelp, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
            return;
        QTextCursor c = editor->textCursor();
        lspClient->signatureHelp(QUrl::fromLocalFile(currentFile).toString(), c.blockNumber(), c.positionInBlock());
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

    // Session restore on startup (after editor is ready)
    if (m_sessionManager && m_sessionManager->hasSavedSession()) {
        QTimer::singleShot(500, this, [this]() {
            m_sessionManager->restoreSession();
        });
    }

    // Zen Mode shortcut (Ctrl+Shift+Alt+Z — avoids conflict with Redo Ctrl+Shift+Z)
    QShortcut *shortcutZenMode = new QShortcut(QKeySequence("Ctrl+Shift+Alt+Z"), this);
    connect(shortcutZenMode, &QShortcut::activated, this, [this]() {
        if (!m_zenMode) {
            m_zenMode = new ZenMode(this, getCurrentCodeEditor(), this);
        }
        m_zenMode->setEditor(getCurrentCodeEditor());
        m_zenMode->toggle();
    });

    // Escape exits Zen Mode
    QShortcut *shortcutEscape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(shortcutEscape, &QShortcut::activated, this, [this]() {
        if (m_zenMode && m_zenMode->isActive()) {
            m_zenMode->exit();
        }
    });

    // Bookmarks: Ctrl+K toggles, F2 next, Shift+F2 previous
    QShortcut *shortcutBookmarkToggle = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(shortcutBookmarkToggle, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || !editor->bookmarkManager()) return;
        QString path = editor->filePath();
        int line = editor->textCursor().blockNumber();
        QString text = editor->textCursor().block().text();
        editor->bookmarkManager()->toggleBookmark(path, line, text);
        editor->update();
    });
    QShortcut *shortcutBookmarkNext = new QShortcut(QKeySequence("Shift+F2"), this);
    connect(shortcutBookmarkNext, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && editor->bookmarkManager())
            editor->bookmarkManager()->nextBookmark();
    });
    QShortcut *shortcutBookmarkPrev = new QShortcut(QKeySequence("Ctrl+Shift+F2"), this);
    connect(shortcutBookmarkPrev, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && editor->bookmarkManager())
            editor->bookmarkManager()->previousBookmark();
    });

    // Toggle folding: Ctrl+Shift+[
    QShortcut *shortcutFoldToggle = new QShortcut(QKeySequence("Ctrl+Shift+["), this);
    connect(shortcutFoldToggle, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && editor->foldManager()) {
            int line = editor->textCursor().blockNumber();
            editor->foldManager()->toggleFold(line);
        }
    });

    QShortcut *shortcutCloseWindow = new QShortcut(QKeySequence("Ctrl+W"), this);
    connect(shortcutCloseWindow, &QShortcut::activated, this, &QWidget::close);

    // Keyboard shortcuts previously defined on .ui action shortcuts (now removed with menus)
    QShortcut *shortcutSave = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(shortcutSave, &QShortcut::activated, this, &MainWindow::on_action_save_triggered);

    QShortcut *shortcutSaveAs = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    connect(shortcutSaveAs, &QShortcut::activated, this, &MainWindow::on_action_save_as_triggered);

    QShortcut *shortcutOpenProject = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(shortcutOpenProject, &QShortcut::activated, this, &MainWindow::on_action_open_project_triggered);

    QShortcut *shortcutOpenFile = new QShortcut(QKeySequence("Ctrl+Shift+O"), this);
    connect(shortcutOpenFile, &QShortcut::activated, this, &MainWindow::on_action_open_file_triggered);

    connect(findReplaceBar, &FindReplaceBar::replaceAllComplete, this, [](int count) {
        qDebug() << "Replace all complete:" << count;
    });

    // The native menu bar is empty and hidden — all actions are accessed via
    // keyboard shortcuts, command palette (Ctrl+Shift+P), and right-click context menus.
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
    connect(lspClient, &RustLspClientAdapter::diagnosticsReceived, this, &MainWindow::onDiagnosticsReceived);
    connect(lspClient, &RustLspClientAdapter::serverStarted, this, []() {
        qDebug() << "LSP server started";
    });
    connect(lspClient, &RustLspClientAdapter::serverFailed, this, [](const QString &err) {
        qDebug() << "LSP server failed:" << err;
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
    connect(lspClient, &RustLspClientAdapter::definitionReceived, this, jumpToLocation);
    // Note: declarationReceived, typeDefinitionReceived, implementationReceived are handled via separate requests
    // that return to the same handler pattern



    // Note: documentSymbol now uses the Rust adapter's completion callback pattern

    connect(lspClient, &RustLspClientAdapter::completionReceived, this, &MainWindow::onCompletionReceived);
    // Route LSP results (rename, code action, etc.) to the RefactoringManager
    connect(lspClient, &RustLspClientAdapter::completionReceived, m_refactoringManager, &RefactoringManager::onLspResultReceived);

    connect(lspClient, &RustLspClientAdapter::hoverReceived, this, [this](const QJsonObject &hover, int) {
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



    // DAP connections
    debugPanel->setClient(dapClient);
    connect(dapClient, &RustDapClientAdapter::initialized, this, &MainWindow::onDapInitialized);
    connect(dapClient, &RustDapClientAdapter::stopped, this, &MainWindow::onDapStopped);
    connect(dapClient, &RustDapClientAdapter::continued, this, &MainWindow::onDapContinued);
    connect(dapClient, &RustDapClientAdapter::stackTraceReceived, this, &MainWindow::onStackTraceReceived);
    connect(dapClient, &RustDapClientAdapter::scopesReceived, this, &MainWindow::onScopesReceived);
    connect(dapClient, &RustDapClientAdapter::variablesReceived, this, &MainWindow::onVariablesReceived);
    connect(dapClient, &RustDapClientAdapter::evaluationReceived, this, [this](const QString &expr, const QString &result) {
        debugPanel->showEvaluation(expr, result);
    });
    connect(debugPanel, &DebugPanel::frameActivated, this, [this](int frameId) {
        if (frameId >= 0) {
            m_currentFrameId = frameId;
            dapClient->scopes(frameId);
        }
    });
    connect(debugPanel, &DebugPanel::evaluateRequested, this, [this](const QString &expr) {
        Q_UNUSED(expr)
    });
    connect(debugPanel, &DebugPanel::watchRequested, this, [this](const QString &expr) {
        Q_UNUSED(expr)
    });
    connect(dapClient, &RustDapClientAdapter::serverFailed, this, [](const QString &err) {
        qDebug() << "DAP server failed:" << err;
    });

    // Problem panel connections
    connect(problemPanel, &ProblemPanel::problemActivated, this, &MainWindow::onProblemActivated);
    connect(problemPanel, &ProblemPanel::filterChanged, this, &MainWindow::onProblemsFilterChanged);

    // Updater connections
    connect(updater, &RustUpdaterAdapter::updateAvailable, this, &MainWindow::onUpdateAvailable);

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
    connect(lspClient, &RustLspClientAdapter::diagnosticsReceived, m_codeActionCtrl, &CodeActionController::onDiagnosticsReceived);
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

    // New feature panels — added to bottom panel
    m_regexTester = new RegexTester(this);
    bottomPanelTabs->addTab(tr("Regex"));
    bottomPanelStack->addWidget(m_regexTester);
    m_regexTester->hide();

    m_dataFormatter = new DataFormatter(this);
    bottomPanelTabs->addTab(tr("Format"));
    bottomPanelStack->addWidget(m_dataFormatter);
    m_dataFormatter->hide();

    m_markdownPreview = new MarkdownPreview(this);
    bottomPanelTabs->addTab(tr("Preview"));
    bottomPanelStack->addWidget(m_markdownPreview);
    m_markdownPreview->hide();

    m_globalReplacePreview = new GlobalReplacePreview(this);
    bottomPanelTabs->addTab(tr("Replace"));
    bottomPanelStack->addWidget(m_globalReplacePreview);
    m_globalReplacePreview->hide();

    // ── P0/P1/P2 Feature Modules ──────────────────────────────────────
    m_sessionManager = new SessionManager(this, ui->tabWidget, this);
    m_refactoringManager = new RefactoringManager(this);
    m_codeLensManager = new CodeLensManager(this);
    // When Code Lens items arrive, feed them to the matching editor
    connect(m_codeLensManager, &CodeLensManager::codeLensUpdated, this, [this](const QString &uri) {
        QString filePath = QUrl(uri).toLocalFile();
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            if (CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                if (ed->filePath() == filePath) {
                    ed->setCodeLensItems(m_codeLensManager->itemsForDocument(uri));
                    break;
                }
            }
        }
    });
    m_gitBlame = new GitBlame(this);
    m_statusBarWidget = new StatusBarWidget(this);
    m_encodingManager = new EncodingManager(this);
    m_diffViewer = new DiffViewerWidget(this);
    m_notificationCenter = new NotificationCenter(this);

    // Add DiffViewer to bottom panel tabs
    bottomPanelTabs->addTab(tr("Diff"));
    bottomPanelStack->addWidget(m_diffViewer);
    m_diffViewer->hide();

    // Replace default status bar with custom status bar
    statusBar()->setVisible(false);
    m_statusBarWidget->setObjectName("customStatusBar");
    statusBar()->addPermanentWidget(m_statusBarWidget);
    statusBar()->setVisible(true);
    m_statusBarWidget->setLanguage("Text");
    m_statusBarWidget->setEncoding("UTF-8");
    m_statusBarWidget->setLineEnding("LF");
    m_statusBarWidget->setIndentation("Spaces: 4");

    // ── P1/P2 Feature Modules ─────────────────────────────────────────
    m_gitStash = new GitStashWidget(this);
    bottomPanelTabs->addTab(tr("Stash"));
    bottomPanelStack->addWidget(m_gitStash);
    m_gitStash->hide();

    m_gitRebase = new GitRebaseWidget(this);
    bottomPanelTabs->addTab(tr("Rebase"));
    bottomPanelStack->addWidget(m_gitRebase);
    m_gitRebase->hide();

    m_taskRunnerUI = new TaskRunnerUI(this);
    bottomPanelTabs->addTab(tr("Tasks"));
    bottomPanelStack->addWidget(m_taskRunnerUI);
    m_taskRunnerUI->hide();

    CodeEditor *currentEditor = getCurrentCodeEditor();
    BookmarkManager *bm = currentEditor ? currentEditor->bookmarkManager() : nullptr;
    m_bookmarkPanel = new BookmarkPanelWidget(bm, this);
    bottomPanelTabs->addTab(tr("Bookmarks"));
    bottomPanelStack->addWidget(m_bookmarkPanel);
    m_bookmarkPanel->hide();

    // Test Panel
    m_testPanel = new TestPanel(this);
    bottomPanelTabs->addTab(tr("Tests"));
    bottomPanelStack->addWidget(m_testPanel);
    m_testPanel->hide();

    // P3: Plugin Marketplace
    m_pluginMarketplace = new PluginMarketplaceWidget(m_pluginRegistry, this);
    bottomPanelTabs->addTab(tr("Marketplace"));
    bottomPanelStack->addWidget(m_pluginMarketplace);
    m_pluginMarketplace->hide();

    // P3: Theme Marketplace
    m_themeMarketplace = new ThemeMarketplaceWidget(this);
    m_themeMarketplace->loadBuiltinThemes();
    bottomPanelTabs->addTab(tr("Themes"));
    bottomPanelStack->addWidget(m_themeMarketplace);
    m_themeMarketplace->hide();

    // P2: CSS Breadcrumb parser
    m_cssBreadcrumbParser = new CssBreadcrumbParser(this);

    // Breadcrumb bar below tab bar
    m_breadcrumbBar = new BreadcrumbBarWidget(ui->editorContainer);
    ui->editorContainerLayout->insertWidget(1, m_breadcrumbBar);

    // Connect theme marketplace to theme manager
    connect(m_themeMarketplace, &ThemeMarketplaceWidget::themeInstalled, this, [this](const QString &themeName) {
        // Theme application would be handled by ThemeManager
        qDebug() << "Theme installed:" << themeName;
    });
    connect(m_pluginMarketplace, &PluginMarketplaceWidget::pluginInstalled, this, [this](const QString &pluginId) {
        // Plugin installation handled by PluginRegistry
        qDebug() << "Plugin installed:" << pluginId;
    });
    connect(m_pluginMarketplace, &PluginMarketplaceWidget::pluginUninstalled, this, [this](const QString &pluginId) {
        qDebug() << "Plugin uninstalled:" << pluginId;
    });

    // Test Runner shortcut (Ctrl+Shift+T)
    QShortcut *shortcutTestRun = new QShortcut(QKeySequence("Ctrl+Shift+T"), this);
    connect(shortcutTestRun, &QShortcut::activated, this, [this]() {
        m_testPanel->show();
        int idx = bottomPanelTabs->count() - 1;
        bottomPanelTabs->setCurrentIndex(idx);
        bottomPanelStack->setCurrentIndex(idx);
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, 200);
        } else {
            ui->bottomPanelContainer->show();
        }
        m_testPanel->runAllTests();
    });

    // Task Runner shortcuts
    QShortcut *shortcutTaskRun = new QShortcut(QKeySequence("Ctrl+Shift+B"), this);
    connect(shortcutTaskRun, &QShortcut::activated, this, [this]() {
        if (!m_taskRunnerUI) return;
        QString projectPath = projectDir.isEmpty() ? QDir::homePath() : projectDir;
        m_taskRunnerUI->detectTasks(projectPath);
        ui->bottomPanelContainer->show();
        int idx = bottomPanelTabs->count() - 2; // Tasks tab
        bottomPanelTabs->setCurrentIndex(idx);
        bottomPanelStack->setCurrentIndex(idx);
    });

    // Connect task runner to terminal
    connect(m_taskRunnerUI, &TaskRunnerUI::taskRun, this, [this](const QString &command) {
        if (command == "refresh") {
            QString projectPath = projectDir.isEmpty() ? QDir::homePath() : projectDir;
            m_taskRunnerUI->detectTasks(projectPath);
            return;
        }
        // Show terminal and run command
        ui->bottomPanelContainer->show();
        editorStack->setCurrentWidget(terminalPanel);
        terminalButton->setChecked(true);
    });

    // Connect GitStash refresh to Git panel
    connect(m_gitStash, &GitStashWidget::stashApplied, this, [this](int) {
        qDebug() << "Stash applied";
    });
    connect(m_gitStash, &GitStashWidget::stashPopped, this, [this](int) {
        qDebug() << "Stash popped";
    });

    // Connect notification center to status bar
    connect(m_notificationCenter, &NotificationCenter::unreadCountChanged, this, [this](int count) {
        if (m_statusBarWidget) {
            m_statusBarWidget->setErrorCount(count, 0);
        }
    });

    // P2: CSS Breadcrumb - update on cursor move for HTML/CSS files
    // Single connection per editor, managed externally to avoid accumulation
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index < 0 || !m_cssBreadcrumbParser) return;
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor) return;
        QString path = editor->filePath();
        if (path.endsWith(".html") || path.endsWith(".css") || path.endsWith(".scss") ||
            path.endsWith(".xml") || path.endsWith(".svg")) {
            // Disconnect previous breadcrumb connections first
            disconnect(m_cssBreadcrumbConnection);
            // Single connection per tab switch — UniqueConnection prevents duplicates on same editor
            m_cssBreadcrumbConnection = connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, editor]() {
                QTextCursor cursor = editor->textCursor();
                QList<DomBreadcrumbElement> hierarchy = m_cssBreadcrumbParser->parseDomHierarchy(
                    editor, cursor.blockNumber(), cursor.positionInBlock());
                if (!hierarchy.isEmpty()) {
                    QString breadcrumb = CssBreadcrumbParser::breadcrumbText(hierarchy);
                    editor->setToolTip(breadcrumb);
                }
            });
        }
    });

    // Connect session manager signals
    connect(m_sessionManager, &SessionManager::sessionRestored, this, [this]() {
        qDebug() << "Session restored";
    });
    // Open files requested by session restore
    connect(m_sessionManager, &SessionManager::sessionFileRequested, this,
            [this](const QString &filePath, int cursorLine, int cursorColumn, bool activate) {
        if (!QFile::exists(filePath)) return;
        openFileInTab(filePath);
        if (activate) {
            CodeEditor *editor = getCurrentCodeEditor();
            if (editor) {
                QTextBlock block = editor->document()->findBlockByNumber(cursorLine);
                if (block.isValid()) {
                    QTextCursor cursor(editor->document());
                    cursor.setPosition(block.position() + cursorColumn);
                    editor->setTextCursor(cursor);
                }
            }
        }
    });
    // Hot exit: restore unsaved buffers as untitled tabs
    connect(m_sessionManager, &SessionManager::hotExitFileRequested, this,
            [this](const QString &originalPath, const QString &content, int cursorLine, int cursorColumn) {
        CodeEditor *editor = new CodeEditor(this);
        editor->setFilePath(originalPath);
        if (!originalPath.isEmpty()) {
            editor->setLanguageForFile(originalPath);
        }
        editor->installEventFilter(this);
        connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
        QSettings settings;
        QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
        editor->setFont(savedFont);
        editor->setTabWidth(settings.value("editor/tabWidth", editor->tabWidth()).toInt());
        editor->setPlainText(content);
        editor->document()->setModified(true);
        QString displayName = originalPath.isEmpty() ? tr("Untitled") : QFileInfo(originalPath).fileName();
        int tabIndex = openFiles.size();
        connect(editor, &QPlainTextEdit::modificationChanged, this,
                [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
        connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
            lspDebounceTimer->start();
        });
        openFiles.append({originalPath, displayName, true});
        showEditorInterface();
        ui->tabWidget->addTab(editor, "*" + displayName);
        int tabBarIndex = tabBar->addTab("*" + displayName);
        tabBar->setTabData(tabBarIndex, originalPath);
        tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createTabCloseButton(originalPath));
        ui->tabWidget->setCurrentWidget(editor);
        tabBar->setCurrentIndex(tabBarIndex);
        // Restore cursor position
        QTextBlock block = editor->document()->findBlockByNumber(cursorLine);
        if (block.isValid()) {
            QTextCursor cursor(editor->document());
            cursor.setPosition(block.position() + cursorColumn);
            editor->setTextCursor(cursor);
        }
    });

    // Git Blame toggle: Ctrl+Shift+G
    QShortcut *shortcutGitBlame = new QShortcut(QKeySequence("Ctrl+Shift+G"), this);
    connect(shortcutGitBlame, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || !m_gitBlame) return;
        if (editor->blameEnabled()) {
            editor->setBlameEnabled(false);
            editor->setBlameData({});
        } else {
            QString path = editor->filePath();
            if (!path.isEmpty()) {
                m_gitBlame->requestBlame(path);
            }
        }
    });

    // When blame data arrives, feed it to the matching editor
    connect(m_gitBlame, &GitBlame::blameReceived, this, [this](const QString &filePath) {
        // Find the editor that matches the blamed file path
        CodeEditor *targetEditor = nullptr;
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (ed && ed->filePath() == filePath) {
                targetEditor = ed;
                break;
            }
        }
        if (!targetEditor) return;
        QMap<int, BlameLineInfo> data;
        for (const BlameLineInfo &info : m_gitBlame->blameForFile()) {
            data[info.line] = info;
        }
        targetEditor->setBlameData(data);
        targetEditor->setBlameEnabled(true);
        targetEditor->update();
    });

    // ── Status Bar dynamic updates ────────────────────────────────────────
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        updateStatusBar();
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && m_gitBlame && m_gitBlame->isEnabled() && !editor->filePath().isEmpty()) {
            m_gitBlame->requestBlame(editor->filePath());
        }
    });

    // Refactoring shortcuts
    QShortcut *shortcutRename = new QShortcut(QKeySequence("F2"), this);
    connect(shortcutRename, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && lspClient->isRunning() && !currentFile.isEmpty()) {
            m_refactoringManager->renameSymbol(editor, lspClient, currentFile);
        }
    });
    // Handle rename dialog request from RefactoringManager
    connect(m_refactoringManager, &RefactoringManager::renameDialogRequested, this,
            [this](const QString &symbol, const QString &filePath, int line, int column) {
        bool ok = false;
        QString newName = QInputDialog::getText(this, tr("Rename Symbol"),
                                                  tr("Rename '%1' to:").arg(symbol),
                                                  QLineEdit::Normal, symbol, &ok);
        if (ok && !newName.isEmpty() && newName != symbol) {
            CodeEditor *editor = getCurrentCodeEditor();
            if (editor) {
                m_refactoringManager->renameWithNewName(editor, lspClient, filePath, newName);
            }
        }
    });
    // Connect refactoring results
    connect(m_refactoringManager, &RefactoringManager::refactoringApplied, this, [this](int fileCount) {
        m_notificationCenter->addNotification(tr("Refactoring Complete"),
            tr("Applied changes to %1 file(s)").arg(fileCount), "success");
    });
    connect(m_refactoringManager, &RefactoringManager::refactoringFailed, this, [this](const QString &reason) {
        m_notificationCenter->addNotification(tr("Refactoring Failed"), reason, "error");
    });
    // Extract Method: Ctrl+Alt+M
    QShortcut *shortcutExtractMethod = new QShortcut(QKeySequence("Ctrl+Alt+M"), this);
    connect(shortcutExtractMethod, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && lspClient->isRunning()) {
            m_refactoringManager->extractMethod(editor, lspClient, currentFile);
        }
    });
    // Extract Variable: Ctrl+Alt+V
    QShortcut *shortcutExtractVariable = new QShortcut(QKeySequence("Ctrl+Alt+V"), this);
    connect(shortcutExtractVariable, &QShortcut::activated, this, [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && lspClient->isRunning()) {
            m_refactoringManager->extractVariable(editor, lspClient, currentFile);
        }
    });
    connect(m_httpClient, &HttpClientPanel::requestSent, this, [](const QString &url, int code) {
        qDebug() << "HTTP request:" << url << "->" << code;
    });

    // Plugin registry (uses Rust adapter)
    m_pluginRegistry->setRegistryUrl(registryUrl);
    QTimer::singleShot(5000, m_pluginRegistry, [this]() {
        m_pluginRegistry->checkForUpdates();
    });

    // Init plugin developer API wiring
    setupPluginApis();

    // Config validator - validate settings on startup (Rust adapter handles this internally)

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

QPushButton* MainWindow::createTabCloseButton(const QString &filePath)
{
    QPushButton *closeBtn = new QPushButton();
    ThemeIcons::instance()->setIcon(closeBtn, ":/icons/close.svg");
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this, filePath]() {
        // Look up the file's index in openFiles by path (not tabBar index)
        for (int i = 0; i < openFiles.size(); ++i) {
            if (openFiles[i].filePath == filePath) {
                on_tabWidget_tabCloseRequested(i);
                return;
            }
        }
    });
    return closeBtn;
}

// createWelcomeWidget() removed — replaced by standalone WelcomeMenuScreen
// shown between splash and main window startup.

