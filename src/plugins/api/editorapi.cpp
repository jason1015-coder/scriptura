#include "editorapi.h"
#include "mainwindow.h"
#include "codeeditor.h"
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>
#include <QIcon>
#include <QPainter>

PluginEditorApi::PluginEditorApi(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

PluginEditorApi::~PluginEditorApi() = default;

// ── Internal Helpers ────────────────────────────────────────────

QTextEdit::ExtraSelection PluginEditorApi::makeSelection(int line, const QColor &color) const
{
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(color);
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    if (m_mainWindow) {
        CodeEditor *editor = m_mainWindow->getCurrentCodeEditor();
        if (editor) {
            QTextBlock block = editor->document()->findBlockByNumber(line);
            if (block.isValid()) {
                sel.cursor = QTextCursor(block);
                sel.cursor.movePosition(QTextCursor::StartOfBlock);
                sel.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            }
        }
    }
    return sel;
}

void PluginEditorApi::applyDecorations()
{
    CodeEditor *editor = m_mainWindow ? m_mainWindow->getCurrentCodeEditor() : nullptr;
    if (!editor) return;

    QList<QTextEdit::ExtraSelection> selections;
    for (auto it = m_decorations.constBegin(); it != m_decorations.constEnd(); ++it) {
        selections.append(makeSelection(it.value().line, it.value().color));
    }
    // Use plugin-specific selections so they don't overwrite LSP diagnostics
    editor->setPluginExtraSelections(selections);
}

void PluginEditorApi::applyMarkers()
{
    // Gutter markers are drawn via the line number area; for now we
    // store them and rely on the editor re-paint integration.
    // A full implementation would use a custom line-number-area delegate.
    Q_UNUSED(m_markers);
}

void PluginEditorApi::applyAnnotations()
{
    // Annotations require a custom extra-selection or overlay approach.
    // For now we just log — a production implementation might use
    // QTextDocument block formats or a dedicated annotation panel.
    Q_UNUSED(m_annotations);
}

// ── Decoration API ──────────────────────────────────────────────

void PluginEditorApi::addLineDecoration(const QString &id, int line,
                                         const QColor &color, const QString &tooltip)
{
    DecorationEntry entry;
    entry.line = line;
    entry.color = color;
    entry.tooltip = tooltip;
    m_decorations[id] = entry;
    applyDecorations();
}

void PluginEditorApi::removeDecoration(const QString &id)
{
    m_decorations.remove(id);
    applyDecorations();
}

void PluginEditorApi::clearAllDecorations()
{
    m_decorations.clear();
    applyDecorations();
}

// ── Gutter Marker API ───────────────────────────────────────────

void PluginEditorApi::addGutterMarker(const QString &id, int line,
                                       const QIcon &icon, const QString &tooltip)
{
    MarkerEntry entry;
    entry.line = line;
    entry.icon = icon;
    entry.tooltip = tooltip;
    m_markers[id] = entry;
    applyMarkers();
}

void PluginEditorApi::removeGutterMarker(const QString &id, int line)
{
    Q_UNUSED(line);
    m_markers.remove(id);
    applyMarkers();
}

void PluginEditorApi::clearAllGutterMarkers()
{
    m_markers.clear();
    applyMarkers();
}

// ── Annotation API ──────────────────────────────────────────────

void PluginEditorApi::addAnnotation(const QString &id, int line,
                                     const QString &text, const QColor &color)
{
    AnnotationEntry entry;
    entry.line = line;
    entry.text = text;
    entry.color = color;
    m_annotations[id] = entry;
    applyAnnotations();
}

void PluginEditorApi::removeAnnotation(const QString &id)
{
    m_annotations.remove(id);
    applyAnnotations();
}

void PluginEditorApi::clearAllAnnotations()
{
    m_annotations.clear();
    applyAnnotations();
}

// ── Editor Read Access ──────────────────────────────────────────

QString PluginEditorApi::selectedText() const
{
    CodeEditor *editor = m_mainWindow ? m_mainWindow->getCurrentCodeEditor() : nullptr;
    return editor ? editor->textCursor().selectedText() : QString();
}

int PluginEditorApi::cursorLine() const
{
    CodeEditor *editor = m_mainWindow ? m_mainWindow->getCurrentCodeEditor() : nullptr;
    return editor ? editor->textCursor().blockNumber() : -1;
}

int PluginEditorApi::cursorColumn() const
{
    CodeEditor *editor = m_mainWindow ? m_mainWindow->getCurrentCodeEditor() : nullptr;
    return editor ? editor->textCursor().positionInBlock() : -1;
}

int PluginEditorApi::lineCount() const
{
    CodeEditor *editor = m_mainWindow ? m_mainWindow->getCurrentCodeEditor() : nullptr;
    return editor ? editor->document()->blockCount() : 0;
}

void PluginEditorApi::onEditorChanged()
{
    applyDecorations();
    applyMarkers();
    applyAnnotations();
}
