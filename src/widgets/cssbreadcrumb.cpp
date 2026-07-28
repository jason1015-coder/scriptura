#include "cssbreadcrumb.h"
#include "codeeditor.h"
#include <QTextBlock>
#include <QRegularExpression>

CssBreadcrumbParser::CssBreadcrumbParser(QObject *parent) : QObject(parent) {}

QList<DomBreadcrumbElement> CssBreadcrumbParser::parseDomHierarchy(CodeEditor *editor, int line, int column)
{
    QList<DomBreadcrumbElement> result;
    if (!editor) return result;

    QList<TagMatch> tags = extractTags(editor, line);

    // Build nesting stack using a simplified approach
    QList<TagMatch> stack;
    for (const TagMatch &tag : tags) {
        if (tag.isClosing) {
            // Remove matching opening tag from stack
            for (int i = stack.size() - 1; i >= 0; --i) {
                if (stack[i].tag == tag.tag) {
                    stack.removeAt(i);
                    break;
                }
            }
        } else {
            stack.append(tag);
        }
    }

    // Convert stack to breadcrumb elements
    for (const TagMatch &tag : stack) {
        DomBreadcrumbElement elem;
        elem.tag = tag.tag;
        elem.id = tag.id;
        elem.classes = tag.classes;
        elem.startLine = tag.line;
        elem.startColumn = tag.column;
        result.append(elem);
    }

    // Add current tag if cursor is on a tag line
    if (!tags.isEmpty()) {
        for (int i = tags.size() - 1; i >= 0; --i) {
            if (tags[i].line == line && !tags[i].isClosing) {
                bool alreadyInResult = false;
                for (const auto &r : result) {
                    if (r.tag == tags[i].tag && r.startLine == tags[i].line) {
                        alreadyInResult = true;
                        break;
                    }
                }
                if (!alreadyInResult) {
                    DomBreadcrumbElement elem;
                    elem.tag = tags[i].tag;
                    elem.id = tags[i].id;
                    elem.classes = tags[i].classes;
                    elem.startLine = tags[i].line;
                    elem.startColumn = tags[i].column;
                    result.append(elem);
                }
                break;
            }
        }
    }

    return result;
}

QString CssBreadcrumbParser::breadcrumbText(const QList<DomBreadcrumbElement> &elements)
{
    QStringList parts;
    for (const DomBreadcrumbElement &elem : elements) {
        QString text = elem.tag;
        if (!elem.id.isEmpty()) text += "#" + elem.id;
        if (!elem.classes.isEmpty()) {
            QString classes = elem.classes;
            text += "." + classes.replace(' ', '.');
        }
        parts.append(text);
    }
    return parts.join(" > ");
}

QString CssBreadcrumbParser::cssSelector(const QList<DomBreadcrumbElement> &elements)
{
    QStringList parts;
    for (const DomBreadcrumbElement &elem : elements) {
        QString sel = elem.tag;
        if (!elem.id.isEmpty()) sel += "#" + elem.id;
        if (!elem.classes.isEmpty()) {
            QString classes = elem.classes;
            sel += "." + classes.replace(' ', '.');
        }
        parts.append(sel);
    }
    return parts.join(" > ");
}

QList<CssBreadcrumbParser::TagMatch> CssBreadcrumbParser::extractTags(CodeEditor *editor, int upToLine)
{
    QList<TagMatch> result;
    if (!editor) return result;

    static QRegularExpression tagRe(R"(<(/?)([a-zA-Z][a-zA-Z0-9:-]*)([^>]*)(/?)>)");
    static QRegularExpression idRe(R"(id=["']([^"']*)["'])");
    static QRegularExpression classRe(R"(class=["']([^"']*)["'])");

    QTextBlock block = editor->document()->firstBlock();
    int lineNum = 0;

    while (block.isValid() && lineNum <= upToLine) {
        QString text = block.text();
        QRegularExpressionMatchIterator it = tagRe.globalMatch(text);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (isInStringOrComment(text, match.capturedStart()))
                continue;

            TagMatch tag;
            tag.isClosing = !match.captured(1).isEmpty();
            tag.tag = match.captured(2);
            tag.line = lineNum;
            tag.column = match.capturedStart();

            QString attrs = match.captured(3);

            QRegularExpressionMatch idMatch = idRe.match(attrs);
            if (idMatch.hasMatch()) tag.id = idMatch.captured(1);

            QRegularExpressionMatch classMatch = classRe.match(attrs);
            if (classMatch.hasMatch()) tag.classes = classMatch.captured(1);

            result.append(tag);
        }

        block = block.next();
        lineNum++;
    }

    return result;
}

bool CssBreadcrumbParser::isInStringOrComment(const QString &line, int column) const
{
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inLineComment = false;

    for (int i = 0; i < column && i < line.length(); ++i) {
        QChar c = line.at(i);
        if (inLineComment) continue;
        if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == '/' && i + 1 < line.length() && line.at(i + 1) == '/') {
            inLineComment = true;
        }
    }

    return inSingleQuote || inDoubleQuote;
}
