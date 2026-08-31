#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QVector>
#include <QWidget>
#include <QColor>
#include <QEvent>
#include <QSet>
#include <QCache>
#include "multi-cursor.h"
#include "languageregistry.h"
#include "gitblame.h"
#include "codelensmanager.h"

// Standalone LSP types (formerly in LspClient)
struct LspPosition {
    int line = 0;
    int character = 0;
};

struct LspInlayHint {
    LspPosition position;
    QString label;
    bool paddingLeft = false;
    bool paddingRight = false;
};

class FoldManager;
class BracketColorizer;
class BookmarkManager;
class SnippetManager;
struct CodeLensItem;

class CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CodeHighlighter(QTextDocument *parent = nullptr);
    void setLanguage(const QString &language);
    void setDarkMode(bool dark);
    void setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                        const QColor &number, const QColor &preprocessor, const QColor &tag,
                        const QColor &attribute, const QColor &cssProperty,
                        const QColor &variable, const QColor &function, const QColor &escape,
                        const QColor &trailingSpace = QColor());

    QString currentLanguage() const { return m_language; }

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureIndex = 0;
    };

    void initializeFormats();
    void addRule(const QString &pattern, const QTextCharFormat &format, int captureIndex = 0);
    void rebuildRules();

    // Block-level handlers for multi-line constructs
    void handleCStyleBlockComment(const QString &text);
    void handlePythonTripleString(const QString &text);
    void handleHtmlComment(const QString &text);

    QString m_language;
    QString m_languageKey;  // Cache key for the current language
    QVector<HighlightingRule> m_rules;
    const LanguageDefinition *m_langDef = nullptr;

    enum BlockState { BlockNormal = 0, BlockInComment = 1, BlockInTripleDouble = 2, BlockInTripleSingle = 3, BlockInHtmlComment = 4 };
    bool m_darkMode = false;

    // Pre-built format cache for common languages
    struct LanguageFormats {
        QVector<HighlightingRule> rules;
        QString keywordPattern;
    };
    QCache<QString, LanguageFormats> m_formatCache;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_attributeFormat;
    QTextCharFormat m_cssPropertyFormat;
    QTextCharFormat m_variableFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_escapeFormat;
    QTextCharFormat m_trailingSpaceFormat;
};

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
    friend class LineNumberArea;
public:
    CodeEditor(QWidget *parent = nullptr);
    void setLanguageForFile(const QString &filePath);
    void setDarkMode(bool dark);
    void setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                        const QColor &number, const QColor &preprocessor, const QColor &tag,
                        const QColor &attribute, const QColor &cssProperty,
                        const QColor &variable, const QColor &function, const QColor &escape,
                        const QColor &trailingSpace = QColor());
    void setTabWidth(int spaces);
    int tabWidth() const;
    void setDiagnostics(const QList<QTextEdit::ExtraSelection> &diags);
    void setDiagnosticTooltips(const QList<QPair<QTextCursor, QString>> &tips);

    // Plugin extra selections (decorations) — rendered on top of diagnostics
    void setPluginExtraSelections(const QList<QTextEdit::ExtraSelection> &selections);
    QList<QTextEdit::ExtraSelection> pluginExtraSelections() const { return m_pluginExtraSelections; }
    void clearPluginExtraSelections();
    void setInlayHints(const QList<LspInlayHint> &hints);
    void setGhostText(const QString &text);
    void clearGhostText();
    QString ghostText() const { return m_ghostText; }

    void setBreakpointLine(int line, bool enabled);
    QSet<int> breakpointLines() const { return m_breakpointLines; }
    void clearBreakpoints();
    void highlightCurrentLine(int line);

    // Code folding
    FoldManager* foldManager() const { return m_foldManager; }

    // Bracket colorization
    BracketColorizer* bracketColorizer() const { return m_bracketColorizer; }
    void setBracketColorization(bool enabled);
    bool bracketColorizationEnabled() const { return m_bracketColorEnabled; }

    // Bookmarks
    BookmarkManager* bookmarkManager() const { return m_bookmarkManager; }

    // Snippets
    SnippetManager* snippetManager() const { return m_snippetManager; }

    // Code Lens
    void setCodeLensItems(const QList<CodeLensItem> &items) { m_codeLensItems = items; update(); }
    QList<CodeLensItem> codeLensItems() const { return m_codeLensItems; }

    // File path tracking (for bookmarks, file watcher, etc.)
    void setFilePath(const QString &path) { m_filePath = path; }
    QString filePath() const { return m_filePath; }

    // Git blame display
    void setBlameData(const QMap<int, BlameLineInfo> &data) { m_blameData = data; lineNumberArea->update(); }
    bool blameEnabled() const { return m_blameEnabled; }
    void setBlameEnabled(bool enabled) { m_blameEnabled = enabled; lineNumberArea->update(); }

    // Smart indentation
    void setSmartIndent(bool enabled) { m_smartIndent = enabled; }
    bool smartIndent() const { return m_smartIndent; }

    // Ctrl+D select next occurrence
    void selectNextOccurrence();
    void selectAllOccurrences();
    void addCursorAbove();
    void addCursorBelow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount = 0);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

signals:
    void breakpointToggled(int line, bool enabled);
    void ghostTextAccepted(const QString &text);

private:
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;
    void drawIndentGuides(QPaintEvent *event);
    void drawInlayHints(QPaintEvent *event);
    void drawGhostText(QPaintEvent *event);
    void updateHoverTooltip(const QPoint &pos);
    void updateAllSelections();
    void handleSmartIndent(QKeyEvent *event);
    // Returns true if the key was handled (auto-close performed). The caller
    // must NOT rely on event->isAccepted() afterwards: QKeyEvent arrives
    // accepted by default, so a void handler that silently does nothing would
    // make the caller swallow the typed character ("editor won't accept input").
    bool handleBracketAutoClose(QKeyEvent *event);

    QWidget *lineNumberArea;
    CodeHighlighter *syntaxHighlighter;
    MultiCursorManager *m_multiCursor;
    bool m_showIndentGuides = true;
    int m_tabWidth = 4;
    QList<QTextEdit::ExtraSelection> m_diagnosticSelections;
    QList<QTextEdit::ExtraSelection> m_pluginExtraSelections;
    QList<QPair<QTextCursor, QString>> m_diagnosticTooltips;
    QList<QTextEdit::ExtraSelection> m_extraCursors;
    QList<LspInlayHint> m_inlayHints;
    QPoint m_lastMousePos;
    bool m_hoveringDiagnostic = false;
    bool m_columnSelectionMode = false;
    QSet<int> m_breakpointLines;
    int m_currentDebugLine = -1;
    QString m_ghostText;
    bool m_acceptGhostText = false;

    // New features
    FoldManager *m_foldManager;
    BracketColorizer *m_bracketColorizer;
    BookmarkManager *m_bookmarkManager;
    SnippetManager *m_snippetManager;
    bool m_bracketColorEnabled = true;
    bool m_smartIndent = true;
    bool m_blameEnabled = false;
    QMap<int, BlameLineInfo> m_blameData;
    QString m_filePath;
    QList<CodeLensItem> m_codeLensItems;
};

class LineNumberArea : public QWidget
{
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
    {
        setObjectName("lineNumberArea");
    }

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

#endif // CODEEDITOR_H
