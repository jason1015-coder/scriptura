#include "emmetparser.h"
#include <QRegularExpression>
#include <QStack>

const QStringList EmmetParser::m_selfClosingTags = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"
};

EmmetParser::EmmetParser(QObject *parent) : QObject(parent) {}

QString EmmetParser::expandAbbreviation(const QString &abbreviation) const
{
    QString abbr = abbreviation.trimmed();
    if (abbr.isEmpty()) return QString();

    // CSS shorthand expansion
    if (isCSSShorthand(abbr)) {
        return expandCSSShorthand(abbr);
    }

    ParseNode root = parseGroup(abbr);
    return renderNode(root).trimmed();
}

bool EmmetParser::isAbbreviation(const QString &text)
{
    if (text.isEmpty()) return false;
    // Emmet abbreviations typically contain tag chars, dots, #, >, +, *, ^, {}
    static QRegularExpression re(R"([a-z][\w.-]*(?:[#.\[\]{}>+*^:@]|\{[^}]*\}))");
    return re.match(text).hasMatch();
}

bool EmmetParser::isCSSShorthand(const QString &text)
{
    // CSS shorthands like m10, p10-20, fs14, bgc#fff
    static QRegularExpression re(R"(^(m|p|fs|fw|lh|ws|ov|pos|d|fd|fg|ji|ai|bc|bgc|c|ff|bp|br|sh|tac|tal|tar|o|z|g|gg|gt|gap)(\d|[a-zA-Z#].*)$)");
    return re.match(text).hasMatch();
}

QString EmmetParser::extractAbbreviation(const QString &text, int cursorPos)
{
    if (cursorPos <= 0 || cursorPos > text.length()) return QString();

    int start = cursorPos - 1;
    // Walk back to find the start of the abbreviation
    while (start > 0) {
        QChar c = text.at(start - 1);
        if (c.isLetterOrNumber() || c == '.' || c == '#' || c == '>' || c == '+' ||
            c == '^' || c == '*' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '(' || c == ')' || c == '-' || c == ':' || c == '@') {
            start--;
        } else {
            break;
        }
    }

    return text.mid(start, cursorPos - start);
}

EmmetParser::TagInfo EmmetParser::parseTag(const QString &tagStr)
{
    TagInfo info;
    info.tag = "div"; // default

    static QRegularExpression tagRe(R"(^([a-zA-Z][\w-]*)?)");
    static QRegularExpression idRe(R"(#([\w-]+))");
    static QRegularExpression classRe(R"(\.([\w-]+))");
    static QRegularExpression attrRe(R"(\[([\w-]+)(?:=([^\]]*))?\])");
    static QRegularExpression textRe(R"(\{([^}]*)\})");
    static QRegularExpression repeatRe(R"(\*(\d+))");

    QRegularExpressionMatch tagMatch = tagRe.match(tagStr);
    if (tagMatch.hasMatch() && !tagMatch.captured(1).isEmpty()) {
        info.tag = tagMatch.captured(1);
    }

    // Extract ID
    QRegularExpressionMatchIterator it = idRe.globalMatch(tagStr);
    if (it.hasNext()) {
        info.id = it.next().captured(1);
    }

    // Extract classes
    it = classRe.globalMatch(tagStr);
    while (it.hasNext()) {
        info.classes.append(it.next().captured(1));
    }

    // Extract attributes
    it = attrRe.globalMatch(tagStr);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        info.attributes[m.captured(1)] = m.captured(2);
    }

    // Extract text
    QRegularExpressionMatch textMatch = textRe.match(tagStr);
    if (textMatch.hasMatch()) {
        info.text = textMatch.captured(1);
    }

    // Extract repeat count
    QRegularExpressionMatch repeatMatch = repeatRe.match(tagStr);
    if (repeatMatch.hasMatch()) {
        info.repeatCount = repeatMatch.captured(1).toInt();
    }

    return info;
}

EmmetParser::ParseNode EmmetParser::parseGroup(const QString &group) const
{
    ParseNode root;
    root.tag = "root";

    QStack<ParseNode*> stack;
    stack.push(&root);

    int i = 0;
    while (i < group.length()) {
        QChar c = group.at(i);

        if (c == '>') {
            // Child operator - next token becomes child of current
            i++;
            continue;
        }

        if (c == '+') {
            // Sibling operator - next token becomes sibling
            i++;
            continue;
        }

        if (c == '^') {
            // Climb up - move to parent
            i++;
            if (stack.size() > 1) stack.pop();
            continue;
        }

        if (c == '*') {
            // Multiplication
            i++;
            int num = 0;
            while (i < group.length() && group.at(i).isDigit()) {
                num = num * 10 + group.at(i).digitValue();
                i++;
            }
            if (!stack.isEmpty() && num > 0) {
                stack.top()->repeatCount = num;
            }
            continue;
        }

        // Parse tag or text
        if (c == '{') {
            // Text content
            i++;
            int start = i;
            while (i < group.length() && group.at(i) != '}') i++;
            if (!stack.isEmpty()) {
                stack.top()->text = group.mid(start, i - start);
            }
            if (i < group.length()) i++;
            continue;
        }

        // Parse tag element
        int tagStart = i;
        while (i < group.length() && group.at(i) != '>' && group.at(i) != '+' &&
               group.at(i) != '^' && group.at(i) != '*' && group.at(i) != '{') {
            i++;
        }

        QString tagStr = group.mid(tagStart, i - tagStart);
        if (!tagStr.isEmpty()) {
            TagInfo info = parseTag(tagStr);

            ParseNode node;
            node.tag = info.tag;
            node.id = info.id;
            node.classes = info.classes;
            node.attributes = info.attributes;
            node.text = info.text;
            node.repeatCount = info.repeatCount;

            if (!stack.isEmpty()) {
                stack.top()->children.append(node);
                stack.push(&stack.top()->children.last());
            }
        }
    }

    return root;
}

QString EmmetParser::renderNode(const ParseNode &node, int indent) const
{
    QString prefix(indent * 2, ' ');
    QString result;

    if (node.tag == "root") {
        for (const ParseNode &child : node.children) {
            result += renderNode(child, indent);
        }
        return result;
    }

    QMap<QString, QString> attrs = buildAttributes(node.id, node.classes, node.attributes);

    int count = qMax(1, node.repeatCount);
    for (int n = 0; n < count; ++n) {
        QString content;
        for (const ParseNode &child : node.children) {
            content += renderNode(child, indent + 1);
        }

        if (node.text.isEmpty() && content.isEmpty() && m_selfClosingTags.contains(node.tag)) {
            result += prefix + renderSelfClosing(node.tag, attrs) + "\n";
        } else {
            QString inner = node.text;
            if (!content.isEmpty()) {
                if (!inner.isEmpty()) inner += "\n";
                inner += content + prefix;
            }
            result += prefix + renderTag(node.tag, attrs, inner) + "\n";
        }
    }

    return result;
}

QString EmmetParser::renderSelfClosing(const QString &tag, const QMap<QString, QString> &attrs) const
{
    QString attrStr;
    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            attrStr += " " + it.key();
        } else {
            attrStr += " " + it.key() + "=\"" + it.value() + "\"";
        }
    }
    return "<" + tag + attrStr + " />";
}

QString EmmetParser::renderTag(const QString &tag, const QMap<QString, QString> &attrs, const QString &content) const
{
    QString attrStr;
    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            attrStr += " " + it.key();
        } else {
            attrStr += " " + it.key() + "=\"" + it.value() + "\"";
        }
    }
    return "<" + tag + attrStr + ">" + content + "</" + tag + ">";
}

QMap<QString, QString> EmmetParser::buildAttributes(const QString &id, const QStringList &classes, const QMap<QString, QString> &attrs) const
{
    QMap<QString, QString> result = attrs;
    if (!id.isEmpty()) result["id"] = id;
    if (!classes.isEmpty()) result["class"] = classes.join(" ");
    return result;
}

QString EmmetParser::expandCSSShorthand(const QString &shorthand)
{
    static QRegularExpression propRe(R"(^(m|p|fs|fw|lh|ws|ov|pos|d|fd|fg|ji|ai|bc|bgc|c|ff|bp|br|sh|tac|tal|tar|o|z|g|gg|gt|gap)(.+)$)");
    QRegularExpressionMatch match = propRe.match(shorthand);
    if (!match.hasMatch()) return shorthand;

    QString prop = match.captured(1);
    QString value = match.captured(2);

    // Map shorthand prefixes to CSS properties
    QMap<QString, QString> propMap = {
        {"m", "margin"}, {"p", "padding"}, {"fs", "font-size"},
        {"fw", "font-weight"}, {"lh", "line-height"}, {"ws", "white-space"},
        {"ov", "overflow"}, {"pos", "position"}, {"d", "display"},
        {"fd", "flex-direction"}, {"fg", "flex-grow"}, {"ji", "justify-items"},
        {"ai", "align-items"}, {"bc", "border-color"}, {"bgc", "background-color"},
        {"c", "color"}, {"ff", "font-family"}, {"bp", "breakpoint"},
        {"br", "border-radius"}, {"sh", "box-shadow"}, {"tac", "text-align"},
        {"tal", "text-align"}, {"tar", "text-align"}, {"o", "opacity"},
        {"z", "z-index"}, {"g", "gap"}, {"gg", "grid-gap"},
        {"gt", "grid-template"}, {"gap", "gap"}
    };

    QString cssProp = propMap.value(prop, prop);

    // Add px suffix for numeric values
    if (!value.isEmpty() && !value.endsWith("px") && !value.endsWith("em") &&
        !value.endsWith("rem") && !value.endsWith("%") && !value.contains(":")) {
        bool isNum = false;
        value.toInt(&isNum);
        if (isNum) value += "px";
    }

    // Handle # color shorthand
    if (value.startsWith("#")) {
        // Already a color value
    }

    return cssProp + ": " + value + ";";
}
