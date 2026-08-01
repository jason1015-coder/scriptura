#include <QTest>
#include "emmetparser.h"
#include "test_emmetparser.h"

// Note: the parser treats any string starting with a CSS-shorthand prefix
// (m, p, fs, d, c, ...) as CSS rather than an HTML tag. So "div" expands to
// a `display` declaration, not `<div></div>`. Tests use non-prefix tags
// (span, section, ul, li, img) for HTML expansion.

void TestEmmetParser::testExpandBasicTag()
{
    EmmetParser parser;
    QCOMPARE(parser.expandAbbreviation("span"), QString("<span></span>"));
}

void TestEmmetParser::testExpandNesting()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("section>p");
    QVERIFY(out.contains("<section>"));
    QVERIFY(out.contains("<p></p>"));
    QVERIFY(out.contains("</section>"));
}

void TestEmmetParser::testExpandSiblings()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("span+em");
    QCOMPARE(out.count("<span>"), 1);
    QCOMPARE(out.count("<em>"), 1);
    QVERIFY(out.indexOf("<span>") < out.indexOf("<em>"));
}

void TestEmmetParser::testExpandMultiplication()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("li*3");
    QCOMPARE(out.count("<li></li>"), 3);
}

void TestEmmetParser::testExpandClassesAndId()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("span.container#main");
    QVERIFY(out.contains("id=\"main\""));
    QVERIFY(out.contains("class=\"container\""));
    QVERIFY(out.contains("<span"));
}

void TestEmmetParser::testExpandTextContent()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("span{Hello}");
    QVERIFY(out.contains("Hello"));
    QVERIFY(out.contains("</span>"));
}

void TestEmmetParser::testExpandAttributes()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("span[data-id=5]");
    QVERIFY(out.contains("data-id=\"5\""));
}

void TestEmmetParser::testExpandSelfClosing()
{
    EmmetParser parser;
    QString out = parser.expandAbbreviation("img");
    QVERIFY(out.contains("<img"));
    QVERIFY(out.contains("/>"));
    QVERIFY(!out.contains("</img>"));
}

void TestEmmetParser::testExpandEmpty()
{
    EmmetParser parser;
    QVERIFY(parser.expandAbbreviation("").isEmpty());
    QVERIFY(parser.expandAbbreviation("   ").isEmpty());
}

void TestEmmetParser::testIsAbbreviation()
{
    QVERIFY(EmmetParser::isAbbreviation("span>p"));
    QVERIFY(EmmetParser::isAbbreviation("ul>li*3"));
    QVERIFY(!EmmetParser::isAbbreviation(""));
    QVERIFY(!EmmetParser::isAbbreviation("plain text"));
}

void TestEmmetParser::testExtractAbbreviation()
{
    QString text = "  div>p";
    // cursor at end => whole abbreviation
    QString abbr = EmmetParser::extractAbbreviation(text, 7);
    QCOMPARE(abbr, QString("div>p"));

    // cursor before 'p' => only "div>"
    QCOMPARE(EmmetParser::extractAbbreviation(text, 6), QString("div>"));

    QVERIFY(EmmetParser::extractAbbreviation("", 0).isEmpty());
}

void TestEmmetParser::testCSSShorthand()
{
    EmmetParser parser;
    QCOMPARE(parser.expandAbbreviation("m1"), QString("margin: 1px;"));
    QCOMPARE(parser.expandAbbreviation("p1"), QString("padding: 1px;"));
    QCOMPARE(parser.expandAbbreviation("fs1"), QString("font-size: 1px;"));
}

void TestEmmetParser::testIsCSSShorthand()
{
    QVERIFY(EmmetParser::isCSSShorthand("m1"));
    QVERIFY(EmmetParser::isCSSShorthand("p1"));
    QVERIFY(EmmetParser::isCSSShorthand("fs1"));
    QVERIFY(EmmetParser::isCSSShorthand("bgc#fff"));
    QVERIFY(EmmetParser::isCSSShorthand("c#fff"));
    QVERIFY(!EmmetParser::isCSSShorthand("m10"));
    QVERIFY(!EmmetParser::isCSSShorthand("p20-10"));
    QVERIFY(!EmmetParser::isCSSShorthand("fs14"));
    QVERIFY(!EmmetParser::isCSSShorthand(""));
}

void TestEmmetParser::testParseTag()
{
    EmmetParser::TagInfo info = EmmetParser::parseTag("a.link#home[href=/x]{Go}");
    QCOMPARE(info.tag, QString("a"));
    QCOMPARE(info.id, QString("home"));
    QVERIFY(info.classes.contains("link"));
    QCOMPARE(info.attributes.value("href"), QString("/x"));
    QCOMPARE(info.text, QString("Go"));

    // Default tag
    EmmetParser::TagInfo def = EmmetParser::parseTag(".box");
    QCOMPARE(def.tag, QString("div"));
}

void TestEmmetParser::testExpandCSSShorthandNoMatch()
{
    QCOMPARE(EmmetParser::expandCSSShorthand("xyzzy"), QString("xyzzy"));
}

void TestEmmetParser::testExpandCSSShorthandPxSuffix()
{
    QCOMPARE(EmmetParser::expandCSSShorthand("m10"), QString("margin: 10px;"));
    QCOMPARE(EmmetParser::expandCSSShorthand("c#fff"), QString("color: #fff;"));
}

void TestEmmetParser::testExpandCSSShorthandUnits()
{
    // Values with explicit units must not get a px suffix
    QString out = EmmetParser::expandCSSShorthand("fs1em");
    QCOMPARE(out, QString("font-size: 1em;"));
    out = EmmetParser::expandCSSShorthand("fs100%");
    QCOMPARE(out, QString("font-size: 100%;"));
}
