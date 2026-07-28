#include "codelensmanager.h"
#include "codeeditor.h"
#include "rust_adapter.h"
#include <QTextBlock>
#include <QPainter>

CodeLensManager::CodeLensManager(QObject *parent)
    : QObject(parent)
{
}

void CodeLensManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) clearAll();
    }
}

void CodeLensManager::requestCodeLens(CodeEditor *editor, RustLspClientAdapter *lspClient, const QString &documentUri)
{
    if (!m_enabled || !editor || !lspClient || documentUri.isEmpty()) return;

    // Fire immediately — the caller (onEditorTextChanged) already debounces at 500ms
    // The Rust LSP client calls textDocument/codeLens via its generic request pipeline.
    // We use documentSymbol as a data source and convert results to lens items.
    // When the LSP backend adds dedicated codeLens support, this call will switch to
    // lspClient->codeLens(documentUri).
    lspClient->documentSymbol(documentUri);
}

void CodeLensManager::onCodeLensReceived(const QJsonArray &lenses, const QString &documentUri)
{
    QList<CodeLensItem> items;
    for (const QJsonValue &v : lenses) {
        QJsonObject lens = v.toObject();
        CodeLensItem item = parseCodeLens(lens);
        if (!item.title.isEmpty()) {
            items.append(item);
        }
    }

    m_items[documentUri] = items;
    emit codeLensUpdated(documentUri);
}

CodeLensItem CodeLensManager::parseCodeLens(const QJsonObject &lens)
{
    CodeLensItem item;
    QJsonObject range = lens["range"].toObject();
    QJsonObject start = range["start"].toObject();
    item.line = start["line"].toInt();
    item.column = start["character"].toInt();
    item.rawData = lens;

    QJsonObject command = lens["command"].toObject();
    item.title = command["title"].toString();
    item.command = command["command"].toString();
    item.commandArgs = command["arguments"].toArray().isEmpty()
        ? QJsonObject()
        : command["arguments"].toArray().first().toObject();

    return item;
}

QList<CodeLensItem> CodeLensManager::itemsForDocument(const QString &documentUri) const
{
    return m_items.value(documentUri);
}

QList<CodeLensItem> CodeLensManager::itemsAtLine(const QString &documentUri, int line) const
{
    QList<CodeLensItem> result;
    const QList<CodeLensItem> &all = m_items.value(documentUri);
    for (const CodeLensItem &item : all) {
        if (item.line == line) result.append(item);
    }
    return result;
}

void CodeLensManager::clearDocument(const QString &documentUri)
{
    if (m_items.remove(documentUri) > 0) {
        emit codeLensUpdated(documentUri);
    }
}

void CodeLensManager::clearAll()
{
    m_items.clear();
}
