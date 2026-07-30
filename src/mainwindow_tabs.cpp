#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "minimap.h"
#include "breadcrumb.h"
#include "rust_adapter.h"
#include "themeicons.h"
#include "foldmanager.h"
#include "bookmarkmanager.h"
#include "snippetmanager.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QMenu>
#include <QInputDialog>
#include <QStorageInfo>
#include <QUrl>
#include <QSignalBlocker>
#include <QStringConverter>
#include "encodingmanager.h"
#include "applicationdock.h"
#include "pluginmarketplace.h"
#include "themarketplace.h"

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
    
    // Update the universal search file model to the current project
    if (m_universalSearch) {
        m_universalSearch->setFileModel(fileModel, projectDir);
    }
    ui->fileTreeView->hideColumn(2);
    ui->fileTreeView->hideColumn(3);
    // Disable goUpButton to restrict access to other directories when project is opened
    goUpButton->setEnabled(false);

    autoSaveTimer->start(30000);

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
        
    if (currentFile.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), 
            projectDir.isEmpty() ? QString() : projectDir,
            tr("C/C++ Files (*.c *.cpp *.h *.hpp *.hxx);;Python Files (*.py);;JavaScript Files (*.js *.ts);;HTML Files (*.html);;CSS Files (*.css);;Markdown Files (*.md);;JSON Files (*.json);;XML Files (*.xml);;All Files (*)"));
        if (fileName.isEmpty())
            return;
        currentFile = fileName;
    }

    QFile file(currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString errorMsg;
        QString errorStr = file.errorString();
        if (errorStr.contains("Permission", Qt::CaseInsensitive)) {
            errorMsg = tr("Permission denied. Please check file permissions.");
        } else if (errorStr.contains("disk", Qt::CaseInsensitive) || errorStr.contains("space", Qt::CaseInsensitive)) {
            errorMsg = tr("Disk full. Cannot save file.");
        } else if (currentFile.contains("://")) {
            errorMsg = tr("Network path unavailable. Please check connection.");
        } else {
            errorMsg = tr("Cannot open file for writing: %1").arg(errorStr);
        }
        QMessageBox::warning(this, tr("Error"), errorMsg);
        return;
    }

    // Check disk space before writing
    QStorageInfo storage(QFileInfo(currentFile).absolutePath());
    qint64 contentSize = editor->toPlainText().toUtf8().size();
    if (storage.bytesAvailable() < contentSize * 2) {
        QMessageBox::warning(this, tr("Warning"), 
            tr("Low disk space. Available: %1 MB").arg(storage.bytesAvailable() / (1024 * 1024)));
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    QString content = editor->toPlainText();
    // Preserve original line endings
    QString lineEnding = m_fileLineEndings.value(currentFile, "LF");
    if (lineEnding == "CRLF") {
        content.replace("\n", "\r\n");
    } else if (lineEnding == "CR") {
        content.replace("\n", "\r");
    }
    out << content;
    file.close();
    
    for (OpenFile &f : openFiles) {
        if (f.filePath == currentFile) {
            f.modified = false;
            break;
        }
    }
    
    setWindowTitle(QFileInfo(currentFile).fileName() + " - Scriptura");
}

void MainWindow::on_action_save_as_triggered()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (!editor)
        return;
        
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
    
    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].filePath == currentFile) {
            openFiles[i].filePath = fileName;
            openFiles[i].fileName = QFileInfo(fileName).fileName();
            openFiles[i].modified = false;
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
    QSettings settings;
    QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
    editor->setFont(savedFont);
    editor->setTabWidth(settings.value("editor/tabWidth", editor->tabWidth()).toInt());
    int w = settings.value("editor/width", 0).toInt();
    if (w > 0) editor->setMinimumWidth(w);
    editor->setPlainText(content);

    // Create minimap for this editor
    Minimap *minimap = new Minimap(editor, this);
    minimap->setDocument(editor->document());
    connect(minimap, &Minimap::viewportRequested, editor, [editor](int position) {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, position);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    });

    // Create breadcrumb for this editor
    Breadcrumb *breadcrumb = new Breadcrumb(editor, this);
    breadcrumb->setFilePath(fileName);
    connect(breadcrumb, &Breadcrumb::breadcrumbClicked, this, [this](const QString &path) {
        // Handle breadcrumb navigation
    });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, breadcrumb, &Breadcrumb::updateFromCursor);

    int tabIndex = openFiles.size();
    connect(editor, &QPlainTextEdit::modificationChanged, this,
            [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
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
    tabBar->setTabToolTip(tabBarIndex, fileName);
    tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createTabCloseButton(fileName));
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
        connect(editor, &CodeEditor::breakpointToggled, this, &MainWindow::onBreakpointToggled);
        QSettings settings;
        QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
        editor->setFont(savedFont);
        int savedTabWidth = settings.value("editor/tabWidth", editor->tabWidth()).toInt();
        editor->setTabWidth(savedTabWidth);
        int savedEditorWidth = settings.value("editor/width", 0).toInt();
        if (savedEditorWidth > 0)
            editor->setMinimumWidth(savedEditorWidth);
        editor->setPlainText(content);

        // Create minimap for this editor
        Minimap *minimap = new Minimap(editor, this);
        minimap->setDocument(editor->document());
        connect(minimap, &Minimap::viewportRequested, editor, [editor](int position) {
            QTextCursor cursor(editor->document());
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, position);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        });

        // Create breadcrumb for this editor
        Breadcrumb *breadcrumb = new Breadcrumb(editor, this);
        breadcrumb->setFilePath(path);
        connect(breadcrumb, &Breadcrumb::breadcrumbClicked, this, [this](const QString &path) {
            // Handle breadcrumb navigation
        });
        connect(editor, &QPlainTextEdit::cursorPositionChanged, breadcrumb, &Breadcrumb::updateFromCursor);

        int tabIndex = openFiles.size();
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateCursorPosition);
        connect(editor, &QPlainTextEdit::modificationChanged, this,
                [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
        connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
            lspDebounceTimer->start();
        });

        OpenFile openFile;
        openFile.filePath = path;
        openFile.fileName = fileInfo.fileName();
        openFiles.append(openFile);

        showEditorInterface();
        ui->tabWidget->addTab(editor, openFile.fileName);
        int tabBarIndex = tabBar->addTab(openFile.fileName);
        tabBar->setTabData(tabBarIndex, path);
        tabBar->setTabButton(tabBarIndex, QTabBar::RightSide, createTabCloseButton(path));
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
    m_fileEncodings.remove(closedPath);
    m_fileLineEndings.remove(closedPath);
    // LSP: Close file in language server
    QString closedUri = QUrl::fromLocalFile(closedPath).toString();
    lspClient->didClose(closedUri);
    openFiles.removeAt(index);

    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    
    // Find and remove the corresponding tabBar tab by file path
    for (int i = 0; i < tabBar->count(); ++i) {
        QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString && data.toString() == closedPath) {
            tabBar->removeTab(i);
            break;
        }
    }
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

    QMenu menu(this);    QAction *newFileAction = menu.addAction(tr("New File..."));
    ThemeIcons::instance()->setIcon(newFileAction, ":/icons/file.svg");

    QAction *newFolderAction = menu.addAction(tr("New Folder..."));
    ThemeIcons::instance()->setIcon(newFolderAction, ":/icons/folder.svg");

    menu.addSeparator();

    QAction *renameAction = menu.addAction(tr("Rename..."));
    QAction *deleteAction = menu.addAction(tr("Delete"));
    ThemeIcons::instance()->setIcon(deleteAction, ":/icons/close.svg");

    // Disable rename/delete if nothing selected
    if (!index.isValid()) {
        renameAction->setEnabled(false);
        deleteAction->setEnabled(false);
    }

    QAction *selected = menu.exec(ui->fileTreeView->mapToGlobal(pos));
    if (!selected)
        return;

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

void MainWindow::goUpClicked()
{
    if (!rootIndex.isValid())
        return;
    
    // When a project is opened, restrict navigation to project directory only
    if (!projectDir.isEmpty()) {
        // Already at project root, do not go up
        return;
    }
    
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
    QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
}

void MainWindow::on_action_clone_window_triggered()
{
    QStringList args;
    if (!projectDir.isEmpty())
        args << "--project" << projectDir;
    for (const OpenFile &f : openFiles)
        args << f.filePath;
    QProcess::startDetached(QApplication::applicationFilePath(), args);
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
        // File tab - find by file path
        QString filePath = data.toString();
        for (int i = 0; i < openFiles.size(); ++i) {
            if (openFiles[i].filePath == filePath) {
                ui->tabWidget->setCurrentIndex(i);
                editorStack->setCurrentWidget(ui->tabWidget);
                break;
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
        // If no tabs remain, show editor interface
        if (tabBar->count() == 0) {
            showEditorInterface();
        }
        updateTabBarVisibility();
    });
    return closeBtn;
}

void MainWindow::onBottomTabChanged(int index)
{
    // This slot is now called from showBottomPanelIndex() after state is updated.
    // The stack index and panel visibility are already handled there.
    // This slot is kept for external notification purposes (e.g., plugin API).
    Q_UNUSED(index);
}

