#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "problempanel.h"
#include "todopanel.h"
#include "rust_adapter.h"
#include "findreplace.h"
#include "projectsearch.h"
#include "commandpalette.h"
#include "themeicons.h"
#include "rust_adapter.h"
#include "windowanimator.h"
#include "applicationdock.h"

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


void MainWindow::toggleTerminalPanel()
{
    if (editorStack->currentWidget() == terminalPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
        m_activeDockAppId.clear();
    } else {
        m_previousEditorStackIndex = editorStack->currentIndex();
        editorStack->setCurrentWidget(terminalPanel);
        m_activeDockAppId = "com.scriptura.terminal";
        if (ui->bottomPanelContainer->isVisible()) {
            ui->bottomPanelContainer->hide();
            problemPanel->hide();
            gitPanel->hide();
        }
        if (!terminalPanel->isRunning()) {
            terminalPanel->startShell(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        } else {
            terminalPanel->setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        }
    }
    if (m_appDock) m_appDock->setActiveApp(m_activeDockAppId);
}

void MainWindow::toggleSidebar()
{
    if (sidebarToggleButton->isChecked()) {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->sidebarDrawer, true, 200);
        } else {
            ui->sidebarDrawer->setMaximumWidth(240);
            ui->sidebarDrawer->setMinimumWidth(48);
        }
    } else {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->sidebarDrawer, false, 200);
        } else {
            ui->sidebarDrawer->setMaximumWidth(0);
            ui->sidebarDrawer->setMinimumWidth(0);
        }
    }
}


void MainWindow::toggleTodoPanel()
{
    if (editorStack->currentWidget() == todoPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
        m_activeDockAppId.clear();
    } else {
        m_previousEditorStackIndex = editorStack->currentIndex();
        editorStack->setCurrentWidget(todoPanel);
        m_activeDockAppId = "com.scriptura.todo";
        if (ui->bottomPanelContainer->isVisible()) {
            ui->bottomPanelContainer->hide();
            problemPanel->hide();
            gitPanel->hide();
        }
    }
    if (m_appDock) m_appDock->setActiveApp(m_activeDockAppId);
}

void MainWindow::toggleProblemPanel()
{
    bool isVisible = problemPanel->isVisible() && ui->bottomPanelContainer->isVisible();
    if (!isVisible) {
        bottomPanelTabs->setCurrentIndex(0);
        bottomPanelStack->setCurrentIndex(0);
        problemPanel->show();
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, true, 200);
        } else {
            ui->bottomPanelContainer->show();
        }
        // Close other panels
        if (editorStack->currentWidget() == todoPanel || editorStack->currentWidget() == terminalPanel) {
            if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
                editorStack->setCurrentIndex(m_previousEditorStackIndex);
            }
        }
        if (gitPanel->isVisible()) gitPanel->hide();
        m_activeDockAppId = "com.scriptura.problems";
    } else {
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, false, 200);
        } else {
            ui->bottomPanelContainer->hide();
        }
        problemPanel->hide();
        m_activeDockAppId.clear();
    }
    if (m_appDock) m_appDock->setActiveApp(m_activeDockAppId);
}

void MainWindow::showGitPanel()
{
    bottomPanelTabs->setCurrentIndex(1);
    bottomPanelStack->setCurrentIndex(1);
    gitPanel->show();
    if (m_windowAnimator) {
        m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, true, 200);
    } else {
        ui->bottomPanelContainer->show();
    }
    if (editorStack->currentWidget() == todoPanel || editorStack->currentWidget() == terminalPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count())
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
    }
    if (problemPanel->isVisible()) problemPanel->hide();
    m_activeDockAppId = "com.scriptura.git";
    if (m_appDock) m_appDock->setActiveApp(m_activeDockAppId);
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
    bottomPanelTabs->setCurrentIndex(bottomPanelStack->currentIndex());
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
            // Skip if file no longer exists or path is empty
            if (openFiles[i].filePath.isEmpty() || !fileInfo.exists()) {
                continue;
            }
            // Skip if file is not writable
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
                openFiles[i].modified = false;
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
                // Find and save the modified file
                for (int i = 0; i < openFiles.size(); ++i) {
                    if (openFiles[i].filePath == f.filePath && openFiles[i].modified) {
                        ui->tabWidget->setCurrentIndex(i);
                        currentFile = openFiles[i].filePath;
                        on_action_save_triggered();
                        if (openFiles[i].modified) {
                            // Save failed or cancelled
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
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }
    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"pull"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
        gitPanel->detectMergeConflicts();
    } else {
        gitPanel->setOutput(tr("Failed to run git pull. The operation may have timed out."));
    }
    // Close other panels
    if (editorStack->currentWidget() == terminalPanel || editorStack->currentWidget() == todoPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
    }
    showGitPanel();
}

void MainWindow::on_action_git_fetch_triggered()
{
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }
    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"fetch"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
    } else {
        gitPanel->setOutput(tr("Failed to run git fetch. The operation may have timed out."));
    }
    // Close other panels
    if (editorStack->currentWidget() == terminalPanel || editorStack->currentWidget() == todoPanel) {
        if (m_previousEditorStackIndex >= 0 && m_previousEditorStackIndex < editorStack->count()) {
            editorStack->setCurrentIndex(m_previousEditorStackIndex);
        }
    }
    if (problemPanel->isVisible()) {
        problemPanel->hide();
    }
    showGitPanel();
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
    bottomPanelTabs->setCurrentIndex(2);
    bottomPanelStack->setCurrentIndex(2);
    projectSearchPanel->show();
    ui->bottomPanelContainer->show();
    projectSearchPanel->setRootPath(root);
    projectSearchPanel->search(QString(), root);
}

void MainWindow::on_action_command_palette_triggered()
{
    if (!commandPalette) return;
    commandPalette->registerCommand({"open-project", tr("Open Project..."), "Ctrl+Shift+O", [this]() { on_action_open_project_triggered(); }});
    commandPalette->registerCommand({"open-file", tr("Open File..."), "Ctrl+O", [this]() { on_action_open_file_triggered(); }});
    commandPalette->registerCommand({"save", tr("Save"), "Ctrl+S", [this]() { on_action_save_triggered(); }});
    commandPalette->registerCommand({"save-as", tr("Save As..."), "Ctrl+Shift+S", [this]() { on_action_save_as_triggered(); }});
    commandPalette->registerCommand({"new-file", tr("New File..."), "Ctrl+N", [this]() { on_action_add_file_directory_triggered(); }});
    commandPalette->registerCommand({"undo", tr("Undo"), "Ctrl+Z", [this]() { on_action_Undo_triggered(); }});
    commandPalette->registerCommand({"redo", tr("Redo"), "Ctrl+Y", [this]() { on_action_Redo_triggered(); }});
    commandPalette->registerCommand({"cut", tr("Cut"), "Ctrl+X", [this]() { on_actionCu_t_triggered(); }});
    commandPalette->registerCommand({"copy", tr("Copy"), "Ctrl+C", [this]() { on_action_copy_triggered(); }});
    commandPalette->registerCommand({"paste", tr("Paste"), "Ctrl+V", [this]() { on_action_Paste_triggered(); }});
    commandPalette->registerCommand({"find", tr("Find in File"), "Ctrl+F", [this]() { on_action_find_triggered(); }});
    commandPalette->registerCommand({"replace", tr("Find and Replace"), "Ctrl+H", [this]() { on_action_replace_triggered(); }});
    commandPalette->registerCommand({"project-search", tr("Project Search"), "Ctrl+Shift+F", [this]() { on_action_project_search_triggered(); }});
    commandPalette->registerCommand({"git-commit", tr("Git Commit..."), "", [this]() { on_action_git_commit_triggered(); }});
    commandPalette->registerCommand({"git-push", tr("Git Push..."), "", [this]() { on_action_git_push_triggered(); }});
    commandPalette->registerCommand({"git-pull", tr("Git Pull..."), "", [this]() { on_action_git_pull_triggered(); }});
    commandPalette->registerCommand({"git-fetch", tr("Git Fetch"), "", [this]() { on_action_git_fetch_triggered(); }});
    commandPalette->registerCommand({"find-references", tr("Find References"), "", [this]() {
        CodeEditor *editor = getCurrentCodeEditor();
        if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
            return;
        QTextCursor c = editor->textCursor();
        lspClient->references(QUrl::fromLocalFile(currentFile).toString(),
                              c.blockNumber(), c.positionInBlock());
    }});
    commandPalette->registerCommand({"theme", tr("Theme"), "Ctrl+T", [this]() { on_action_theme_triggered(); }});
    commandPalette->registerCommand({"editor-settings", tr("Editor Settings"), "", [this]() { on_action_editor_settings_triggered(); }});

}
