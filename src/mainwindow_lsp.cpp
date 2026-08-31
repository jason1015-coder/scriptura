#include <QStandardPaths>
#include <QInputDialog>
#include <QLineEdit>
#include <QDesktopServices>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "rust_adapter.h"

#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QTextCursor>
#include <QToolTip>
#include <QProcess>
#include <QMessageBox>
#include <QDebug>

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
                QString langId = QFileInfo(filePath).suffix().toLower();
                lspClient->initialize(QUrl::fromLocalFile(projectDir.isEmpty() ? QDir::homePath() : projectDir).toString(), langId);
            });
        }
    }
}

void MainWindow::startLanguageServerForProject(const QString &projectPath)
{
    QDir dir(projectPath);
    QStringList filters = {"*.cpp", "*.c", "*.h", "*.hpp", "*.py", "*.js", "*.ts", "*.java", "*.rs", "*.go"};
    QSet<QString> foundExtensions;

    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setNameFilters(filters);
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &fi : files) {
        foundExtensions.insert(fi.suffix().toLower());
    }

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
        return;
    }

    QString rootUri = QUrl::fromLocalFile(projectPath).toString();
    if (lspClient->startServer(serverCommand, serverArgs, rootUri)) {
        QTimer::singleShot(500, this, [this, projectPath, rootUri, filters]() {
            QString langId = "plaintext";
            QDir dir(projectPath);
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            dir.setNameFilters(filters);
            QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
            if (!files.isEmpty()) {
                langId = files.first().suffix().toLower();
            }
            lspClient->initialize(rootUri, langId);

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

void MainWindow::onDiagnosticsReceived(const QString &uri, const QJsonArray &diagnostics)
{
    // Update squiggly underlines in editor
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!editor)
            continue;

        QString tabUri = QUrl::fromLocalFile(openFiles[i].filePath).toString();
        if (tabUri == uri) {
            QList<QTextEdit::ExtraSelection> extraSelections;
            QList<QPair<QTextCursor, QString>> diagnosticTooltips;

            for (const QJsonValue &v : diagnostics) {
                QJsonObject diag = v.toObject();
                int severity = diag["severity"].toInt();
                int line = diag["range"].toObject()["start"].toObject()["line"].toInt();
                int column = diag["range"].toObject()["start"].toObject()["character"].toInt();
                int endLine = diag["range"].toObject()["end"].toObject()["line"].toInt();
                int endColumn = diag["range"].toObject()["end"].toObject()["character"].toInt();
                QString message = diag["message"].toString();

                // Create extra selection for the diagnostic range
                QTextBlock startBlock = editor->document()->findBlockByNumber(line);
                QTextBlock endBlock = editor->document()->findBlockByNumber(endLine);
                if (startBlock.isValid() && endBlock.isValid()) {
                    QTextCursor cursor(editor->document());
                    cursor.setPosition(startBlock.position() + column);
                    cursor.setPosition(endBlock.position() + endColumn, QTextCursor::KeepAnchor);

                    QTextEdit::ExtraSelection sel;
                    sel.format.setUnderlineStyle(
                        severity <= 1 ? QTextCharFormat::WaveUnderline : // Error
                        severity == 2 ? QTextCharFormat::SpellCheckUnderline : // Warning
                        QTextCharFormat::NoUnderline);
                    sel.format.setUnderlineColor(
                        severity <= 1 ? QColor(255, 0, 0) :
                        severity == 2 ? QColor(255, 180, 0) :
                        QColor(0, 150, 255));
                    sel.cursor = cursor;
                    extraSelections.append(sel);

                    if (!message.isEmpty()) {
                        diagnosticTooltips.append({cursor, message});
                    }
                }
            }
            editor->setDiagnosticTooltips(diagnosticTooltips);
            editor->setDiagnostics(extraSelections);
            break;
        }
    }
}

void MainWindow::onProblemActivated(const QString &fileUri, int line, int column)
{
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

    QModelIndex index = fileModel->index(localPath);
    if (index.isValid()) {
        on_fileTreeView_clicked(index);
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

void MainWindow::on_action_git_commit_triggered()
{
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
            QMessageBox::information(this, tr("Git Commit"), output + error);
        } else {
            QMessageBox::warning(this, tr("Git Commit"), tr("Failed to run git commit."));
        }
    }
}

void MainWindow::on_action_git_push_triggered()
{
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
        QMessageBox::information(this, tr("Git Push"), output + error);
    } else {
        QMessageBox::warning(this, tr("Git Push"), tr("Failed to run git push."));
    }
}

void MainWindow::onEditorTextChanged()
{
    if (currentFile.isEmpty() || !lspClient->isRunning())
        return;

    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;

    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->didChange(uri, editor->toPlainText());

    // Request Code Lens update for this document
    if (m_codeLensManager && m_codeLensManager->isEnabled()) {
        m_codeLensManager->requestCodeLens(editor, lspClient, uri);
    }
}

void MainWindow::requestHover()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty() || !lspClient->isRunning())
        return;
    QTextCursor cursor = editor->textCursor();
    lspClient->hover(QUrl::fromLocalFile(currentFile).toString(), cursor.blockNumber(), cursor.positionInBlock());
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
}

void MainWindow::on_action_check_updates_triggered()
{
    on_action_editor_settings_triggered();
}

void MainWindow::on_action_format_document_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->formatting(uri, QJsonObject());
    qDebug() << "Formatting requested for" << currentFile;
}

void MainWindow::on_action_go_to_definition_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    lspClient->definition(uri, cursor.blockNumber(), cursor.positionInBlock());
}

void MainWindow::on_action_go_to_declaration_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    lspClient->declaration(uri, cursor.blockNumber(), cursor.positionInBlock());
}

void MainWindow::on_action_go_to_type_definition_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    lspClient->typeDefinition(uri, cursor.blockNumber(), cursor.positionInBlock());
}

void MainWindow::on_action_go_to_implementation_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    QTextCursor cursor = editor->textCursor();
    lspClient->implementation(uri, cursor.blockNumber(), cursor.positionInBlock());
}

void MainWindow::onCompletionReceived(const QJsonArray &items, int requestId)
{
    Q_UNUSED(requestId)
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;

    // Show completion popup.
    //   - Parent to the editor and store in a QPointer so the pointer auto-nulls
    //     when the editor/tab is destroyed (raw pointer here dangled and
    //     segfaulted hideCompletion()/clear() when switching/closing files).
    //   - Keep it a plain child widget of the editor (no Qt::Popup/Qt::Tool
    //     window flags). A Qt::Popup grabs keyboard focus and silenced ALL
    //     typing/arrow keys in the editor until Escape was pressed — which
    //     presented as "the editor became read-only". As a child widget it
    //     never steals focus; navigation is forwarded by eventFilter and it
    //     auto-hides when the editor regains input.
    if (m_completionPopup) {
        m_completionPopup->clear();
    } else {
        // Parent to the viewport (not the editor body) so local coordinates
        // align with cursorRect(), which is offset by the line-number gutter.
        m_completionPopup = new QListWidget(editor->viewport());
        m_completionPopup->setFocusPolicy(Qt::NoFocus);
        m_completionPopup->setFrameShape(QFrame::StyledPanel);
        m_completionPopup->hide();

        // Accept the selection on Enter (routed here by MainWindow's event
        // filter) and on mouse double-click. These are connected ONCE at
        // creation — the old code connected on every completion response,
        // which accumulated one handler per keystroke and left the Enter
        // path (itemActivated) with no receiver at all, so Enter appeared
        // dead while the popup was visible.
        auto insertSelected = [this, editor](QListWidgetItem *item) {
            if (!item) return;
            QString insert = item->data(Qt::UserRole).toString();
            QTextCursor cursor = editor->textCursor();
            cursor.insertText(insert);
            editor->setTextCursor(cursor);
            hideCompletion();
        };
        connect(m_completionPopup, &QListWidget::itemActivated, this, insertSelected);
        connect(m_completionPopup, &QListWidget::itemDoubleClicked, this, insertSelected);

        // Dismiss the popup as soon as the user types (or the document is
        // otherwise edited) so it can never sit on top of the caret and
        // swallow arrows/Enter indefinitely. Completion is manual
        // (Ctrl+Space), so typing means "I'm not accepting this list".
        connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::hideCompletion);
    }

    for (const QJsonValue &v : items) {
        QJsonObject item = v.toObject();
        QString label = item["label"].toString();
        QString kind = item["kind"].toString();
        QString detail = item["detail"].toString();
        QString insertText = item["insertText"].toString();
        if (insertText.isEmpty()) insertText = label;

        QListWidgetItem *wi = new QListWidgetItem(
            QString("%1  (%2)").arg(label, kind), m_completionPopup);
        wi->setData(Qt::UserRole, insertText);
        if (!detail.isEmpty())
            wi->setToolTip(detail);
        m_completionPopup->addItem(wi);
    }

    if (m_completionPopup->count() == 0) {
        hideCompletion();
        return;
    }

    // Position popup near cursor in local coordinates (child of the editor).
    QRect rect = editor->cursorRect();
    QPoint pos = rect.bottomLeft() + QPoint(2, 2);
    QRect geo(pos, m_completionPopup->sizeHint());
    geo.setHeight(qMin(200, m_completionPopup->sizeHint().height()));
    m_completionPopup->setGeometry(geo);
    m_completionPopup->raise();
    m_completionPopup->show();
    m_completionPopup->setCurrentRow(0);
}

void MainWindow::hideCompletion()
{
    if (m_completionPopup)
        m_completionPopup->hide();
}
