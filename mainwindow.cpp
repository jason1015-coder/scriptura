#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileSystemModel>
#include <QDir>
#include <QTextCursor>
#include <QSplitter>
#include <QHBoxLayout>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    sidebarWidth = 250;
    fileExplorerCollapsed = false;

    QHBoxLayout *centralLayout = qobject_cast<QHBoxLayout*>(centralWidget()->layout());
    QLayoutItem *editorItem = nullptr;
    QLayoutItem *sidebarItem = nullptr;
    if (centralLayout) {
        editorItem = centralLayout->takeAt(1);
        sidebarItem = centralLayout->takeAt(0);
    }

    sidebarWidget = new QWidget(this);
    sidebarWidget->setMinimumWidth(0);
    sidebarWidget->setLayout(ui->sidebarLayout);

    QWidget *editorWidget = new QWidget(this);
    editorWidget->setLayout(ui->editorLayout);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(sidebarWidget);
    mainSplitter->addWidget(editorWidget);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setHandleWidth(0);
    mainSplitter->setSizes(QList<int>() << sidebarWidth << 750);

    if (centralLayout) {
        centralLayout->addWidget(mainSplitter, 1);
        delete editorItem;
        delete sidebarItem;
    }

    collapseSidebarButton = new QToolButton(ui->centralwidget);
    collapseSidebarButton->setGeometry(0, 0, 32, 32);
    collapseSidebarButton->setText("◀");
    collapseSidebarButton->setToolTip("Collapse file explorer");
    collapseSidebarButton->raise();
    connect(collapseSidebarButton, &QToolButton::clicked, this, &MainWindow::toggleFileExplorerCollapsed);
    
    fileModel = new QFileSystemModel(this);
    fileModel->setRootPath(QDir::homePath());
    ui->fileTreeView->setModel(fileModel);
    
    QToolBar *toolbar = new QToolBar(this);
    toolbar->setObjectName("fileToolbar");
    ui->sidebarLayout->insertWidget(0, toolbar);
    
    goUpButton = new QToolButton(this);
    goUpButton->setText("Go Up");
    goUpButton->setEnabled(false);
    connect(goUpButton, &QToolButton::clicked, this, &MainWindow::handleGoUpClicked);
    
    connect(ui->fileTreeView, &QTreeView::clicked, this, &MainWindow::handleFileTreeClicked);
    
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

void MainWindow::handleFileTreeClicked(const QModelIndex &index)
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
        
        QPlainTextEdit *editor = new QPlainTextEdit(this);
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

void MainWindow::handleGoUpClicked()
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

void MainWindow::on_action_settings_triggered()
{
    QMessageBox::information(this, tr("Settings"), tr("Settings are not implemented yet."));
}

void MainWindow::toggleFileExplorerCollapsed()
{
    if (fileExplorerCollapsed) {
        sidebarWidget->show();
        QList<int> sizes = mainSplitter->sizes();
        int editorWidth = sizes.value(1, 750);
        mainSplitter->setSizes(QList<int>() << sidebarWidth << editorWidth);
        fileExplorerCollapsed = false;
        collapseSidebarButton->setText("◀");
        collapseSidebarButton->setToolTip("Collapse file explorer");
    } else {
        QList<int> sizes = mainSplitter->sizes();
        sidebarWidth = sizes.value(0, sidebarWidth);
        if (sidebarWidth <= 0) {
            sidebarWidth = 250;
        }
        int editorWidth = sizes.value(1, 750);
        mainSplitter->setSizes(QList<int>() << 0 << editorWidth);
        sidebarWidget->hide();
        fileExplorerCollapsed = true;
        collapseSidebarButton->setText("▶");
        collapseSidebarButton->setToolTip("Show file explorer");
    }
}
