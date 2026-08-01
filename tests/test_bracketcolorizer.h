#ifndef TEST_BRACKETCOLORIZER_H
#define TEST_BRACKETCOLORIZER_H

#include <QObject>

class TestBracketColorizer : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testEmptyDocument();
    void testSimplePair();
    void testNestedPairs();
    void testMismatchedBrackets();
    void testStringsIgnored();
    void testLineCommentsIgnored();
    void testDepthAt();
    void testPairAtInvalid();
    void testSetEnabledOff();
    void testSetEnabledOn();
    void testCustomColors();
    void testSetBracketColorsWhenDisabled();
    void testClearColors();
    void testExtraSelectionsCount();
    void testRepeatedBracketDepth();
};

#endif // TEST_BRACKETCOLORIZER_H
