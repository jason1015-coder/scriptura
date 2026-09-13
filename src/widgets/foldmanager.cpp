#include "foldmanager.h"
#include "rust_adapter.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QPainter>
#include <QPen>
#include <QPalette>
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

FoldManager::FoldManager(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
{
    reattachDocument();
    detectRegions();
}

void FoldManager::reattachDocument()
{
    disconnectDocument();
    if (!m_editor || !m_editor->document()) {
        m_document = nullptr;
        return;
    }

    m_document = m_editor->document();
    connect(m_document, &QTextDocument::contentsChanged,
            this, &FoldManager::detectRegions);
}

void FoldManager::disconnectDocument()
{
    if (!m_document)
        return;

    disconnect(m_document, &QTextDocument::contentsChanged,
               this, &FoldManager::detectRegions);
    disconnect(m_document, &QTextDocument::contentsChange,
               this, &FoldManager::detectRegions);
    m_document = nullptr;
}

void FoldManager::detectRegions()
{
    // Rust owns all fold detection (brace, indent, keyword).
    // Qt keeps hidden-line/viewport application only.
    if (!m_editor || !m_editor->document()) return;

    const QString flat = [&]() {
        QString s;
        QTextBlock b = m_editor->document()->begin();
        bool first = true;
        while (b.isValid()) {
            if (!first) s += '\n';
            first = false;
            s += b.text();
            b = b.next();
        }
        return s;
    }();
    const QString json = RustTextBufferAdapter::foldRanges(flat, false);
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

    m_regions.clear();
    m_hiddenLines.clear();

    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        FoldRegion r;
        r.startLine = o.value("startLine").toInt(-1);
        r.endLine = o.value("endLine").toInt(-1);
        r.indentLevel = o.value("indentLevel").toInt(0);
        r.collapsed = false;
        r.valid = r.startLine >= 0 && r.endLine > r.startLine;
        if (r.valid) m_regions.append(r);
    }

    std::sort(m_regions.begin(), m_regions.end(),
              [](const FoldRegion &a, const FoldRegion &b) {
                  return a.startLine < b.startLine ||
                         (a.startLine == b.startLine && a.endLine < b.endLine);
              });

    updateHiddenLines();
    updateBlockVisibility();
    emit regionsChanged();
}

void FoldManager::toggleFold(int line)
{
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions[i].startLine == line || 
            (m_regions[i].startLine <= line && m_regions[i].endLine >= line)) {
            m_regions[i].collapsed = !m_regions[i].collapsed;
            updateHiddenLines();
            updateBlockVisibility();
            emit foldStateChanged(line, m_regions[i].collapsed);
            emit regionsChanged();
            return;
        }
    }
}

void FoldManager::foldAll()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        m_regions[i].collapsed = true;
    }
    updateHiddenLines();
    updateBlockVisibility();
    emit regionsChanged();
}

void FoldManager::unfoldAll()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        m_regions[i].collapsed = false;
    }
    updateHiddenLines();
    updateBlockVisibility();
    emit regionsChanged();
}

void FoldManager::foldAtLevel(int level)
{
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions[i].indentLevel >= level) {
            m_regions[i].collapsed = true;
        }
    }
    updateHiddenLines();
    updateBlockVisibility();
    emit regionsChanged();
}

void FoldManager::unfoldAtLevel(int level)
{
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions[i].indentLevel >= level) {
            m_regions[i].collapsed = false;
        }
    }
    updateHiddenLines();
    updateBlockVisibility();
    emit regionsChanged();
}

bool FoldManager::isFolded(int line) const
{
    for (const FoldRegion &r : m_regions) {
        if (r.startLine == line) return r.collapsed;
    }
    return false;
}

bool FoldManager::isFoldStart(int line) const
{
    for (const FoldRegion &r : m_regions) {
        if (r.startLine == line) return true;
    }
    return false;
}

bool FoldManager::isFoldEnd(int line) const
{
    for (const FoldRegion &r : m_regions) {
        if (r.endLine == line) return true;
    }
    return false;
}

bool FoldManager::isRegionVisible(int line) const
{
    return !m_hiddenLines.contains(line);
}

int FoldManager::foldedLineCount() const
{
    return m_hiddenLines.size();
}

int FoldManager::visibleLineCount() const
{
    if (!m_editor || !m_editor->document()) return 0;
    return m_editor->document()->blockCount() - m_hiddenLines.size();
}

FoldRegion FoldManager::regionAt(int line) const
{
    for (const FoldRegion &r : m_regions) {
        if (r.startLine == line) return r;
    }
    FoldRegion invalid;
    invalid.startLine = -1;
    invalid.endLine = -1;
    invalid.indentLevel = 0;
    invalid.collapsed = false;
    invalid.valid = false;
    return invalid;
}

bool FoldManager::isLineHidden(int blockNumber) const
{
    return m_hiddenLines.contains(blockNumber);
}

void FoldManager::updateHiddenLines()
{
    m_hiddenLines.clear();

    for (const FoldRegion &r : m_regions) {
        if (r.collapsed) {
            // Hide all lines between start and end (exclusive of start, inclusive of end)
            for (int line = r.startLine + 1; line <= r.endLine; ++line) {
                m_hiddenLines.insert(line);
            }
        }
    }
}

void FoldManager::updateBlockVisibility()
{
    if (!m_editor || !m_editor->document())
        return;

    QTextDocument *doc = m_editor->document();
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        bool hidden = m_hiddenLines.contains(block.blockNumber());
        if (block.isVisible() != hidden)
            block.setVisible(!hidden);
        block = block.next();
    }
}

void FoldManager::paintFoldIndicator(QPainter &painter, int x, int y, int blockNumber, int blockHeight)
{
    bool hasFold = isFoldStart(blockNumber);
    bool isCollapsed = isFolded(blockNumber);
    
    if (!hasFold) return;
    
    // Draw fold indicator circle
    int size = qMin(12, blockHeight - 2);
    int cx = x + size / 2;
    int cy = y + blockHeight / 2;
    
    // Background circle - use editor palette since FoldManager is a QObject
    QPalette pal = m_editor ? m_editor->palette() : QPalette();
    QColor bgColor = pal.color(QPalette::Base);
    QColor borderColor = pal.color(QPalette::Mid);
    
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(bgColor);
    painter.drawEllipse(cx - size/2, cy - size/2, size, size);
    
    // Plus/Minus symbol
    painter.setPen(QPen(pal.color(QPalette::Text), 1));
    int halfSize = size / 4;
    
    // Horizontal line (always drawn)
    painter.drawLine(cx - halfSize, cy, cx + halfSize, cy);
    
    // Vertical line (only for plus)
    if (!isCollapsed) {
        painter.drawLine(cx, cy - halfSize, cx, cy + halfSize);
    }
}
