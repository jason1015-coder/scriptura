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
#include "codeeditor.h"
#include "lspclient.h"
#include "problempanel.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

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
    void on_action_about_triggered();
    void on_action_editor_settings_triggered();
    void on_action_theme_triggered();
    void on_action_license_triggered();
    void on_fileTreeView_clicked(const QModelIndex &index);
    void on_tabWidget_tabCloseRequested(int index);
    void goUpClicked();
    void on_action_open_file_triggered();
    void onSearchTextChanged(const QString &text);
    void onSearchNext();
    void onSearchPrev();
    void showSearchBar(bool show);
    void performSearch(const QString &text, bool forward);
    void showKeyboardShortcuts();
    void onEditorTextChanged();
    void on_placeholderButton_clicked(bool checked);

private:
    Ui::MainWindow *ui;
    QString currentFile;
    QString projectDir;
    QModelIndex rootIndex;
    QList<OpenFile> openFiles;
    QFileSystemModel *fileModel;
    QToolButton *goUpButton;
      QToolButton *placeholderButton;
      QToolButton *fileTreeToggleButton;
      QToolButton *terminalButton;
      QToolButton *problemsButton;
      QLineEdit   *searchLineEdit;
     QPushButton *searchPrevBtn;
     QPushButton *searchNextBtn;
     QLabel      *searchCountLabel;
     QWidget     *searchBarWidget;
     QWidget     *welcomeWidget;
     QVBoxLayout *recentProjectsLayout;
     QStackedWidget *editorStack;
   ProblemPanel *problemPanel;
      TodoPanel    *todoPanel;
      GitPanel     *gitPanel;
      TerminalPanel *terminalPanel;
      Theme selectedTheme;

    QTimer *autoSaveTimer;
    QTimer *lspDebounceTimer;
    LspClient *lspClient;
    QStringList recentProjects;
    int maxRecentProjects = 10;
    QStringList m_languageServers; // Configured language servers

    void updateCursorPosition();
    void updateStatusBar();
    void updateTabModified(int index, bool modified);
    QString findTerminal();
    QPlainTextEdit* getCurrentEditor();
    CodeEditor* getCurrentCodeEditor();
    QWidget* createWelcomeWidget();
    QWidget* createKeyboardShortcutsWidget();
    void startLanguageServer(const QString &filePath);
    void startLanguageServerForProject(const QString &projectPath);
    void stopLanguageServer();
    void onDiagnosticsReceived(const QString &uri, const QList<LspClient::Diagnostic> &diagnostics);
    void onProblemActivated(const QString &fileUri, int line, int column);
    void onProblemsFilterChanged(ProblemPanel::Filter filter);
    void toggleProblemPanel();
    void toggleTerminalPanel();
    void showWelcomeScreen();
    void showEditorInterface();
    void applyTheme(const Theme &theme);
    void setSidebarCollapsed(bool collapsed);
    void loadRecentProjects();
    void saveRecentProjects();
    void updateRecentProjectsOnWelcome();
    void autoSave();
    bool checkUnsavedChanges();

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