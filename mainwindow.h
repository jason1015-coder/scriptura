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
#include "codeeditor.h"

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

private slots:
    void on_action_open_project_triggered();
    void on_action_save_triggered();
    void on_actionCu_t_triggered();
    void on_action_copy_triggered();
    void on_action_Paste_triggered();
    void on_action_Undo_triggered();
    void on_action_add_file_directory_triggered();
    void on_action_delete_file_directory_triggered();
    void on_action_new_window_triggered();
    void on_action_clone_window_triggered();
    void on_action_about_triggered();
    void on_action_editor_settings_triggered();
    void on_action_theme_triggered();
    void on_action_license_triggered();
    void on_fileTree_clicked(const QModelIndex &index);
    void on_tabWidget_tabCloseRequested(int index);
    void on_goUp_triggered();

private:
    Ui::MainWindow *ui;
    QString currentFile;
    QString projectDir;
    QModelIndex rootIndex;
    QList<OpenFile> openFiles;
    QFileSystemModel *fileModel;
    QToolButton *goUpButton;
    QWidget *welcomeWidget;
    QStackedWidget *editorStack;
    
    void updateCursorPosition();
    QPlainTextEdit* getCurrentEditor();
    QWidget* createWelcomeWidget();
    void showWelcomeScreen();
    void showEditorInterface();
};

#endif // MAINWINDOW_H