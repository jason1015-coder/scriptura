#include "bracketcolorizer.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QPainter>

BracketColorizer::BracketColorizer(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_enabled(true)
    , m_bracketColors(defaultColors())
{
    if (m_editor) {
        connect(m_editor->document(), &QTextDocument::contentsChanged,
                this, &BracketColorizer::updateColors);
    }
}

void BracketColorizer::updateColors()
{
    if (!m_enabled || !m_editor || !m_editor->document()) {
        m_extraSelections.clear();
        applyExtraSelections();
        return;
    }
    
    m_pairs.clear();
    findBracketPairs();
    
    // Build ExtraSelections for bracket coloring (avoids conflicts with syntax highlighter)
    m_extraSelections.clear();
    
    for (const BracketPair &pair : m_pairs) {
        QColor color = colorForDepth(pair.depth);
        
        // Color opening bracket
        QTextEdit::ExtraSelection openSel;
        QTextCursor openCursor(m_editor->document());
        openCursor.setPosition(pair.openPos);
        openCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        openSel.cursor = openCursor;
        openSel.format.setForeground(color);
        openSel.format.setFontWeight(QFont::Bold);
        m_extraSelections.append(openSel);
        
        // Color closing bracket
        QTextEdit::ExtraSelection closeSel;
        QTextCursor closeCursor(m_editor->document());
        closeCursor.setPosition(pair.closePos);
        closeCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        closeSel.cursor = closeCursor;
        closeSel.format.setForeground(color);
        closeSel.format.setFontWeight(QFont::Bold);
        m_extraSelections.append(closeSel);
    }
    
    applyExtraSelections();
    emit colorsChanged();
}

void BracketColorizer::applyExtraSelections()
{
    if (m_editor) {
        // This will be called by CodeEditor to merge with other extra selections
        // For now, we store them and let CodeEditor apply them
        m_editor->update();
    }
}

void BracketColorizer::clearColors()
{
    m_pairs.clear();
    m_extraSelections.clear();
    applyExtraSelections();
}

void BracketColorizer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (enabled) {
            updateColors();
        } else {
            clearColors();
        }
    }
}

void BracketColorizer::setBracketColors(const QList<QColor> &colors)
{
    m_bracketColors = colors;
    if (m_enabled) {
        updateColors();
    }
}

int BracketColorizer::depthAt(int position) const
{
    for (const BracketPair &pair : m_pairs) {
        if (position == pair.openPos || position == pair.closePos) {
            return pair.depth;
        }
    }
    return -1;
}

BracketPair BracketColorizer::pairAt(int position) const
{
    for (const BracketPair &pair : m_pairs) {
        if (position == pair.openPos || position == pair.closePos) {
            return pair;
        }
    }
    BracketPair invalid;
    invalid.openPos = -1;
    invalid.closePos = -1;
    invalid.depth = -1;
    return invalid;
}

void BracketColorizer::findBracketPairs()
{
    QTextDocument *doc = m_editor->document();
    if (!doc) return;
    
    // Stack-based bracket matching
    struct BracketInfo {
        QChar character;
        int position;
        int depth;
    };
    
    QList<BracketInfo> stack;
    int depth = 0;
    
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        QString text = block.text();
        int blockStart = block.position();
        
        // Simple state tracking for strings and comments
        bool inString = false;
        QChar stringChar;
        bool inLineComment = false;
        
        for (int i = 0; i < text.length(); ++i) {
            QChar c = text.at(i);
            int pos = blockStart + i;
            
            // Skip if in comment or string
            if (inLineComment) continue;
            
            if (c == '\'' || c == '"') {
                if (!inString) {
                    inString = true;
                    stringChar = c;
                } else if (c == stringChar) {
                    inString = false;
                }
                continue;
            }
            
            if (inString) continue;
            
            // Check for line comment
            if (c == '/' && i + 1 < text.length() && text.at(i + 1) == '/') {
                inLineComment = true;
                continue;
            }
            
            // Opening brackets
            if (c == '(' || c == '[' || c == '{') {
                BracketInfo info;
                info.character = c;
                info.position = pos;
                info.depth = depth;
                stack.append(info);
                depth++;
            }
            // Closing brackets
            else if (c == ')' || c == ']' || c == '}') {
                if (!stack.isEmpty()) {
                    BracketInfo last = stack.takeLast();
                    depth--;
                    
                    // Check if brackets match
                    bool matches = false;
                    if (last.character == '(' && c == ')') matches = true;
                    if (last.character == '[' && c == ']') matches = true;
                    if (last.character == '{' && c == '}') matches = true;
                    
                    if (matches) {
                        BracketPair pair;
                        pair.openPos = last.position;
                        pair.closePos = pos;
                        pair.depth = last.depth;
                        pair.openChar = last.character;
                        pair.closeChar = c;
                        m_pairs.append(pair);
                    }
                }
            }
        }
        
        block = block.next();
    }
}

int BracketColorizer::findMatchingBracket(int position, QChar open, QChar close) const
{
    QTextDocument *doc = m_editor->document();
    if (!doc) return -1;
    
    QTextCursor cursor(doc);
    cursor.setPosition(position);
    
    // Determine direction based on bracket type
    bool isClosing = (close != QChar());
    int direction = isClosing ? -1 : 1;
    QChar targetOpen = isClosing ? open : close;
    QChar targetClose = isClosing ? close : open;
    
    int depth = 0;
    
    while (true) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, direction);
        
        if (cursor.atEnd() || cursor.atStart()) {
            return -1;
        }
        
        QChar c = doc->characterAt(cursor.position());
        
        if (c == targetOpen) {
            depth++;
        } else if (c == targetClose) {
            depth--;
            if (depth == 0) {
                return cursor.position();
            }
        }
    }
    
    return -1;
}

QColor BracketColorizer::colorForDepth(int depth) const
{
    if (m_bracketColors.isEmpty()) {
        return Qt::gray;
    }
    return m_bracketColors.at(depth % m_bracketColors.size());
}

const QList<QColor> BracketColorizer::defaultColors()
{
    return {
        QColor(86, 182, 194),   // Cyan
        QColor(198, 120, 221),  // Purple
        QColor(152, 195, 121),  // Green
        QColor(229, 192, 123),  // Yellow
        QColor(190, 80, 70),    // Red
        QColor(86, 182, 194),   // Cyan (repeat)
    };
}
