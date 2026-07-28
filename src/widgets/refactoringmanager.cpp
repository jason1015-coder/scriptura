#include "refactoringmanager.h"
#include "rust_adapter.h"
#include "codeeditor.h"

#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>

RefactoringManager::RefactoringManager(QObject *parent)
    : QObject(parent)
{
}

void RefactoringManager::renameSymbol(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath)
{
    if (!editor || !lspClient || filePath.isEmpty()) return;

    QTextCursor cursor = editor->textCursor();
    // If there's no selection, select the word under cursor
    if (!cursor.hasSelection() || cursor.selectedText().isEmpty()) {
        cursor.select(QTextCursor::WordUnderCursor);
        editor->setTextCursor(cursor);
    }

    QString symbol = cursor.selectedText();
    if (symbol.isEmpty()) {
        emit refactoringFailed(tr("No symbol under cursor to rename"));
        return;
    }

    m_currentEditor = editor;
    m_currentFilePath = filePath;

    // Emit signal so MainWindow can show a rename dialog
    // The dialog should call renameWithNewName() with the result
    emit renameDialogRequested(symbol, filePath, cursor.blockNumber(), cursor.positionInBlock());
}

void RefactoringManager::renameWithNewName(CodeEditor *editor, RustLspClientAdapter *lspClient,
                                            const QString &filePath, const QString &newName)
{
    if (!editor || !lspClient || filePath.isEmpty() || newName.isEmpty()) {
        emit refactoringFailed(tr("Invalid rename parameters"));
        return;
    }

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }

    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_currentEditor = editor;
    m_currentFilePath = filePath;

    // Send rename request to LSP with the new name
    QString uri = QUrl::fromLocalFile(filePath).toString();
    int requestId = lspClient->rename(uri, line, character, newName);

    // Store the request ID for response handling
    m_pendingRefactorings[requestId] = "rename";
}

void RefactoringManager::onRenameReceived(const QJsonObject &result)
{
    if (result.isEmpty()) {
        emit refactoringFailed(tr("No rename results returned"));
        return;
    }

    if (applyWorkspaceEdit(result)) {
        int fileCount = result["changes"].toObject().keys().size();
        // Handle documentChanges if present (more modern LSP format)
        if (fileCount == 0 && result.contains("documentChanges")) {
            fileCount = result["documentChanges"].toArray().size();
        }
        emit refactoringApplied(fileCount);
    } else {
        emit refactoringFailed(tr("Failed to apply rename edits"));
    }
}

void RefactoringManager::extractMethod(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath)
{
    if (!editor || !lspClient || filePath.isEmpty()) return;

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        emit refactoringFailed(tr("Select code to extract into a method"));
        return;
    }

    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();
    QTextBlock startBlock = editor->document()->findBlock(startPos);
    QTextBlock endBlock = editor->document()->findBlock(endPos);

    if (!startBlock.isValid()) {
        emit refactoringFailed(tr("Invalid selection range"));
        return;
    }

    int startLine = startBlock.blockNumber();
    int endLine = endBlock.blockNumber();

    m_currentEditor = editor;
    m_currentFilePath = filePath;

    QString uri = QUrl::fromLocalFile(filePath).toString();
    int requestId = lspClient->codeAction(uri, startLine, 0, endLine, 10000);

    // Tag this request as an extract method refactoring
    m_pendingRefactorings[requestId] = "extract.method";
}

void RefactoringManager::extractVariable(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath)
{
    if (!editor || !lspClient || filePath.isEmpty()) return;

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        emit refactoringFailed(tr("Select an expression to extract into a variable"));
        return;
    }

    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();
    QTextBlock startBlock = editor->document()->findBlock(startPos);
    QTextBlock endBlock = editor->document()->findBlock(endPos);

    if (!startBlock.isValid()) {
        emit refactoringFailed(tr("Invalid selection range"));
        return;
    }

    int startLine = startBlock.blockNumber();
    int endLine = endBlock.blockNumber();

    m_currentEditor = editor;
    m_currentFilePath = filePath;

    QString uri = QUrl::fromLocalFile(filePath).toString();
    int requestId = lspClient->codeAction(uri, startLine, 0, endLine, 10000);

    // Tag as extract variable
    m_pendingRefactorings[requestId] = "extract.variable";
}

void RefactoringManager::inlineSymbol(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath)
{
    if (!editor || !lspClient || filePath.isEmpty()) return;

    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber();
    int character = cursor.positionInBlock();

    m_currentEditor = editor;
    m_currentFilePath = filePath;

    QString uri = QUrl::fromLocalFile(filePath).toString();
    int requestId = lspClient->codeAction(uri, line, character, line, character);

    // Tag as inline refactoring
    m_pendingRefactorings[requestId] = "inline";
}

bool RefactoringManager::isRefactoringAvailable(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath)
{
    if (!editor || !lspClient || filePath.isEmpty()) return false;
    if (!lspClient->isRunning()) return false;
    return true;
}

void RefactoringManager::onCodeActionReceived(const QJsonArray &actions, int requestId)
{
    QString refactoringType = m_pendingRefactorings.value(requestId);
    if (refactoringType.isEmpty()) return;

    m_pendingRefactorings.remove(requestId);

    // Find the first matching refactoring code action
    for (const QJsonValue &v : actions) {
        QJsonObject action = v.toObject();
        QString kind = action["kind"].toString();

        bool matchesType = false;
        if (refactoringType == "extract.method" && kind.contains("refactor.extract.function")) {
            matchesType = true;
        } else if (refactoringType == "extract.variable" && kind.contains("refactor.extract.variable")) {
            matchesType = true;
        } else if (refactoringType == "inline" && kind.contains("refactor.inline")) {
            matchesType = true;
        }

        if (!matchesType) continue;

        // Execute the code action by sending it back to LSP
        QJsonObject command = action["command"].toObject();
        if (command.isEmpty()) continue;

        // Apply the workspace edit directly if present
        QJsonObject edit = action["edit"].toObject();
        if (!edit.isEmpty()) {
            if (applyWorkspaceEdit(edit)) {
                emit refactoringApplied(1);
            } else {
                emit refactoringFailed(tr("Failed to apply refactoring edits"));
            }
            return;
        }

        // If no direct edit, emit the command for the LSP to process
        m_currentEditor = nullptr;
        return;
    }

    // No matching refactoring found
    if (refactoringType == "extract.method") {
        emit refactoringFailed(tr("Extract method not supported by language server"));
    } else if (refactoringType == "extract.variable") {
        emit refactoringFailed(tr("Extract variable not supported by language server"));
    } else if (refactoringType == "inline") {
        emit refactoringFailed(tr("Inline not supported by language server"));
    } else {
        emit refactoringFailed(tr("Refactoring not supported by language server"));
    }
}

void RefactoringManager::onLspResultReceived(const QJsonArray &result, int requestId)
{
    // Check if this request ID matches a pending refactoring
    QString refactoringType = m_pendingRefactorings.value(requestId);
    if (refactoringType.isEmpty()) return;

    if (refactoringType == "rename") {
        m_pendingRefactorings.remove(requestId);
        // Rename result — the array should contain the WorkspaceEdit as first element
        if (!result.isEmpty()) {
            QJsonObject edit = result.first().toObject();
            onRenameReceived(edit);
        } else {
            emit refactoringFailed(tr("Empty rename result from language server"));
        }
    } else if (refactoringType.startsWith("extract.") || refactoringType == "inline") {
        // Don't remove here — onCodeActionReceived handles its own removal
        onCodeActionReceived(result, requestId);
    }
}

QList<WorkspaceEditEntry> RefactoringManager::parseWorkspaceChanges(const QJsonObject &edit) const
{
    QList<WorkspaceEditEntry> entries;

    // Parse "changes" map (URI -> TextEdit[])
    QJsonObject changes = edit["changes"].toObject();
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        QString uri = it.key();
        QString filePath = QUrl(uri).toLocalFile();
        QJsonArray edits = it.value().toArray();

        for (const QJsonValue &v : edits) {
            QJsonObject textEdit = v.toObject();
            QJsonObject range = textEdit["range"].toObject();
            QJsonObject start = range["start"].toObject();
            QJsonObject end = range["end"].toObject();

            WorkspaceEditEntry entry;
            entry.filePath = filePath;
            entry.startLine = start["line"].toInt();
            entry.startColumn = start["character"].toInt();
            entry.endLine = end["line"].toInt();
            entry.endColumn = end["character"].toInt();
            entry.newText = textEdit["newText"].toString();
            entries.append(entry);
        }
    }

    // Parse "documentChanges" (more modern LSP format)
    QJsonArray docChanges = edit["documentChanges"].toArray();
    for (const QJsonValue &v : docChanges) {
        QJsonObject docChange = v.toObject();
        QString uri;

        if (docChange.contains("textDocument")) {
            QJsonObject textDoc = docChange["textDocument"].toObject();
            uri = textDoc["uri"].toString();
        } else {
            continue;
        }

        QString filePath = QUrl(uri).toLocalFile();
        QJsonArray edits = docChange["edits"].toArray();

        for (const QJsonValue &e : edits) {
            QJsonObject textEdit = e.toObject();
            QJsonObject range = textEdit["range"].toObject();
            QJsonObject start = range["start"].toObject();
            QJsonObject end = range["end"].toObject();

            WorkspaceEditEntry entry;
            entry.filePath = filePath;
            entry.startLine = start["line"].toInt();
            entry.startColumn = start["character"].toInt();
            entry.endLine = end["line"].toInt();
            entry.endColumn = end["character"].toInt();
            entry.newText = textEdit["newText"].toString();
            entries.append(entry);
        }
    }

    return entries;
}

bool RefactoringManager::applyTextEdits(const QString &filePath, const QList<WorkspaceEditEntry> &entries)
{
    if (entries.isEmpty()) return false;

    // Read the current file content
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for workspace edit:" << filePath;
        return false;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Save backup for undo
    saveBackup(filePath, content);

    // Split content into lines for position-based editing
    QStringList lines = content.split('\n');

    // Group edits by line (apply bottom-up to preserve positions)
    QMap<int, QList<WorkspaceEditEntry>> editsByLine;
    for (const WorkspaceEditEntry &entry : entries) {
        if (entry.filePath != filePath) continue;
        editsByLine[entry.startLine].append(entry);
    }

    // Apply edits from bottom to top
    QList<int> sortedLines = editsByLine.keys();
    std::sort(sortedLines.begin(), sortedLines.end(), std::greater<int>());

    for (int lineNum : sortedLines) {
        const QList<WorkspaceEditEntry> &lineEdits = editsByLine[lineNum];

        // Sort edits on the same line from right to left
        QList<WorkspaceEditEntry> sorted = lineEdits;
        std::sort(sorted.begin(), sorted.end(), [](const WorkspaceEditEntry &a, const WorkspaceEditEntry &b) {
            return a.startColumn > b.startColumn;
        });

        for (const WorkspaceEditEntry &entry : sorted) {
            if (lineNum >= lines.size()) continue;

            QString &line = lines[lineNum];
            int start = qMin(entry.startColumn, line.length());
            int end = qMin(entry.endColumn, line.length());

            // Handle multi-line replacements
            if (entry.startLine != entry.endLine) {
                // Replace from start position to end of line
                QString modifiedLine = line.left(start) + entry.newText;
                line = modifiedLine;

                // Remove lines between start+1 and end
                int removeCount = entry.endLine - entry.startLine;
                for (int i = 0; i < removeCount && (lineNum + 1) < lines.size(); ++i) {
                    lines.removeAt(lineNum + 1);
                }

                // Handle the end line
                if (lineNum + 1 < lines.size()) {
                    QString endLine = lines[lineNum + 1];
                    line += endLine.mid(entry.endColumn);
                    lines.removeAt(lineNum + 1);
                }
            } else {
                // Single line replacement
                line = line.left(start) + entry.newText + line.mid(end);
            }
        }
    }

    // Write the modified content back
    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "Cannot write file for workspace edit:" << filePath;
        return false;
    }
    outFile.write(lines.join('\n').toUtf8());
    outFile.close();

    return true;
}

void RefactoringManager::saveBackup(const QString &filePath, const QString &content)
{
    m_fileBackups[filePath] = content;
}

bool RefactoringManager::restoreFromBackup(const QString &filePath)
{
    if (!m_fileBackups.contains(filePath)) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    file.write(m_fileBackups[filePath].toUtf8());
    file.close();
    m_fileBackups.remove(filePath);
    return true;
}

bool RefactoringManager::applyWorkspaceEdit(const QJsonObject &edit)
{
    QList<WorkspaceEditEntry> allEntries = parseWorkspaceChanges(edit);
    if (allEntries.isEmpty()) {
        qWarning() << "Workspace edit has no changes";
        return false;
    }

    // Group entries by file
    QMap<QString, QList<WorkspaceEditEntry>> entriesByFile;
    for (const WorkspaceEditEntry &entry : allEntries) {
        if (!entry.filePath.isEmpty()) {
            entriesByFile[entry.filePath].append(entry);
        }
    }

    // Apply edits per file
    bool allSucceeded = true;
    for (auto it = entriesByFile.begin(); it != entriesByFile.end(); ++it) {
        const QString &filePath = it.key();
        if (!applyTextEdits(filePath, it.value())) {
            qWarning() << "Failed to apply edits to:" << filePath;
            allSucceeded = false;
        }
    }

    // Reload the current editor
    if (m_currentEditor && !m_currentFilePath.isEmpty()) {
        QFile file(m_currentFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_currentEditor->setPlainText(QString::fromUtf8(file.readAll()));
            file.close();
        }
    }

    return allSucceeded;
}
