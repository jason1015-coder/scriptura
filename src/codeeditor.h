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
#include "multi-cursor.h"
#include "lspclient.h"

class CodeHighlighter : public QSyntaxHighlighter
{
public:
    explicit CodeHighlighter(QTextDocument *parent = nullptr);
    void setLanguage(const QString &language);
    void setDarkMode(bool dark);
    void setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                        const QColor &number, const QColor &preprocessor, const QColor &tag,
                        const QColor &attribute, const QColor &cssProperty,
                        const QColor &variable, const QColor &function, const QColor &escape,
                        const QColor &trailingSpace);

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
    void setupCStyle();
    void setupPython();
    void setupJava();
    void setupJavaScript();
    void setupTypeScript();
    void setupRust();
    void setupGo();
    void setupShell();
    void setupHtml();
    void setupCss();
    void setupScript();
    void setupPlainText();

    void handleCStyleBlockComment(const QString &text);
    void handlePythonTripleString(const QString &text);
    void handleHtmlComment(const QString &text);

    QString language;
    QVector<HighlightingRule> rules;
    enum BlockState { BlockNormal = 0, BlockInComment = 1, BlockInTripleDouble = 2, BlockInTripleSingle = 3, BlockInHtmlComment = 4 };
    bool darkMode = false;

    QTextCharFormat keywordFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat preprocessorFormat;
    QTextCharFormat tagFormat;
    QTextCharFormat attributeFormat;
    QTextCharFormat cssPropertyFormat;
    QTextCharFormat variableFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat escapeFormat;
    QTextCharFormat trailingSpaceFormat;
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
    void setInlayHints(const QList<LspClient::InlayHint> &hints);
    void setGhostText(const QString &text);
    void clearGhostText();
    QString ghostText() const { return m_ghostText; }

    void setBreakpointLine(int line, bool enabled);
    QSet<int> breakpointLines() const { return m_breakpointLines; }
    void clearBreakpoints();
    void highlightCurrentLine(int line);

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

    QWidget *lineNumberArea;
    CodeHighlighter *syntaxHighlighter;
    MultiCursorManager *m_multiCursor;
    bool m_showIndentGuides = true;
    int m_tabWidth = 4;
    QList<QTextEdit::ExtraSelection> m_diagnosticSelections;
    QList<QTextEdit::ExtraSelection> m_pluginExtraSelections;
    QList<QPair<QTextCursor, QString>> m_diagnosticTooltips;
    QList<QTextEdit::ExtraSelection> m_extraCursors;
    QList<LspClient::InlayHint> m_inlayHints;
    QPoint m_lastMousePos;
    bool m_hoveringDiagnostic = false;
    bool m_columnSelectionMode = false;
    QSet<int> m_breakpointLines;
    int m_currentDebugLine = -1;
    QString m_ghostText;
    bool m_acceptGhostText = false;
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
