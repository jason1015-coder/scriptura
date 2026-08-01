#ifndef TEST_CODELENSMANAGER_H
#define TEST_CODELENSMANAGER_H

#include <QObject>

class TestCodeLensManager : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetEnabledOffClears();
    void testSetEnabledSameValue();
    void testClearAll();
    void testClearDocument();
    void testClearDocumentNonexistent();
    void testItemsAtLine();
    void testItemsAtLineNone();
    void testRequestCodeLensDisabled();
    void testParseCodeLensFull();
    void testParseCodeLensMissingRange();
    void testReceiveLensFiltersEmptyTitles();
};

#endif // TEST_CODELENSMANAGER_H
