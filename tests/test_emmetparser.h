#ifndef TEST_EMMETPARSER_H
#define TEST_EMMETPARSER_H

#include <QObject>

class TestEmmetParser : public QObject
{
    Q_OBJECT
private slots:
    void testExpandBasicTag();
    void testExpandNesting();
    void testExpandSiblings();
    void testExpandMultiplication();
    void testExpandClassesAndId();
    void testExpandTextContent();
    void testExpandAttributes();
    void testExpandSelfClosing();
    void testExpandEmpty();
    void testIsAbbreviation();
    void testExtractAbbreviation();
    void testCSSShorthand();
    void testIsCSSShorthand();
    void testParseTag();
    void testExpandCSSShorthandNoMatch();
    void testExpandCSSShorthandPxSuffix();
    void testExpandCSSShorthandUnits();
};

#endif // TEST_EMMETPARSER_H
