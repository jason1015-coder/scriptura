#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QFileSystemModel>
#include <QTabWidget>
#include <QTreeView>
#include <QPlainTextEdit>

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
    void on_fileTree_clicked(const QModelIndex &index);
    void on_tabWidget_tabCloseRequested(int index);

private:
    Ui::MainWindow *ui;
    QString currentFile;
    QString projectDir;
    QList<OpenFile> openFiles;
    QFileSystemModel *fileModel;
    
    void updateCursorPosition();
    QPlainTextEdit* getCurrentEditor();
};

#endif // MAINWINDOW_H