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
#include <memory>
#include "codeeditor.h"
#include "lspclient.h"
#include "problempanel.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "updater.h"
#include "configvalidator.h"
#include "pluginmanager.h"
#include "plugincontext.h"
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
#include "permission.h"

class DapClient;
class DebugPanel;
class Workspace;
class TaskRunner;

class FindReplaceBar;
class ProjectSearchPanel;
class CommandPalette;

enum class ThemeColorFamily {
    Default = 0,
    Blue = 1,
    Green = 2,
    Red = 3,
    Yellow = 4,
    Brown = 5,
    Cyan = 6,
    Violet = 7
};

enum class ThemeMode {
    Light = 0,
    Dark = 1
};

enum class ThemeFeature {
    None = 0x0,
    HighContrast = 0x1
};

Q_DECLARE_FLAGS(ThemeFeatures, ThemeFeature)

struct Theme {
    ThemeColorFamily family;
    ThemeMode mode;
    ThemeFeatures features;

    Theme(ThemeColorFamily f = ThemeColorFamily::Default, ThemeMode m = ThemeMode::Light,
          ThemeFeatures feat = ThemeFeatures())
        : family(f), mode(m), features(feat) {}

    bool isDark() const { return mode == ThemeMode::Dark; }

    bool operator==(const Theme &other) const {
        return family == other.family && mode == other.mode && features == other.features;
    }
    bool operator!=(const Theme &other) const { return !(*this == other); }
};

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
    LspClient* getLspClient() const { return lspClient; }
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
    void onSettingsTabCloseRequested(int tabIndex);

private:
    enum class TabType {
        File = 0,
        ThemeSettings = 1,
        EditorSettings = 2,
        KeyboardShortcuts = 3,
        UpdaterSettings = 4
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
    QWidget     *welcomeWidget;
    QVBoxLayout *recentProjectsLayout;
    QStackedWidget *editorStack;
    ProblemPanel *problemPanel;
    TodoPanel    *todoPanel;
    GitPanel     *gitPanel;
    TerminalPanel *terminalPanel;
    Theme selectedTheme;

    // Settings page widgets
    QWidget *themeSettingsWidget;
    QWidget *editorSettingsWidget;
    QWidget *keyboardShortcutsPageWidget;
    QWidget *updaterSettingsWidget;

    QTimer *autoSaveTimer;
    QTimer *lspDebounceTimer;
    QTimer *m_hoverTimer;
    QStringList recentProjects;
    int maxRecentProjects = 10;
     QStringList m_languageServers;
     QString registryUrl;
     Updater *updater;
    ConfigValidator *configValidator;
    LspClient *lspClient;
    PluginManager *pluginManager;
    PluginContext *pluginContext;
    PluginManagerDialog *pluginManagerDialog;
    int m_previousEditorStackIndex;
    
    // Debugger
    DapClient *dapClient;
    DebugPanel *debugPanel;
    std::unique_ptr<DebugConfigurationManager> debugConfigManager;
    bool m_isDebugging;
    int m_currentFrameId = 0;
    QMap<QString, QString> m_breakpointConditions; // key: "file:line"
    QListWidget *m_completionPopup = nullptr;

    // Workspace & Productivity
    Workspace *m_workspace;
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

    // Plugin Developer API instances (owned by PluginContext)
    void setupPluginApis();
    AiInlineCompletion *m_aiInline;
    HttpClientPanel *m_httpClient;
    CodeActionController *m_codeActionCtrl;
    SqliteViewerPanel *m_sqliteViewer;
    PluginRegistry *m_pluginRegistry;

    void updateCursorPosition();
    void updateStatusBar();
    void updateTabModified(int index, bool modified);
    void updateTopTabBar();
    void updateBottomTabBar();
    void updateTabBarVisibility();
    QIcon createSymbolIcon(QChar symbol) const;
    QPushButton* createTabCloseButton(int tabIndex);
    QPushButton* createSettingsTabCloseButton(int tabIndex);
    QString findTerminal();
    QPlainTextEdit* getCurrentEditor();
    QWidget* createWelcomeWidget();
    QWidget* createKeyboardShortcutsWidget();
    QWidget* createThemeSettingsWidget();
    QWidget* createEditorSettingsWidget();
    QWidget* createKeyboardShortcutsPageWidget();
    QWidget* createUpdaterSettingsWidget();
    void startLanguageServer(const QString &filePath);
    void startLanguageServerForProject(const QString &projectPath);
    void stopLanguageServer();
    void onDiagnosticsReceived(const QString &uri, const QList<LspClient::Diagnostic> &diagnostics);
    void onProblemActivated(const QString &fileUri, int line, int column);
    void onProblemsFilterChanged(ProblemPanel::Filter filter);
    void toggleProblemPanel();
    void toggleTerminalPanel();
    void toggleTodoPanel();
    void showGitPanel();
    void showBottomPanel(QWidget *panel);
    void onUpdateAvailable(const QString &version, const QString &downloadUrl);
    void onUpdateCheckFailed(const QString &error);
    void showWelcomeScreen();
    void showEditorInterface();
    void loadProjectDirectory(const QString &dirName);
    void openFileInTab(const QString &fileName);
    void applyTheme(const Theme &theme);
    void setSidebarCollapsed(bool collapsed);
    void loadRecentProjects();
    void saveRecentProjects();
    void updateRecentProjectsOnWelcome();
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
    void onStackTraceReceived(int threadId, const QList<DapClient::StackFrame> &frames);
    void onScopesReceived(int frameId, const QList<DapClient::Scope> &scopes);
    void onVariablesReceived(int variablesReference, const QList<DapClient::Variable> &variables);
    void onDapLogMessage(const QString &msg);

    QPalette buildBasePalette(ThemeColorFamily family, ThemeMode mode);
    QColor highContrastAccentColor(ThemeColorFamily family, ThemeMode mode);
    void applyHighContrastPalette(QPalette &p, ThemeColorFamily family, ThemeMode mode);
    struct SyntaxColors { QColor keyword, string, comment, number, preprocessor, tag, attribute, cssProperty, variable, function, escape; };
    SyntaxColors baseSyntaxColors(ThemeMode mode);
    SyntaxColors highContrastSyntaxColor(ThemeColorFamily family, ThemeMode mode, const SyntaxColors &base);
    void applyHighContrastSyntax(SyntaxColors &c, ThemeColorFamily family, ThemeMode mode);
    void updateFamilyButtonPreview(QPushButton *btn, ThemeColorFamily family, ThemeMode mode, ThemeFeatures features);

    Theme themeFromLegacyInt(int legacy) const;
    int themeToLegacyInt(const Theme &theme) const;

};

#endif // MAINWINDOW_H
