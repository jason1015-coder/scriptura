#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "codeeditor.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect available = screen->availableGeometry();
        resize(qMin(available.width() * 7 / 10, 900), 
               qMin(available.height() * 7 / 10, 800));
    }
    
    fileModel = new QFileSystemModel(this);
    fileModel->setRootPath(QDir::homePath());
    ui->fileTreeView->setModel(fileModel);
    
    QToolBar *toolbar = new QToolBar(this);
    toolbar->setObjectName("fileToolbar");
    ui->sidebarLayout->insertWidget(0, toolbar);
    
    goUpButton = new QToolButton(this);
    goUpButton->setText("Go Up");
    goUpButton->setEnabled(false);
    connect(goUpButton, &QToolButton::clicked, this, &MainWindow::on_goUp_triggered);
    toolbar->addWidget(goUpButton);
    
    connect(ui->fileTreeView, &QTreeView::clicked, this, &MainWindow::on_fileTree_clicked);
    
    ui->tabWidget->clear();
    
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::on_tabWidget_tabCloseRequested);
}

MainWindow::~MainWindow()
{
    delete ui;
}

QPlainTextEdit* MainWindow::getCurrentEditor()
{
    return qobject_cast<QPlainTextEdit*>(ui->tabWidget->currentWidget());
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

    projectDir = dirName;
    rootIndex = fileModel->index(projectDir);
    ui->fileTreeView->setRootIndex(rootIndex);
    ui->fileTreeView->hideColumn(1);
    ui->fileTreeView->hideColumn(2);
    ui->fileTreeView->hideColumn(3);
    goUpButton->setEnabled(rootIndex.parent().isValid());
    
    setWindowTitle(QFileInfo(projectDir).fileName() + " - Scriptura");
}

void MainWindow::on_action_save_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;
        
    if (currentFile.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), 
            projectDir.isEmpty() ? QString() : projectDir, tr("All Files (*)"));
        if (fileName.isEmpty())
            return;
        currentFile = fileName;
    }

    QFile file(currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file for writing: %1").arg(file.errorString()));
        return;
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

void MainWindow::on_action_add_file_directory_triggered()
{
    if (projectDir.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please open a project first."));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, tr("Create New File"), projectDir, tr("All Files (*)"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.close();
            QMessageBox::information(this, tr("Success"), tr("File created: %1").arg(fileName));
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

void MainWindow::on_fileTree_clicked(const QModelIndex &index)
{
    QString path = fileModel->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir()) {
        rootIndex = index;
        ui->fileTreeView->setRootIndex(index);
        goUpButton->setEnabled(rootIndex.parent().isValid());
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
                ui->tabWidget->setCurrentIndex(i);
                return;
            }
        }
        
        CodeEditor *editor = new CodeEditor(this);
        editor->setPlainText(content);
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
        
        OpenFile openFile;
        openFile.filePath = path;
        openFile.fileName = fileInfo.fileName();
        openFiles.append(openFile);
        
        ui->tabWidget->addTab(editor, openFile.fileName);
        ui->tabWidget->setCurrentWidget(editor);
        
        currentFile = path;
        setWindowTitle(openFile.fileName + " - Scriptura");
    }
}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    if (index >= 0 && index < openFiles.size()) {
        if (openFiles[index].filePath == currentFile) {
            currentFile = "";
        }
        openFiles.removeAt(index);
    }
    
    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    delete widget;
    
    if (ui->tabWidget->count() > 0) {
        QPlainTextEdit *editor = getCurrentEditor();
        if (editor) {
            currentFile = openFiles[ui->tabWidget->currentIndex()].filePath;
            setWindowTitle(QFileInfo(currentFile).fileName() + " - Scriptura");
        }
    } else {
        setWindowTitle(projectDir.isEmpty() ? "Scriptura" : QFileInfo(projectDir).fileName() + " - Scriptura");
    }
}

void MainWindow::on_goUp_triggered()
{
    if (!rootIndex.isValid())
        return;
    
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
    QProcess::startDetached("konsole", QStringList());
}

void MainWindow::on_action_clone_window_triggered()
{
    bool ok;
    QString defaultDir = !projectDir.isEmpty() ? projectDir : QDir::homePath();
    QString targetDir = QInputDialog::getText(this, tr("Clone Terminal"), tr("Working directory:"), QLineEdit::Normal, defaultDir, &ok);
    if (!ok || targetDir.isEmpty())
        return;

    QDir dir(targetDir);
    if (!dir.exists())
        return;

    QProcess::startDetached("konsole", QStringList() << "--workdir" << targetDir);
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, tr("About Scriptura"), 
        tr("Scriptura\nA simple Qt-based text editor with project file browsing.\n\n"
           "Built with C++17 and Qt Widgets."));
}

void MainWindow::on_action_editor_settings_triggered()
{
    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
    if (!editor)
        return;

    bool ok;
    QFont font = QFontDialog::getFont(&ok, editor->font(), this, tr("Editor Settings"));
    if (ok) {
        editor->setFont(font);
        QSettings settings;
        settings.setValue("editor/font", font);
    }
}

void MainWindow::on_action_theme_triggered()
{
    static bool darkMode = false;
    darkMode = !darkMode;

    QPalette palette;
    if (darkMode) {
        palette.setColor(QPalette::Window, QColor(53, 53, 53));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(25, 25, 25));
        palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, Qt::white);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(53, 53, 53));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, Qt::black);
    } else {
        palette = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(palette);
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