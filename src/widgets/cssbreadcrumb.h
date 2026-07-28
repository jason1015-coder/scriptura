#ifndef CSSBREADCRUMB_H
#define CSSBREADCRUMB_H

#include <QObject>
#include <QStringList>
#include <QList>

class CodeEditor;

/**
 * Represents a single element in the DOM hierarchy breadcrumb.
 */
struct DomBreadcrumbElement {
    QString tag;
    QString id;
    QString classes;
    int startLine;      // 0-based line where this element starts
    int startColumn;    // 0-based column where this element starts
};

/**
 * Parses HTML/CSS DOM hierarchy for breadcrumb navigation.
 * Shows nesting like: html > body > div.container > ul > li.active
 */
class CssBreadcrumbParser : public QObject
{
    Q_OBJECT
public:
    explicit CssBreadcrumbParser(QObject *parent = nullptr);

    // Parse DOM hierarchy at a given cursor position
    QList<DomBreadcrumbElement> parseDomHierarchy(CodeEditor *editor, int line, int column);

    // Get breadcrumb text for display
    static QString breadcrumbText(const QList<DomBreadcrumbElement> &elements);

    // Get the full CSS selector for the element at cursor
    static QString cssSelector(const QList<DomBreadcrumbElement> &elements);

private:
    struct TagMatch {
        QString tag;
        QString id;
        QString classes;
        int line;
        int column;
        bool isClosing;
    };

    QList<TagMatch> extractTags(CodeEditor *editor, int upToLine);
    bool isInStringOrComment(const QString &line, int column) const;
};

#endif // CSSBREADCRUMB_H
