#include "bracketcolorizer.h"
#include "rust_adapter.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

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

    // Rust owns bracket matching. Qt only applies ExtraSelection colors.
    m_pairs.clear();
    {
        QTextDocument *doc = m_editor->document();
        QString flat;
        QVector<int> charToPos;
        QTextBlock b = doc->begin();
        bool first = true;
        while (b.isValid()) {
            if (!first) {
                flat += '\n';
                charToPos.append(b.position() - 1);
            }
            first = false;
            const QString t = b.text();
            flat += t;
            for (int i = 0; i < t.size(); ++i)
                charToPos.append(b.position() + i);
            b = b.next();
        }
        const QString json = RustTextBufferAdapter::bracketPairs(flat);
        const QJsonDocument jdoc = QJsonDocument::fromJson(json.toUtf8());
        const QJsonArray arr = jdoc.array();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const int co = o.value("open").toInt(-1);
            const int cc = o.value("close").toInt(-1);
            const int depth = o.value("depth").toInt(0);
            if (co < 0 || cc < 0 || co >= charToPos.size() || cc >= charToPos.size())
                continue;
            BracketPair pair;
            pair.openPos = charToPos.at(co);
            pair.closePos = charToPos.at(cc);
            pair.depth = depth;
            pair.openChar = flat.at(co);
            pair.closeChar = flat.at(cc);
            m_pairs.append(pair);
        }
    }

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
