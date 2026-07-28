#ifndef BRACKETCOLORIZER_H
#define BRACKETCOLORIZER_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QColor>
#include <QTextCursor>
#include <QTextEdit>

class QPlainTextEdit;

/**
 * Represents a matched pair of brackets with their positions
 */
struct BracketPair {
    int openPos;        // Document position of opening bracket
    int closePos;       // Document position of closing bracket
    int depth;          // Nesting depth (0 = outermost)
    QChar openChar;     // The opening bracket character
    QChar closeChar;    // The closing bracket character
};

/**
 * Colors matching bracket pairs with distinct colors based on nesting depth.
 * Supports (), [], {}, and their nesting.
 */
class BracketColorizer : public QObject
{
    Q_OBJECT
public:
    explicit BracketColorizer(QPlainTextEdit *editor, QObject *parent = nullptr);

    // Core operations
    void updateColors();
    void clearColors();
    
    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    // Custom colors (optional - uses defaults if not set)
    void setBracketColors(const QList<QColor> &colors);
    
    // Query
    int depthAt(int position) const;
    BracketPair pairAt(int position) const;
    QList<QTextEdit::ExtraSelection> extraSelections() const { return m_extraSelections; }
    
signals:
    void colorsChanged();

private:
    void findBracketPairs();
    int findMatchingBracket(int position, QChar open, QChar close) const;
    QColor colorForDepth(int depth) const;
    void applyExtraSelections();
    
    QPlainTextEdit *m_editor;
    QList<BracketPair> m_pairs;
    QList<QColor> m_bracketColors;
    QList<QTextEdit::ExtraSelection> m_extraSelections;
    bool m_enabled;
    
    // Default colors for different nesting depths
    static const QList<QColor> defaultColors();
};

#endif // BRACKETCOLORIZER_H
