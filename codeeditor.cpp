#include "codeeditor.h"

#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextOption>

namespace {

QString languageForFile(const QString &filePath)
{
    const QString extension = QFileInfo(filePath).suffix().toLower();

    if (extension == "py" || extension == "pyw")
        return "python";
    if (extension == "c")
        return "c";
    if (extension == "cpp" || extension == "cc" || extension == "cxx" || extension == "h" || extension == "hh" || extension == "hpp" || extension == "hxx")
        return "cpp";
    if (extension == "java")
        return "java";
    if (extension == "js" || extension == "jsx" || extension == "mjs")
        return "javascript";
    if (extension == "ts" || extension == "tsx")
        return "typescript";
    if (extension == "rs")
        return "rust";
    if (extension == "go")
        return "go";
    if (extension == "sh" || extension == "bash" || extension == "zsh")
        return "shell";
    if (extension == "html" || extension == "htm")
        return "html";
    if (extension == "css" || extension == "scss" || extension == "sass" || extension == "less")
        return "css";
    if (extension == "scr")
        return "script";

    return "text";
}

}

CodeHighlighter::CodeHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    initializeFormats();
    setLanguage("text");
}

void CodeHighlighter::setLanguage(const QString &newLanguage)
{
    language = newLanguage.toLower();
    rules.clear();
    inBlockComment = false;
    inTripleString = false;
    inHtmlComment = false;
    tripleDelimiter.clear();

    if (language == "python")
        setupPython();
    else if (language == "c" || language == "cpp")
        setupCStyle();
    else if (language == "java")
        setupJava();
    else if (language == "javascript")
        setupJavaScript();
    else if (language == "typescript")
        setupTypeScript();
    else if (language == "rust")
        setupRust();
    else if (language == "go")
        setupGo();
    if (language == "shell")
        setupShell();
    else if (language == "html")
        setupHtml();
    else if (language == "css")
        setupCss();
    else if (language == "script")
        setupScript();
    else
        setupPlainText();

    rehighlight();
}

void CodeHighlighter::setDarkMode(bool dark)
{
    if (darkMode == dark)
        return;
    darkMode = dark;
    initializeFormats();
    setLanguage(language);
}

void CodeHighlighter::setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                                   const QColor &number, const QColor &preprocessor, const QColor &tag,
                                   const QColor &attribute, const QColor &cssProperty,
                                   const QColor &variable, const QColor &function, const QColor &escape,
                                   const QColor &trailingSpace)
{
    keywordFormat.setForeground(keyword);
    keywordFormat.setFontWeight(QFont::Bold);

    stringFormat.setForeground(string);

    commentFormat.setForeground(comment);
    commentFormat.setFontItalic(true);

    numberFormat.setForeground(number);

    preprocessorFormat.setForeground(preprocessor);
    preprocessorFormat.setFontWeight(QFont::Bold);

    tagFormat.setForeground(tag);
    tagFormat.setFontWeight(QFont::Bold);

    attributeFormat.setForeground(attribute);

    this->cssPropertyFormat.setForeground(cssProperty);
    this->cssPropertyFormat.setFontWeight(QFont::Bold);

    this->variableFormat.setForeground(variable);
    this->variableFormat.setFontWeight(QFont::Bold);

    this->functionFormat.setForeground(function);
    this->functionFormat.setFontWeight(QFont::Bold);

    escapeFormat.setForeground(escape);

    trailingSpaceFormat.setBackground(trailingSpace);

    setLanguage(language);
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    // Performance optimization: skip highlighting for very long lines
    if (text.length() > 10000) {
        return;
    }

    if (language == "c" || language == "cpp" || language == "java" || language == "javascript" || language == "typescript" || language == "rust" || language == "go" || language == "css" || language == "script") {
        handleCStyleBlockComment(text);
        if (inBlockComment)
            return;
    }

    if (language == "html" && inHtmlComment) {
        handleHtmlComment(text);
        if (inHtmlComment)
            return;
    }

    for (const HighlightingRule &rule : rules) {
        QRegularExpressionMatchIterator iterator = rule.pattern.globalMatch(text);
        while (iterator.hasNext()) {
            QRegularExpressionMatch match = iterator.next();
            const int start = match.capturedStart(rule.captureIndex);
            const int length = match.capturedLength(rule.captureIndex);
            if (start >= 0)
                setFormat(start, length, rule.format);
        }
    }

    if (language == "html")
        handleHtmlComment(text);

    if (language == "python")
        handlePythonTripleString(text);
}

void CodeHighlighter::initializeFormats()
{
    if (darkMode) {
        keywordFormat.setForeground(QColor("#93c5fd"));
        keywordFormat.setFontWeight(QFont::Bold);

        stringFormat.setForeground(QColor("#86efac"));

        commentFormat.setForeground(QColor("#94a3b8"));
        commentFormat.setFontItalic(true);

        numberFormat.setForeground(QColor("#c084fc"));

        preprocessorFormat.setForeground(QColor("#a855f7"));
        preprocessorFormat.setFontWeight(QFont::Bold);

        tagFormat.setForeground(QColor("#60a5fa"));
        tagFormat.setFontWeight(QFont::Bold);

        attributeFormat.setForeground(QColor("#fbbf24"));

        cssPropertyFormat.setForeground(QColor("#2dd4bf"));
        cssPropertyFormat.setFontWeight(QFont::Bold);

        variableFormat.setForeground(QColor("#38bdf8"));
        variableFormat.setFontWeight(QFont::Bold);

        functionFormat.setForeground(QColor("#f97316"));
        functionFormat.setFontWeight(QFont::Bold);

        escapeFormat.setForeground(QColor("#22d3ee"));

        trailingSpaceFormat.setBackground(QColor("#7f1d1d"));
    } else {
        keywordFormat.setForeground(QColor("#1d4ed8"));
        keywordFormat.setFontWeight(QFont::Bold);

        stringFormat.setForeground(QColor("#15803d"));

        commentFormat.setForeground(QColor("#64748b"));
        commentFormat.setFontItalic(true);

        numberFormat.setForeground(QColor("#9333ea"));

        preprocessorFormat.setForeground(QColor("#7e22ce"));
        preprocessorFormat.setFontWeight(QFont::Bold);

        tagFormat.setForeground(QColor("#2563eb"));
        tagFormat.setFontWeight(QFont::Bold);

        attributeFormat.setForeground(QColor("#a16207"));

        cssPropertyFormat.setForeground(QColor("#0f766e"));
        cssPropertyFormat.setFontWeight(QFont::Bold);

        variableFormat.setForeground(QColor("#0369a1"));
        variableFormat.setFontWeight(QFont::Bold);

        functionFormat.setForeground(QColor("#d97706"));
        functionFormat.setFontWeight(QFont::Bold);

        escapeFormat.setForeground(QColor("#0e7490"));

        trailingSpaceFormat.setBackground(QColor("#fecaca"));
    }
}

void CodeHighlighter::addRule(const QString &pattern, const QTextCharFormat &format, int captureIndex)
{
    HighlightingRule rule;
    rule.pattern = QRegularExpression(pattern);
    rule.format = format;
    rule.captureIndex = captureIndex;
    rules.append(rule);
}

void CodeHighlighter::setupCStyle()
{
    addRule("\\b(alignas|alignof|and|and_eq|asm|auto|bitand|bitor|break|case|catch|char|char8_t|char16_t|char32_t|class|compl|concept|const|consteval|constexpr|constinit|const_cast|continue|co_await|co_return|co_yield|decltype|default|delete|do|double|dynamic_cast|else|enum|explicit|export|extern|false|for|friend|goto|if|inline|int|long|mutable|namespace|new|noexcept|not|not_eq|nullptr|operator|or|or_eq|private|protected|public|register|reinterpret_cast|requires|return|short|signed|sizeof|static|static_assert|static_cast|struct|switch|template|this|thread_local|throw|true|try|typedef|typeid|typename|union|unsigned|using|virtual|void|volatile|while|xor|xor_eq)\\b", keywordFormat);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!switch\\b)(?!catch\\b)(?!return\\b)(?!sizeof\\b)(?!typeof\\b)(?!new\\b)(?!delete\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\bint\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bfloat\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bdouble\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bchar\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bbool\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bauto\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bString\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?(?:[fFlLuU]|ll|LL)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("#\\s*\\w+.*", preprocessorFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupPython()
{
    addRule("\\b(False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\\b", keywordFormat);
    addRule("^\\s*def\\s+([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\b(print|len|range|str|int|float|list|dict|set|tuple|open|sum|enumerate|zip|map|filter|sorted|reversed|abs|round|isinstance|issubclass|super|property|staticmethod|classmethod)\\b", variableFormat);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!return\\b)(?!print\\b)(?!len\\b)(?!range\\b)(?!str\\b)(?!int\\b)(?!float\\b)(?!list\\b)(?!dict\\b)(?!set\\b)(?!tuple\\b)(?!open\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("^\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("#[^\\n]*", commentFormat);
}

void CodeHighlighter::setupJava()
{
    addRule("\\b(abstract|assert|boolean|break|byte|case|catch|char|class|const|continue|default|do|double|else|enum|extends|final|finally|float|for|goto|if|implements|import|instanceof|int|interface|long|native|new|package|private|protected|public|return|short|static|strictfp|super|switch|synchronized|this|throw|throws|transient|try|void|volatile|while|var|record|sealed|permits|yields)\\b", keywordFormat);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!switch\\b)(?!catch\\b)(?!return\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\bint\\s+(?:final\\s+)?(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bString\\s+(?:final\\s+)?(?:\\*\\s*)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bvar\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("=\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule(",\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("\\(\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?[fFdDlL]?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupJavaScript()
{
    addRule("\\b(async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|false|finally|for|function|if|import|in|instanceof|let|new|null|of|return|super|switch|this|throw|true|try|typeof|undefined|var|void|while|with|yield|static|get|set)\\b", keywordFormat);
    addRule("\\b\\d+(?:\\.\\d+)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!switch\\b)(?!catch\\b)(?!return\\b)(?!typeof\\b)(?!delete\\b)(?!void\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\blet\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bconst\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bvar\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("=\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule(",\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("\\(\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("`(?:\\\\.|[^`\\\\])*`", stringFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupTypeScript()
{
    addRule("\\b(abstract|any|as|async|await|boolean|break|case|catch|class|const|continue|debugger|declare|default|delete|do|else|enum|export|extends|false|finally|for|from|function|get|if|implements|import|in|instanceof|interface|keyof|let|module|namespace|new|null|number|object|of|private|protected|public|readonly|return|set|static|string|super|switch|symbol|this|throw|true|try|type|typeof|undefined|unknown|var|void|while|with|yield)\\b", keywordFormat);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!switch\\b)(?!catch\\b)(?!return\\b)(?!typeof\\b)(?!delete\\b)(?!void\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\blet\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bconst\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("\\bvar\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("=\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule(",\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("\\(\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("`(?:\\\\.|[^`\\\\])*`", stringFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupRust()
{
    addRule("\\b(as|async|await|break|const|continue|crate|dyn|else|enum|extern|false|fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|self|Self|static|struct|super|trait|true|type|unsafe|use|where|while)\\b", keywordFormat);
    addRule("\\bfn\\s+([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!match\\b)(?!loop\\b)(?!return\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\blet\\s+(?:mut\\s+)?([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("=\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule(",\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("\\(\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:_\\d+)*(?:\\.\\d+(?:_\\d+)*)?(?:[eE][+-]?\\d+)?(?:f32|f64|i8|i16|i32|i64|i128|isize|u8|u16|u32|u64|u128|usize)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupGo()
{
    addRule("\\b(break|default|func|interface|select|case|defer|go|map|struct|chan|else|goto|package|switch|const|fallthrough|if|range|type|continue|for|import|return|var)\\b", keywordFormat);
    addRule("\\bfunc\\s+(?:\\([^)]*\\)\\s*)?([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\b(?!if\\b)(?!for\\b)(?!range\\b)(?!switch\\b)(?!select\\b)(?!defer\\b)(?!go\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    addRule("\\bvar\\s+([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("=\\s*([A-Za-z_]\\w*)\\s*(?==)", variableFormat, 1);
    addRule(":\\s*=\\s*([A-Za-z_]\\w*)", variableFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?(?:[iI]|[eE][+-]?\\d+)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
    addRule("`[^`]*`", stringFormat);
    addRule("//[^\\n]*", commentFormat);
}

void CodeHighlighter::setupShell()
{
    addRule("\\b(if|then|else|elif|fi|for|while|do|done|case|esac|function|select|in|time|coproc)\\b", keywordFormat);
    addRule("\\b\\d+\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("^\\s*([A-Za-z_]\\w*)\\s*\\(\\s*\\)", functionFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("'[^']*'", stringFormat);
    addRule("#[^\\n]*", commentFormat);
    addRule("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?", variableFormat);
}

void CodeHighlighter::setupHtml()
{
    addRule("<\\s*/?\\s*([A-Za-z][A-Za-z0-9:-]*)", tagFormat, 1);
    addRule("\\s([A-Za-z_:][A-Za-z0-9:_.-]*)\\s*=", attributeFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
}

void CodeHighlighter::setupCss()
{
    addRule("\\b(color|background|background-color|margin|padding|display|position|top|right|bottom|left|width|height|min-width|max-width|min-height|max-height|font|font-size|font-weight|font-family|flex|grid|gap|border|border-radius|overflow|z-index|cursor|opacity|transform|transition|box-shadow|text-align|line-height)\\s*:", cssPropertyFormat, 1);
    addRule("\\b(rgb|hsl|calc|var|clamp|min|max)\\s*(?=\\()", functionFormat, 1);
    addRule("[ \\t]+$", trailingSpaceFormat);
    addRule("\\b\\d+(?:\\.\\d+)?(?:px|em|rem|%|vh|vw|s|ms)?\\b", numberFormat);
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    addRule("'(?:\\\\.|[^'\\\\])*'", stringFormat);
}

void CodeHighlighter::setupScript()
{
    // Script language keywords: print, let, var, true, false
    addRule("\\b(print|let|var|true|false)\\b", keywordFormat);
    // Function calls (print is already highlighted as keyword)
    addRule("\\b(?!print\\b)(?!let\\b)(?!var\\b)(?!true\\b)(?!false\\b)([A-Za-z_]\\w*)\\s*(?=\\()", functionFormat, 1);
    // Variable declarations
    addRule("\\b(let|var)\\s+([A-Za-z_]\\w*)", variableFormat, 2);
    // Identifiers
    addRule("\\b([A-Za-z_]\\w*)\\b", variableFormat);
    // Numbers
    addRule("\\b\\d+(?:\\.\\d+)?\\b", numberFormat);
    // Strings
    addRule("\"(?:\\\\.|[^\"\\\\])*\"", stringFormat);
    // Comments
    addRule("#[^\\n]*", commentFormat);
    // Trailing spaces
    addRule("[ \\t]+$", trailingSpaceFormat);
}

void CodeHighlighter::setupPlainText()
{
}

void CodeHighlighter::handleCStyleBlockComment(const QString &text)
{
    int index = 0;

    if (inBlockComment) {
        index = text.indexOf("*/");
        if (index >= 0) {
            setFormat(0, index + 2, commentFormat);
            inBlockComment = false;
            index = 0;
        } else {
            setFormat(0, text.length(), commentFormat);
            return;
        }
    }

    while ((index = text.indexOf("/*", index)) >= 0) {
        const int start = index;
        const int end = text.indexOf("*/", index + 2);
        int length;

        if (end >= 0) {
            length = end - start + 2;
            index = end + 2;
        } else {
            length = text.length() - start;
            inBlockComment = true;
        }

        setFormat(start, length, commentFormat);

        if (inBlockComment)
            return;
    }
}

void CodeHighlighter::handlePythonTripleString(const QString &text)
{
    if (inTripleString) {
        const int end = text.indexOf(tripleDelimiter);
        if (end >= 0) {
            setFormat(0, end + 3, stringFormat);
            inTripleString = false;
            tripleDelimiter.clear();
        } else {
            setFormat(0, text.length(), stringFormat);
            return;
        }
    }

    QRegularExpressionMatchIterator iterator = QRegularExpression("\"\"\"|'''").globalMatch(text);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        const QString delimiter = match.captured(0);
        const int matchStart = match.capturedStart();
        const int end = text.indexOf(delimiter, matchStart + 3);

        if (end >= 0) {
            setFormat(matchStart, end + 3 - matchStart, stringFormat);
        } else {
            setFormat(matchStart, text.length() - matchStart, stringFormat);
            inTripleString = true;
            tripleDelimiter = delimiter;
            return;
        }
    }
}

void CodeHighlighter::handleHtmlComment(const QString &text)
{
    if (inHtmlComment) {
        const int end = text.indexOf("-->");
        if (end >= 0) {
            setFormat(0, end + 3, commentFormat);
            inHtmlComment = false;
        } else {
            setFormat(0, text.length(), commentFormat);
            return;
        }
    }

    int index = 0;
    while ((index = text.indexOf("<!--", index)) >= 0) {
        const int start = index;
        const int end = text.indexOf("-->", index + 4);
        int length;

        if (end >= 0) {
            length = end + 4 - start;
            index = end + 4;
        } else {
            length = text.length() - start;
            inHtmlComment = true;
        }

        setFormat(start, length, commentFormat);

        if (inHtmlComment)
            return;
    }
}

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , lineNumberArea(new LineNumberArea(this))
    , syntaxHighlighter(new CodeHighlighter(document()))
    , m_tabWidth(4)
{
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    QTextOption option = document()->defaultTextOption();
    option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
    option.setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    document()->setDefaultTextOption(option);
}

void CodeEditor::setLanguageForFile(const QString &filePath)
{
    syntaxHighlighter->setLanguage(languageForFile(filePath));
}

void CodeEditor::setDarkMode(bool dark)
{
    syntaxHighlighter->setDarkMode(dark);
    highlightCurrentLine();
}

void CodeEditor::setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                                const QColor &number, const QColor &preprocessor, const QColor &tag,
                                const QColor &attribute, const QColor &cssProperty,
                                const QColor &variable, const QColor &function, const QColor &escape,
                                const QColor &trailingSpace)
{
    syntaxHighlighter->setThemeColors(keyword, string, comment, number, preprocessor, tag,
                                       attribute, cssProperty, variable, function, escape, trailingSpace);
    highlightCurrentLine();
}

void CodeEditor::setTabWidth(int spaces)
{
    m_tabWidth = spaces;
    QTextOption option = document()->defaultTextOption();
    option.setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_tabWidth);
    document()->setDefaultTextOption(option);
}

int CodeEditor::tabWidth() const
{
    return m_tabWidth;
}

void CodeEditor::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        QTextOption option = document()->defaultTextOption();
        option.setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_tabWidth);
        document()->setDefaultTextOption(option);
    }
}

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);
    drawIndentGuides(event);
}

void CodeEditor::drawIndentGuides(QPaintEvent *event)
{
    if (!m_showIndentGuides)
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(128, 128, 128, 80), 1, Qt::SolidLine));

    qreal tabStop = document()->defaultTextOption().tabStopDistance();
    if (tabStop <= 0)
        tabStop = fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4;

    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString text = block.text();
            if (!text.isEmpty() && !text.trimmed().isEmpty()) {
                qreal width = 0;
                for (const QChar &c : text) {
                    if (c == QLatin1Char(' ')) {
                        width += fontMetrics().horizontalAdvance(QLatin1Char(' '));
                    } else if (c == QLatin1Char('\t')) {
                        width = (qFloor(width / tabStop) + 1) * tabStop;
                    } else {
                        break;
                    }
                }

                for (qreal pos = tabStop; pos <= width + 0.1; pos += tabStop) {
                    int guideX = static_cast<int>(pos) + static_cast<int>(contentOffset().x());
                    painter.drawLine(guideX, static_cast<int>(top),
                                   guideX, static_cast<int>(bottom));
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
    }
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        digits++;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = palette().color(QPalette::Highlight);
        lineColor.setAlpha(40);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                            Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::setDiagnostics(const QList<QTextEdit::ExtraSelection> &diags)
{
    m_diagnosticSelections = diags;
    // Merge with current extra selections (current line highlight)
    QList<QTextEdit::ExtraSelection> selections;
    QList<QTextEdit::ExtraSelection> existing = extraSelections();

    // Keep current line highlight
    for (const QTextEdit::ExtraSelection &sel : existing) {
        if (sel.format.hasProperty(QTextFormat::FullWidthSelection)) {
            selections.append(sel);
            break;
        }
    }

    selections.append(diags);
    setExtraSelections(selections);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    updateHoverTooltip(event->pos());
    QPlainTextEdit::mouseMoveEvent(event);
}

void CodeEditor::leaveEvent(QEvent *event)
{
    m_hoveringDiagnostic = false;
    QPlainTextEdit::leaveEvent(event);
}

bool CodeEditor::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent*>(event);
        updateHoverTooltip(helpEvent->pos());
        if (m_hoveringDiagnostic)
            return true;
    }
    return QPlainTextEdit::event(event);
}

void CodeEditor::updateHoverTooltip(const QPoint &pos)
{
    // Check if mouse is over a diagnostic selection
    QTextCursor cursor = cursorForPosition(pos);
    int cursorPos = cursor.position();

    for (const QTextEdit::ExtraSelection &sel : m_diagnosticSelections) {
        if (sel.cursor.selectionStart() <= cursorPos && cursorPos <= sel.cursor.selectionEnd()) {
            // Could show diagnostic message in tooltip here
            // For now, just track that we're hovering
            m_hoveringDiagnostic = true;
            return;
        }
    }
    m_hoveringDiagnostic = false;
}
