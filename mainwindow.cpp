#include <QShortcut>
#include <QCloseEvent>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "codeeditor.h"
#include "todopanel.h"
#include "gitpanel.h"
#include "terminalpanel.h"
#include "updater.h"
#include "configvalidator.h"

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
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QDialog>
#include <QScrollArea>
#include <QButtonGroup>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , selectedTheme(ThemeColorFamily::Default, ThemeMode::Light)
    , autoSaveTimer(new QTimer(this))
    , lspDebounceTimer(new QTimer(this))
    , lspClient(new LspClient(this))
    , problemPanel(new ProblemPanel(this))
    , todoPanel(new TodoPanel(this))
    , gitPanel(new GitPanel(this))
    , terminalPanel(new TerminalPanel(this))
    , updater(new Updater(this))
    , configValidator(new ConfigValidator(this))
{
    ui->setupUi(this);

    loadRecentProjects();

    QSettings settings;
    int legacyTheme = settings.value("theme/selected", 0).toInt();
    selectedTheme = themeFromLegacyInt(legacyTheme);
    applyTheme(selectedTheme);

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
    connect(goUpButton, &QToolButton::clicked, this, &MainWindow::goUpClicked);
    toolbar->addWidget(goUpButton);

    searchBarWidget = new QWidget(this);
    searchBarWidget->setVisible(false);
    QHBoxLayout *searchLayout = new QHBoxLayout(searchBarWidget);
    searchLayout->setContentsMargins(2, 2, 2, 2);
    searchLayout->setSpacing(4);

    searchLineEdit = new QLineEdit(searchBarWidget);
    searchLineEdit->setPlaceholderText(tr("Search in file..."));
    searchLineEdit->setClearButtonEnabled(true);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchNext);
    searchLayout->addWidget(searchLineEdit, 1);

    searchPrevBtn = new QPushButton("▲", searchBarWidget);
    searchPrevBtn->setFixedSize(24, 24);
    searchPrevBtn->setToolTip(tr("Previous match"));
    connect(searchPrevBtn, &QPushButton::clicked, this, &MainWindow::onSearchPrev);
    searchLayout->addWidget(searchPrevBtn);

    searchNextBtn = new QPushButton("▼", searchBarWidget);
    searchNextBtn->setFixedSize(24, 24);
    searchNextBtn->setToolTip(tr("Next match"));
    connect(searchNextBtn, &QPushButton::clicked, this, &MainWindow::onSearchNext);
    searchLayout->addWidget(searchNextBtn);

    searchCountLabel = new QLabel(searchBarWidget);
    searchCountLabel->setFixedWidth(48);
    searchCountLabel->setAlignment(Qt::AlignCenter);
    searchLayout->addWidget(searchCountLabel);

    toolbar->addWidget(searchBarWidget);

    ui->iconSideBar->setFixedWidth(50);
     ui->iconSideBar->setStyleSheet("QWidget { background-color: palette(button); border-right: 1px solid palette(shadow); }");

     placeholderButton = findChild<QToolButton*>("placeholderButton");
      placeholderButton->setToolTip(tr("Todo Manager"));
      placeholderButton->setCheckable(true);
      placeholderButton->setStyleSheet(
          "QToolButton { border: none; background: transparent; font-size: 20px; padding: 4px; }"
          "QToolButton:hover { background: palette(highlight); border-radius: 4px; }"
          "QToolButton:checked { background: palette(highlight); border-radius: 4px; }"
      );

     fileTreeToggleButton = findChild<QToolButton*>("fileTreeToggleButton");
    fileTreeToggleButton->setToolTip(tr("Toggle File Tree"));
    fileTreeToggleButton->setStyleSheet(
        "QToolButton { border: none; background: transparent; font-size: 20px; padding: 4px; }"
        "QToolButton:checked { background: palette(highlight); border-radius: 4px; }"
    );

    terminalButton = findChild<QToolButton*>("terminalButton");
    terminalButton->setToolTip(tr("New Terminal"));
    terminalButton->setStyleSheet(
        "QToolButton { border: none; background: transparent; font-size: 16px; font-family: monospace; padding: 4px; }"
        "QToolButton:hover { background: palette(highlight); border-radius: 4px; }"
    );

    problemsButton = new QToolButton(this);
    problemsButton->setText("⚠");
    problemsButton->setToolTip(tr("Toggle Problems"));
    problemsButton->setCheckable(true);
    problemsButton->setStyleSheet(
        "QToolButton { border: none; background: transparent; font-size: 18px; padding: 4px; }"
        "QToolButton:checked { background: palette(highlight); border-radius: 4px; }"
    );
    ui->iconSideBar->layout()->addWidget(problemsButton);

    terminalButton = new QToolButton(this);
    terminalButton->setText(">_");
    terminalButton->setToolTip(tr("Toggle Terminal"));
    terminalButton->setCheckable(true);
    terminalButton->setStyleSheet(
        "QToolButton { border: none; background: transparent; font-size: 16px; font-family: monospace; padding: 4px; }"
        "QToolButton:checked { background: palette(highlight); border-radius: 4px; }"
    );
    ui->iconSideBar->layout()->addWidget(terminalButton);

    connect(fileTreeToggleButton, &QToolButton::clicked, this, [this](bool checked) {
        Q_UNUSED(checked);
        setSidebarCollapsed(!fileTreeToggleButton->isChecked());
    });
    connect(terminalButton, &QToolButton::clicked, this, &MainWindow::toggleTerminalPanel);
    connect(problemsButton, &QToolButton::clicked, this, &MainWindow::toggleProblemPanel);
     connect(placeholderButton, &QToolButton::toggled, this, &MainWindow::on_placeholderButton_clicked);

     connect(ui->fileTreeView, &QTreeView::clicked, this, &MainWindow::on_fileTreeView_clicked);

    ui->tabWidget->clear();
    ui->tabWidget->setTabsClosable(true);
    
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::on_tabWidget_tabCloseRequested);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateStatusBar);

    // Add problem panel at the bottom
    ui->editorLayout->addWidget(problemPanel);
    problemPanel->hide();

    // Add todo panel at the bottom
    ui->editorLayout->addWidget(todoPanel);
    todoPanel->hide();

    // Add git panel at the bottom
    ui->editorLayout->addWidget(gitPanel);
    gitPanel->hide();

    // Add terminal panel at the bottom
    ui->editorLayout->addWidget(terminalPanel);
    terminalPanel->hide();

    ui->editorLayout->removeWidget(ui->tabWidget);
    editorStack = new QStackedWidget(this);
    welcomeWidget = createWelcomeWidget();
    editorStack->addWidget(welcomeWidget);
    editorStack->addWidget(ui->tabWidget);
    ui->editorLayout->addWidget(editorStack);
    showWelcomeScreen();

    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);

    // LSP debounce timer for text changes
    lspDebounceTimer->setSingleShot(true);
    lspDebounceTimer->setInterval(500); // 500ms debounce
    connect(lspDebounceTimer, &QTimer::timeout, this, &MainWindow::onEditorTextChanged);

    QShortcut *shortcutDialog = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(shortcutDialog, &QShortcut::activated, this, &MainWindow::showKeyboardShortcuts);

    // LSP connections
    connect(lspClient, &LspClient::diagnosticsReceived, this, &MainWindow::onDiagnosticsReceived);
    connect(lspClient, &LspClient::serverStarted, this, []() {
        qDebug() << "LSP server started";
    });
    connect(lspClient, &LspClient::serverFailed, this, [](const QString &err) {
        qDebug() << "LSP server failed:" << err;
    });
    connect(lspClient, &LspClient::logMessage, this, [](const QString &msg) {
        qDebug() << "LSP:" << msg;
    });

    // Problem panel connections
    connect(problemPanel, &ProblemPanel::problemActivated, this, &MainWindow::onProblemActivated);
    connect(problemPanel, &ProblemPanel::filterChanged, this, &MainWindow::onProblemsFilterChanged);

    // Updater connections
    connect(updater, &Updater::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(updater, &Updater::updateCheckFailed, this, &MainWindow::onUpdateCheckFailed);

    // Config validator - validate settings on startup
    configValidator->resetInvalidSettings();

    setSidebarCollapsed(settings.value("ui/sidebarCollapsed", true).toBool());
}

MainWindow::~MainWindow()
{
    delete ui;
}

QPlainTextEdit* MainWindow::getCurrentEditor()
{
    return qobject_cast<QPlainTextEdit*>(ui->tabWidget->currentWidget());
}

CodeEditor* MainWindow::getCurrentCodeEditor()
{
    return qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());
}

QWidget* MainWindow::createWelcomeWidget()
{
    QWidget *widget = new QWidget(this);

    QLabel *titleLabel = new QLabel(tr("Welcome to Scriptura"), widget);
    titleLabel->setObjectName("welcomeTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *descriptionLabel = new QLabel(tr("Open a project or create a new file to start editing."), widget);
    descriptionLabel->setAlignment(Qt::AlignCenter);

    QPushButton *openProjectButton = new QPushButton(tr("Open Project"), widget);
    QPushButton *newFileButton = new QPushButton(tr("New File"), widget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(openProjectButton);
    buttonLayout->addWidget(newFileButton);
    buttonLayout->addStretch();

    QFrame *recentProjectsFrame = new QFrame(widget);
    recentProjectsFrame->setFrameShape(QFrame::NoFrame);
    recentProjectsLayout = new QVBoxLayout(recentProjectsFrame);
    recentProjectsLayout->addWidget(new QLabel(tr("<b>Recent Projects:</b>"), recentProjectsFrame));

    updateRecentProjectsOnWelcome();

    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addStretch();
    layout->addLayout(buttonLayout);
    layout->addWidget(recentProjectsFrame);
    layout->addWidget(createKeyboardShortcutsWidget());
    layout->addStretch();

    connect(openProjectButton, &QPushButton::clicked, this, &MainWindow::on_action_open_project_triggered);
    connect(newFileButton, &QPushButton::clicked, this, [this]() {
        on_action_add_file_directory_triggered();
    });

    return widget;
}

QWidget* MainWindow::createKeyboardShortcutsWidget()
{
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);

    QLabel *shortcutsLabel = new QLabel(tr(
        "<b>Keyboard Shortcuts:</b><br>"
        "Ctrl+S: Save &nbsp;&nbsp; "
        "Ctrl+Shift+S: Save As &nbsp;&nbsp; "
        "Ctrl+Z: Undo &nbsp;&nbsp; "
        "Ctrl+Y: Redo &nbsp;&nbsp; "
        "Ctrl+F: Find &nbsp;&nbsp; "
        "Ctrl+K: Keyboard Shortcuts"
    ), widget);
    shortcutsLabel->setAlignment(Qt::AlignCenter);
    shortcutsLabel->setOpenExternalLinks(false);
    layout->addWidget(shortcutsLabel);

    return widget;
}

void MainWindow::showWelcomeScreen()
{
    editorStack->setCurrentWidget(welcomeWidget);
}

void MainWindow::showEditorInterface()
{
    editorStack->setCurrentWidget(ui->tabWidget);
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

    if (!recentProjects.contains(dirName)) {
        recentProjects.prepend(dirName);
        while (recentProjects.size() > maxRecentProjects)
            recentProjects.removeLast();
        saveRecentProjects();
        updateRecentProjectsOnWelcome();
    }

    projectDir = dirName;
     rootIndex = fileModel->index(projectDir);
     ui->fileTreeView->setRootIndex(rootIndex);
     ui->fileTreeView->hideColumn(1);
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
    problemPanel->clearAll();
    problemPanel->setCurrentFile(QString()); // Show all problems
    problemPanel->show();
    problemsButton->setChecked(true);
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

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    for (int i = 0; i < openFiles.size(); i++) {
        if (openFiles[i].filePath == fileName) {
            showEditorInterface();
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    CodeEditor *editor = new CodeEditor(this);
    editor->setLanguageForFile(fileName);
    QSettings settings;
    QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
    editor->setFont(savedFont);
    editor->setTabWidth(settings.value("editor/tabWidth", editor->tabWidth()).toInt());
    int w = settings.value("editor/width", 0).toInt();
    if (w > 0) editor->setMinimumWidth(w);
    editor->setPlainText(content);
    int tabIndex = openFiles.size();
    connect(editor, &QPlainTextEdit::modificationChanged, this,
            [this, tabIndex](bool m) { updateTabModified(tabIndex, m); });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);

    openFiles.append({fileName, QFileInfo(fileName).fileName(), false});
    showEditorInterface();
    ui->tabWidget->addTab(editor, QFileInfo(fileName).fileName());
    ui->tabWidget->setCurrentWidget(editor);
    currentFile = fileName;
    setWindowTitle(QFileInfo(fileName).fileName() + " - Scriptura");
    showSearchBar(true);
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

void MainWindow::on_fileTreeView_clicked(const QModelIndex &index)
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
                showEditorInterface();
                ui->tabWidget->setCurrentIndex(i);
                return;
            }
        }
        
        CodeEditor *editor = new CodeEditor(this);
        editor->setLanguageForFile(path);
        QSettings settings;
        QFont savedFont = settings.value("editor/font", editor->font()).value<QFont>();
        editor->setFont(savedFont);
        int savedTabWidth = settings.value("editor/tabWidth", editor->tabWidth()).toInt();
        editor->setTabWidth(savedTabWidth);
        int savedEditorWidth = settings.value("editor/width", 0).toInt();
        if (savedEditorWidth > 0)
            editor->setMinimumWidth(savedEditorWidth);
        editor->setPlainText(content);
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
        ui->tabWidget->setCurrentWidget(editor);

        currentFile = path;
        setWindowTitle(openFile.fileName + " - Scriptura");
        showSearchBar(true);

        // LSP: Open file in language server
        startLanguageServer(path);
        QString uri = QUrl::fromLocalFile(path).toString();
        QString langId = QFileInfo(path).suffix().toLower();
        lspClient->didOpen(uri, langId, content);
        problemPanel->setCurrentFile(uri);
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

    if (index >= 0 && index < openFiles.size()) {
        if (openFiles[index].filePath == currentFile) {
            currentFile = "";
        }
        // LSP: Close file in language server
        QString closedPath = openFiles[index].filePath;
        QString closedUri = QUrl::fromLocalFile(closedPath).toString();
        lspClient->didClose(closedUri);
        openFiles.removeAt(index);
    }

    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    delete widget;

    if (ui->tabWidget->count() > 0) {
        showEditorInterface();
        updateStatusBar();
        // Update problem panel to show current file's problems
        QString newCurrent = QUrl::fromLocalFile(currentFile).toString();
        problemPanel->setCurrentFile(newCurrent);
    } else {
        showWelcomeScreen();
        setWindowTitle(projectDir.isEmpty() ? "Scriptura" : QFileInfo(projectDir).fileName() + " - Scriptura");
        showSearchBar(false);
        problemPanel->hide();
        problemsButton->setChecked(false);
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
    // Use internal terminal if available, otherwise fall back to external
    if (terminalPanel && !terminalPanel->isRunning()) {
        terminalPanel->startShell(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        terminalPanel->show();
        terminalButton->setChecked(true);
    } else {
        QString term = findTerminal();
        QProcess::startDetached(term, QStringList());
    }
}

void MainWindow::toggleTerminalPanel()
{
    if (terminalPanel->isVisible()) {
        terminalPanel->hide();
        terminalButton->setChecked(false);
    } else {
        terminalPanel->show();
        terminalButton->setChecked(true);
        if (!terminalPanel->isRunning()) {
            terminalPanel->startShell(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        }
    }
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

    QString term = findTerminal();
    QStringList args;
#ifdef Q_OS_MAC
    args << "-a" << "Terminal" << targetDir;
#elif defined(Q_OS_WIN)
    args << "/k" << "cd" << "/d" << QDir::toNativeSeparators(targetDir);
#else
    if (term == "gnome-terminal")
        args << "--working-directory" << targetDir;
    else if (term == "konsole")
        args << "--workdir" << targetDir;
    else if (term == "xfce4-terminal")
        args << "--working-directory" << targetDir;
    else if (term == "alacritty" || term == "kitty")
        args << "--working-directory" << targetDir;
    else
        args << "--working-directory" << targetDir;
#endif
    QProcess::startDetached(term, args);
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, tr("About Scriptura"), 
        tr("Scriptura\nA simple Qt-based text editor with project file browsing.\n\n"
           "Built with C++17 and Qt Widgets."));
}

void MainWindow::on_action_editor_settings_triggered()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Editor Settings"));
    dialog->resize(400, 300);

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - dialog->width()) / 2;
        int y = (screenGeometry.height() - dialog->height()) / 2;
        dialog->move(x, y);
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());

    QHBoxLayout *fontLayout = new QHBoxLayout();
    QLabel *fontLabel = new QLabel(tr("Font:"), dialog);
    QFontComboBox *fontCombo = new QFontComboBox(dialog);
    fontCombo->setFontFilters(QFontComboBox::ScalableFonts);
    if (editor) fontCombo->setCurrentFont(editor->font());
    fontLayout->addWidget(fontLabel);
    fontLayout->addWidget(fontCombo);
    mainLayout->addLayout(fontLayout);

    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel(tr("Size:"), dialog);
    QSpinBox *sizeSpin = new QSpinBox(dialog);
    sizeSpin->setRange(8, 72);
    if (editor) sizeSpin->setValue(editor->font().pointSize());
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeSpin);
    sizeLayout->addStretch();
    mainLayout->addLayout(sizeLayout);

    QHBoxLayout *tabLayout = new QHBoxLayout();
    QLabel *tabLabel = new QLabel(tr("Tab Width:"), dialog);
    QSpinBox *tabSpin = new QSpinBox(dialog);
    tabSpin->setRange(1, 16);
    if (editor) tabSpin->setValue(editor->tabWidth());
    tabLayout->addWidget(tabLabel);
    tabLayout->addWidget(tabSpin);
    tabLayout->addStretch();
    mainLayout->addLayout(tabLayout);

    QHBoxLayout *widgetSizeLayout = new QHBoxLayout();
    QLabel *widgetSizeLabel = new QLabel(tr("Editor Width:"), dialog);
    QSpinBox *widgetSizeSpin = new QSpinBox(dialog);
    widgetSizeSpin->setRange(200, 2000);
    widgetSizeSpin->setSuffix(" px");
    if (editor) widgetSizeSpin->setValue(editor->minimumWidth());
    widgetSizeLayout->addWidget(widgetSizeLabel);
    widgetSizeLayout->addWidget(widgetSizeSpin);
    widgetSizeLayout->addStretch();
    mainLayout->addLayout(widgetSizeLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    if (dialog->exec() == QDialog::Accepted) {
        QSettings settings;

        QFont font = fontCombo->currentFont();
        font.setPointSize(sizeSpin->value());
        settings.setValue("editor/font", font);

        int tabWidth = tabSpin->value();
        settings.setValue("editor/tabWidth", tabWidth);

        int editorWidth = widgetSizeSpin->value();
        settings.setValue("editor/width", editorWidth);

        for (int i = 0; i < ui->tabWidget->count(); i++) {
            if (CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                ed->setFont(font);
                ed->setTabWidth(tabWidth);
                if (editorWidth > 0)
                    ed->setMinimumWidth(editorWidth);
            }
        }
    }
}

void MainWindow::applyTheme(const Theme &theme)
{
    QPalette palette = buildBasePalette(theme.family, theme.mode);
    bool isDark = theme.isDark();
    auto syntax = baseSyntaxColors(theme.mode);

    if (theme.features.testFlag(ThemeFeature::HighContrast)) {
        applyHighContrastPalette(palette, theme.family, theme.mode);
        applyHighContrastSyntax(syntax, theme.family, theme.mode);
    }

    QApplication::setStyle("Fusion");
    QApplication::setPalette(palette);
    QString sheet = qApp->styleSheet();
    qApp->setStyleSheet(sheet);

    for (QWidget *widget : QApplication::allWidgets()) {
        widget->setPalette(palette);
        widget->setAutoFillBackground(true);
        if (CodeEditor *editor = qobject_cast<CodeEditor*>(widget)) {
            editor->viewport()->setPalette(palette);
            editor->viewport()->setAutoFillBackground(true);
            editor->setDarkMode(isDark);
            QColor trailingBg = isDark ? QColor("#7f1d1d") : QColor("#fecaca");
            editor->setThemeColors(syntax.keyword, syntax.string, syntax.comment, syntax.number,
                                   syntax.preprocessor, syntax.tag, syntax.attribute,
                                   syntax.cssProperty, syntax.variable, syntax.function,
                                   syntax.escape, trailingBg);
        }
    }

    QSettings settings;
    settings.setValue("theme/selected", themeToLegacyInt(theme));
}

void MainWindow::on_action_theme_triggered()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Theme Selection"));
    dialog->resize(500, 350);
    dialog->setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

    struct FamilyEntry {
        ThemeColorFamily family;
        QString name;
        QColor lightBg, darkBg;
        QColor lightText, darkText;
    };

    FamilyEntry families[] = {
        {ThemeColorFamily::Default, "Default", QColor(240,240,240), QColor(45,45,45), Qt::black, Qt::white},
        {ThemeColorFamily::Blue, "Blue", QColor(240,248,255), QColor(25,35,50), Qt::black, Qt::white},
        {ThemeColorFamily::Green, "Green", QColor(240,255,240), QColor(25,45,30), Qt::black, Qt::white},
        {ThemeColorFamily::Red, "Red", QColor(255,245,245), QColor(45,25,25), Qt::black, Qt::white},
        {ThemeColorFamily::Yellow, "Yellow", QColor(255,255,240), QColor(45,45,25), Qt::black, Qt::white},
        {ThemeColorFamily::Brown, "Brown", QColor(255,250,240), QColor(40,30,20), Qt::black, Qt::white},
        {ThemeColorFamily::Cyan, "Cyan", QColor(240,255,255), QColor(25,45,45), Qt::black, Qt::white},
        {ThemeColorFamily::Violet, "Violet", QColor(245,240,255), QColor(35,25,50), Qt::black, Qt::white}
    };

    QGroupBox *familyGroup = new QGroupBox(tr("Color Family"), dialog);
    QGridLayout *familyGrid = new QGridLayout(familyGroup);
    QButtonGroup *familyBtnGroup = new QButtonGroup(familyGroup);

    int familySelected = static_cast<int>(selectedTheme.family);
    for (int i = 0; i < 8; ++i) {
        QPushButton *btn = new QPushButton(families[i].name, familyGroup);
        btn->setCheckable(true);
        btn->setProperty("familyIdx", i);
        updateFamilyButtonPreview(btn, families[i].family, selectedTheme.mode, selectedTheme.features);
        familyGrid->addWidget(btn, i / 4, i % 4);
        familyBtnGroup->addButton(btn);
        if (i == familySelected)
            btn->setChecked(true);
    }

    QGroupBox *modeGroup = new QGroupBox(tr("Mode"), dialog);
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    QRadioButton *lightRadio = new QRadioButton(tr("Light"), modeGroup);
    QRadioButton *darkRadio = new QRadioButton(tr("Dark"), modeGroup);
    QButtonGroup *modeBtnGroup = new QButtonGroup(modeGroup);
    modeBtnGroup->addButton(lightRadio);
    modeBtnGroup->addButton(darkRadio);
    modeLayout->addWidget(lightRadio);
    modeLayout->addWidget(darkRadio);
    if (selectedTheme.mode == ThemeMode::Light)
        lightRadio->setChecked(true);
    else
        darkRadio->setChecked(true);

    QGroupBox *featuresGroup = new QGroupBox(tr("Features"), dialog);
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresGroup);
    QCheckBox *hcCheckbox = new QCheckBox(tr("High Contrast"), featuresGroup);
    hcCheckbox->setChecked(selectedTheme.features.testFlag(ThemeFeature::HighContrast));
    featuresLayout->addWidget(hcCheckbox);

    mainLayout->addWidget(familyGroup);
    mainLayout->addWidget(modeGroup);
    mainLayout->addWidget(featuresGroup);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton *applyButton = new QPushButton(tr("Apply"), dialog);
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    Theme currentTheme = selectedTheme;

    auto refreshFamilyPreviews = [&]() {
        ThemeMode mode = lightRadio->isChecked() ? ThemeMode::Light : ThemeMode::Dark;
        ThemeFeatures features;
        if (hcCheckbox->isChecked())
            features |= ThemeFeature::HighContrast;
        for (int i = 0; i < 8; ++i) {
            QAbstractButton *abtn = familyBtnGroup->button(i);
            QPushButton *btn = qobject_cast<QPushButton*>(abtn);
            if (btn)
                updateFamilyButtonPreview(btn, families[i].family, mode, features);
        }
        currentTheme.mode = mode;
        currentTheme.features = features;
    };

    connect(familyBtnGroup, &QButtonGroup::buttonClicked, [&](QAbstractButton *button) {
        currentTheme.family = static_cast<ThemeColorFamily>(button->property("familyIdx").toInt());
        currentTheme.mode = lightRadio->isChecked() ? ThemeMode::Light : ThemeMode::Dark;
        currentTheme.features = hcCheckbox->isChecked() ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures();
    });

    connect(lightRadio, &QRadioButton::toggled, refreshFamilyPreviews);
    connect(hcCheckbox, &QCheckBox::toggled, refreshFamilyPreviews);

    connect(applyButton, &QPushButton::clicked, dialog, &QDialog::accept);

    int result = dialog->exec();
    if (result == QDialog::Accepted && currentTheme != selectedTheme) {
        selectedTheme = currentTheme;
        applyTheme(selectedTheme);
    }
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

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    QSettings settings;
    settings.setValue("ui/sidebarCollapsed", collapsed);

    if (collapsed) {
        ui->fileTreeGroupBox->hide();
        if (QToolBar *tb = findChild<QToolBar*>("fileToolbar"))
            tb->hide();
        if (fileTreeToggleButton)
            fileTreeToggleButton->setChecked(false);
    } else {
        ui->fileTreeGroupBox->show();
        if (QToolBar *tb = findChild<QToolBar*>("fileToolbar"))
            tb->show();
        if (fileTreeToggleButton)
            fileTreeToggleButton->setChecked(true);
    }
}

void MainWindow::showSearchBar(bool show)
{
    searchBarWidget->setVisible(show);
    if (show) {
        searchLineEdit->clear();
        searchCountLabel->clear();
    }
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor)
        editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
}

void MainWindow::performSearch(const QString &text, bool forward)
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;

    QList<QTextEdit::ExtraSelection> selections;
    if (text.isEmpty()) {
        editor->setExtraSelections(selections);
        searchCountLabel->clear();
        searchLineEdit->setFocus();
        return;
    }

    QTextDocument *doc = editor->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    QTextDocument::FindFlags flags = forward ? QTextDocument::FindFlags() : QTextDocument::FindFlags(QTextDocument::FindBackward);

    int matchIndex = 0;
    int activeIndex = 0;
    QTextCursor activeCursor;

    if (!editor->extraSelections().isEmpty()) {
        activeCursor = editor->textCursor();
        activeIndex = editor->extraSelections().first().cursor.positionInBlock();
    }

    while (!cursor.isNull()) {
        cursor = doc->find(text, cursor, flags);
        if (cursor.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        if (matchIndex == activeIndex) {
            sel.cursor = cursor;
            sel.cursor.select(QTextCursor::WordUnderCursor);
            sel.format.setBackground(QColor(255, 215, 0, 200));
            sel.format.setForeground(Qt::black);
            activeCursor = cursor;
        } else {
            sel.cursor = cursor;
            sel.cursor.select(QTextCursor::WordUnderCursor);
            sel.format.setBackground(QColor(255, 215, 0, 70));
        }
        selections.append(sel);
        matchIndex++;
    }

    if (selections.isEmpty()) {
        editor->setExtraSelections(selections);
        searchCountLabel->setText("0/0");
        searchLineEdit->setFocus();
        return;
    }

    if (activeIndex >= selections.size())
        activeIndex = 0;

    if (activeIndex < selections.size()) {
        selections[activeIndex].format.setBackground(QColor(255, 215, 0, 200));
        selections[activeIndex].format.setForeground(Qt::black);
        selections[activeIndex].cursor.clearSelection();
    }

    editor->setExtraSelections(selections);
    editor->setTextCursor(selections[activeIndex].cursor);
    editor->centerCursor();

    searchCountLabel->setText(QString("%1/%2").arg(activeIndex + 1).arg(selections.size()));
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    searchCountLabel->clear();
    performSearch(text, true);
}

void MainWindow::onSearchNext()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || editor->extraSelections().isEmpty())
        return;

    const QList<QTextEdit::ExtraSelection> &selections = editor->extraSelections();
    int current = 0;
    QTextCursor tc = editor->textCursor();
    int pos = tc.position();
    for (int i = 0; i < selections.size(); ++i) {
        if (selections[i].cursor.selectionStart() >= pos) {
            current = i;
            break;
        }
        current = (i + 1) % selections.size();
    }
    current = (current + 1) % selections.size();

    QList<QTextEdit::ExtraSelection> newSel = selections;
    newSel[current].format.setBackground(QColor(255, 215, 0, 200));
    newSel[current].format.setForeground(Qt::black);
    newSel[current].cursor.clearSelection();

    for (int i = 0; i < newSel.size(); ++i) {
        if (i != current) {
            newSel[i].format.setBackground(QColor(255, 215, 0, 70));
            newSel[i].format.setForeground(Qt::NoBrush);
        }
    }

    editor->setExtraSelections(newSel);
    editor->setTextCursor(newSel[current].cursor);
    editor->centerCursor();
    searchCountLabel->setText(QString("%1/%2").arg(current + 1).arg(selections.size()));
}

void MainWindow::onSearchPrev()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || editor->extraSelections().isEmpty())
        return;

    const QList<QTextEdit::ExtraSelection> &selections = editor->extraSelections();
    int current = 0;
    QTextCursor tc = editor->textCursor();
    int pos = tc.position();
    for (int i = 0; i < selections.size(); ++i) {
        if (selections[i].cursor.selectionStart() <= pos) {
            current = i;
        }
    }
    current = (current - 1 + selections.size()) % selections.size();

    QList<QTextEdit::ExtraSelection> newSel = selections;
    newSel[current].format.setBackground(QColor(255, 215, 0, 200));
    newSel[current].format.setForeground(Qt::black);
    newSel[current].cursor.clearSelection();

    for (int i = 0; i < newSel.size(); ++i) {
        if (i != current) {
            newSel[i].format.setBackground(QColor(255, 215, 0, 70));
            newSel[i].format.setForeground(Qt::NoBrush);
        }
    }

    editor->setExtraSelections(newSel);
    editor->setTextCursor(newSel[current].cursor);
    editor->centerCursor();
    searchCountLabel->setText(QString("%1/%2").arg(current + 1).arg(selections.size()));
}

QString MainWindow::findTerminal()
{
#ifdef Q_OS_WIN
    return "cmd.exe";
#elif defined(Q_OS_MAC)
    return "open";
#else
    QStringList candidates = {"konsole", "gnome-terminal", "xfce4-terminal",
                              "mate-terminal", "alacritty", "kitty", "xterm"};
    for (const QString &term : candidates) {
        if (!QStandardPaths::findExecutable(term).isEmpty())
            return term;
    }
    return "xterm";
#endif
}

void MainWindow::updateFamilyButtonPreview(QPushButton *btn, ThemeColorFamily family, ThemeMode mode, ThemeFeatures features)
{
    QPalette p = buildBasePalette(family, mode);
    if (features.testFlag(ThemeFeature::HighContrast))
        applyHighContrastPalette(p, family, mode);

    bool isDark = (mode == ThemeMode::Dark);
    QColor bg = p.color(QPalette::Window);
    QColor accent = highContrastAccentColor(family, mode);

    btn->setStyleSheet(QString("background-color: %1; color: %2; padding: 8px; font-weight: bold; border: 2px solid %2;")
                       .arg(bg.name()).arg(accent.name()));
}

QPalette MainWindow::buildBasePalette(ThemeColorFamily family, ThemeMode mode)
{
    QPalette p;
    bool isDark = (mode == ThemeMode::Dark);

    auto s = [&](QPalette::ColorRole role, const QColor &c) { p.setColor(role, c); };

    switch (family) {
    case ThemeColorFamily::Default:
        if (isDark) {
            s(QPalette::Window, QColor(53, 53, 53));
            s(QPalette::WindowText, Qt::white);
            s(QPalette::Base, QColor(25, 25, 25));
            s(QPalette::AlternateBase, QColor(53, 53, 53));
            s(QPalette::ToolTipBase, Qt::white);
            s(QPalette::ToolTipText, Qt::white);
            s(QPalette::Text, Qt::white);
            s(QPalette::Button, QColor(53, 53, 53));
            s(QPalette::ButtonText, Qt::white);
            s(QPalette::BrightText, Qt::red);
            s(QPalette::Link, QColor(42, 130, 218));
            s(QPalette::Highlight, QColor(42, 130, 218));
            s(QPalette::HighlightedText, Qt::white);
            s(QPalette::Light, QColor(70, 70, 70));
            s(QPalette::Mid, QColor(45, 45, 45));
            s(QPalette::Dark, QColor(35, 35, 35));
            s(QPalette::Midlight, QColor(60, 60, 60));
        } else {
            p = QApplication::style()->standardPalette();
        }
        return p;

    case ThemeColorFamily::Blue:
        if (isDark) {
            s(QPalette::Window, QColor(25, 35, 50));
            s(QPalette::Base, QColor(15, 25, 35));
            s(QPalette::AlternateBase, QColor(35, 45, 55));
            s(QPalette::Button, QColor(25, 35, 50));
            s(QPalette::Highlight, QColor(100, 150, 255));
        } else {
            s(QPalette::Window, QColor(240, 248, 255));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(230, 240, 250));
            s(QPalette::Button, QColor(240, 248, 255));
            s(QPalette::Highlight, QColor(100, 150, 255));
        }
        break;

    case ThemeColorFamily::Green:
        if (isDark) {
            s(QPalette::Window, QColor(25, 45, 30));
            s(QPalette::Base, QColor(15, 35, 20));
            s(QPalette::AlternateBase, QColor(35, 55, 40));
            s(QPalette::Button, QColor(25, 45, 30));
            s(QPalette::Highlight, QColor(100, 200, 100));
        } else {
            s(QPalette::Window, QColor(240, 255, 240));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(230, 250, 230));
            s(QPalette::Button, QColor(240, 255, 240));
            s(QPalette::Highlight, QColor(100, 200, 100));
        }
        break;

    case ThemeColorFamily::Red:
        if (isDark) {
            s(QPalette::Window, QColor(45, 25, 25));
            s(QPalette::Base, QColor(35, 15, 15));
            s(QPalette::AlternateBase, QColor(55, 35, 35));
            s(QPalette::Button, QColor(45, 25, 25));
            s(QPalette::Highlight, QColor(255, 100, 100));
        } else {
            s(QPalette::Window, QColor(255, 245, 245));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(255, 235, 235));
            s(QPalette::Button, QColor(255, 245, 245));
            s(QPalette::Highlight, QColor(255, 100, 100));
        }
        break;

    case ThemeColorFamily::Yellow:
        if (isDark) {
            s(QPalette::Window, QColor(45, 45, 25));
            s(QPalette::Base, QColor(35, 35, 15));
            s(QPalette::AlternateBase, QColor(55, 55, 35));
            s(QPalette::Button, QColor(45, 45, 25));
            s(QPalette::Highlight, QColor(200, 200, 50));
        } else {
            s(QPalette::Window, QColor(255, 255, 240));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(255, 250, 220));
            s(QPalette::Button, QColor(255, 255, 240));
            s(QPalette::Highlight, QColor(200, 200, 50));
        }
        break;

    case ThemeColorFamily::Brown:
        if (isDark) {
            s(QPalette::Window, QColor(40, 30, 20));
            s(QPalette::Base, QColor(30, 20, 10));
            s(QPalette::AlternateBase, QColor(50, 40, 30));
            s(QPalette::Button, QColor(40, 30, 20));
            s(QPalette::Highlight, QColor(180, 130, 80));
        } else {
            s(QPalette::Window, QColor(255, 250, 240));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(250, 240, 220));
            s(QPalette::Button, QColor(255, 250, 240));
            s(QPalette::Highlight, QColor(180, 130, 80));
        }
        break;

    case ThemeColorFamily::Cyan:
        if (isDark) {
            s(QPalette::Window, QColor(25, 45, 45));
            s(QPalette::Base, QColor(15, 35, 35));
            s(QPalette::AlternateBase, QColor(35, 55, 55));
            s(QPalette::Button, QColor(25, 45, 45));
            s(QPalette::Highlight, QColor(50, 180, 200));
        } else {
            s(QPalette::Window, QColor(240, 255, 255));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(220, 250, 250));
            s(QPalette::Button, QColor(240, 255, 255));
            s(QPalette::Highlight, QColor(50, 180, 200));
        }
        break;

    case ThemeColorFamily::Violet:
        if (isDark) {
            s(QPalette::Window, QColor(35, 25, 50));
            s(QPalette::Base, QColor(25, 15, 40));
            s(QPalette::AlternateBase, QColor(45, 35, 60));
            s(QPalette::Button, QColor(35, 25, 50));
            s(QPalette::Highlight, QColor(150, 100, 220));
        } else {
            s(QPalette::Window, QColor(245, 240, 255));
            s(QPalette::Base, Qt::white);
            s(QPalette::AlternateBase, QColor(235, 225, 245));
            s(QPalette::Button, QColor(245, 240, 255));
            s(QPalette::Highlight, QColor(150, 100, 220));
        }
        break;
    }

    QColor text = isDark ? Qt::white : Qt::black;
    QColor btnText = isDark ? Qt::white : Qt::black;
    QColor hlText = isDark ? Qt::white : Qt::black;

    s(QPalette::WindowText, text);
    s(QPalette::Text, text);
    s(QPalette::ButtonText, btnText);
    s(QPalette::HighlightedText, hlText);

    return p;
}

QColor MainWindow::highContrastAccentColor(ThemeColorFamily family, ThemeMode mode)
{
    bool isDark = (mode == ThemeMode::Dark);
    switch (family) {
    case ThemeColorFamily::Default:
        return isDark ? QColor("#4d94ff") : QColor("#0055dd");
    case ThemeColorFamily::Blue:
        return isDark ? QColor("#80bfff") : QColor("#0044aa");
    case ThemeColorFamily::Green:
        return isDark ? QColor("#33ff99") : QColor("#006633");
    case ThemeColorFamily::Red:
        return isDark ? QColor("#ff5555") : QColor("#cc0000");
    case ThemeColorFamily::Yellow:
        return isDark ? QColor("#ffdd00") : QColor("#997a00");
    case ThemeColorFamily::Brown:
        return isDark ? QColor("#ddaa55") : QColor("#885522");
    case ThemeColorFamily::Cyan:
        return isDark ? QColor("#00eeff") : QColor("#008899");
    case ThemeColorFamily::Violet:
        return isDark ? QColor("#bb88ff") : QColor("#6633cc");
    }
    return isDark ? QColor("#4d94ff") : QColor("#0055dd");
}

void MainWindow::applyHighContrastPalette(QPalette &p, ThemeColorFamily family, ThemeMode mode)
{
    bool isDark = (mode == ThemeMode::Dark);
    QColor accent = highContrastAccentColor(family, mode);

    if (isDark) {
        p.setColor(QPalette::Window, Qt::black);
        p.setColor(QPalette::WindowText, accent);
        p.setColor(QPalette::Base, QColor(20, 20, 20));
        p.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
        p.setColor(QPalette::Text, accent);
        p.setColor(QPalette::Button, Qt::black);
        p.setColor(QPalette::ButtonText, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::Link, accent);
    } else {
        p.setColor(QPalette::Window, Qt::white);
        p.setColor(QPalette::WindowText, accent);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        p.setColor(QPalette::Text, accent);
        p.setColor(QPalette::Button, Qt::white);
        p.setColor(QPalette::ButtonText, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Link, accent);
    }
}

MainWindow::SyntaxColors MainWindow::baseSyntaxColors(ThemeMode mode)
{
    SyntaxColors c;
    if (mode == ThemeMode::Dark) {
        c.keyword = QColor("#93c5fd");
        c.string = QColor("#86efac");
        c.comment = QColor("#94a3b8");
        c.number = QColor("#c084fc");
        c.preprocessor = QColor("#a855f7");
        c.tag = QColor("#60a5fa");
        c.attribute = QColor("#fbbf24");
        c.cssProperty = QColor("#2dd4bf");
        c.variable = QColor("#38bdf8");
        c.function = QColor("#f97316");
        c.escape = QColor("#22d3ee");
    } else {
        c.keyword = QColor("#1d4ed8");
        c.string = QColor("#15803d");
        c.comment = QColor("#64748b");
        c.number = QColor("#9333ea");
        c.preprocessor = QColor("#7e22ce");
        c.tag = QColor("#2563eb");
        c.attribute = QColor("#a16207");
        c.cssProperty = QColor("#0f766e");
        c.variable = QColor("#0369a1");
        c.function = QColor("#d97706");
        c.escape = QColor("#0e7490");
    }
    return c;
}

MainWindow::SyntaxColors MainWindow::highContrastSyntaxColor(ThemeColorFamily family, ThemeMode mode, const SyntaxColors &base)
{
    QColor accent = highContrastAccentColor(family, mode);
    bool isDark = (mode == ThemeMode::Dark);

    // Blend accent with base colors to create vibrant, family-tinted syntax colors
    // while maintaining strong contrast against the high contrast background
    auto blend = [&](const QColor &baseColor, float accentWeight) -> QColor {
        if (isDark) {
            // In dark mode, boost saturation and lightness for visibility on near-black
            int r = qMin(255, baseColor.red() + static_cast<int>(accent.red() * accentWeight));
            int g = qMin(255, baseColor.green() + static_cast<int>(accent.green() * accentWeight));
            int b = qMin(255, baseColor.blue() + static_cast<int>(accent.blue() * accentWeight));
            return QColor(r, g, b);
        } else {
            // In light mode, deepen colors for contrast against white
            int r = qMax(0, baseColor.red() - static_cast<int>((255 - accent.red()) * accentWeight * 0.3f));
            int g = qMax(0, baseColor.green() - static_cast<int>((255 - accent.green()) * accentWeight * 0.3f));
            int b = qMax(0, baseColor.blue() - static_cast<int>((255 - accent.blue()) * accentWeight * 0.3f));
            return QColor(r, g, b);
        }
    };

    SyntaxColors result;
    result.keyword = blend(base.keyword, 0.4f);
    result.string = blend(base.string, 0.35f);
    result.comment = isDark ? QColor("#b0b0b0") : QColor("#555555");
    result.number = blend(base.number, 0.45f);
    result.preprocessor = blend(base.preprocessor, 0.5f);
    result.tag = blend(base.tag, 0.4f);
    result.attribute = blend(base.attribute, 0.45f);
    result.cssProperty = blend(base.cssProperty, 0.35f);
    result.variable = blend(base.variable, 0.4f);
    result.function = blend(base.function, 0.5f);
    result.escape = blend(base.escape, 0.4f);
    return result;
}

void MainWindow::applyHighContrastSyntax(SyntaxColors &c, ThemeColorFamily family, ThemeMode mode)
{
    SyntaxColors base = baseSyntaxColors(mode);
    c = highContrastSyntaxColor(family, mode, base);
}

void MainWindow::updateStatusBar()
{
    QPlainTextEdit *editor = getCurrentEditor();
    if (editor) {
        int line = editor->textCursor().blockNumber() + 1;
        int column = editor->textCursor().positionInBlock() + 1;
        ui->statusbar->showMessage(QString("Line %1, Column %2").arg(line).arg(column));
    }
}

void MainWindow::updateTabModified(int index, bool modified)
{
    if (index < 0 || index >= openFiles.size())
        return;
    openFiles[index].modified = modified;
    QString title = openFiles[index].fileName;
    if (modified)
        title = "*" + title;
    ui->tabWidget->setTabText(index, title);
}

void MainWindow::loadRecentProjects()
{
    QSettings settings;
    recentProjects = settings.value("recentProjects").toStringList();
}

void MainWindow::saveRecentProjects()
{
    QSettings settings;
    settings.setValue("recentProjects", recentProjects);
}

void MainWindow::updateRecentProjectsOnWelcome()
{
    if (!recentProjectsLayout) return;

    while (recentProjectsLayout->count() > 1) {
        QLayoutItem *item = recentProjectsLayout->takeAt(recentProjectsLayout->count() - 1);
        delete item->widget();
        delete item;
    }

    for (const QString &project : recentProjects) {
        QPushButton *btn = new QPushButton(QFileInfo(project).fileName(), this);
        btn->setToolTip(project);
        btn->setProperty("projectPath", project);
        connect(btn, &QPushButton::clicked, this, [this, project]() {
            QDir dir(project);
            if (dir.exists()) {
                projectDir = project;
                rootIndex = fileModel->index(project);
                ui->fileTreeView->setRootIndex(rootIndex);
                ui->fileTreeView->hideColumn(1);
                ui->fileTreeView->hideColumn(2);
                ui->fileTreeView->hideColumn(3);
                goUpButton->setEnabled(rootIndex.parent().isValid());
                setWindowTitle(QFileInfo(project).fileName() + " - Scriptura");
            }
        });
        recentProjectsLayout->addWidget(btn);
    }
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (checkUnsavedChanges()) {
        autoSaveTimer->stop();
        event->accept();
    } else {
        event->ignore();
    }
}


void MainWindow::showKeyboardShortcuts()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Keyboard Shortcuts"));
    dialog->resize(400, 300);
    dialog->setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QLabel *shortcutsLabel = new QLabel(tr(
        "<b>File Operations:</b><br>"
        "Ctrl+N: New File &nbsp;&nbsp; "
        "Ctrl+S: Save &nbsp;&nbsp; "
        "Ctrl+Shift+S: Save As &nbsp;&nbsp; "
        "Ctrl+O: Open File<br><br>"
        "<b>Edit Operations:</b><br>"
        "Ctrl+Z: Undo &nbsp;&nbsp; "
        "Ctrl+Y: Redo &nbsp;&nbsp; "
        "Ctrl+X: Cut &nbsp;&nbsp; "
        "Ctrl+C: Copy &nbsp;&nbsp; "
        "Ctrl+V: Paste<br><br>"
        "<b>View/Navigation:</b><br>"
        "Ctrl+F: Find &nbsp;&nbsp; "
        "F3: Find Next &nbsp;&nbsp; "
        "Shift+F3: Find Previous &nbsp;&nbsp; "
        "Ctrl+K: Keyboard Shortcuts<br><br>"
        "<b>Project:</b><br>"
        "Ctrl+E: Project Settings &nbsp;&nbsp; "
        "Ctrl+T: Theme Selection"
    ), dialog);
    shortcutsLabel->setTextFormat(Qt::RichText);
    shortcutsLabel->setAlignment(Qt::AlignLeft);
    layout->addWidget(shortcutsLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);

    dialog->exec();
}

Theme MainWindow::themeFromLegacyInt(int legacy) const
{
    if (legacy < 2) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy));
    }
    if (legacy >= 30) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy - 30),
                     ThemeFeatures(ThemeFeature::HighContrast));
    }
    int familyIndex = (legacy - 2) / 4 + 1;
    int remainder = (legacy - 2) % 4;
    ThemeMode mode = static_cast<ThemeMode>(remainder / 2);
    bool hc = (remainder % 2) != 0;
    return Theme(static_cast<ThemeColorFamily>(familyIndex), mode,
                 hc ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures());
}

int MainWindow::themeToLegacyInt(const Theme &theme) const
{
    int family = static_cast<int>(theme.family);
    int mode = static_cast<int>(theme.mode);
    bool hc = theme.features.testFlag(ThemeFeature::HighContrast);

    if (family == 0) {
        return hc ? 30 + mode : mode;
    }
    return 2 + (family - 1) * 4 + mode * 2 + (hc ? 1 : 0);
}

void MainWindow::startLanguageServer(const QString &filePath)
{
    Q_UNUSED(filePath)

    // Language server configuration based on file extension
    static QMap<QString, QPair<QString, QStringList>> serverConfigs = {
        {"cpp",  {"/usr/bin/clangd", QStringList()}},
        {"c",    {"/usr/bin/clangd", QStringList()}},
        {"py",   {"/usr/bin/pyright-langserver", QStringList("--stdio")}},
        {"js",   {"/usr/bin/typescript-language-server", QStringList("--stdio")}},
        {"ts",   {"/usr/bin/typescript-language-server", QStringList("--stdio")}},
        {"java", {"/usr/bin/jdtls", QStringList()}},
        {"rs",   {"/usr/bin/rust-analyzer", QStringList()}},
        {"go",   {"/usr/bin/gopls", QStringList()}},
    };

    QString ext = QFileInfo(filePath).suffix().toLower();
    auto it = serverConfigs.find(ext);
    if (it == serverConfigs.end())
        return;

    QString command = it.value().first;
    QStringList args = it.value().second;

    if (!lspClient->isRunning()) {
        QString rootUri = QUrl::fromLocalFile(projectDir.isEmpty() ? QDir::homePath() : projectDir).toString();
        if (lspClient->startServer(command, args, rootUri)) {
            // Wait for server to be ready, then initialize
            QTimer::singleShot(500, this, [this, filePath]() {
                QString uri = QUrl::fromLocalFile(filePath).toString();
                QString langId = QFileInfo(filePath).suffix().toLower();
                lspClient->initialize(QUrl::fromLocalFile(projectDir.isEmpty() ? QDir::homePath() : projectDir).toString(), langId);
            });
        }
    }
}

void MainWindow::startLanguageServerForProject(const QString &projectPath)
{
    // Determine the best language server for the project based on file types found
    QDir dir(projectPath);
    QStringList filters = {"*.cpp", "*.c", "*.h", "*.hpp", "*.py", "*.js", "*.ts", "*.java", "*.rs", "*.go"};
    QSet<QString> foundExtensions;

    // Scan project directory for source files (limit to root and one level deep)
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setNameFilters(filters);
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &fi : files) {
        foundExtensions.insert(fi.suffix().toLower());
    }

    // Also check subdirectories (one level deep)
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &subdir : subdirs) {
        QDir sub(subdir.absoluteFilePath());
        sub.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        sub.setNameFilters(filters);
        QFileInfoList subFiles = sub.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : subFiles) {
            foundExtensions.insert(fi.suffix().toLower());
        }
    }

    // Pick the most appropriate language server
    QString serverCommand;
    QStringList serverArgs;

    if (foundExtensions.contains("cpp") || foundExtensions.contains("c") || foundExtensions.contains("h")) {
        serverCommand = "/usr/bin/clangd";
    } else if (foundExtensions.contains("py")) {
        serverCommand = "/usr/bin/pyright-langserver";
        serverArgs = QStringList("--stdio");
    } else if (foundExtensions.contains("js") || foundExtensions.contains("ts")) {
        serverCommand = "/usr/bin/typescript-language-server";
        serverArgs = QStringList("--stdio");
    } else if (foundExtensions.contains("java")) {
        serverCommand = "/usr/bin/jdtls";
    } else if (foundExtensions.contains("rs")) {
        serverCommand = "/usr/bin/rust-analyzer";
    } else if (foundExtensions.contains("go")) {
        serverCommand = "/usr/bin/gopls";
    } else {
        // No recognized source files, don't start a server
        return;
    }

    QString rootUri = QUrl::fromLocalFile(projectPath).toString();
    if (lspClient->startServer(serverCommand, serverArgs, rootUri)) {
        QTimer::singleShot(500, this, [this, projectPath, rootUri, filters]() {
            // Determine language ID from first found file
            QString langId = "plaintext";
            QDir dir(projectPath);
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            dir.setNameFilters(filters);
            QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
            if (!files.isEmpty()) {
                langId = files.first().suffix().toLower();
            }
            lspClient->initialize(rootUri, langId);

            // Open only a limited number of source files for diagnostics
            // to avoid overwhelming the language server
            const int maxFilesToOpen = 50;
            int openedCount = 0;
            for (const QFileInfo &fi : files) {
                if (openedCount >= maxFilesToOpen)
                    break;
                QFile file(fi.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    QString content = in.readAll();
                    file.close();
                    QString uri = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
                    lspClient->didOpen(uri, fi.suffix().toLower(), content);
                    openedCount++;
                }
            }
        });
    }
}

void MainWindow::stopLanguageServer()
{
    if (lspClient->isRunning()) {
        lspClient->shutdown();
        lspClient->exit();
    }
}

void MainWindow::onDiagnosticsReceived(const QString &uri, const QList<LspClient::Diagnostic> &diagnostics)
{
    // Update problem panel
    problemPanel->setProblems(uri, diagnostics);

    // Update squiggly underlines in editor
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!editor)
            continue;

        QString tabUri = QUrl::fromLocalFile(openFiles[i].filePath).toString();
        if (tabUri == uri) {
            QList<QTextEdit::ExtraSelection> extraSelections;

            for (const LspClient::Diagnostic &diag : diagnostics) {
                QList<QTextEdit::ExtraSelection> diagSelections =
                    lspClient->createDiagnosticSelections(diag, editor->document());
                extraSelections.append(diagSelections);
            }

            // Merge with existing selections (like current line highlight)
            QList<QTextEdit::ExtraSelection> existing = editor->extraSelections();
            // Keep only the current line highlight (first selection)
            if (!existing.isEmpty() && existing.first().format.hasProperty(QTextFormat::FullWidthSelection)) {
                extraSelections.prepend(existing.first());
            }
            editor->setExtraSelections(extraSelections);
            break;
        }
    }
}

void MainWindow::onProblemActivated(const QString &fileUri, int line, int column)
{
    // Find the file in open tabs or open it
    QString localPath = QUrl(fileUri).toLocalFile();
    for (int i = 0; i < openFiles.size(); ++i) {
        if (openFiles[i].filePath == localPath) {
            ui->tabWidget->setCurrentIndex(i);
            CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (editor) {
                QTextCursor cursor = editor->textCursor();
                QTextBlock block = editor->document()->findBlockByNumber(line);
                if (block.isValid()) {
                    cursor.setPosition(block.position() + column);
                    editor->setTextCursor(cursor);
                    editor->setFocus();
                }
            }
            return;
        }
    }

    // File not open, open it
    QModelIndex index = fileModel->index(localPath);
    if (index.isValid()) {
        on_fileTreeView_clicked(index);
        // After opening, navigate to the line
        QTimer::singleShot(100, this, [this, line, column]() {
            CodeEditor *editor = getCurrentCodeEditor();
            if (editor) {
                QTextCursor cursor = editor->textCursor();
                QTextBlock block = editor->document()->findBlockByNumber(line);
                if (block.isValid()) {
                    cursor.setPosition(block.position() + column);
                    editor->setTextCursor(cursor);
                    editor->setFocus();
                }
            }
        });
    }
}

void MainWindow::onProblemsFilterChanged(ProblemPanel::Filter filter)
{
    Q_UNUSED(filter)
    // Could update status bar or other UI elements based on filter
}

void MainWindow::toggleProblemPanel()
{
    if (problemPanel->isVisible()) {
        problemPanel->hide();
        problemsButton->setChecked(false);
    } else {
        problemPanel->show();
        problemsButton->setChecked(true);
        // Update with current file's problems
        if (!currentFile.isEmpty()) {
            QString uri = QUrl::fromLocalFile(currentFile).toString();
            problemPanel->setCurrentFile(uri);
        }
    }
}

void MainWindow::on_action_git_commit_triggered()
{
    // Check if git is available
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }

    bool ok;
    QString message = QInputDialog::getText(this, tr("Git Commit"), tr("Commit message:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !message.isEmpty()) {
        QProcess gitProcess(this);
        gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
        gitProcess.start("git", {"commit", "-m", message});
        if (gitProcess.waitForFinished(10000)) {
            QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
            QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
            gitPanel->setOutput(output + error);
        } else {
            gitPanel->setOutput(tr("Failed to run git commit. The operation may have timed out."));
        }
        gitPanel->show();
    }
}

void MainWindow::on_action_git_push_triggered()
{
    // Check if git is available
    if (QStandardPaths::findExecutable("git").isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Git is not installed or not in PATH."));
        return;
    }

    QProcess gitProcess(this);
    gitProcess.setWorkingDirectory(projectDir.isEmpty() ? QDir::currentPath() : projectDir);
    gitProcess.start("git", {"push"});
    if (gitProcess.waitForFinished(30000)) {
        QString output = QString::fromLocal8Bit(gitProcess.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(gitProcess.readAllStandardError());
        gitPanel->setOutput(output + error);
    } else {
        gitPanel->setOutput(tr("Failed to run git push. The operation may have timed out."));
    }
    gitPanel->show();
}

void MainWindow::on_placeholderButton_clicked(bool checked)
{
    if (checked) {
        todoPanel->show();
    } else {
        todoPanel->hide();
    }
}

void MainWindow::onEditorTextChanged()
{
    // Debounced: send text change to LSP server
    if (currentFile.isEmpty() || !lspClient->isRunning())
        return;

    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;

    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->didChange(uri, editor->toPlainText());
}

void MainWindow::onUpdateAvailable(const QString &version, const QString &downloadUrl)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Update Available"),
        tr("Version %1 is available. Would you like to download it?\n\n%2")
            .arg(version).arg(downloadUrl),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(downloadUrl));
    }
}

void MainWindow::onUpdateCheckFailed(const QString &error)
{
    qDebug() << "Update check failed:" << error;
    // Silent fail - don't show error to user for background update check
}
