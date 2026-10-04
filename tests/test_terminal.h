#ifndef TEST_TERMINAL_H
#define TEST_TERMINAL_H

#include <QObject>

class TerminalBuffer;

class TestTerminal : public QObject
{
    Q_OBJECT
private slots:
    // Emulator — buffer and printing
    void testPrintAndCursorAdvance();
    void testCarriageReturnAndLineFeed();
    void testAutowrapAtRightMargin();
    void testWrapDisabled();
    void testScrollbackAccumulates();
    void testHistoryLimitTrimsOldestLines();

    // Emulator — cursor control and erasing
    void testCursorPosition();
    void testCursorRelativeMovement();
    void testEraseInLine();
    void testEraseInDisplay();
    void testScrollRegionAndIndexing();
    void testInsertAndDeleteChars();

    // Emulator — attributes and colours
    void testSgrColors();
    void testSgrBoldAndReverse();
    void testTrueColor();
    void testUnderlineAndStrikeAttributes();

    // Emulator — modes, titles, replies
    void testOscTitle();
    void testAlternateScreenRoundTrip();
    void testPrivateModes();
    void testCursorPositionReport();
    void testDeviceAttributesReport();
    void testParseParams();

    // Emulator — text handling
    void testUtf8AndWideCharacters();
    void testDecodeUtf8ReplacesInvalid();

    // Emulator — input encoding
    void testEncodeKeyPlainText();
    void testEncodeKeyControl();
    void testEncodeKeyCursorAndModifiers();
    void testEncodeKeyApplicationCursor();
    void testEncodeMouse();

    // Session
    void testSessionRunsCommandAndReportsOutput();
    void testSessionReportsExitCode();
    void testSessionFailureOnMissingProgram();
    void testSessionDefaultShell();

    // View
    void testViewFollowsOutput();
    void testViewSelectionCopy();
    void testViewFontSizeClamped();

    // Panel
    void testPanelRunsCommandInTerminal();
    void testPanelSessionLifecycle();
};

// Helpers shared by the cases below.
QString rowText(TerminalBuffer *buffer, int screenRow);

#endif // TEST_TERMINAL_H