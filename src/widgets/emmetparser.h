#ifndef EMMETPARSER_H
#define EMMETPARSER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

/**
 * Parses Emmet abbreviations and expands them into HTML/CSS markup.
 * Supports:
 * - Tag expansion: div → <div></div>
 * - Nesting: div>p → <div><p></p></div>
 * - Siblings: div+p → <div></div><p></p>
 * - Multiplication: li*3 → <li></li><li></li><li></li>
 * - Classes and IDs: div.container#main → <div id="main" class="container"></div>
 * - Text content: div{Hello} → <div>Hello</div>
 * - Attributes: div[data-id=5] → <div data-id="5"></div>
 * - Climb-up: div>p^span → <div><p></p></div><span></span>
 * - CSS shorthand: m10 → margin: 10px;
 */
class EmmetParser : public QObject
{
    Q_OBJECT
public:
    explicit EmmetParser(QObject *parent = nullptr);

    // Parse an Emmet abbreviation and return expanded HTML
    QString expandAbbreviation(const QString &abbreviation) const;

    // Check if a string looks like an Emmet abbreviation
    static bool isAbbreviation(const QString &text);

    // Get the abbreviation that starts at the given position in the text
    static QString extractAbbreviation(const QString &text, int cursorPos);

    // Expand CSS shorthand
    static QString expandCSSShorthand(const QString &shorthand);

    // Check if text is a CSS shorthand
    static bool isCSSShorthand(const QString &text);

    // Parse tag with attributes: div.class#id[attr=value]{text}
    struct TagInfo {
        QString tag;
        QString id;
        QStringList classes;
        QMap<QString, QString> attributes;
        QString text;
        int repeatCount = 1;
    };

    static TagInfo parseTag(const QString &tagStr);

private:
    struct ParseNode {
        QString tag;
        QString id;
        QStringList classes;
        QMap<QString, QString> attributes;
        QString text;
        int repeatCount = 1;
        QList<ParseNode> children;
    };

    ParseNode parseGroup(const QString &group) const;
    QString renderNode(const ParseNode &node, int indent = 0) const;
    QString renderSelfClosing(const QString &tag, const QMap<QString, QString> &attrs) const;
    QString renderTag(const QString &tag, const QMap<QString, QString> &attrs, const QString &content) const;
    QMap<QString, QString> buildAttributes(const QString &id, const QStringList &classes, const QMap<QString, QString> &attrs) const;

    static const QStringList m_selfClosingTags;
};

#endif // EMMETPARSER_H
