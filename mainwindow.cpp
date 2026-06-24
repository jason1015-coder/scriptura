#include <QShortcut>
#include <QCloseEvent>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , darkMode(false)
    , selectedTheme(ThemeDefaultLight)
    , autoSaveTimer(new QTimer(this))
{
    ui->setupUi(this);

    loadRecentProjects();

    QSettings settings;
    int themeIndex = settings.value("theme/selected", ThemeDefaultLight).toInt();
    selectedTheme = static_cast<ThemeType>(themeIndex);
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

    connect(fileTreeToggleButton, &QToolButton::clicked, this, [this](bool checked) {
        Q_UNUSED(checked);
        setSidebarCollapsed(!fileTreeToggleButton->isChecked());
    });
    connect(terminalButton, &QToolButton::clicked, this, &MainWindow::on_action_new_window_triggered);
    
    connect(ui->fileTreeView, &QTreeView::clicked, this, &MainWindow::on_fileTreeView_clicked);

    ui->tabWidget->clear();
    ui->tabWidget->setTabsClosable(true);
    
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::on_tabWidget_tabCloseRequested);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::updateStatusBar);

    ui->editorLayout->removeWidget(ui->tabWidget);
    editorStack = new QStackedWidget(this);
    welcomeWidget = createWelcomeWidget();
    editorStack->addWidget(welcomeWidget);
    editorStack->addWidget(ui->tabWidget);
    ui->editorLayout->addWidget(editorStack);
    showWelcomeScreen();

    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);

    QShortcut *shortcutDialog = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(shortcutDialog, &QShortcut::activated, this, &MainWindow::showKeyboardShortcuts);

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
    goUpButton->setEnabled(rootIndex.parent().isValid());

    autoSaveTimer->start(30000);

    setWindowTitle(QFileInfo(projectDir).fileName() + " - Scriptura");
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
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot open file: %1").arg(file.errorString()));
        return;
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
        openFiles.removeAt(index);
    }
    
    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    delete widget;
    
    if (ui->tabWidget->count() > 0) {
        showEditorInterface();
        updateStatusBar();
    } else {
        showWelcomeScreen();
        setWindowTitle(projectDir.isEmpty() ? "Scriptura" : QFileInfo(projectDir).fileName() + " - Scriptura");
        showSearchBar(false);
    }
}

void MainWindow::goUpClicked()
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
    QString term = findTerminal();
    QProcess::startDetached(term, QStringList());
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

void MainWindow::applyTheme(ThemeType theme)
{
    QPalette palette;
    bool isDark = false;
    QColor keyword, string, comment, number, preprocessor, tag, attribute, cssProperty, variable, function, escape;

    switch (theme) {
    case ThemeBlueLight:
        palette.setColor(QPalette::Window, QColor(240, 248, 255));
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(230, 240, 250));
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QColor(240, 248, 255));
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Highlight, QColor(100, 150, 255));
        palette.setColor(QPalette::HighlightedText, Qt::black);
        isDark = false;
        keyword = QColor("#1d4ed8");
        string = QColor("#15803d");
        comment = QColor("#64748b");
        number = QColor("#9333ea");
        preprocessor = QColor("#7e22ce");
        tag = QColor("#2563eb");
        attribute = QColor("#a16207");
        cssProperty = QColor("#0f766e");
        variable = QColor("#0369a1");
        function = QColor("#d97706");
        escape = QColor("#0e7490");
        break;

    case ThemeBlueDark:
        palette.setColor(QPalette::Window, QColor(25, 35, 50));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(15, 25, 35));
        palette.setColor(QPalette::AlternateBase, QColor(35, 45, 55));
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(25, 35, 50));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::Highlight, QColor(100, 150, 255));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        isDark = true;
        keyword = QColor("#93c5fd");
        string = QColor("#86efac");
        comment = QColor("#94a3b8");
        number = QColor("#c084fc");
        preprocessor = QColor("#a855f7");
        tag = QColor("#60a5fa");
        attribute = QColor("#fbbf24");
        cssProperty = QColor("#2dd4bf");
        variable = QColor("#38bdf8");
        function = QColor("#f97316");
        escape = QColor("#22d3ee");
        break;

    case ThemeGreenLight:
        palette.setColor(QPalette::Window, QColor(240, 255, 240));
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(230, 250, 230));
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QColor(240, 255, 240));
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Highlight, QColor(100, 200, 100));
        palette.setColor(QPalette::HighlightedText, Qt::black);
        isDark = false;
        keyword = QColor("#16a34a");
        string = QColor("#15803d");
        comment = QColor("#64748b");
        number = QColor("#9333ea");
        preprocessor = QColor("#7e22ce");
        tag = QColor("#2563eb");
        attribute = QColor("#a16207");
        cssProperty = QColor("#0f766e");
        variable = QColor("#0369a1");
        function = QColor("#d97706");
        escape = QColor("#0e7490");
        break;

    case ThemeGreenDark:
        palette.setColor(QPalette::Window, QColor(25, 45, 30));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(15, 35, 20));
        palette.setColor(QPalette::AlternateBase, QColor(35, 55, 40));
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(25, 45, 30));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::Highlight, QColor(100, 200, 100));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        isDark = true;
        keyword = QColor("#86efac");
        string = QColor("#4ade80");
        comment = QColor("#94a3b8");
        number = QColor("#c084fc");
        preprocessor = QColor("#a855f7");
        tag = QColor("#60a5fa");
        attribute = QColor("#fbbf24");
        cssProperty = QColor("#2dd4bf");
        variable = QColor("#38bdf8");
        function = QColor("#f97316");
        escape = QColor("#22d3ee");
        break;

    case ThemeHighContrastLight:
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, Qt::white);
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Highlight, Qt::black);
        palette.setColor(QPalette::HighlightedText, Qt::white);
        isDark = false;
        keyword = QColor("#0000cd");
        string = QColor("#006400");
        comment = QColor("#696969");
        number = QColor("#800080");
        preprocessor = QColor("#800080");
        tag = QColor("#0000cd");
        attribute = QColor("#8b4513");
        cssProperty = QColor("#006400");
        variable = QColor("#0000cd");
        function = QColor("#ff8c00");
        escape = QColor("#006400");
        break;

    case ThemeHighContrastDark:
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(20, 20, 20));
        palette.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, Qt::black);
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::Highlight, Qt::white);
        palette.setColor(QPalette::HighlightedText, Qt::black);
        isDark = true;
        keyword = QColor("#6495ed");
        string = QColor("#90ee90");
        comment = QColor("#d3d3d3");
        number = QColor("#dda0dd");
        preprocessor = QColor("#dda0dd");
        tag = QColor("#6495ed");
        attribute = QColor("#daa520");
        cssProperty = QColor("#90ee90");
        variable = QColor("#6495ed");
        function = QColor("#ffa500");
        escape = QColor("#90ee90");
        break;

    case ThemeDefaultLight:
        isDark = false;
        keyword = QColor("#1d4ed8");
        string = QColor("#15803d");
        comment = QColor("#64748b");
        number = QColor("#9333ea");
        preprocessor = QColor("#7e22ce");
        tag = QColor("#2563eb");
        attribute = QColor("#a16207");
        cssProperty = QColor("#0f766e");
        variable = QColor("#0369a1");
        function = QColor("#d97706");
        escape = QColor("#0e7490");
        palette = QApplication::style()->standardPalette();
        break;

    case ThemeDefaultDark:
    default:
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
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Light, QColor(70, 70, 70));
        palette.setColor(QPalette::Mid, QColor(45, 45, 45));
        palette.setColor(QPalette::Dark, QColor(35, 35, 35));
        palette.setColor(QPalette::Midlight, QColor(60, 60, 60));
        isDark = true;
        keyword = QColor("#93c5fd");
        string = QColor("#86efac");
        comment = QColor("#94a3b8");
        number = QColor("#c084fc");
        preprocessor = QColor("#a855f7");
        tag = QColor("#60a5fa");
        attribute = QColor("#fbbf24");
        cssProperty = QColor("#2dd4bf");
        variable = QColor("#38bdf8");
        function = QColor("#f97316");
        escape = QColor("#22d3ee");
        break;
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
            editor->setThemeColors(keyword, string, comment, number, preprocessor, tag,
                                   attribute, cssProperty, variable, function, escape, trailingBg);
        }
    }

    QSettings settings;
    settings.setValue("theme/selected", static_cast<int>(theme));
}

void MainWindow::on_action_theme_triggered()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Theme Selection"));
    dialog->resize(400, 300);
    dialog->setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

    QScrollArea *scrollArea = new QScrollArea(dialog);
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);

    QButtonGroup *themeGroup = new QButtonGroup(scrollContent);

    struct ThemeInfo {
        QString name;
        ThemeType type;
        QColor bgColor, textColor;
    };

    ThemeInfo themes[] = {
        {"Default Light", ThemeDefaultLight, QColor(240, 240, 240), Qt::black},
        {"Default Dark", ThemeDefaultDark, QColor(45, 45, 45), Qt::white},
        {"Blue Light", ThemeBlueLight, QColor(240, 248, 255), Qt::black},
        {"Blue Dark", ThemeBlueDark, QColor(25, 35, 50), Qt::white},
        {"Green Light", ThemeGreenLight, QColor(240, 255, 240), Qt::black},
        {"Green Dark", ThemeGreenDark, QColor(25, 45, 30), Qt::white},
        {"High Contrast Light", ThemeHighContrastLight, Qt::white, Qt::black},
        {"High Contrast Dark", ThemeHighContrastDark, Qt::black, Qt::white}
    };

    for (const ThemeInfo &themeInfo : themes) {
        QPushButton *btn = new QPushButton(themeInfo.name, scrollContent);
        btn->setCheckable(true);
        btn->setProperty("themeType", static_cast<int>(themeInfo.type));
        btn->setStyleSheet(QString("background-color: %1; color: %2; padding: 10px;")
                          .arg(themeInfo.bgColor.name()).arg(themeInfo.textColor.name()));
        scrollLayout->addWidget(btn);
        themeGroup->addButton(btn);
    }

    for (QAbstractButton *btn : themeGroup->buttons()) {
        btn->setChecked(btn->property("themeType").toInt() == static_cast<int>(selectedTheme));
    }

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton *applyButton = new QPushButton(tr("Apply"), dialog);
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    ThemeType chosenTheme = selectedTheme;

    connect(themeGroup, &QButtonGroup::buttonClicked, [this, &chosenTheme](QAbstractButton *button) {
        chosenTheme = static_cast<ThemeType>(button->property("themeType").toInt());
    });

    connect(applyButton, &QPushButton::clicked, dialog, &QDialog::accept);

    int result = dialog->exec();
    if (result == QDialog::Accepted && chosenTheme != selectedTheme) {
        selectedTheme = chosenTheme;
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
            QFile file(openFiles[i].filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
                if (editor)
                    out << editor->toPlainText();
                file.close();
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
                tr("There are unsaved changes. Save before closing?"),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (reply == QMessageBox::Save) {
                on_action_save_triggered();
            } else if (reply == QMessageBox::Cancel) {
                return false;
            }
            break;
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
