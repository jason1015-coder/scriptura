#ifndef CODELENSMANAGER_H
#define CODELENSMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QTimer>
#include <QTextCursor>

class CodeEditor;
class RustLspClientAdapter;

/**
 * A single CodeLens item rendered above a line in the editor.
 */
struct CodeLensItem {
    int line;                   // 0-based line number
    int column;                 // 0-based character offset
    QString title;              // Display text (e.g. "3 references")
    QString command;            // Command to execute on click
    QJsonObject commandArgs;    // Arguments for the command
    QJsonObject rawData;        // Full LSP CodeLens object
};

/**
 * Manages CodeLens annotations for code editors.
 * Requests CodeLens items from LSP and renders them as
 * overlay text above functions/classes.
 */
class CodeLensManager : public QObject
{
    Q_OBJECT
public:
    explicit CodeLensManager(QObject *parent = nullptr);

    // Request code lens items for a document
    void requestCodeLens(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &documentUri);

    // Get all code lens items for a document
    QList<CodeLensItem> itemsForDocument(const QString &documentUri) const;

    // Clear all items for a document
    void clearDocument(const QString &documentUri);

    // Clear all items
    void clearAll();

    // Check if code lens is enabled
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    // Get the code lens for a specific line (returns empty list if none)
    QList<CodeLensItem> itemsAtLine(const QString &documentUri, int line) const;

signals:
    void codeLensUpdated(const QString &documentUri);

private slots:
    void onCodeLensReceived(const QJsonArray &lenses, const QString &documentUri);

private:
    QMap<QString, QList<CodeLensItem>> m_items;  // documentUri -> items
    bool m_enabled = true;

    CodeLensItem parseCodeLens(const QJsonObject &lens);
};

#endif // CODELENSMANAGER_H
