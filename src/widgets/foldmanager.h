#ifndef FOLDMANAGER_H
#define FOLDMANAGER_H

#include <QObject>
#include <QList>
#include <QTextBlock>
#include <QRegularExpression>
#include <QSet>

class QPlainTextEdit;

/**
 * Represents a foldable region in the code (function, class, block, etc.)
 */
struct FoldRegion {
    int startLine;      // Line number (0-based) of the opening brace/keyword
    int endLine;        // Line number (0-based) of the closing brace
    int indentLevel;    // Indentation level for nesting detection
    bool collapsed;     // Whether this region is currently folded
    bool valid;         // Whether this region is still valid
};

/**
 * Manages code folding regions for a QPlainTextEdit.
 * Detects foldable regions based on language-specific rules and brace matching.
 */
class FoldManager : public QObject
{
    Q_OBJECT
public:
    explicit FoldManager(QPlainTextEdit *editor, QObject *parent = nullptr);

    // Core operations
    void detectRegions();
    void toggleFold(int line);
    void foldAll();
    void unfoldAll();
    void foldAtLevel(int level);
    void unfoldAtLevel(int level);
    
    // Query
    bool isFolded(int line) const;
    bool isFoldStart(int line) const;
    bool isFoldEnd(int line) const;
    bool isRegionVisible(int line) const;
    int foldedLineCount() const;
    int visibleLineCount() const;
    
    // Get regions
    const QList<FoldRegion>& regions() const { return m_regions; }
    FoldRegion regionAt(int line) const;
    
    // Line visibility (for paint events)
    bool isLineHidden(int blockNumber) const;
    QSet<int> hiddenLines() const { return m_hiddenLines; }
    
    // Gutter icon
    int foldIndicatorWidth() const { return 20; }
    void paintFoldIndicator(QPainter &painter, int x, int y, int blockNumber, int blockHeight);

signals:
    void regionsChanged();
    void foldStateChanged(int line, bool collapsed);

private:
    void detectBraceFolds();
    void detectKeywordFolds();
    void updateHiddenLines();
    int findMatchingBrace(int line) const;
    int findMatchingBraceReverse(int line) const;
    int calculateIndent(const QString &line) const;
    
    QPlainTextEdit *m_editor;
    QList<FoldRegion> m_regions;
    QSet<int> m_hiddenLines;
    
    // Language-specific fold keywords
    QRegularExpression m_foldStartPattern;
    QRegularExpression m_foldEndPattern;
    bool m_useBraceFolding;
};

#endif // FOLDMANAGER_H
