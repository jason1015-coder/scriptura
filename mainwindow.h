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
#include "codeeditor.h"

enum ThemeType {
    ThemeBlueLight,
    ThemeBlueDark,
    ThemeGreenLight,
    ThemeGreenDark,
    ThemeHighContrastLight,
    ThemeHighContrastDark,
    ThemeDefaultLight,
    ThemeDefaultDark
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

private:
    Ui::MainWindow *ui;
    QString currentFile;
    QString projectDir;
    QModelIndex rootIndex;
    QList<OpenFile> openFiles;
    QFileSystemModel *fileModel;
    QToolButton *goUpButton;
    QToolButton *fileTreeToggleButton;
    QToolButton *terminalButton;
    QLineEdit   *searchLineEdit;
    QPushButton *searchPrevBtn;
    QPushButton *searchNextBtn;
    QLabel      *searchCountLabel;
    QWidget     *searchBarWidget;
    QWidget     *welcomeWidget;
    QVBoxLayout *recentProjectsLayout;
    QStackedWidget *editorStack;
    bool darkMode;
    ThemeType selectedTheme;

    QTimer *autoSaveTimer;
    QStringList recentProjects;
    int maxRecentProjects = 10;

    void updateCursorPosition();
    void updateStatusBar();
    void updateTabModified(int index, bool modified);
    QString findTerminal();
    QPlainTextEdit* getCurrentEditor();
    CodeEditor* getCurrentCodeEditor();
    QWidget* createWelcomeWidget();
    QWidget* createKeyboardShortcutsWidget();
    void showWelcomeScreen();
    void showEditorInterface();
    void applyTheme(ThemeType theme);
    void setSidebarCollapsed(bool collapsed);
    void loadRecentProjects();
    void saveRecentProjects();
    void updateRecentProjectsOnWelcome();
    void autoSave();
    bool checkUnsavedChanges();
};

#endif // MAINWINDOW_H