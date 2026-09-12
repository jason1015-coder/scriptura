#ifndef TEST_FOLDMANAGER_H
#define TEST_FOLDMANAGER_H

#include <QObject>

class TestFoldManager : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testDetectBraceRegions();
    void testNoBraceRegions();
    void testToggleFold();
    void testFoldAllUnfoldAll();
    void testFoldAtLevel();
    void testUnfoldAtLevel();
    void testIsFolded();
    void testIsFoldStartEnd();
    void testRegionAtInvalid();
    void testVisibleLineCount();
    void testIsLineHidden();
    void testNestedBraceFolds();
    void testKeywordFolds();
    // Regression tests for the MANUAL_TEST_LOG fixes
    void testFoldingHidesTextBlocks();
    void testUnfoldingRestoresTextBlocks();
    void testNestedFoldVisibleLineBehavior();
    void testFoldIndicatorPaintDoesNotCrash();
    void testReattachDocumentOnNewDocument();
};

#endif // TEST_FOLDMANAGER_H
