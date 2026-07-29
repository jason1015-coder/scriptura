#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QFileSystemModel>
#include <QTabWidget>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QStackedWidget>
#include <QMenu>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QTimer>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QShortcut>
#include <QColor>
#include <QPalette>
#include <QRadioButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QTabBar>
#include <QScrollArea>
#include <memory>
#include "codeeditor.h"
#include "problempanel.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "debugpanel.h"
#include "findreplace.h"
#include "zenmode.h"
#include "filewatcher.h"
#include "outlinepanel.h"
#include "snippetmanager.h"
#include "regextester.h"
#include "dataformatter.h"
#include "markdownpreview.h"
#include "globalreplacepreview.h"
#include "sessionmanager.h"
#include "refactoringmanager.h"
#include "codelensmanager.h"
#include "gitblame.h"
#include "statusbarwidget.h"
#include "encodingmanager.h"
#include "diffviewer.h"
#include "notificationcenter.h"
#include "gitstash.h"
#include "gitrebase.h"
#include "shortcuteditor.h"
#include "taskrunnerui.h"
#include "bookmarkpanel.h"
#include "cssbreadcrumb.h"
#include "testrunner.h"
#include "testpanel.h"
#include "pluginmarketplace.h"
#include "themarketplace.h"
#include "snippeteditordialog.h"
#include "projectsearch.h"
#include "commandpalette.h"
#include "minimap.h"
#include "splitmanager.h"
#include "breadcrumb.h"
#include "aiinlinecompletion.h"
#include "universalsearch.h"
#include "httpclientpanel.h"
#include "codeactionui.h"
#include "sqliteviewer.h"
#include "customtitlebar.h"
#include "windowanimator.h"
#include "thememanager.h"
#include "themeicons.h"
#include "rust_adapter.h"

class DebugPanel;
class FindReplaceBar;
class ProjectSearchPanel;
class CommandPalette;
class ApplicationDock;
class ApplicationManager;

#include "themedefs.h"
#include "breadcrumbbar.h"
#include "debugconfiguration.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct OpenFile {
    QString filePath;
    QString fileName;
    bool modified = false;
};

class PluginManagerDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &initialProject = QString(), const QStringList &initialFiles = QStringList(), QWidget *parent = nullptr);
    ~MainWindow();

    CodeEditor* getCurrentCodeEditor();
    QString currentProjectPath() const { return projectDir; }
    RustLspClientAdapter* getLspClient() const { return lspClient; }
    ProblemPanel* getProblemPanel() const { return problemPanel; }
    TerminalPanel* getTerminalPanel() const { return terminalPanel; }
    GitPanel* getGitPanel() const { return gitPanel; }
    bool isDarkModeEnabled() const { return selectedTheme.mode == ThemeMode::Dark; }

#ifdef Q_OS_WIN
    void enableMicaEffect(HWND hwnd, bool darkMode);
#endif

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void on_action_open_project_triggered();
    void on_action_save_triggered();
    void on_action_save_as_triggered();
    void on_actionCu_t_triggered();
    void on_action_copy_triggered();
    void on_action_Paste_triggered();
    void on_action_Undo_triggered();
    void on_action_Redo_triggered();
    void on_action_add_file_directory_triggered();
    void on_action_delete_file_directory_triggered();
    void on_action_new_window_triggered();
    void on_action_clone_window_triggered();
    void on_action_git_commit_triggered();
    void on_action_git_push_triggered();
    void on_action_git_pull_triggered();
    void on_action_git_fetch_triggered();
    void on_action_about_triggered();
    void on_action_editor_settings_triggered();
    void on_action_theme_triggered();
    void on_action_license_triggered();
    void on_action_manage_plugins_triggered();
    void on_action_check_updates_triggered();
    void on_action_format_document_triggered();
    void on_action_go_to_definition_triggered();
    void on_action_go_to_declaration_triggered();
    void on_action_go_to_type_definition_triggered();
    void on_action_go_to_implementation_triggered();
    void on_action_show_document_symbols_triggered();
    void on_fileTreeView_clicked(const QModelIndex &index);
    void on_fileTreeView_contextMenu(const QPoint &pos);
    void on_tabWidget_tabCloseRequested(int index);
    void goUpClicked();
    void on_action_open_file_triggered();
    void showSearchBar(bool show);
    void on_action_find_triggered();
    void on_action_replace_triggered();
    void on_action_project_search_triggered();
    void on_action_command_palette_triggered();
    void showKeyboardShortcuts();
    void onEditorTextChanged();
    void requestHover();
    void on_placeholderButton_clicked(bool checked);
    void on_action_run_debug_triggered();
    void on_action_stop_debug_triggered();
    void on_action_step_over_triggered();
    void on_action_step_into_triggered();
    void on_action_step_out_triggered();
    void on_action_continue_debug_triggered();
    void on_action_toggle_breakpoint_triggered();

    void toggleSidebar();
    void onBottomTabChanged(int index);
    void onTopTabChanged(int index);

private:
    enum class TabType {
        File = 0,
        Settings = 1
    };

    Ui::MainWindow *ui;
    QString currentFile;
    QString projectDir;
    QModelIndex rootIndex;
    QList<OpenFile> openFiles;
    QFileSystemModel *fileModel = nullptr;
    ThemeFileIconProvider *m_fileIconProvider = nullptr;
    QToolButton *goUpButton;
    QToolButton *placeholderButton;
    QToolButton *fileTreeToggleButton;
    QToolButton *terminalButton;
    QToolButton *problemsButton;
    QToolButton *gitButton;
    QToolButton *sidebarToggleButton;
    QToolButton *settingsButton;
    QTabBar *tabBar;
    QTabBar *bottomPanelTabs;
    QStackedWidget *bottomPanelStack;
    FindReplaceBar *findReplaceBar;
    ProjectSearchPanel *projectSearchPanel;
    CommandPalette *commandPalette;
    QStackedWidget *editorStack;
    ProblemPanel *problemPanel;
    TodoPanel    *todoPanel;
    GitPanel     *gitPanel;
    TerminalPanel *terminalPanel;
    Theme selectedTheme;

    // Unified scrollable settings page
    QWidget *unifiedSettingsWidget;

    QTimer *autoSaveTimer;
    QTimer *lspDebounceTimer;
    QTimer *m_hoverTimer;
    QStringList recentProjects;
    int maxRecentProjects = 10;
    QStringList m_languageServers;
    QString registryUrl;
    RustUpdaterAdapter *updater;
    RustConfigValidatorAdapter *configValidator;
    RustLspClientAdapter *lspClient;
    RustPluginManagerAdapter *pluginManager;
    PluginManagerDialog *pluginManagerDialog;
    int m_previousEditorStackIndex;
    
    // Debugger
    RustDapClientAdapter *dapClient;
    DebugPanel *debugPanel;
    std::unique_ptr<DebugConfigurationManager> debugConfigManager;
    bool m_isDebugging;
    int m_currentFrameId = 0;
    QMap<QString, QString> m_breakpointConditions; // key: "file:line"
    QListWidget *m_completionPopup = nullptr;
    QMap<QString, QString> m_fileEncodings;  // filePath -> encoding name
    QMap<QString, QString> m_fileLineEndings; // filePath -> line ending style

    // Workspace & Productivity
    RustWorkspaceAdapter *m_workspace;
    QStringList recentFiles;
    void openRecentFile(const QString &path);
    void addRecentFile(const QString &path);
    
    // Modular UI components
    CustomTitleBar *m_titleBar;
    WindowAnimator *m_windowAnimator;
    ThemeManager *m_themeManager;
    
    // UI/UX Polish
    Minimap *m_minimap;
    SplitManager *m_splitManager;
    Breadcrumb *m_breadcrumb;
    QWidget *m_inspectorDrawer;
    UniversalSearchPopup *m_universalSearch;

    // Plugin Developer API instances (owned by PluginContext)
    void setupPluginApis();
    AiInlineCompletion *m_aiInline;
    HttpClientPanel *m_httpClient;
    CodeActionController *m_codeActionCtrl;
    SqliteViewerPanel *m_sqliteViewer;
    RustPluginRegistryAdapter *m_pluginRegistry;
    BreadcrumbBarWidget *m_breadcrumbBar;
    QMetaObject::Connection m_cssBreadcrumbConnection;

    // Application Dock (floating bottom bar for applications)
    ApplicationDock *m_appDock = nullptr;
    ApplicationManager *m_appManager = nullptr;

    void setupApplicationDock();
    void showApplicationTab(const QString &appId);
    void repositionDock();

    // P0/P1/P2/P3 Feature Modules
    ZenMode *m_zenMode;
    FileWatcher *m_fileWatcher;
    OutlinePanel *m_outlinePanel;
    RegexTester *m_regexTester;
    DataFormatter *m_dataFormatter;
    MarkdownPreview *m_markdownPreview;
    GlobalReplacePreview *m_globalReplacePreview;
    SessionManager *m_sessionManager;
    RefactoringManager *m_refactoringManager;
    CodeLensManager *m_codeLensManager;
    GitBlame *m_gitBlame;
    StatusBarWidget *m_statusBarWidget;
    EncodingManager *m_encodingManager;
    DiffViewerWidget *m_diffViewer;
    NotificationCenter *m_notificationCenter;
    GitStashWidget *m_gitStash;
    GitRebaseWidget *m_gitRebase;
    TaskRunnerUI *m_taskRunnerUI;
    BookmarkPanelWidget *m_bookmarkPanel;
    TestPanel *m_testPanel;
    CssBreadcrumbParser *m_cssBreadcrumbParser;
    SnippetEditorDialog *m_snippetEditorDialog;
    PluginMarketplaceWidget *m_pluginMarketplace;
    ThemeMarketplaceWidget *m_themeMarketplace;

    void updateCursorPosition();
    void updateStatusBar();
    void updateTabModified(int index, bool modified);
    void updateTopTabBar();
    void updateBottomTabBar();
    void updateTabBarVisibility();
    QIcon createSymbolIcon(QChar symbol) const;
    QPushButton* createTabCloseButton(const QString &filePath);
    QPushButton* createSettingsTabCloseButton(int tabIndex);
    QString findTerminal();
    QPlainTextEdit* getCurrentEditor();
    QWidget* createUnifiedSettingsWidget();
    void startLanguageServer(const QString &filePath);
    void startLanguageServerForProject(const QString &projectPath);
    void stopLanguageServer();
    void onDiagnosticsReceived(const QString &uri, const QJsonArray &diagnostics);
    void onProblemActivated(const QString &fileUri, int line, int column);
    void onProblemsFilterChanged(ProblemPanel::Filter filter);
    void toggleProblemPanel();
    void toggleTerminalPanel();
    void toggleTodoPanel();
    void showGitPanel();
    void showBottomPanel(QWidget *panel);
    void onUpdateAvailable(const QString &version, const QString &downloadUrl);
    void onUpdateCheckFailed(const QString &error);
    void onTestResultsReceived(const QString &output);
    // showWelcomeScreen() removed — see WelcomeMenuScreen instead
    void showEditorInterface();
    void loadProjectDirectory(const QString &dirName);
    void openFileInTab(const QString &fileName);
    void applyTheme(const Theme &theme);
    void setSidebarCollapsed(bool collapsed);
    void toggleInspector();
    void loadRecentProjects();
    void saveRecentProjects();
    void autoSave();
    bool checkUnsavedChanges();
    
    // Debugger methods
    void startDebug(const QString &configName);
    void stopDebug();
    void loadDebugConfigurations();
    void onBreakpointToggled(int line, bool enabled);
    void updateDapBreakpoints();
    void onCompletionReceived(const QJsonArray &items, int requestId);
    void hideCompletion();
    void onDapInitialized();
    void onDapStopped(const QString &reason);
    void onDapContinued();
    void onStackTraceReceived(int threadId, const QJsonArray &frames);
    void onScopesReceived(int frameId, const QJsonArray &scopes);
    void onVariablesReceived(int varRef, const QJsonArray &variables);
    void onDapLogMessage(const QString &msg);

    QPalette buildBasePalette(ThemeColorFamily family, ThemeMode mode);
    void updateFamilyButtonPreview(QPushButton *btn, ThemeColorFamily family, ThemeMode mode, ThemeFeatures features);

    Theme themeFromLegacyInt(int legacy) const;
    int themeToLegacyInt(const Theme &theme) const;

    // Frameless window resize state
    int m_resizeEdge = 0;
    bool m_resizing = false;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeometry;

};

#endif // MAINWINDOW_H
