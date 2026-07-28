#ifndef REFACTORINGMANAGER_H
#define REFACTORINGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>

class CodeEditor;
class RustLspClientAdapter;

struct WorkspaceEditEntry {
    QString filePath;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    QString newText;
};

class RefactoringManager : public QObject
{
    Q_OBJECT
public:
    explicit RefactoringManager(QObject *parent = nullptr);

    void renameSymbol(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath);
    void renameWithNewName(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath, const QString &newName);
    void extractMethod(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath);
    void extractVariable(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath);
    void inlineSymbol(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath);

    bool isRefactoringAvailable(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &filePath);

signals:
    void refactoringApplied(int fileCount);
    void refactoringFailed(const QString &reason);
    void renameDialogRequested(const QString &symbol, const QString &filePath, int line, int column);

public slots:
    void onRenameReceived(const QJsonObject &result);
    void onCodeActionReceived(const QJsonArray &actions, int requestId);
    // Connected to RustLspClientAdapter::completionReceived(QJsonArray, int)
    // Routes LSP results to the right handler based on pending request type.
    void onLspResultReceived(const QJsonArray &result, int requestId);

private:
    bool applyWorkspaceEdit(const QJsonObject &edit);
    QList<WorkspaceEditEntry> parseWorkspaceChanges(const QJsonObject &edit) const;
    bool applyTextEdits(const QString &filePath, const QList<WorkspaceEditEntry> &entries);
    void saveBackup(const QString &filePath, const QString &content);
    bool restoreFromBackup(const QString &filePath);

    CodeEditor *m_currentEditor = nullptr;
    QString m_currentFilePath;
    QMap<QString, QString> m_fileBackups;  // filePath -> original content (for undo)
    int m_nextRequestId = 1000;
    QMap<int, QString> m_pendingRefactorings;  // requestId -> refactoring type
};

#endif // REFACTORINGMANAGER_H
