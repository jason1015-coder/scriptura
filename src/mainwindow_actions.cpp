#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "rust_adapter.h"
#include "findreplace.h"
#include "projectsearch.h"
#include "themeicons.h"
#include "rust_adapter.h"
#include "windowanimator.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QInputDialog>
#include <QMenu>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QUrl>
#include <QDesktopServices>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "version.h"

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

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, tr("About Scriptura"),
        tr("Scriptura\nA simple Qt-based text editor with project file browsing.\n\n"
           "Version: %1\n\n"
           "Built with C++17 and Qt Widgets.\n\n"
           "License: MIT").arg(SCRIPTURA_VERSION));
}

void MainWindow::toggleInspector()
{
    if (!m_inspectorDrawer)
        return;

    const bool visible = m_inspectorDrawer->width() > 0;
    const int targetWidth = visible ? 0 : 280;

    QPropertyAnimation *animation = new QPropertyAnimation(m_inspectorDrawer, "maximumWidth", this);
    animation->setDuration(200);
    animation->setStartValue(m_inspectorDrawer->width());
    animation->setEndValue(targetWidth);
    animation->setEasingCurve(QEasingCurve::InOutCubic);

    connect(animation, &QPropertyAnimation::finished, this, [this, targetWidth]() {
        if (targetWidth == 0) {
            m_inspectorDrawer->setMinimumWidth(0);
            m_inspectorDrawer->setMaximumWidth(0);
        } else {
            m_inspectorDrawer->setMinimumWidth(280);
            m_inspectorDrawer->setMaximumWidth(280);
        }
    });

    animation->start(QAbstractAnimation::DeleteWhenStopped);

    if (m_titleBar) {
        m_titleBar->inspectorToggleButton->setChecked(!visible);
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

void MainWindow::updateStatusBar()
{
    if (!m_statusBarWidget) return;

    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor) {
        m_statusBarWidget->setFileName(tr("(no file)"));
        m_statusBarWidget->setLanguage(tr("Text"));
        m_statusBarWidget->setCursorPosition(1, 1);
        m_statusBarWidget->setModified(false);
        return;
    }

    // File name
    QString filePath = editor->filePath();
    QString fileName = filePath.isEmpty() ? tr("Untitled") : QFileInfo(filePath).fileName();
    m_statusBarWidget->setFileName(fileName);
    m_statusBarWidget->setModified(editor->document()->isModified());

    // Language detection
    QString lang = tr("Text");
    if (!filePath.isEmpty()) {
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || ext == "cxx") lang = "C++";
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
        else if (ext == "xml") lang = "XML";
        else if (ext == "yaml" || ext == "yml") lang = "YAML";
        else if (ext == "sh" || ext == "bash") lang = "Shell";
        else if (ext == "sql") lang = "SQL";
        else if (ext == "toml") lang = "TOML";
        else if (ext == "rb") lang = "Ruby";
        else if (ext == "php") lang = "PHP";
        else if (ext == "swift") lang = "Swift";
        else if (ext == "kt") lang = "Kotlin";
        else if (ext == "dart") lang = "Dart";
        else if (ext == "lua") lang = "Lua";
    }
    m_statusBarWidget->setLanguage(lang);

    // Cursor position
    int line = editor->textCursor().blockNumber() + 1;
    int column = editor->textCursor().positionInBlock() + 1;
    m_statusBarWidget->setCursorPosition(line, column);
    m_statusBarWidget->setLineCount(editor->document()->blockCount());

    // Encoding (use EncodingManager for actual detection)
    if (m_encodingManager && !filePath.isEmpty()) {
        m_statusBarWidget->setEncoding(m_encodingManager->detectEncoding(filePath));
        m_statusBarWidget->setLineEnding(m_encodingManager->detectLineEnding(filePath));
    } else {
        m_statusBarWidget->setEncoding("UTF-8");
        m_statusBarWidget->setLineEnding("LF");
    }
    m_statusBarWidget->setIndentation(QString("Spaces: %1").arg(editor->tabWidth()));
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
    // Update status bar modified indicator
    if (m_statusBarWidget) {
        m_statusBarWidget->setModified(modified);
    }
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
    int idx = bottomPanelStack->currentIndex();
    if (idx >= 0 && idx < m_panelButtons.size()) {
        showBottomPanelIndex(idx);
    }
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

void MainWindow::autoSave()
{
    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].modified) {
            QFileInfo fileInfo(openFiles[i].filePath);
            if (openFiles[i].filePath.isEmpty() || !fileInfo.exists()) {
                continue;
            }
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
                // Clear the dirty flag on the document AND force the tab title to
                // update so the '*' disappears instantly (not waiting on the
                // modificationChanged signal). updateTabModified() refreshes the
                // openFiles[i].modified flag and both the bottom and top tab bars.
                if (editor)
                    editor->document()->setModified(false);
                updateTabModified(i, false);
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
                for (int i = 0; i < openFiles.size(); ++i) {
                    if (openFiles[i].filePath == f.filePath && openFiles[i].modified) {
                        ui->tabWidget->setCurrentIndex(i);
                        currentFile = openFiles[i].filePath;
                        on_action_save_triggered();
                        if (openFiles[i].modified) {
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

void MainWindow::on_action_git_pull_triggered()
{
    QMessageBox::information(this, tr("Git Pull"),
        tr("Git functionality is provided by the Git application.\n"
           "Install it from the Plugin Marketplace to use Git features."));
}

void MainWindow::on_action_git_fetch_triggered()
{
    QMessageBox::information(this, tr("Git Fetch"),
        tr("Git functionality is provided by the Git application.\n"
           "Install it from the Plugin Marketplace to use Git features."));
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
    showBottomPanelIndex(0);
    projectSearchPanel->show();
    ui->bottomPanelContainer->show();
    projectSearchPanel->setRootPath(root);
    projectSearchPanel->search(QString(), root);
}

void MainWindow::on_action_command_palette_triggered()
{
    // The command palette was merged into the universal search popup so there
    // is a single surface for commands, files, settings, and themes.
    if (m_universalSearch)
        m_universalSearch->openSearch();
}
