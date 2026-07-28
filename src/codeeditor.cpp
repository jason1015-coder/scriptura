#include "codeeditor.h"
#include "foldmanager.h"
#include "bracketcolorizer.h"
#include "bookmarkmanager.h"
#include "snippetmanager.h"
#include "codelensmanager.h"
#include "rust_backend.h"

#include <cmath>
#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QTextBlock>
#include <QTextFormat>
#include <QToolTip>
#include <QTextOption>
#include <QPixmap>
#include <algorithm>

// -----------------------------------------------------------------------
// Helper patterns used by the highlighting rule builder
// -----------------------------------------------------------------------

namespace {

inline QString functionCallPattern()
{
    return "\\b(?!if\\b)(?!for\\b)(?!while\\b)(?!switch\\b)(?!catch\\b)(?!return\\b)(?!sizeof\\b)(?!typeof\\b)(?!new\\b)(?!delete\\b)(?!void\\b)(?!print\\b)([A-Za-z_]\\w*)\\s*(?=\\()";
}

inline QString numberPattern(const QString &suffix = "")
{
    return "\\b\\d+(?:\\.\\d+)?" + suffix + "\\b";
}

} // anonymous namespace

// -----------------------------------------------------------------------
// CodeHighlighter - uses LanguageRegistry for data-driven definitions
// -----------------------------------------------------------------------

CodeHighlighter::CodeHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_formatCache(64)  // Cache up to 64 language format sets (covers 2 modes × ~30 languages)
{
    // Ensure the LanguageRegistry is populated
    LanguageRegistry::instance();
    initializeFormats();
    setLanguage("text");
}

void CodeHighlighter::setLanguage(const QString &newLanguage)
{
    const QString lang = newLanguage.toLower();
    if (m_language == lang)
        return;

    m_language = lang;
    m_langDef = LanguageRegistry::instance().findByName(lang);

    if (!m_langDef && lang != "text") {
        m_langDef = LanguageRegistry::instance().findByName("text");
    }

    rebuildRules();
    rehighlight();
}

void CodeHighlighter::setDarkMode(bool dark)
{
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    initializeFormats();
    if (m_langDef || m_language == "text")
        rebuildRules();
    rehighlight();
}

void CodeHighlighter::setThemeColors(const QColor &keyword, const QColor &string, const QColor &comment,
                                     const QColor &number, const QColor &preprocessor, const QColor &tag,
                                     const QColor &attribute, const QColor &cssProperty,
                                     const QColor &variable, const QColor &function, const QColor &escape,
                                     const QColor &trailingSpace)
{
    m_keywordFormat.setForeground(keyword);
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_stringFormat.setForeground(string);

    m_commentFormat.setForeground(comment);
    m_commentFormat.setFontItalic(true);

    m_numberFormat.setForeground(number);

    m_preprocessorFormat.setForeground(preprocessor);
    m_preprocessorFormat.setFontWeight(QFont::Bold);

    m_tagFormat.setForeground(tag);
    m_tagFormat.setFontWeight(QFont::Bold);

    m_attributeFormat.setForeground(attribute);

    m_cssPropertyFormat.setForeground(cssProperty);
    m_cssPropertyFormat.setFontWeight(QFont::Bold);

    m_variableFormat.setForeground(variable);
    m_variableFormat.setFontWeight(QFont::Bold);

    m_functionFormat.setForeground(function);
    m_functionFormat.setFontWeight(QFont::Bold);

    m_escapeFormat.setForeground(escape);

    m_trailingSpaceFormat.setBackground(trailingSpace);

    rebuildRules();
    rehighlight();
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    // Performance: skip very long blocks (>10000 chars) to avoid freezing
    if (text.length() > 10000) {
        setCurrentBlockState(BlockNormal);
        return;
    }

    const bool hasCStyle = m_langDef && m_langDef->hasCStyleComments;
    const bool hasHtml = m_langDef && m_langDef->hasHtmlComments;
    const bool hasPyTriple = m_langDef && m_langDef->hasPythonTripleStrings;

    // Handle multi-line constructs first
    if (hasCStyle && (previousBlockState() == BlockInComment || text.contains("/*"))) {
        handleCStyleBlockComment(text);
        if (currentBlockState() == BlockInComment)
            return;
    }

    if (hasHtml && (previousBlockState() == BlockInHtmlComment || text.contains("<!--"))) {
        handleHtmlComment(text);
        if (currentBlockState() == BlockInHtmlComment)
            return;
    }

    if (hasPyTriple && (previousBlockState() == BlockInTripleDouble || previousBlockState() == BlockInTripleSingle || text.contains("\"\"\"") || text.contains("'''"))) {
        handlePythonTripleString(text);
        if (currentBlockState() != BlockNormal)
            return;
    }

    // Apply all highlighting rules
    if (text.isEmpty())
        return;

    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator iterator = rule.pattern.globalMatch(text);
        while (iterator.hasNext()) {
            QRegularExpressionMatch match = iterator.next();
            const int start = match.capturedStart(rule.captureIndex);
            const int length = match.capturedLength(rule.captureIndex);
            if (start >= 0 && length > 0)
                setFormat(start, length, rule.format);
        }
    }

    // Handle HTML comments that start mid-block
    if (hasHtml && previousBlockState() == BlockInHtmlComment) {
        handleHtmlComment(text);
    }
}

void CodeHighlighter::initializeFormats()
{
    if (m_darkMode) {
        m_keywordFormat.setForeground(QColor("#93c5fd"));
        m_keywordFormat.setFontWeight(QFont::Bold);

        m_stringFormat.setForeground(QColor("#86efac"));

        m_commentFormat.setForeground(QColor("#94a3b8"));
        m_commentFormat.setFontItalic(true);

        m_numberFormat.setForeground(QColor("#c084fc"));

        m_preprocessorFormat.setForeground(QColor("#a855f7"));
        m_preprocessorFormat.setFontWeight(QFont::Bold);

        m_tagFormat.setForeground(QColor("#60a5fa"));
        m_tagFormat.setFontWeight(QFont::Bold);

        m_attributeFormat.setForeground(QColor("#fbbf24"));

        m_cssPropertyFormat.setForeground(QColor("#2dd4bf"));
        m_cssPropertyFormat.setFontWeight(QFont::Bold);

        m_variableFormat.setForeground(QColor("#38bdf8"));
        m_variableFormat.setFontWeight(QFont::Bold);

        m_functionFormat.setForeground(QColor("#f97316"));
        m_functionFormat.setFontWeight(QFont::Bold);

        m_escapeFormat.setForeground(QColor("#22d3ee"));

        m_trailingSpaceFormat.setBackground(QColor("#7f1d1d"));
    } else {
        m_keywordFormat.setForeground(QColor("#1d4ed8"));
        m_keywordFormat.setFontWeight(QFont::Bold);

        m_stringFormat.setForeground(QColor("#15803d"));

        m_commentFormat.setForeground(QColor("#64748b"));
        m_commentFormat.setFontItalic(true);

        m_numberFormat.setForeground(QColor("#9333ea"));

        m_preprocessorFormat.setForeground(QColor("#7e22ce"));
        m_preprocessorFormat.setFontWeight(QFont::Bold);

        m_tagFormat.setForeground(QColor("#2563eb"));
        m_tagFormat.setFontWeight(QFont::Bold);

        m_attributeFormat.setForeground(QColor("#a16207"));

        m_cssPropertyFormat.setForeground(QColor("#0f766e"));
        m_cssPropertyFormat.setFontWeight(QFont::Bold);

        m_variableFormat.setForeground(QColor("#0369a1"));
        m_variableFormat.setFontWeight(QFont::Bold);

        m_functionFormat.setForeground(QColor("#d97706"));
        m_functionFormat.setFontWeight(QFont::Bold);

        m_escapeFormat.setForeground(QColor("#0e7490"));

        m_trailingSpaceFormat.setBackground(QColor("#fecaca"));
    }
}

void CodeHighlighter::addRule(const QString &pattern, const QTextCharFormat &format, int captureIndex)
{
    HighlightingRule rule;
    rule.pattern = QRegularExpression(pattern);
    rule.format = format;
    rule.captureIndex = captureIndex;
    m_rules.append(rule);
}

void CodeHighlighter::rebuildRules()
{
    m_rules.clear();
    m_languageKey = m_language + (m_darkMode ? "_dark" : "_light");

    // Check cache first
    if (const LanguageFormats *cached = m_formatCache.object(m_languageKey)) {
        m_rules = cached->rules;
        return;
    }

    // Always add trailing space rule
    addRule("[ \\t]+$", m_trailingSpaceFormat);

    if (!m_langDef || m_language == "text") {
        // No rules for plain text beyond trailing spaces
        auto *formats = new LanguageFormats();
        formats->rules = m_rules;
        m_formatCache.insert(m_languageKey, formats);
        return;
    }

    const QString &lang = m_language;

    // ---- Keywords ----
    if (!m_langDef->keywords.isEmpty()) {
        const QString kwPatternStr = "\\b(" + m_langDef->keywords.join("|") + ")\\b";
        addRule(kwPatternStr, m_keywordFormat);
    }

    // ---- Builtins as variable format ----
    if (!m_langDef->builtins.isEmpty()) {
        const QString builtinPattern = "\\b(" + m_langDef->builtins.join("|") + ")\\b";
        addRule(builtinPattern, m_variableFormat);
    }

    // ---- Language-specific rules ----
    if (lang == "python") {
        addRule("^\\s*def\\s+([A-Za-z_]\\w*)\\s*(?=\\()", m_functionFormat, 1);
        addRule("^\\s*class\\s+([A-Za-z_]\\w*)", m_keywordFormat, 1);
        addRule("^\\s*([A-Za-z_]\\w*)\\s*(?==)", m_variableFormat, 1);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    } else if (lang == "cpp" || lang == "c") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(int|long|float|double|char|bool|auto|void|short|signed|unsigned|size_t|ssize_t|int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|wchar_t|char16_t|char32_t|String|QString)\\s+(?:\\*\\s*)?([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern("(?:[fFlLuU]|ll|LL)?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#\\s*\\w+.*", m_preprocessorFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "java") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(int|long|float|double|char|boolean|byte|short|String)\\s+(?:final\\s+)?(?:\\*\\s*)?([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule("\\bvar\\s+([A-Za-z_]\\w*)", m_variableFormat, 1);
        addRule(numberPattern("[fFdDlL]?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "javascript" || lang == "typescript") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(let|const|var)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        if (m_langDef->templateStringDelimiter == "`") {
            addRule("`(?:\\\\.|[^`\\\\])*`", m_stringFormat);
        }
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "rust") {
        addRule("\\bfn\\s+([A-Za-z_]\\w*)\\s*(?=\\()", m_functionFormat, 1);
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\blet\\s+(?:mut\\s+)?([A-Za-z_]\\w*)", m_variableFormat, 1);
        addRule(numberPattern("(?:_\\d+)*(?:\\.\\d+(?:_\\d+)*)?(?:[eE][+-]?\\d+)?(?:f32|f64|i8|i16|i32|i64|i128|isize|u8|u16|u32|u64|u128|usize)?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "go") {
        addRule("\\bfunc\\s+(?:\\([^)]*\\)\\s*)?([A-Za-z_]\\w*)\\s*(?=\\()", m_functionFormat, 1);
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\bvar\\s+([A-Za-z_]\\w*)", m_variableFormat, 1);
        addRule(":\\s*=\\s*([A-Za-z_]\\w*)", m_variableFormat, 1);
        addRule(numberPattern("(?:[iI]|[eE][+-]?\\d+)?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("`[^`]*`", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "shell") {
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("^\\s*([A-Za-z_]\\w*)\\s*\\(\\s*\\)", m_functionFormat, 1);
        addRule("'[^']*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
        addRule("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?", m_variableFormat);
    } else if (lang == "swift") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(func|var|let)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "kotlin") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(fun|val|var|lateinit\\s+var)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "ruby") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(def|alias)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    } else if (lang == "php") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\$(\\w+)", m_variableFormat);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
        addRule("#[^\\n]*", m_commentFormat);  // PHP also uses # for comments
    } else if (lang == "csharp") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(int|long|float|double|char|bool|byte|short|decimal|string|var|object|dynamic)\\s+(\\w+)", m_variableFormat, 2);
        addRule(numberPattern("[fFdDmMlL]?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "dart") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(var|final|const|late)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "lua") {
        addRule("\\bfunction\\s+(?:[A-Za-z_]\\w*\\s*\\.\\s*)?([A-Za-z_]\\w*)\\s*(?=\\()", m_functionFormat, 1);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("\\[\\[.*?\\]\\]", m_stringFormat);
        addRule("--\\[\\[.*?\\]\\]", m_commentFormat);
        addRule("--[^\\n]*", m_commentFormat);
    } else if (lang == "r") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    } else if (lang == "scala") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(var|val|def|val\\s+lazy)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "objectivec") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("@\\w+", m_preprocessorFormat);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "yaml" || lang == "toml") {
        addRule("#[^\\n]*", m_commentFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("^\\s*\\w+(?=\\s*:)", m_keywordFormat);
        addRule(numberPattern(), m_numberFormat);
    } else if (lang == "json") {
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule(numberPattern(), m_numberFormat);
        addRule("\\b(true|false|null)\\b", m_keywordFormat);
        addRule("//[^\\n]*", m_commentFormat);
    } else if (lang == "markdown") {
        addRule("^#{1,6}\\s.*$", m_keywordFormat);
        addRule("\\*\\*\\S(?:.*?\\S)?\\*\\*", m_stringFormat);
        addRule("__(?:.*?)__", m_stringFormat);
        addRule("\\*(?:.*?)\\*", m_commentFormat);
        addRule("`[^`]+`", m_numberFormat);
        addRule("^\\s*[-*+]\\s", m_variableFormat);
        addRule("^\\s*\\d+\\.\\s", m_numberFormat);
    } else if (lang == "sql") {
        addRule("--[^\\n]*", m_commentFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule(numberPattern(), m_numberFormat);
    } else if (lang == "perl") {
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\$\\{?\\w+\\}?", m_variableFormat);
        addRule("@\\w+", m_variableFormat);
        addRule("%\\w+", m_variableFormat);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    } else if (lang == "haskell") {
        addRule("^[A-Z][A-Za-z_0-9]*\\s*(?=::)", m_functionFormat);
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'[^']*'", m_stringFormat);
        addRule("--[^\\n]*", m_commentFormat);
    } else if (lang == "elixir") {
        addRule("\\bdef(?:p|macro|guard|guardp)?\\s+([A-Za-z_]\\w*)", m_functionFormat, 1);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    } else if (lang == "html") {
        addRule("<\\s*/?\\s*([A-Za-z][A-Za-z0-9:-]*)", m_tagFormat, 1);
        addRule("\\s([A-Za-z_:][A-Za-z0-9:_.-]*)\\s*=", m_attributeFormat, 1);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
    } else if (lang == "css") {
        const QStringList cssProps = {"color", "background", "background-color", "margin",
            "padding", "display", "position", "top", "right", "bottom", "left",
            "width", "height", "min-width", "max-width", "min-height", "max-height",
            "font", "font-size", "font-weight", "font-family", "flex", "grid", "gap",
            "border", "border-radius", "overflow", "z-index", "cursor", "opacity",
            "transform", "transition", "box-shadow", "text-align", "line-height"};
        addRule("\\b(" + cssProps.join("|") + ")\\s*:", m_cssPropertyFormat, 1);
        addRule("\\b(rgb|hsl|calc|var|clamp|min|max)\\s*(?=\\()", m_functionFormat, 1);
        addRule(numberPattern("(?:px|em|rem|%|vh|vw|s|ms)?"), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("'(?:\\\\.|[^'\\\\])*'", m_stringFormat);
    } else if (lang == "script") {
        addRule("\\b(print|let|var|true|false)\\b", m_keywordFormat);
        addRule(functionCallPattern(), m_functionFormat, 1);
        addRule("\\b(let|var)\\s+([A-Za-z_]\\w*)", m_variableFormat, 2);
        addRule("\\b([A-Za-z_]\\w*)\\b", m_variableFormat);
        addRule(numberPattern(), m_numberFormat);
        addRule("\"(?:\\\\.|[^\"\\\\])*\"", m_stringFormat);
        addRule("#[^\\n]*", m_commentFormat);
    }

    // Cache the built rules
    auto *formats = new LanguageFormats();
    formats->rules = m_rules;
    m_formatCache.insert(m_languageKey, formats);
}

// -----------------------------------------------------------------------
// Multi-line construct handlers
// -----------------------------------------------------------------------

void CodeHighlighter::handleCStyleBlockComment(const QString &text)
{
    int index = 0;

    if (previousBlockState() == BlockInComment) {
        index = text.indexOf("*/");
        if (index >= 0) {
            setFormat(0, index + 2, m_commentFormat);
            setCurrentBlockState(BlockNormal);
            index = 0; // Continue scanning for more comments
        } else {
            setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(BlockInComment);
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
            setCurrentBlockState(BlockInComment);
        }

        setFormat(start, length, m_commentFormat);

        if (currentBlockState() == BlockInComment)
            return;
    }
}

void CodeHighlighter::handlePythonTripleString(const QString &text)
{
    int start = 0;
    BlockState prevState = static_cast<BlockState>(previousBlockState());

    if (prevState == BlockInTripleDouble) {
        const int end = text.indexOf("\"\"\"");
        if (end >= 0) {
            setFormat(0, end + 3, m_stringFormat);
            start = end + 3;
            setCurrentBlockState(BlockNormal);
        } else {
            setFormat(0, text.length(), m_stringFormat);
            setCurrentBlockState(BlockInTripleDouble);
            return;
        }
    } else if (prevState == BlockInTripleSingle) {
        const int end = text.indexOf("'''");
        if (end >= 0) {
            setFormat(0, end + 3, m_stringFormat);
            start = end + 3;
            setCurrentBlockState(BlockNormal);
        } else {
            setFormat(0, text.length(), m_stringFormat);
            setCurrentBlockState(BlockInTripleSingle);
            return;
        }
    }

    QRegularExpressionMatchIterator iterator = QRegularExpression("\"\"\"|'''").globalMatch(text, start);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        const QString delimiter = match.captured(0);
        const int matchStart = match.capturedStart();
        const int end = text.indexOf(delimiter, matchStart + 3);

        if (end >= 0) {
            setFormat(matchStart, end + 3 - matchStart, m_stringFormat);
            start = end + 3;
        } else {
            setFormat(matchStart, text.length() - matchStart, m_stringFormat);
            setCurrentBlockState(delimiter == "\"\"\"" ? BlockInTripleDouble : BlockInTripleSingle);
            return;
        }
    }
}

void CodeHighlighter::handleHtmlComment(const QString &text)
{
    int start = 0;

    if (previousBlockState() == BlockInHtmlComment) {
        const int end = text.indexOf("-->");
        if (end >= 0) {
            setFormat(0, end + 3, m_commentFormat);
            start = end + 3;
            setCurrentBlockState(BlockNormal);
        } else {
            setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(BlockInHtmlComment);
            return;
        }
    }

    while ((start = text.indexOf("<!--", start)) >= 0) {
        const int blockStart = start;
        const int end = text.indexOf("-->", start + 4);
        int length;

        if (end >= 0) {
            length = end + 4 - blockStart;
            start = end + 4;
        } else {
            length = text.length() - blockStart;
            setCurrentBlockState(BlockInHtmlComment);
        }

        setFormat(blockStart, length, m_commentFormat);

        if (currentBlockState() == BlockInHtmlComment)
            return;
    }
}

// -----------------------------------------------------------------------
// CodeEditor implementation with performance optimizations
// -----------------------------------------------------------------------

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , lineNumberArea(new LineNumberArea(this))
    , syntaxHighlighter(new CodeHighlighter(document()))
    , m_multiCursor(new MultiCursorManager(this))
    , m_tabWidth(4)
    , m_foldManager(new FoldManager(this, this))
    , m_bracketColorizer(new BracketColorizer(this, this))
    , m_bookmarkManager(new BookmarkManager(this))
    , m_snippetManager(new SnippetManager(this))
{
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, [this]() { highlightCurrentLine(); });
    connect(m_multiCursor, &MultiCursorManager::cursorsChanged, this, [this]() {
        m_extraCursors.clear();
        QColor color = palette().color(QPalette::Highlight);
        for (const QTextCursor &cursor : m_multiCursor->cursors()) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = cursor;
            sel.format.setBackground(QColor(color.red(), color.green(), color.blue(), 80));
            m_extraCursors.append(sel);
        }
        updateAllSelections();
    });

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    QTextOption option = document()->defaultTextOption();
    option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
    option.setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    document()->setDefaultTextOption(option);
}

void CodeEditor::setLanguageForFile(const QString &filePath)
{
    m_filePath = filePath;
    syntaxHighlighter->setLanguage(
        LanguageRegistry::instance().languageForFile(filePath));
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

// -----------------------------------------------------------------------
// Performance-optimized paintEvent
// -----------------------------------------------------------------------

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);
    drawIndentGuides(event);
    drawInlayHints(event);
    drawGhostText(event);
}

// -----------------------------------------------------------------------
// Performance: drawIndentGuides with batch line drawing
// -----------------------------------------------------------------------

void CodeEditor::drawIndentGuides(QPaintEvent *event)
{
    if (!m_showIndentGuides)
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    const qreal tabStop = document()->defaultTextOption().tabStopDistance();
    const qreal charWidth = fontMetrics().horizontalAdvance(QLatin1Char(' '));
    const qreal effectiveTabStop = (tabStop > 0) ? tabStop : charWidth * m_tabWidth;

    // Pre-compute the content offset
    const QPointF offset = contentOffset();

    // Collect all guide x-positions and y-ranges per indent level for batch drawing
    QVector<QPair<int, QPair<int, int>>> guideLines; // (x, top, bottom) pairs

    QTextBlock block = firstVisibleBlock();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(offset).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // Pre-allocate reasonable size
    guideLines.reserve(blockCount() * 4);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString text = block.text();
            if (!text.isEmpty() && !text.trimmed().isEmpty()) {
                qreal width = 0;
                for (const QChar &c : text) {
                    if (c == QLatin1Char(' ')) {
                        width += charWidth;
                    } else if (c == QLatin1Char('\t')) {
                        width = (std::floor(width / effectiveTabStop) + 1) * effectiveTabStop;
                    } else {
                        break;
                    }
                }

                // Batch each indent level
                for (qreal pos = effectiveTabStop; pos <= width + 0.1; pos += effectiveTabStop) {
                    int guideX = static_cast<int>(pos + offset.x());
                    guideLines.append(qMakePair(guideX, qMakePair(top, bottom)));
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
    }

    // Batch draw all guide lines
    if (!guideLines.isEmpty()) {
        // Sort by x to minimize pen changes (same-color lines)
        std::sort(guideLines.begin(), guideLines.end(),
                  [](const QPair<int, QPair<int, int>> &a,
                     const QPair<int, QPair<int, int>> &b) {
                      return a.first < b.first;
                  });

        painter.setPen(QPen(QColor(128, 128, 128, 80), 1, Qt::SolidLine));

        for (const auto &guide : guideLines) {
            painter.drawLine(guide.first, guide.second.first,
                           guide.first, guide.second.second);
        }
    }
}

// -----------------------------------------------------------------------
// Performance: drawInlayHints with visible line range filtering
// -----------------------------------------------------------------------

void CodeEditor::drawInlayHints(QPaintEvent *event)
{
    if (m_inlayHints.isEmpty())
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Compute visible line range
    QTextBlock firstBlock = firstVisibleBlock();
    const int firstVisibleLine = firstBlock.blockNumber();
    const QPointF offset = contentOffset();
    const int viewportBottom = event->rect().bottom();

    int lastVisibleLine = firstVisibleLine;
    QTextBlock block = firstBlock;
    qreal blockTop = blockBoundingGeometry(block).translated(offset).top();
    while (block.isValid() && blockTop <= viewportBottom) {
        lastVisibleLine = block.blockNumber();
        block = block.next();
        blockTop += blockBoundingRect(block).height();
    }

    const QFont &editorFont = font();
    painter.setFont(editorFont);
    const QColor hintColor(128, 128, 128, 180);
    painter.setPen(QPen(hintColor, 1));

    for (const LspInlayHint &hint : m_inlayHints) {
        // Skip hints outside visible range
        if (hint.position.line < firstVisibleLine || hint.position.line > lastVisibleLine)
            continue;

        QTextBlock hintBlock = document()->findBlockByNumber(hint.position.line);
        if (!hintBlock.isValid())
            continue;

        QTextCursor cursor(hintBlock);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, hint.position.character);

        QRect rect = cursorRect(cursor);
        if (!event->rect().intersects(rect))
            continue;

        QString label = hint.label;
        if (hint.paddingLeft)
            label.prepend(" ");
        if (hint.paddingRight)
            label.append(" ");

        QRect textRect = rect;
        textRect.setWidth(fontMetrics().horizontalAdvance(label));
        textRect.setHeight(fontMetrics().height());

        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
}

// -----------------------------------------------------------------------
// Performance: drawGhostText with minimal updates
// -----------------------------------------------------------------------

void CodeEditor::drawGhostText(QPaintEvent *event)
{
    if (m_ghostText.isEmpty())
        return;

    QPainter painter(viewport());
    painter.setFont(font());

    QTextCursor cursor(textCursor());
    cursor.clearSelection();
    QRect rect = cursorRect(cursor);
    if (!event->rect().intersects(rect))
        return;

    QRect textRect = rect;
    textRect.setWidth(fontMetrics().horizontalAdvance(m_ghostText));
    textRect.setHeight(fontMetrics().height());

    painter.setPen(QPen(QColor(128, 128, 128, 140), 1));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_ghostText);
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
    // Preserve existing selections (diagnostics + plugin decorations + bracket colors) and add/update current line highlight
    QList<QTextEdit::ExtraSelection> extraSelections;
    extraSelections.append(m_diagnosticSelections);
    extraSelections.append(m_pluginExtraSelections);

    // Add bracket colorization selections
    if (m_bracketColorizer && m_bracketColorEnabled) {
        extraSelections.append(m_bracketColorizer->extraSelections());
    }

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = palette().color(QPalette::Highlight);
        lineColor.setAlpha(40);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        // Replace existing line highlight if present
        bool found = false;
        for (int i = 0; i < extraSelections.size(); ++i) {
            if (extraSelections[i].format.hasProperty(QTextFormat::FullWidthSelection)) {
                extraSelections[i] = selection;
                found = true;
                break;
            }
        }
        if (!found)
            extraSelections.prepend(selection);
    }

    extraSelections.append(m_extraCursors);
    setExtraSelections(extraSelections);
}

void CodeEditor::updateAllSelections()
{
    highlightCurrentLine();
}

void CodeEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        if (event->modifiers() & Qt::ShiftModifier) {
            m_columnSelectionMode = true;
        }
        m_multiCursor->addCursor(cursor);
    } else {
        m_multiCursor->clear();
        m_columnSelectionMode = false;
        QPlainTextEdit::mousePressEvent(event);
    }
}

void CodeEditor::selectNextOccurrence()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        // Find next occurrence after current selection
        QTextCursor next = document()->find(selectedText, cursor.selectionEnd());
        if (!next.isNull()) {
            m_multiCursor->addCursor(next);
            setTextCursor(next);
        }
    }
}

void CodeEditor::selectAllOccurrences()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) return;
    QString selectedText = cursor.selectedText();
    QTextCursor searchCursor(document());
    searchCursor.movePosition(QTextCursor::Start);
    while (true) {
        QTextCursor found = document()->find(selectedText, searchCursor);
        if (found.isNull()) break;
        m_multiCursor->addCursor(found);
        searchCursor = found;
        searchCursor.movePosition(QTextCursor::Right);
    }
}

void CodeEditor::addCursorAbove()
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();
    if (blockNum == 0) return;
    QTextBlock block = document()->findBlockByNumber(blockNum - 1);
    QTextCursor newCursor(block);
    newCursor.setPosition(block.position() + qMin(cursor.positionInBlock(), block.length() - 1));
    m_multiCursor->addCursor(newCursor);
}

void CodeEditor::addCursorBelow()
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();
    if (blockNum >= document()->blockCount() - 1) return;
    QTextBlock block = document()->findBlockByNumber(blockNum + 1);
    QTextCursor newCursor(block);
    newCursor.setPosition(block.position() + qMin(cursor.positionInBlock(), block.length() - 1));
    m_multiCursor->addCursor(newCursor);
}

void CodeEditor::setBracketColorization(bool enabled)
{
    m_bracketColorEnabled = enabled;
    if (m_bracketColorizer) {
        m_bracketColorizer->setEnabled(enabled);
    }
}

void CodeEditor::handleSmartIndent(QKeyEvent *event)
{
    if (!m_smartIndent) return;
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString lineText = block.text();
    int indent = 0;
    for (const QChar &c : lineText) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += m_tabWidth;
        else break;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Check if line ends with { or (
        QString trimmed = lineText.trimmed();
        if (trimmed.endsWith('{') || trimmed.endsWith('(') || trimmed.endsWith('[')) {
            indent += m_tabWidth;
        }
        cursor.insertText("\n" + QString(indent, ' '));
        setTextCursor(cursor);
        event->accept();
    }
}

void CodeEditor::handleBracketAutoClose(QKeyEvent *event)
{
    QMap<QChar, QChar> pairs = {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}};
    if (event->text().isEmpty() || !pairs.contains(event->text().at(0)))
        return;
    
    QTextCursor cursor = textCursor();
    QChar open = event->text().at(0);
    QChar close = pairs[open];
    
    // For quotes, check if next char is the same quote — skip over it
    if (open == close) {
        QChar nextChar = document()->characterAt(cursor.position());
        if (nextChar == open) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            event->accept();
            return;
        }
    }
    
    // Skip if next character is alphanumeric (don't auto-close before word chars)
    QChar nextChar = document()->characterAt(cursor.position());
    if (nextChar.isLetterOrNumber())
        return;
    
    cursor.insertText(QString(open) + close);
    cursor.movePosition(QTextCursor::Left);
    setTextCursor(cursor);
    event->accept();
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+D: Select next occurrence
    if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier) {
        selectNextOccurrence();
        event->accept();
        return;
    }
    // Ctrl+Shift+L: Select all occurrences
    if (event->key() == Qt::Key_L && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        selectAllOccurrences();
        event->accept();
        return;
    }
    // Alt+Shift+Up/Down: Add cursor above/below
    if (event->key() == Qt::Key_Up && (event->modifiers() & Qt::AltModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        addCursorAbove();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down && (event->modifiers() & Qt::AltModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        addCursorBelow();
        event->accept();
        return;
    }
    // Smart indentation on Enter
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_smartIndent && !m_multiCursor->hasCursors()) {
        handleSmartIndent(event);
        return;
    }
    // Bracket auto-close
    if (!event->text().isEmpty() && !m_multiCursor->hasCursors() && m_bracketColorEnabled) {
        handleBracketAutoClose(event);
        if (event->isAccepted()) return;
    }
    // Emmet expansion on Tab when cursor follows an abbreviation
    if (event->key() == Qt::Key_Tab && !event->modifiers().testFlag(Qt::ControlModifier)) {
        QTextCursor cursor = textCursor();
        // Select text from start of line to cursor to find abbreviation
        QTextCursor lineStart(cursor);
        lineStart.movePosition(QTextCursor::StartOfBlock);
        QString lineText = lineStart.selectedText() + cursor.block().text().left(cursor.positionInBlock());
        // Look for Emmet-like pattern at end of line (word chars, >, +, *, ^, #, .)
        QRegularExpression emmetRe("([a-zA-Z0-9\[\]()>+*^#._@\-]+)$");
        QRegularExpressionMatch match = emmetRe.match(lineText);
        if (match.hasMatch() && !m_bracketColorEnabled) {
            QString abbreviation = match.captured(1);
            // Simple check: abbreviation should look like Emmet (contain special chars)
            if (abbreviation.contains(QRegularExpression("[>+*^#\[\]()]")) && abbreviation.length() >= 2) {
                // Expand via Rust FFI
                QByteArray abbrBytes = abbreviation.toUtf8();
                char *expanded = rust_emmet_expand(abbrBytes.constData());
                if (expanded) {
                    QString expansion = QString::fromUtf8(expanded);
                    rust_free_string(expanded);
                    if (!expansion.isEmpty()) {
                        // Remove the abbreviation from the line
                        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, lineText.length() - abbreviation.length());
                        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, abbreviation.length());
                        cursor.removeSelectedText();
                        cursor.insertText(expansion);
                        setTextCursor(cursor);
                        event->accept();
                        return;
                    }
                }
            }
        }
    }
    if (!m_ghostText.isEmpty()) {
        if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            QString accepted = m_ghostText;
            QTextCursor cursor(textCursor());
            cursor.insertText(accepted);
            m_ghostText.clear();
            update();
            emit ghostTextAccepted(accepted);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            m_ghostText.clear();
            update();
            event->accept();
            return;
        }
        if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
            m_ghostText.clear();
            update();
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
    }

    if (!m_multiCursor->hasCursors()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        m_multiCursor->clear();
        m_columnSelectionMode = false;
        updateAllSelections();
        return;
    }

    QString text = event->text();
    if (text.isEmpty()) {
        QPlainTextEdit::keyPressEvent(event);
        m_multiCursor->clear();
        return;
    }

    QList<QTextCursor> allCursors;
    allCursors.append(textCursor());
    allCursors.append(m_multiCursor->cursors());

    for (int i = allCursors.size() - 1; i >= 0; --i) {
        QTextCursor c = allCursors[i];
        c.insertText(text);
    }
    m_multiCursor->clear();
}

// -----------------------------------------------------------------------
// Performance-optimized lineNumberAreaPaintEvent
// -----------------------------------------------------------------------

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);

    const QColor altBase = palette().color(QPalette::AlternateBase);
    const QColor base = palette().color(QPalette::Base);

    // Draw a subtle gradient (replaces the base fill since the gradient covers the entire rect)
    if (lineNumberArea->width() > 3) {
        QLinearGradient gradient(0, 0, lineNumberArea->width(), 0);
        gradient.setColorAt(0, altBase);
        gradient.setColorAt(1, base);
        painter.fillRect(event->rect(), gradient);
    } else {
        painter.fillRect(event->rect(), altBase);
    }

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    const QPointF offset = contentOffset();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(offset).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    const int areaWidth = lineNumberArea->width();
    const int fontHeight = fontMetrics().height();
    const QPen numberPen(palette().color(QPalette::Midlight));

    painter.setPen(numberPen);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            int line = blockNumber + 1;

            // Draw breakpoint icon if this line has a breakpoint
            if (m_breakpointLines.contains(line)) {
                QPainterPath path;
                int cx = 9;
                int cy = top + (bottom - top) / 2;
                path.addEllipse(cx - 5, cy - 5, 10, 10);
                painter.setPen(QPen(Qt::red, 2));
                painter.setBrush(Qt::red);
                painter.drawPath(path);
                painter.setPen(numberPen);
            }

            // Draw fold indicator if this line is a fold start
            if (m_foldManager && m_foldManager->isFoldStart(line - 1)) {
                m_foldManager->paintFoldIndicator(painter, 0, top, line - 1, bottom - top);
            }

            // Draw bookmark indicator
            if (m_bookmarkManager && !m_filePath.isEmpty() && m_bookmarkManager->isBookmarked(m_filePath, line - 1)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 215, 0));  // Gold bookmark dot
                painter.drawEllipse(4, top + 3, 8, 8);
                painter.setPen(numberPen);
            }

            // Draw blame annotation if enabled
            if (m_blameEnabled && m_blameData.contains(blockNumber)) {
                const BlameLineInfo &info = m_blameData[blockNumber];
                QFont blameFont = font();
                blameFont.setPointSize(blameFont.pointSize() - 2);
                painter.setFont(blameFont);
                QColor blameColor = palette().color(QPalette::Midlight);
                blameColor.setAlpha(160);
                painter.setPen(blameColor);
                QString blameText = info.author + " " + info.date;
                int blameX = 20 + fontMetrics().horizontalAdvance(QString::number(blockCount() + 1)) + 12;
                painter.drawText(blameX, top, areaWidth - blameX, fontHeight,
                               Qt::AlignLeft | Qt::AlignVCenter, blameText);
                painter.setFont(font());
                painter.setPen(numberPen);
            }

            // Draw Code Lens annotations above the line
            if (!m_codeLensItems.isEmpty()) {
                for (const CodeLensItem &lens : m_codeLensItems) {
                    if (lens.line == blockNumber) {
                        QFont lensFont = font();
                        lensFont.setPointSize(lensFont.pointSize() - 1);
                        painter.setFont(lensFont);
                        QColor lensColor = palette().color(QPalette::Midlight);
                        lensColor.setAlpha(200);
                        painter.setPen(lensColor);
                        int lensX = 20 + fontMetrics().horizontalAdvance(QString::number(blockCount() + 1)) + 12;
                        // Draw above the line number
                        painter.drawText(lensX, top - 2, areaWidth - lensX, fontHeight,
                                       Qt::AlignLeft | Qt::AlignBottom, lens.title);
                        painter.setFont(font());
                        painter.setPen(numberPen);
                    }
                }
            }

            // Draw line number
            QString number = QString::number(line);
            painter.drawText(20, top, areaWidth - 20, fontHeight,
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
    updateAllSelections();
}

void CodeEditor::setPluginExtraSelections(const QList<QTextEdit::ExtraSelection> &selections)
{
    m_pluginExtraSelections = selections;
    updateAllSelections();
}

void CodeEditor::clearPluginExtraSelections()
{
    m_pluginExtraSelections.clear();
    updateAllSelections();
}

void CodeEditor::setDiagnosticTooltips(const QList<QPair<QTextCursor, QString>> &tips)
{
    m_diagnosticTooltips = tips;
}

void CodeEditor::setInlayHints(const QList<LspInlayHint> &hints)
{
    m_inlayHints = hints;
    update();
}

void CodeEditor::setGhostText(const QString &text)
{
    if (m_ghostText != text) {
        m_ghostText = text;
        update();
    }
}

void CodeEditor::clearGhostText()
{
    if (!m_ghostText.isEmpty()) {
        m_ghostText.clear();
        update();
    }
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (event->modifiers() & Qt::AltModifier) {
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);

        bool duplicate = false;
        for (const QTextCursor &existing : m_multiCursor->cursors()) {
            if (existing.selectionStart() == cursor.selectionStart()) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_multiCursor->addCursor(cursor);
        }
    } else {
        updateHoverTooltip(event->pos());
        QPlainTextEdit::mouseMoveEvent(event);
    }
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
    QTextCursor cursor = cursorForPosition(pos);
    int cursorPos = cursor.position();

    for (const QPair<QTextCursor, QString> &tip : m_diagnosticTooltips) {
        if (tip.first.selectionStart() <= cursorPos && cursorPos <= tip.first.selectionEnd()) {
            m_hoveringDiagnostic = true;
            QToolTip::showText(mapToGlobal(pos), tip.second, this);
            return;
        }
    }
    if (m_hoveringDiagnostic)
        QToolTip::hideText();
    m_hoveringDiagnostic = false;
}

void CodeEditor::setBreakpointLine(int line, bool enabled)
{
    if (enabled) {
        m_breakpointLines.insert(line);
    } else {
        m_breakpointLines.remove(line);
    }
    emit breakpointToggled(line, enabled);
    lineNumberArea->update();
}

void CodeEditor::clearBreakpoints()
{
    m_breakpointLines.clear();
    lineNumberArea->update();
}

void CodeEditor::highlightCurrentLine(int line)
{
    m_currentDebugLine = line;

    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);

    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(QColor(255, 255, 0, 100));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = cursor;

    QList<QTextEdit::ExtraSelection> extraSelections;
    extraSelections.append(selection);
    extraSelections.append(m_diagnosticSelections);
    extraSelections.append(m_pluginExtraSelections);
    if (m_bracketColorizer && m_bracketColorEnabled) {
        extraSelections.append(m_bracketColorizer->extraSelections());
    }
    extraSelections.append(m_extraCursors);
    setExtraSelections(extraSelections);
}
