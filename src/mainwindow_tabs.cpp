#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "rust_adapter.h"
#include "themeicons.h"
#include "fileicons.h"
#include "foldmanager.h"
#include "bookmarkmanager.h"
#include "snippetmanager.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include "internals/settings_store.h"
#include <QProcess>
#include <QMenu>
#include <QInputDialog>
#include <QStorageInfo>
#include <QUrl>
#include <QSignalBlocker>
#include <QStringConverter>
#include "encodingmanager.h"
#include "pluginmarketplace.h"
#include "terminal/terminalpanel.h"

namespace {
// Logical edge of a tab's type icon. Matches the file tree's glyph size so the
// two read at the same visual weight.
constexpr int kTabIconSize = 16;
} // namespace

void MainWindow::showEditorInterface()
{
    editorStack->setCurrentWidget(ui->tabWidget);
    updateTabBarVisibility();
}

void MainWindow::updateCursorPosition()
{
    // No-op: cursor position updates are handled by updateStatusBar()
    // connected to cursorPositionChanged in openFileInTab.
}


void MainWindow::on_action_open_project_triggered()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Open Project"), QString(),
        QFileDialog::DontUseNativeDialog);
    if (dirName.isEmpty())
        return;

    if (!recentProjects.contains(dirName)) {
        recentProjects.prepend(dirName);
        while (recentProjects.size() > maxRecentProjects)
            recentProjects.removeLast();
        saveRecentProjects();
    }

    loadProjectDirectory(dirName);
}

void MainWindow::loadProjectDirectory(const QString &dirName)
{
    projectDir = dirName;
    rootIndex = fileModel->index(projectDir);
    ui->fileTreeView->setRootIndex(rootIndex);
    ui->fileTreeView->hideColumn(1);

    // New terminals open in the project folder rather than the home directory.
    if (m_terminalPanel)
        m_terminalPanel->setWorkingDirectory(projectDir);
    
    // Update the universal search file model to the current project
    if (m_universalSearch) {
        m_universalSearch->setFileModel(fileModel, projectDir);
    }
    ui->fileTreeView->hideColumn(2);
    ui->fileTreeView->hideColumn(3);

    // Auto-save is armed via idle debounce (see constructor) — nothing to re-start
    // here; opening a project changes no auto-save behavior.

    setWindowTitle(QFileInfo(projectDir).fileName() + " - Scriptura");

    // Initialize LSP for the project directory
    QString rootUri = QUrl::fromLocalFile(projectDir).toString();
    if (!lspClient->isRunning()) {
        // Try to start a language server for the project
        startLanguageServerForProject(projectDir);
    }


}


void MainWindow::on_action_save_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;

    CodeEditor *codeEditor = qobject_cast<CodeEditor *>(editor);
    QString targetFile = codeEditor && !codeEditor->filePath().isEmpty()
        ? codeEditor->filePath()
        : currentFile;

    if (targetFile.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
            projectDir.isEmpty() ? QString() : projectDir,
            tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
        if (fileName.isEmpty())
            return;
        targetFile = fileName;
        currentFile = targetFile;
        if (codeEditor)
            codeEditor->setFilePath(targetFile);
    } else {
        currentFile = targetFile;
    }

    QFile file(targetFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString errorMsg;
        QString errorStr = file.errorString();
        if (errorStr.contains("Permission", Qt::CaseInsensitive)) {
            errorMsg = tr("Permission denied. Please check file permissions.");
        } else if (errorStr.contains("disk", Qt::CaseInsensitive) || errorStr.contains("space", Qt::CaseInsensitive)) {
            errorMsg = tr("Disk full. Cannot save file.");
        } else if (targetFile.contains("://")) {
            errorMsg = tr("Network path unavailable. Please check connection.");
        } else {
            errorMsg = tr("Cannot open file for writing: %1").arg(errorStr);
        }
        QMessageBox::warning(this, tr("Error"), errorMsg);
        return;
    }

    QStorageInfo storage(QFileInfo(targetFile).absolutePath());
    qint64 contentSize = editor->toPlainText().toUtf8().size();
    if (storage.bytesAvailable() < contentSize * 2) {
        QMessageBox::warning(this, tr("Warning"),
            tr("Low disk space. Available: %1 MB").arg(storage.bytesAvailable() / (1024 * 1024)));
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    QString content = editor->toPlainText();
    QString lineEnding = m_fileLineEndings.value(targetFile, "LF");
    if (lineEnding == "CRLF") {
        content.replace("\n", "\r\n");
    } else if (lineEnding == "CR") {
        content.replace("\n", "\r");
    }
    out << content;
    file.close();

    int editorIndex = ui->tabWidget->indexOf(editor);
    editor->document()->setModified(false);
    if (editorIndex >= 0 && editorIndex < openFiles.size()) {
        if (openFiles[editorIndex].filePath.isEmpty()) {
            // An untitled tab is getting its first real path: promote it so
            // openFiles, the top tab bar id, and the tooltip all agree.
            QString oldId = m_tabIds.value(editor);
            openFiles[editorIndex].filePath = targetFile;
            openFiles[editorIndex].fileName = QFileInfo(targetFile).fileName();
            m_tabIds[editor] = targetFile;
            int barIdx = findTabBarIndexForId(oldId);
            if (barIdx >= 0) {
                tabBar->setTabData(barIdx, targetFile);
                tabBar->setTabToolTip(barIdx, targetFile);
            }
            // The buffer now has a path, so it can show a real type icon
            // instead of the unsaved plain-text glyph.
            updateTabIcon(targetFile);
            addRecentFile(targetFile);
        } else {
            for (OpenFile &f : openFiles) {
                if (f.filePath == targetFile) {
                    f.modified = false;
                    break;
                }
            }
        }
        updateTabModified(editorIndex, false);
    }

    setWindowTitle(QFileInfo(targetFile).fileName() + " - Scriptura");
}

void MainWindow::on_action_save_as_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;

    CodeEditor *codeEditor = qobject_cast<CodeEditor *>(editor);
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File As"),
        currentFile.isEmpty() ? (projectDir.isEmpty() ? QString() : projectDir) : currentFile,
        tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file for writing: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();

    int editorIndex = ui->tabWidget->indexOf(editor);
    QString oldId = m_tabIds.value(editor);
    for (int i = 0; i < openFiles.size(); i++) {
        if (editorIndex >= 0 && i != editorIndex)
            continue;
        if (openFiles[i].filePath == currentFile || (editorIndex >= 0 && i == editorIndex)) {
            const QString oldPath = openFiles[i].filePath;
            openFiles[i].filePath = fileName;
            openFiles[i].fileName = QFileInfo(fileName).fileName();
            openFiles[i].modified = false;
            if (!oldPath.isEmpty() && oldPath != fileName) {
                m_fileLineEndings[fileName] = m_fileLineEndings.take(oldPath);
                m_fileEncodings[fileName] = m_fileEncodings.take(oldPath);
            }
            if (codeEditor)
                codeEditor->setFilePath(fileName);
            // The stored tab id/path changed — refresh the top tab bar entry
            // before updateTabModified() looks it up by the new id.
            m_tabIds[editor] = fileName;
            int barIdx = findTabBarIndexForId(oldId);
            if (barIdx >= 0) {
                tabBar->setTabData(barIdx, fileName);
                tabBar->setTabToolTip(barIdx, fileName);
                tabBar->setTabText(barIdx, openFiles[i].fileName);
            }
            // Save As can retarget a tab at a different file type entirely
            // (untitled -> main.py), so the icon has to be re-derived.
            updateTabIcon(fileName);
            if (editorIndex >= 0)
                updateTabModified(editorIndex, false);
            else
                ui->tabWidget->setTabText(i, openFiles[i].fileName);
            break;
        }
    }

    currentFile = fileName;
    setWindowTitle(QFileInfo(fileName).fileName() + " - Scriptura");
}

void MainWindow::on_action_open_file_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
        projectDir.isEmpty() ? QDir::homePath() : projectDir,
        tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    openFileInTab(fileName);
}


void MainWindow::openFileInTab(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString errorMsg;
        QString errorStr = file.errorString();
        if (errorStr.contains("Permission", Qt::CaseInsensitive)) {
            errorMsg = tr("Permission denied. Please check file permissions.");
        } else if (fileName.contains("://")) {
            errorMsg = tr("Network path unavailable. Please check connection.");
        } else {
            errorMsg = tr("Cannot open file: %1").arg(errorStr);
        }
        QMessageBox::warning(this, tr("Error"), errorMsg);
        return;
    }

    // Check file size and warn for large files
    qint64 fileSize = file.size();
    if (fileSize > 10 * 1024 * 1024) { // 10MB
        QMessageBox::warning(this, tr("Large File"),
            tr("This file is %1 MB. Opening large files may impact performance.").arg(fileSize / (1024 * 1024)));
    }

    // Detect encoding and line endings
    QString encoding = "UTF-8";
    QString lineEnding = "LF";
    if (m_encodingManager) {
        encoding = m_encodingManager->detectEncoding(fileName);
        lineEnding = m_encodingManager->detectLineEnding(fileName);
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    // Store encoding/line ending metadata for this file
    m_fileEncodings[fileName] = encoding;
    m_fileLineEndings[fileName] = lineEnding;

    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].filePath == fileName) {
            showEditorInterface();
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    CodeEditor *editor = new CodeEditor(this);
    editor->setLanguageForFile(fileName);
    editor->installEventFilter(this);
    connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
    QFont savedFont = SettingsStore::instance().value("editor/font", editor->font()).value<QFont>();
    editor->setFont(savedFont);
    editor->setTabWidth(SettingsStore::instance().value("editor/tabWidth", editor->tabWidth()).toInt());
    int w = SettingsStore::instance().value("editor/width", 0).toInt();
    if (w > 0) editor->setMinimumWidth(w);
    editor->setPlainText(content);

    int tabIndex = openFiles.size();
    Q_UNUSED(tabIndex);
    // Look the tab index up dynamically: a captured index goes stale as soon
    // as any other tab closes (the '*' would land on the wrong tab).
    QPointer<CodeEditor> edGuard(editor);
    connect(editor, &QPlainTextEdit::modificationChanged, this,
            [this, edGuard](bool m) {
                if (!edGuard)
                    return;
                int i = ui->tabWidget->indexOf(edGuard);
                if (i >= 0)
                    updateTabModified(i, m);
            });
    // Re-arm the idle-debounce auto-save timer on every edit.
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
        autoSaveTimer->start();
    });
    // Only one cursorPositionChanged connection: updateStatusBar handles everything
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (lspClient->isRunning())
            m_hoverTimer->start();
    });

    openFiles.append({fileName, QFileInfo(fileName).fileName(), false});
    showEditorInterface();
    ui->tabWidget->addTab(editor, QFileInfo(fileName).fileName());
    int tabBarIndex = tabBar->addTab(QFileInfo(fileName).fileName());
    tabBar->setTabData(tabBarIndex, fileName);
    tabBar->setTabIcon(tabBarIndex, ThemeIcons::instance()->statefulIcon(
                                        FileIcons::forFileName(fileName), kTabIconSize));
    tabBar->setTabToolTip(tabBarIndex, fileName);
    tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createEditorTabCloseButton(editor));
    m_tabIds[editor] = fileName;
    ui->tabWidget->setCurrentWidget(editor);
    tabBar->setCurrentIndex(tabBarIndex);
    currentFile = fileName;
    setWindowTitle(QFileInfo(fileName).fileName() + " - Scriptura");
    showSearchBar(true);
    addRecentFile(fileName);
}


void MainWindow::on_fileTreeView_clicked(const QModelIndex &index)
{
    QString path = fileModel->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir()) {
        // Expand/collapse the directory inline instead of changing root
        ui->fileTreeView->setExpanded(index, !ui->fileTreeView->isExpanded(index));

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
                showEditorInterface();
                ui->tabWidget->setCurrentIndex(i);
                return;
            }
        }
        
        CodeEditor *editor = new CodeEditor(this);
        editor->setLanguageForFile(path);
        editor->installEventFilter(this);
        connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
        QFont savedFont = SettingsStore::instance().value("editor/font", editor->font()).value<QFont>();
        editor->setFont(savedFont);
        int savedTabWidth = SettingsStore::instance().value("editor/tabWidth", editor->tabWidth()).toInt();
        editor->setTabWidth(savedTabWidth);
        int savedEditorWidth = SettingsStore::instance().value("editor/width", 0).toInt();
        if (savedEditorWidth > 0)
            editor->setMinimumWidth(savedEditorWidth);
        editor->setPlainText(content);

        int tabIndex = openFiles.size();
        Q_UNUSED(tabIndex);
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
        // Dynamic lookup — a captured index goes stale when other tabs close.
        QPointer<CodeEditor> treeEdGuard(editor);
        connect(editor, &QPlainTextEdit::modificationChanged, this,
                [this, treeEdGuard](bool m) {
                    if (!treeEdGuard)
                        return;
                    int i = ui->tabWidget->indexOf(treeEdGuard);
                    if (i >= 0)
                        updateTabModified(i, m);
                });
        connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
            lspDebounceTimer->start();
        });
        // Re-arm the idle-debounce auto-save timer on every edit.
        connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
            autoSaveTimer->start();
        });

        OpenFile openFile;
        openFile.filePath = path;
        openFile.fileName = fileInfo.fileName();
        openFiles.append(openFile);

        showEditorInterface();
        ui->tabWidget->addTab(editor, openFile.fileName);
        int tabBarIndex = tabBar->addTab(openFile.fileName);
        tabBar->setTabData(tabBarIndex, path);
        tabBar->setTabIcon(tabBarIndex, ThemeIcons::instance()->statefulIcon(
                                            FileIcons::forFileName(path), kTabIconSize));
        tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createEditorTabCloseButton(editor));
        m_tabIds[editor] = path;
        ui->tabWidget->setCurrentWidget(editor);
        tabBar->setCurrentIndex(tabBarIndex);

        currentFile = path;
        setWindowTitle(openFile.fileName + " - Scriptura");
        showSearchBar(true);

        // LSP: Open file in language server
        startLanguageServer(path);
        QString uri = QUrl::fromLocalFile(path).toString();
        QString langId = QFileInfo(path).suffix().toLower();
        lspClient->didOpen(uri, langId, content);
    }
}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    if (index < 0 || index >= openFiles.size() || index >= ui->tabWidget->count())
        return;
    if (openFiles[index].modified) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("%1 has unsaved changes. Save before closing?").arg(openFiles[index].fileName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Save) {
            ui->tabWidget->setCurrentIndex(index);
            currentFile = openFiles[index].filePath;
            on_action_save_triggered();
            if (openFiles[index].modified) {
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    // Remove encoding/line ending metadata on tab close
    QString closedPath = openFiles[index].filePath;
    QWidget *widget = ui->tabWidget->widget(index);
    // Resolve the top-bar tab by its stable id, not by position: the top bar
    // also holds settings/panel tabs so indices don't line up with openFiles.
    QString closedId = m_tabIds.value(widget, closedPath);
    if (!closedPath.isEmpty()) {
        m_fileEncodings.remove(closedPath);
        m_fileLineEndings.remove(closedPath);
        // LSP: Close file in language server
        QString closedUri = QUrl::fromLocalFile(closedPath).toString();
        lspClient->didClose(closedUri);
    }

    int barIdx = findTabBarIndexForId(closedId);
    if (barIdx >= 0)
        tabBar->removeTab(barIdx);
    openFiles.removeAt(index);
    m_tabIds.remove(widget);
    ui->tabWidget->removeTab(index);
    delete widget;

    if (ui->tabWidget->count() > 0) {
        // Do NOT call showEditorInterface() here — onTopTabChanged() was already
        // triggered by tabBar->removeTab() above and correctly switched the
        // editorStack to match the new current tab in the tabBar (which may be a
        // settings tab). Calling showEditorInterface() here would unconditionally
        // switch back to ui->tabWidget, causing the settings page to appear
        // blank when a file is closed while viewing settings.
        updateStatusBar();

    } else {
        // No file tabs remain — only show editor interface if tabBar is also
        // empty (onTopTabChanged(-1) returned early without switching).
        // If settings tabs remain, onTopTabChanged already handled the switch.
        if (tabBar->count() == 0) {
            showEditorInterface();
        }
        setWindowTitle(projectDir.isEmpty() ? "Scriptura" : QFileInfo(projectDir).fileName() + " - Scriptura");
        showSearchBar(false);
    }
}

void MainWindow::on_fileTreeView_contextMenu(const QPoint &pos)
{
    QModelIndex index = ui->fileTreeView->indexAt(pos);
    // Select the item under the cursor for visual feedback
    if (index.isValid()) {
        ui->fileTreeView->setCurrentIndex(index);
    }

    if (!m_fileContextMenu) {
        m_fileContextMenu = new QMenu(this);
        ThemeIcons *icons = ThemeIcons::instance();

        m_ctxNewFile = m_fileContextMenu->addAction(tr("New File..."));
        icons->setIcon(m_ctxNewFile, ":/icons/file.svg");

        m_ctxNewFolder = m_fileContextMenu->addAction(tr("New Folder..."));
        icons->setIcon(m_ctxNewFolder, ":/icons/folder.svg");

        m_fileContextMenu->addSeparator();

        m_ctxRename = m_fileContextMenu->addAction(tr("Rename..."));
        m_ctxDelete = m_fileContextMenu->addAction(tr("Delete"));
        icons->setIcon(m_ctxDelete, ":/icons/close.svg");
    }

    // Rename/Delete need a row to act on; the rest work anywhere.
    const bool hasTarget = index.isValid();
    m_ctxRename->setEnabled(hasTarget);
    m_ctxDelete->setEnabled(hasTarget);

    QAction *selected = m_fileContextMenu->exec(ui->fileTreeView->mapToGlobal(pos));
    if (!selected)
        return;

    QAction *newFileAction = m_ctxNewFile;
    QAction *newFolderAction = m_ctxNewFolder;
    QAction *renameAction = m_ctxRename;
    QAction *deleteAction = m_ctxDelete;

    if (selected == newFileAction || selected == newFolderAction) {
        // Determine target directory
        QString targetDir;
        if (index.isValid() && fileModel->isDir(index)) {
            targetDir = fileModel->filePath(index);
        } else if (index.isValid()) {
            targetDir = fileModel->fileInfo(index).absolutePath();
        } else {
            targetDir = projectDir.isEmpty() ? fileModel->rootPath() : projectDir;
        }

        if (selected == newFileAction) {
            QString fileName = QInputDialog::getText(this, tr("New File"),
                tr("File name:"), QLineEdit::Normal, QString());
            if (!fileName.isEmpty()) {
                QString fullPath = targetDir + QDir::separator() + fileName;
                QFile file(fullPath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.close();
                } else {
                    QMessageBox::warning(this, tr("Error"),
                        tr("Cannot create file: %1").arg(fullPath));
                }
            }
        } else {
            QString dirName = QInputDialog::getText(this, tr("New Folder"),
                tr("Folder name:"), QLineEdit::Normal, QString());
            if (!dirName.isEmpty()) {
                QDir dir(targetDir);
                if (!dir.mkdir(dirName)) {
                    QMessageBox::warning(this, tr("Error"),
                        tr("Cannot create folder: %1").arg(dirName));
                }
            }
        }
    } else if (selected == renameAction && index.isValid()) {
        QString oldPath = fileModel->filePath(index);
        QFileInfo fi(oldPath);
        QString newName = QInputDialog::getText(this, tr("Rename"),
            tr("New name:"), QLineEdit::Normal, fi.fileName());
        if (!newName.isEmpty() && newName != fi.fileName()) {
            QString newPath = fi.dir().absoluteFilePath(newName);
            QFile file(oldPath);
            if (!file.rename(newPath)) {
                QMessageBox::warning(this, tr("Error"),
                    tr("Cannot rename to: %1").arg(newName));
            }
        }
    } else if (selected == deleteAction && index.isValid()) {
        // Reuse the existing delete slot to avoid duplicating logic
        on_action_delete_file_directory_triggered();
    }
}


void MainWindow::onTopTabChanged(int index)
{
    if (index < 0 || index >= tabBar->count())
        return;

    QVariant data = tabBar->tabData(index);
    if (data.typeId() == QMetaType::Int) {
        // Settings tab (unified)
        TabType type = static_cast<TabType>(data.toInt());
        if (type == TabType::Settings) {
            qDebug() << "onTopTabChanged: switching to unifiedSettingsWidget";
            editorStack->setCurrentWidget(unifiedSettingsWidget);
            unifiedSettingsWidget->show();
        }
    } else if (data.typeId() == QMetaType::QString) {
        QString strData = data.toString();
        if (strData.startsWith("panel:")) {
            // Panel tab: the panel is the page on screen, filling the content area.
            bool ok = false;
            int panelIndex = strData.mid(6).toInt(&ok);
            if (ok)
                openPanelAsTab(panelIndex);
        } else {
            // File tab — resolve by stable id (file path, or "untitled:N").
            // The old code matched openFiles by path positionally, which
            // broke for untitled tabs (empty path) and after Save As.
            QString id = strData;
            QWidget *page = nullptr;
            for (auto it = m_tabIds.constBegin(); it != m_tabIds.constEnd(); ++it) {
                if (it.value() == id) {
                    page = it.key();
                    break;
                }
            }
            int i = page ? ui->tabWidget->indexOf(page) : -1;
            if (i < 0) {
                for (int k = 0; k < openFiles.size(); ++k) {
                    if (openFiles[k].filePath == id) {
                        i = k;
                        break;
                    }
                }
            }
            if (i >= 0) {
                currentFile = openFiles[i].filePath;
                ui->tabWidget->setCurrentIndex(i);
                editorStack->setCurrentWidget(ui->tabWidget);
            }
        }
    }
    updateTabBarVisibility();
}



void MainWindow::updateTabBarVisibility()
{
    if (ui->tabWidget->count() > 0 || tabBar->count() > 0) {
        tabBar->show();
    } else {
        tabBar->hide();
    }
}

QPushButton* MainWindow::createSettingsTabCloseButton(int tabIndex)
{
    QPushButton *closeBtn = new QPushButton();
    ThemeIcons::instance()->setIcon(closeBtn, ":/icons/close.svg");
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    // Capture the TabType instead of the (stale) index — indices shift after other tabs close
    TabType type = static_cast<TabType>(tabBar->tabData(tabIndex).toInt());
    connect(closeBtn, &QPushButton::clicked, this, [this, type]() {
        // Find the tab's current index by matching its TabType data
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabData(i).toInt() == static_cast<int>(type)) {
                tabBar->removeTab(i);
                break;
            }
        }
        // After removal, show whatever the newly selected tab holds: a file, the
        // settings page, or a panel — assuming "editor" would blank a panel tab.
        int cur = tabBar->currentIndex();
        if (cur < 0)
            showEditorInterface();
        else
            onTopTabChanged(cur);
        updateTabBarVisibility();
    });
    return closeBtn;
}

int MainWindow::findTabBarIndexForId(const QString &id) const
{
    for (int i = 0; i < tabBar->count(); ++i) {
        QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString && data.toString() == id)
            return i;
    }
    return -1;
}

void MainWindow::updateTabIcon(const QString &tabId)
{
    // A panel tab carries "panel:<index>"; its glyph is the panel's own icon,
    // not a file type.
    if (tabId.startsWith(QLatin1String("panel:"))) {
        bool ok = false;
        const int panelIndex = tabId.mid(6).toInt(&ok);
        const QString iconPath = ok ? m_panelIcons.value(panelIndex) : QString();
        if (!iconPath.isEmpty()) {
            const int index = findTabBarIndexForId(tabId);
            if (index >= 0) {
                tabBar->setTabIcon(index, ThemeIcons::instance()->statefulIcon(
                                                iconPath, kTabIconSize));
            }
        }
        return;
    }

    const int index = findTabBarIndexForId(tabId);
    if (index < 0)
        return;

    // "untitled:N" buffers have no file yet, so FileIcons reports the plain
    // text glyph. Once the buffer is saved the id becomes a real path and this
    // is called again, which is when the language icon appears.
    const bool isUnsavedBuffer = tabId.startsWith(QLatin1String("untitled:"));
    const QString path = isUnsavedBuffer ? QString() : tabId;

    tabBar->setTabIcon(index, ThemeIcons::instance()->statefulIcon(
                                   FileIcons::forFileName(path), kTabIconSize));
}

void MainWindow::updateAllTabIcons()
{
    // Reachable from the themeChanged handler during construction, before
    // setupUi() has handed us the tab bar.
    if (!tabBar)
        return;
    for (int i = 0; i < tabBar->count(); ++i) {
        const QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString)
            updateTabIcon(data.toString());
    }
}

void MainWindow::syncTopBarToCurrentFile()
{
    QWidget *cur = ui->tabWidget->currentWidget();
    if (!cur)
        return;
    QString id = m_tabIds.value(cur);
    if (id.isEmpty())
        return;
    int barIdx = findTabBarIndexForId(id);
    if (barIdx >= 0 && barIdx != tabBar->currentIndex()) {
        QSignalBlocker blocker(tabBar);
        tabBar->setCurrentIndex(barIdx);
    }
}

QPushButton* MainWindow::createEditorTabCloseButton(CodeEditor *editor)
{
    QPushButton *closeBtn = new QPushButton();
    ThemeIcons::instance()->setIcon(closeBtn, ":/icons/close.svg");
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    // Resolve the tab dynamically at click time: captured paths/indices go
    // stale after Save As renames the file or other tabs close.
    QPointer<CodeEditor> guard(editor);
    connect(closeBtn, &QPushButton::clicked, this, [this, guard]() {
        if (!guard)
            return;
        int widx = ui->tabWidget->indexOf(guard);
        if (widx >= 0)
            on_tabWidget_tabCloseRequested(widx);
    });
    return closeBtn;
}

void MainWindow::newUntitledFile()
{
    CodeEditor *editor = new CodeEditor(this);
    editor->installEventFilter(this);
    connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
    QFont savedFont = SettingsStore::instance().value("editor/font", editor->font()).value<QFont>();
    editor->setFont(savedFont);
    editor->setTabWidth(SettingsStore::instance().value("editor/tabWidth", editor->tabWidth()).toInt());
    editor->setPlainText(QString());

    // Dynamic index lookup — a captured index goes stale when other tabs close.
    QPointer<CodeEditor> edGuard(editor);
    connect(editor, &QPlainTextEdit::modificationChanged, this,
            [this, edGuard](bool m) {
                if (!edGuard)
                    return;
                int i = ui->tabWidget->indexOf(edGuard);
                if (i >= 0)
                    updateTabModified(i, m);
            });
    // Re-arm the idle-debounce auto-save timer on every edit.
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
        autoSaveTimer->start();
    });
    // Only one cursorPositionChanged connection: updateStatusBar handles everything
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (lspClient->isRunning())
            m_hoverTimer->start();
    });

    ++m_untitledCounter;
    QString id = QStringLiteral("untitled:%1").arg(m_untitledCounter);
    QString displayName = tr("Untitled %1").arg(m_untitledCounter);
    openFiles.append({QString(), displayName, false});
    m_tabIds[editor] = id;
    showEditorInterface();
    ui->tabWidget->addTab(editor, displayName);
    int tabBarIndex = tabBar->addTab(displayName);
    tabBar->setTabData(tabBarIndex, id);
    tabBar->setTabIcon(tabBarIndex, ThemeIcons::instance()->statefulIcon(
                                        FileIcons::textIcon(), kTabIconSize));
    tabBar->setTabToolTip(tabBarIndex, displayName);
    tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createEditorTabCloseButton(editor));
    ui->tabWidget->setCurrentWidget(editor);
    tabBar->setCurrentIndex(tabBarIndex);
    currentFile = QString();
    setWindowTitle(displayName + " - Scriptura");
    showSearchBar(true);
    editor->setFocus();
}

void MainWindow::rebuildNewTabMenu()
{
    // The "+" menu is the one place that lists every kind of tab this window
    // can open: an empty buffer, an existing file, the settings page, and every
    // registered panel. It is rebuilt only when the panel list changes, because
    // each rebuild re-rasterises every icon.
    if (!m_newTabMenu)
        return;
    m_newTabMenu->clear();
    ThemeIcons *icons = ThemeIcons::instance();

    QAction *emptyAction = m_newTabMenu->addAction(tr("New Empty File"));
    icons->setIcon(emptyAction, ":/icons/file.svg");
    connect(emptyAction, &QAction::triggered, this, &MainWindow::newUntitledFile);

    QAction *openAction = m_newTabMenu->addAction(tr("Open File..."));
    icons->setIcon(openAction, ":/icons/folder.svg");
    connect(openAction, &QAction::triggered, this, &MainWindow::on_action_open_file_triggered);

    QAction *settingsAction = m_newTabMenu->addAction(tr("Settings"));
    icons->setIcon(settingsAction, ":/icons/settings.svg");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::on_action_editor_settings_triggered);

    if (m_panels.isEmpty())
        return;

    // Every registered panel, in one list: picking one opens its tab, picking an
    // open one brings that tab back to the front.
    m_newTabMenu->addSeparator();
    for (int i = 0; i < m_panels.size(); ++i) {
        QAction *panelAction = m_newTabMenu->addAction(m_panels.at(i).title);
        const QString iconPath = m_panelIcons.value(i);
        if (!iconPath.isEmpty())
            icons->setIcon(panelAction, iconPath, ThemeIcons::Role::Normal, 16);
        panelAction->setCheckable(true);
        panelAction->setChecked(findPanelTabIndex(i) >= 0);
        connect(panelAction, &QAction::triggered, this, [this, i]() {
            openPanelAsTab(i);
        });
    }
}

void MainWindow::showNewTabPanel()
{
    // Floating panel listing every available tab type, anchored under the
    // "+" button at the end of the tab bar. Kept alive between uses: a
    // WA_DeleteOnClose menu meant re-registering and re-rasterising every icon
    // on each visit.
    if (!m_newTabMenu) {
        m_newTabMenu = new QMenu(this);
        m_newTabMenu->setObjectName("newTabPanel");
        m_newTabMenu->setStyleSheet(
            "QMenu#newTabPanel { background-color: palette(window);"
            " border: 1px solid palette(mid); border-radius: 10px; padding: 6px; }"
            "QMenu#newTabPanel::item { padding: 6px 24px 6px 12px; border-radius: 6px; }"
            "QMenu#newTabPanel::item:selected { background-color: palette(highlight);"
            " color: palette(highlighted-text); }"
            "QMenu#newTabPanel::separator { height: 1px; background: palette(mid);"
            " margin: 4px 8px; }");
        m_newTabMenuPanelCount = -1;
    }

    // Panels can be registered after the menu was first built.
    const int panelCount = m_panels.size();
    if (m_newTabMenuPanelCount != panelCount) {
        rebuildNewTabMenu();
        m_newTabMenuPanelCount = panelCount;
    }

    QPoint pos;
    if (m_newTabButton)
        pos = m_newTabButton->mapToGlobal(QPoint(0, m_newTabButton->height() + 4));
    else
        pos = tabBar->mapToGlobal(QPoint(tabBar->width(), tabBar->height()));
    m_newTabMenu->popup(pos);
}

