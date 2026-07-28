#include "foldmanager.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QPainter>
#include <QPen>
#include <QPalette>
#include <QApplication>

FoldManager::FoldManager(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_useBraceFolding(true)
{
    if (m_editor) {
        connect(m_editor->document(), &QTextDocument::contentsChanged,
                this, &FoldManager::detectRegions);
    }
    detectRegions();
}

void FoldManager::detectRegions()
{
    m_regions.clear();
    m_hiddenLines.clear();
    
    if (!m_editor || !m_editor->document()) return;
    
    detectBraceFolds();
    detectKeywordFolds();
    updateHiddenLines();
    
    emit regionsChanged();
}

void FoldManager::detectBraceFolds()
{
    if (!m_useBraceFolding) return;
    
    QTextDocument *doc = m_editor->document();
    QTextBlock block = doc->begin();
    
    while (block.isValid()) {
        QString text = block.text().trimmed();
        int line = block.blockNumber();
        
        // Check for opening braces
        if (text.contains('{') && !text.contains('}')) {
            // This line has an opening brace - find its matching close
            int endLine = findMatchingBrace(line);
            if (endLine > line) {
                FoldRegion region;
                region.startLine = line;
                region.endLine = endLine;
                region.indentLevel = calculateIndent(block.text());
                region.collapsed = false;
                region.valid = true;
                m_regions.append(region);
            }
        }
        
        block = block.next();
    }
}

void FoldManager::detectKeywordFolds()
{
    // Language-specific keyword folding (if/else/for/while/function/class)
    QTextDocument *doc = m_editor->document();
    QTextBlock block = doc->begin();
    
    QRegularExpression startPattern(
        R"(^\s*(if|else|else\s+if|for|while|do|function|class|struct|enum|try|catch|switch|case)\s*[({]?)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    while (block.isValid()) {
        QString text = block.text();
        int line = block.blockNumber();
        
        QRegularExpressionMatch match = startPattern.match(text);
        if (match.hasMatch()) {
            // Check if this region is already detected by brace folding
            bool alreadyDetected = false;
            for (const FoldRegion &r : m_regions) {
                if (r.startLine == line) {
                    alreadyDetected = true;
                    break;
                }
            }
            
            if (!alreadyDetected) {
                // Find the next block with same or lesser indentation
                int indent = calculateIndent(text);
                int endLine = line;
                QTextBlock nextBlock = block.next();
                
                while (nextBlock.isValid()) {
                    QString nextText = nextBlock.text().trimmed();
                    if (!nextText.isEmpty()) {
                        int nextIndent = calculateIndent(nextBlock.text());
                        if (nextIndent <= indent && !nextText.startsWith("//")) {
                            endLine = nextBlock.blockNumber() - 1;
                            break;
                        }
                    }
                    endLine = nextBlock.blockNumber();
                    nextBlock = nextBlock.next();
                }
                
                if (endLine > line) {
                    FoldRegion region;
                    region.startLine = line;
                    region.endLine = endLine;
                    region.indentLevel = indent;
                    region.collapsed = false;
                    region.valid = true;
                    m_regions.append(region);
                }
            }
        }
        
        block = block.next();
    }
}

int FoldManager::findMatchingBrace(int line) const
{
    QTextDocument *doc = m_editor->document();
    QTextBlock block = doc->findBlockByNumber(line);
    if (!block.isValid()) return -1;
    
    int depth = 0;
    QString text = block.text();
    
    // Start from the opening brace position
    int startPos = text.indexOf('{');
    if (startPos < 0) return -1;
    
    depth = 1;
    int pos = startPos + 1;
    
    while (block.isValid()) {
        while (pos < text.length()) {
            QChar c = text.at(pos);
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) {
                    return block.blockNumber();
                }
            }
            pos++;
        }
        
        block = block.next();
        if (block.isValid()) {
            text = block.text();
            pos = 0;
        }
    }
    
    return -1;
}

int FoldManager::findMatchingBraceReverse(int line) const
{
    QTextDocument *doc = m_editor->document();
    QTextBlock block = doc->findBlockByNumber(line);
    if (!block.isValid()) return -1;
    
    int depth = 0;
    
    while (block.isValid()) {
        QString text = block.text();
        int pos = (block.blockNumber() == line) ? text.length() - 1 : text.length() - 1;
        
        while (pos >= 0) {
            QChar c = text.at(pos);
            if (c == '}') depth++;
            else if (c == '{') {
                depth--;
                if (depth == 0) {
                    return block.blockNumber();
                }
            }
            pos--;
        }
        
        block = block.previous();
    }
    
    return -1;
}

int FoldManager::calculateIndent(const QString &line) const
{
    int indent = 0;
    for (const QChar &c : line) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += 4;
        else break;
    }
    return indent;
}

void FoldManager::toggleFold(int line)
{
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions[i].startLine == line || 
            (m_regions[i].startLine <= line && m_regions[i].endLine >= line)) {
            m_regions[i].collapsed = !m_regions[i].collapsed;
            updateHiddenLines();
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
    emit regionsChanged();
}

void FoldManager::unfoldAll()
{
    for (int i = 0; i < m_regions.size(); ++i) {
        m_regions[i].collapsed = false;
    }
    updateHiddenLines();
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
