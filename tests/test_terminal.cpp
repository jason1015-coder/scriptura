#include <QClipboard>
#include <QGuiApplication>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>

#include "terminal/terminalemulator.h"
#include "terminal/terminalpanel.h"
#include "terminal/terminalsession.h"
#include "terminal/terminalwidget.h"
#include "test_terminal.h"

QString rowText(TerminalBuffer *buffer, int screenRow)
{
    QString text = buffer->mutableLine(screenRow).toText();
    while (text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    return text;
}

/// Whole visible screen, trailing blanks stripped per row, blank rows dropped.
QString screenText(TerminalBuffer *buffer)
{
    QStringList rows;
    for (int r = 0; r < buffer->rows(); ++r) {
        const QString text = rowText(buffer, r);
        if (!text.isEmpty())
            rows << text;
    }
    return rows.join(QLatin1Char('\n'));
}

// ── Buffer: printing ───────────────────────────────────────────────────────

void TestTerminal::testPrintAndCursorAdvance()
{
    TerminalEmulator emu;
    emu.resize(5, 20);
    emu.feed("hi");

    QCOMPARE(rowText(emu.buffer(), 0), QString("hi"));
    QCOMPARE(emu.buffer()->cursor().col, 2);
    QCOMPARE(emu.buffer()->cursor().row, 0);
}

void TestTerminal::testCarriageReturnAndLineFeed()
{
    TerminalEmulator emu;
    emu.resize(5, 20);
    emu.feed("ab\r\ncd");

    QCOMPARE(rowText(emu.buffer(), 0), QString("ab"));
    QCOMPARE(rowText(emu.buffer(), 1), QString("cd"));
    QCOMPARE(emu.buffer()->cursor().row, 1);
    QCOMPARE(emu.buffer()->cursor().col, 2);

    // CR alone returns to column zero, so the next glyphs overwrite in place.
    emu.feed("\rXY");
    QCOMPARE(rowText(emu.buffer(), 1), QString("XY"));
    QCOMPARE(emu.buffer()->cursor().col, 2);
}

void TestTerminal::testAutowrapAtRightMargin()
{
    TerminalEmulator emu;
    emu.resize(3, 4);
    emu.buffer()->clearScrollback();
    emu.feed("abcd");

    // Writing the last column only arms the wrap: the cursor stays put until a
    // further glyph arrives (DECAWM), so a right-margin prompt does not scroll.
    QCOMPARE(rowText(emu.buffer(), 0), QString("abcd"));
    QCOMPARE(emu.buffer()->cursor().row, 0);
    QCOMPARE(emu.buffer()->cursor().col, 3);
    QVERIFY(emu.buffer()->wrapPending());

    emu.feed("e");
    QCOMPARE(rowText(emu.buffer(), 0), QString("abcd"));
    QCOMPARE(rowText(emu.buffer(), 1), QString("e"));
    QVERIFY(emu.buffer()->line(0).wrapped);
}

void TestTerminal::testWrapDisabled()
{
    TerminalEmulator emu;
    emu.resize(3, 4);
    emu.buffer()->clearScrollback();
    emu.feed("\x1b[?7l"); // DECAWM off
    QVERIFY(!emu.buffer()->autoWrap());

    emu.feed("abcdef");
    // 'e' and 'f' both land in the last column: there is no next row to wrap to.
    QCOMPARE(rowText(emu.buffer(), 0), QString("abcf"));
    // With no autowrap the extra glyphs overwrite the last column.
    QCOMPARE(emu.buffer()->line(0).cells.at(3).ch, U'f');
    QCOMPARE(emu.buffer()->cursor().row, 0);
}

void TestTerminal::testScrollbackAccumulates()
{
    TerminalEmulator emu;
    emu.resize(3, 10);
    emu.buffer()->clearScrollback();
    emu.feed("one\r\ntwo\r\nthree\r\nfour");

    QCOMPARE(emu.buffer()->historyCount(), 1);
    QCOMPARE(emu.buffer()->totalLines(), 4);
    QCOMPARE(emu.buffer()->textAt(0).left(3), QString("one"));
    QCOMPARE(rowText(emu.buffer(), 0), QString("two"));
    QCOMPARE(rowText(emu.buffer(), 1), QString("three"));
    QCOMPARE(rowText(emu.buffer(), 2), QString("four"));
}

void TestTerminal::testHistoryLimitTrimsOldestLines()
{
    TerminalEmulator emu;
    emu.resize(2, 10);
    emu.buffer()->clearScrollback();
    emu.buffer()->setHistoryLimit(3);

    for (int i = 0; i < 8; ++i)
        emu.feed(QByteArray("line") + QByteArray::number(i) + "\r\n");

    QCOMPARE(emu.buffer()->historyCount(), 3);
    // The oldest lines are the ones dropped.
    QCOMPARE(emu.buffer()->textAt(0).trimmed(), QString("line4"));
    QCOMPARE(emu.buffer()->textAt(1).trimmed(), QString("line5"));
    QCOMPARE(emu.buffer()->textAt(2).trimmed(), QString("line6"));
}

// ── Buffer: cursor movement and erasing ────────────────────────────────────

void TestTerminal::testCursorPosition()
{
    TerminalEmulator emu;
    emu.resize(10, 20);
    emu.feed("\x1b[3;5H");
    QCOMPARE(emu.buffer()->cursor().row, 2);
    QCOMPARE(emu.buffer()->cursor().col, 4);
    emu.feed("x");
    QCOMPARE(emu.buffer()->cursor().col, 5);
    QCOMPARE(rowText(emu.buffer(), 2).left(5), QString("    x"));

    // Out-of-range coordinates clamp to the grid.
    emu.feed("\x1b[99;99H");
    QCOMPARE(emu.buffer()->cursor().row, 9);
    QCOMPARE(emu.buffer()->cursor().col, 19);
}

void TestTerminal::testCursorRelativeMovement()
{
    TerminalEmulator emu;
    emu.resize(10, 20);
    emu.feed("\x1b[5;5H");
    emu.feed("\x1b[2A"); // CUU
    QCOMPARE(emu.buffer()->cursor().row, 2);
    emu.feed("\x1b[3B"); // CUD
    QCOMPARE(emu.buffer()->cursor().row, 5);
    emu.feed("\x1b[4C"); // CUF
    QCOMPARE(emu.buffer()->cursor().col, 8);
    emu.feed("\x1b[2D"); // CUB
    QCOMPARE(emu.buffer()->cursor().col, 6);

    // A bare parameter counts as one, and movement never leaves the screen.
    emu.feed("\x1b[99A");
    QCOMPARE(emu.buffer()->cursor().row, 0);
    emu.feed("\x1b[99D");
    QCOMPARE(emu.buffer()->cursor().col, 0);
}

void TestTerminal::testEraseInLine()
{
    TerminalEmulator emu;
    emu.resize(3, 10);

    emu.feed("abcdef\x1b[1;4H\x1b[K"); // EL 0: cursor to end of line
    QCOMPARE(rowText(emu.buffer(), 0), QString("abc"));

    emu.feed("\x1b[2J\x1b[1;1Habcdef\x1b[1;4H\x1b[1K"); // EL 1: start to cursor
    QCOMPARE(rowText(emu.buffer(), 0), QString("    ef"));

    emu.feed("\x1b[2J\x1b[1;1Habcdef\x1b[1;4H\x1b[2K"); // EL 2: whole line
    QVERIFY(rowText(emu.buffer(), 0).isEmpty());
}

void TestTerminal::testEraseInDisplay()
{
    TerminalEmulator emu;
    emu.resize(4, 10);
    emu.feed("aaa\r\nbbb\r\nccc\r\nddd");
    emu.feed("\x1b[2;2H\x1b[J"); // ED 0: cursor to end of screen
    QCOMPARE(rowText(emu.buffer(), 0), QString("aaa"));
    QCOMPARE(rowText(emu.buffer(), 1), QString("b"));
    QVERIFY(rowText(emu.buffer(), 2).isEmpty());
    QVERIFY(rowText(emu.buffer(), 3).isEmpty());

    emu.feed("\x1b[2J\x1b[1;1Haaa\r\nbbb\r\nccc\x1b[2;2H\x1b[1J"); // ED 1
    QVERIFY(rowText(emu.buffer(), 0).isEmpty());
    QCOMPARE(rowText(emu.buffer(), 1), QString("  b"));
    QCOMPARE(rowText(emu.buffer(), 2), QString("ccc"));

    emu.feed("\x1b[2J"); // ED 2: everything
    QCOMPARE(screenText(emu.buffer()), QString());

    // ED 3 drops the scrollback only.
    emu.feed("one\r\ntwo\r\nthree\r\nfour");
    QVERIFY(emu.buffer()->historyCount() > 0);
    emu.feed("\x1b[3J");
    QCOMPARE(emu.buffer()->historyCount(), 0);
}

void TestTerminal::testScrollRegionAndIndexing()
{
    TerminalEmulator emu;
    emu.resize(6, 10);
    emu.feed("\x1b[2;4r"); // rows 2..4 become the scroll region
    QCOMPARE(emu.buffer()->scrollTop(), 1);
    QCOMPARE(emu.buffer()->scrollBottom(), 3);
    // DECSTBM homes the cursor inside the new region.
    QCOMPARE(emu.buffer()->cursor().row, 1);
    QCOMPARE(emu.buffer()->cursor().col, 0);

    // Scrolling inside the region must not disturb the rows outside it.
    emu.feed("a\r\nb\r\nc\r\nd");
    QCOMPARE(rowText(emu.buffer(), 0), QString(""));
    QCOMPARE(rowText(emu.buffer(), 4), QString(""));
}

void TestTerminal::testInsertAndDeleteChars()
{
    TerminalEmulator emu;
    emu.resize(3, 10);

    emu.feed("abcdef\x1b[1;1H\x1b[2@"); // ICH 2 at column 0
    QCOMPARE(rowText(emu.buffer(), 0), QString("  abcdef"));

    emu.feed("\x1b[2J\x1b[1;1Habcdef\x1b[1;3H\x1b[2P"); // DCH 2 at column 2
    QCOMPARE(rowText(emu.buffer(), 0), QString("abef"));

    emu.feed("\x1b[2J\x1b[1;1Habcdef\x1b[1;3H\x1b[2X"); // ECH 2 at column 2
    QCOMPARE(rowText(emu.buffer(), 0), QString("ab  ef"));
}

// ── Attributes and colours ─────────────────────────────────────────────────

void TestTerminal::testSgrColors()
{
    TerminalEmulator emu;
    emu.resize(2, 10);
    emu.feed("\x1b[31;42ma");
    const TermCell cell = emu.buffer()->cellAt(0, 0);
    QCOMPARE(cell.fg, TermColor(1));
    QCOMPARE(cell.bg, TermColor(2));
    QCOMPARE(cell.ch, U'a');

    // Bright colours use the 90-97 / 100-107 ranges.
    emu.feed("\x1b[0m\x1b[91;104mb");
    const TermCell bright = emu.buffer()->cellAt(0, 1);
    QCOMPARE(bright.fg, TermColor(9));
    QCOMPARE(bright.bg, TermColor(12));

    // SGR 0 clears the pen, and 39/49 reset only the colour.
    emu.feed("\x1b[0mc");
    QCOMPARE(emu.buffer()->cellAt(0, 2).fg, kColorDefault);
    emu.feed("\x1b[31;42md\x1b[39me");
    QCOMPARE(emu.buffer()->cellAt(0, 4).fg, kColorDefault);
    QCOMPARE(emu.buffer()->cellAt(0, 4).bg, TermColor(2));

    // Palette resolution: index 1 is the dark red of the standard palette.
    QCOMPARE(emu.paletteColor(1), QColor(128, 0, 0));
    QCOMPARE(emu.paletteColor(9), QColor(255, 0, 0));
    QCOMPARE(emu.paletteColor(232), QColor(8, 8, 8));
}

void TestTerminal::testSgrBoldAndReverse()
{
    TerminalEmulator emu;
    emu.resize(2, 10);
    emu.setDefaultForeground(QColor(10, 10, 10));
    emu.setDefaultBackground(QColor(200, 200, 200));

    emu.feed("\x1b[31;1ma");
    const TermCell bold = emu.buffer()->cellAt(0, 0);
    QVERIFY(bold.attrs & AttrBold);
    // Bold promotes palette colour 1 to its bright variant.
    QCOMPARE(emu.cellForeground(bold), emu.paletteColor(9));

    // SGR 7 reverses the cells written from now on.
    emu.feed("\x1b[0m\x1b[7m\x1b[1;2Hb");
    const TermCell reverse = emu.buffer()->cellAt(0, 1);
    QVERIFY(reverse.attrs & AttrReverse);
    QCOMPARE(emu.cellForeground(reverse), emu.resolveBackground(reverse.bg));
    QCOMPARE(emu.cellBackground(reverse), emu.resolveForeground(reverse.fg));

    // DECSCNM reverses the whole screen at once and the program keeps writing:
    // only the painted colours change, not what was stored.
    emu.feed("\x1b[0m\x1b[?5h\x1b[31;42m\x1b[1;5Hc");
    QVERIFY(emu.reverseVideo());
    const TermCell global = emu.buffer()->cellAt(0, 4);
    QCOMPARE(global.fg, TermColor(1));
    QCOMPARE(global.bg, TermColor(2));
    QCOMPARE(emu.cellForeground(global), emu.resolveBackground(TermColor(2)));
    QCOMPARE(emu.cellBackground(global), emu.resolveForeground(TermColor(1)));

    // Dim fades the colour towards the background instead of leaving it alone.
    emu.feed("\x1b[?5l\x1b[0m\x1b[2m\x1b[1;6Hd");
    const TermCell dim = emu.buffer()->cellAt(0, 5);
    QVERIFY(!emu.reverseVideo());
    QVERIFY(dim.attrs & AttrDim);
    QVERIFY(emu.cellForeground(dim) != emu.resolveForeground(dim.fg));
}

void TestTerminal::testTrueColor()
{
    TerminalEmulator emu;
    emu.resize(2, 10);
    emu.feed("\x1b[38;2;18;52;86ma");
    const TermCell cell = emu.buffer()->cellAt(0, 0);
    QVERIFY(isTrueColor(cell.fg));
    QCOMPARE(emu.cellForeground(cell), QColor(18, 52, 86));

    // The ITU sub-parameter form carries a colour-space id.
    emu.feed("\x1b[38:2:0:1:2:3mb");
    QCOMPARE(emu.cellForeground(emu.buffer()->cellAt(0, 1)), QColor(1, 2, 3));

    // 256-colour index.
    emu.feed("\x1b[38;5;196mc");
    QCOMPARE(emu.cellForeground(emu.buffer()->cellAt(0, 2)), emu.paletteColor(196));

    emu.feed("\x1b[48;2;10;20;30md");
    QCOMPARE(emu.cellBackground(emu.buffer()->cellAt(0, 3)), QColor(10, 20, 30));
}

void TestTerminal::testUnderlineAndStrikeAttributes()
{
    TerminalEmulator emu;
    emu.resize(2, 10);
    emu.feed("\x1b[4;9ma");
    const TermCell cell = emu.buffer()->cellAt(0, 0);
    QVERIFY(cell.attrs & AttrUnderline);
    QVERIFY(cell.attrs & AttrStrike);
    QVERIFY(!(cell.attrs & AttrItalic));

    emu.feed("\x1b[24;29mb");
    const TermCell cleared = emu.buffer()->cellAt(0, 1);
    QVERIFY(!(cleared.attrs & AttrUnderline));
    QVERIFY(!(cleared.attrs & AttrStrike));
}

// ── Modes, titles, replies ─────────────────────────────────────────────────

void TestTerminal::testOscTitle()
{
    TerminalEmulator emu;
    QSignalSpy spy(&emu, &TerminalEmulator::titleChanged);

    emu.feed("\x1b]0;first title\x07");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(emu.currentTitle(), QString("first title"));

    // ST (ESC \) terminates as well as BEL.
    emu.feed("\x1b]2;second title\x1b\\");
    QCOMPARE(emu.currentTitle(), QString("second title"));
    QCOMPARE(spy.count(), 2);

    // The same title again is not re-announced.
    emu.feed("\x1b]2;second title\x07");
    QCOMPARE(spy.count(), 2);
}

void TestTerminal::testAlternateScreenRoundTrip()
{
    TerminalEmulator emu;
    emu.resize(4, 10);
    emu.feed("primary\r\nscrollback\r\n");
    const int historyBefore = emu.buffer()->historyCount();
    emu.feed("\x1b[Hkept");
    const QString primaryRow = rowText(emu.buffer(), 0);
    QCOMPARE(primaryRow, QString("keptary"));

    emu.feed("\x1b[?1049h");
    QVERIFY(emu.alternateScreen());
    // The alternate screen starts empty and keeps no scrollback.
    QCOMPARE(screenText(emu.buffer()), QString());
    QCOMPARE(emu.buffer()->historyCount(), 0);

    emu.feed("fullscreen app");
    QCOMPARE(rowText(emu.buffer(), 0), QString("fullscreen"));
    QCOMPARE(rowText(emu.buffer(), 1), QString(" app"));

    emu.feed("\x1b[?1049l");
    QVERIFY(!emu.alternateScreen());
    // Everything is back, including the scrollback that existed before, and none
    // of what was drawn on the alternate screen leaked into the primary one.
    QCOMPARE(rowText(emu.buffer(), 0), primaryRow);
    QVERIFY(!screenText(emu.buffer()).contains(QStringLiteral("fullscreen")));
    QCOMPARE(emu.buffer()->historyCount(), historyBefore);
    QCOMPARE(rowText(emu.buffer(), 1), QString("scrollback"));
}

void TestTerminal::testPrivateModes()
{
    TerminalEmulator emu;
    emu.resize(6, 20);

    emu.feed("\x1b[?1h");
    QVERIFY(emu.applicationCursorKeys());
    emu.feed("\x1b[?1l");
    QVERIFY(!emu.applicationCursorKeys());

    emu.feed("\x1b[?25l");
    QVERIFY(!emu.cursorVisible());
    emu.feed("\x1b[?25h");
    QVERIFY(emu.cursorVisible());

    emu.feed("\x1b[?1002h");
    QCOMPARE(emu.mouseMode(), 1002);
    emu.feed("\x1b[?1006h");
    QVERIFY(emu.mouseSgr());
    emu.feed("\x1b[?1002l");
    QCOMPARE(emu.mouseMode(), 0);

    emu.feed("\x1b[?1004h");
    QVERIFY(emu.focusEventsEnabled());
    emu.feed("\x1b[?2004h");
    QVERIFY(emu.bracketedPaste());

    // DECOM makes CUP relative to the scroll region origin.
    emu.feed("\x1b[3;6r\x1b[?6h\x1b[1;1H");
    QCOMPARE(emu.buffer()->cursor().row, 2);
}

void TestTerminal::testCursorPositionReport()
{
    TerminalEmulator emu;
    emu.resize(24, 80);
    QSignalSpy spy(&emu, &TerminalEmulator::requestWrite);

    emu.feed("\x1b[3;7H\x1b[6n");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray("\x1b[3;7R"));

    emu.feed("\x1b[?6n");
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toByteArray(), QByteArray("\x1b[?3;7;1R"));
}

void TestTerminal::testDeviceAttributesReport()
{
    TerminalEmulator emu;
    emu.resize(24, 80);
    QSignalSpy spy(&emu, &TerminalEmulator::requestWrite);

    emu.feed("\x1b[c");
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray("\x1b[?1;2c"));

    emu.feed("\x1b[5n");
    QCOMPARE(spy.at(1).at(0).toByteArray(), QByteArray("\x1b[0n"));
}

void TestTerminal::testParseParams()
{
    QList<int> empty = TerminalEmulator::parseParams(QByteArray());
    QVERIFY(empty.isEmpty());

    const QList<int> single = TerminalEmulator::parseParams(QByteArray("5"));
    QCOMPARE(single, QList<int>({5}));

    const QList<int> multi = TerminalEmulator::parseParams(QByteArray("1;2;3"));
    QCOMPARE(multi, QList<int>({1, 2, 3}));

    // An omitted middle parameter is a defaulted zero (the ECMA-48 form).
    const QList<int> gaps = TerminalEmulator::parseParams(QByteArray("1;;3"));
    QCOMPARE(gaps.size(), 3);
    QCOMPARE(gaps.at(0), 1);
    QCOMPARE(gaps.at(1), 0);
    QCOMPARE(gaps.at(2), 3);

    // ":" is the ITU sub-parameter separator and stays inside the same field.
    const QList<int> sub = TerminalEmulator::parseParams(QByteArray("38:2:0:1:2:3"));
    QCOMPARE(sub, QList<int>({38, 2, 0, 1, 2, 3}));
}

// ── Text handling ──────────────────────────────────────────────────────────

void TestTerminal::testUtf8AndWideCharacters()
{
    QCOMPARE(termCharWidth(U'a'), 1);
    QCOMPARE(termCharWidth(0x4E00), 2); // CJK
    QCOMPARE(termCharWidth(0x1F600), 2); // emoji
    QCOMPARE(termCharWidth(U'\0'), 0);

    TerminalEmulator emu;
    emu.resize(3, 10);
    emu.feed(QString::fromUtf8("a\xE4\xB8\xAD").toUtf8()); // "a中"

    QCOMPARE(emu.buffer()->cellAt(0, 0).ch, U'a');
    QCOMPARE(emu.buffer()->cellAt(0, 1).ch, U'中');
    QCOMPARE(emu.buffer()->cellAt(0, 1).width, quint8(2));
    // The trailing half of the wide glyph is marked as a continuation.
    QVERIFY(emu.buffer()->cellAt(0, 2).isContinuation());
    QCOMPARE(emu.buffer()->cursor().col, 3);
    QCOMPARE(rowText(emu.buffer(), 0), QString::fromUtf8("a中"));

    // A split multi-byte sequence survives being fed one byte at a time.
    TerminalEmulator emu2;
    emu2.resize(2, 10);
    const QByteArray utf8 = QString::fromUtf8("中").toUtf8();
    for (char c : utf8)
        emu2.feed(c);
    QCOMPARE(emu2.buffer()->cellAt(0, 0).ch, U'中');

    // Invalid bytes become the replacement character instead of being dropped.
    QCOMPARE(termDecodeUtf8(QByteArray("a\xFF" "b")).size(), 5); // 'a' + U+FFFD + 'b'
    QVERIFY(termDecodeUtf8(QByteArray()).isEmpty());
}

void TestTerminal::testDecodeUtf8ReplacesInvalid()
{
    // Both bytes of the truncated sequence are reported as invalid rather than
    // being held back for a continuation that this one-shot call never sees.
    const QByteArray truncated("\xE4\xB8", 2);
    QCOMPARE(termDecodeUtf8(truncated), QByteArray("\xEF\xBF\xBD\xEF\xBF\xBD"));
}

// ── Input encoding ─────────────────────────────────────────────────────────

void TestTerminal::testEncodeKeyPlainText()
{
    TerminalEmulator emu;

    QCOMPARE(emu.encodeKey(Qt::Key_A, Qt::NoModifier, QStringLiteral("a")), QByteArray("a"));
    QCOMPARE(emu.encodeKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r")), QByteArray("\r"));
    QCOMPARE(emu.encodeKey(Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t")), QByteArray("\t"));
    QCOMPARE(emu.encodeKey(Qt::Key_Backspace, Qt::NoModifier, QString()), QByteArray("\x7f"));
    QCOMPARE(emu.encodeKey(Qt::Key_Escape, Qt::NoModifier, QString()), QByteArray("\x1b"));
    QCOMPARE(emu.encodeKey(Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ")), QByteArray(" "));

    // Alt prefixes the glyph with ESC, the "meta sends escape" convention.
    QCOMPARE(emu.encodeKey(Qt::Key_B, Qt::AltModifier, QStringLiteral("b")),
             QByteArray("\x1b" "b"));

    // Shift+Tab is the reverse-tab sequence, not a literal tab.
    QCOMPARE(emu.encodeKey(Qt::Key_Tab, Qt::ShiftModifier, QStringLiteral("\t")),
             QByteArray("\x1b[Z"));
}

void TestTerminal::testEncodeKeyControl()
{
    TerminalEmulator emu;

    QCOMPARE(emu.encodeKey(Qt::Key_C, Qt::ControlModifier, QStringLiteral("c")),
             QByteArray(1, '\x03'));
    QCOMPARE(emu.encodeKey(Qt::Key_D, Qt::ControlModifier, QStringLiteral("d")),
             QByteArray(1, '\x04'));
    QCOMPARE(emu.encodeKey(Qt::Key_Z, Qt::ControlModifier, QStringLiteral("z")),
             QByteArray(1, '\x1a'));
    // Ctrl+@ and Ctrl+Space are NUL.
    QCOMPARE(emu.encodeKey(Qt::Key_At, Qt::ControlModifier, QStringLiteral("@")),
             QByteArray(1, '\0'));
    // Ctrl+[ is ESC.
    QCOMPARE(emu.encodeKey(Qt::Key_BracketLeft, Qt::ControlModifier, QStringLiteral("[")),
             QByteArray("\x1b"));
    // Ctrl+Backspace is the ASCII backspace, not DEL.
    QCOMPARE(emu.encodeKey(Qt::Key_Backspace, Qt::ControlModifier, QString()),
             QByteArray(1, '\x08'));
}

void TestTerminal::testEncodeKeyCursorAndModifiers()
{
    TerminalEmulator emu;

    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::NoModifier, QString()), QByteArray("\x1b[A"));
    QCOMPARE(emu.encodeKey(Qt::Key_Down, Qt::NoModifier, QString()), QByteArray("\x1b[B"));
    QCOMPARE(emu.encodeKey(Qt::Key_Right, Qt::NoModifier, QString()), QByteArray("\x1b[C"));
    QCOMPARE(emu.encodeKey(Qt::Key_Left, Qt::NoModifier, QString()), QByteArray("\x1b[D"));
    QCOMPARE(emu.encodeKey(Qt::Key_Home, Qt::NoModifier, QString()), QByteArray("\x1b[H"));
    QCOMPARE(emu.encodeKey(Qt::Key_End, Qt::NoModifier, QString()), QByteArray("\x1b[F"));
    QCOMPARE(emu.encodeKey(Qt::Key_Delete, Qt::NoModifier, QString()), QByteArray("\x1b[3~"));
    QCOMPARE(emu.encodeKey(Qt::Key_Insert, Qt::NoModifier, QString()), QByteArray("\x1b[2~"));
    QCOMPARE(emu.encodeKey(Qt::Key_F1, Qt::NoModifier, QString()), QByteArray("\x1bOP"));
    QCOMPARE(emu.encodeKey(Qt::Key_F5, Qt::NoModifier, QString()), QByteArray("\x1b[15~"));

    // xterm's modifier parameter is 1 + shift(1) + alt(2) + ctrl(4).
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::ShiftModifier, QString()), QByteArray("\x1b[1;2A"));
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::ControlModifier, QString()), QByteArray("\x1b[1;5A"));
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::ControlModifier | Qt::ShiftModifier, QString()),
             QByteArray("\x1b[1;6A"));
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::AltModifier | Qt::ControlModifier, QString()),
             QByteArray("\x1b[1;7A"));
    QCOMPARE(emu.encodeKey(Qt::Key_Home, Qt::ShiftModifier, QString()), QByteArray("\x1b[1;2H"));

    // An unhandled key reports itself as such so the host can ignore it.
    bool handled = true;
    const QByteArray data = emu.encodeKey(Qt::Key_Shift, Qt::NoModifier, QString(), &handled);
    QVERIFY(handled);
    QVERIFY(data.isEmpty());
}

void TestTerminal::testEncodeKeyApplicationCursor()
{
    TerminalEmulator emu;
    emu.feed("\x1b[?1h");

    // DECCKM switches the arrow keys to their SS3 form.
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::NoModifier, QString()), QByteArray("\x1bOA"));
    QCOMPARE(emu.encodeKey(Qt::Key_Left, Qt::NoModifier, QString()), QByteArray("\x1bOD"));
    // With a modifier the parameter form is still used.
    QCOMPARE(emu.encodeKey(Qt::Key_Up, Qt::ControlModifier, QString()), QByteArray("\x1b[1;5A"));
}

void TestTerminal::testEncodeMouse()
{
    TerminalEmulator emu;

    // Nothing is reported until a mouse mode is enabled.
    QVERIFY(emu.encodeMouse(0, 1, 1, true, false, Qt::NoModifier).isEmpty());

    emu.feed("\x1b[?1000h");
    // Legacy X10 encoding: button + 32, then col + 32, row + 32.
    QCOMPARE(emu.encodeMouse(0, 5, 3, true, false, Qt::NoModifier),
             QByteArray("\x1b[M") + QByteArray(1, char(32)) + QByteArray(1, char(37))
                 + QByteArray(1, char(35)));
    // A release cannot name its button in the legacy encoding.
    QCOMPARE(emu.encodeMouse(0, 5, 3, false, true, Qt::NoModifier),
             QByteArray("\x1b[M") + QByteArray(1, char(35)) + QByteArray(1, char(37))
                 + QByteArray(1, char(35)));
    QCOMPARE(emu.encodeMouse(0, 5, 3, true, false, Qt::ShiftModifier),
             QByteArray("\x1b[M") + QByteArray(1, char(36)) + QByteArray(1, char(37))
                 + QByteArray(1, char(35)));

    emu.feed("\x1b[?1006h");
    QCOMPARE(emu.encodeMouse(0, 5, 3, true, false, Qt::NoModifier),
             QByteArray("\x1b[<0;5;3M"));
    QCOMPARE(emu.encodeMouse(0, 5, 3, false, true, Qt::NoModifier),
             QByteArray("\x1b[<0;5;3m"));
    QCOMPARE(emu.encodeMouse(2, 10, 20, true, false, Qt::ControlModifier),
             QByteArray("\x1b[<10;10;20M"));
}

// ── Session ────────────────────────────────────────────────────────────────

void TestTerminal::testSessionRunsCommandAndReportsOutput()
{
#ifdef Q_OS_UNIX
    TerminalSession session;
    QSignalSpy dataSpy(&session, &TerminalSession::dataReceived);
    QSignalSpy startedSpy(&session, &TerminalSession::started);

    QString error;
    QVERIFY2(session.start("/bin/sh", {QStringLiteral("-c"), QStringLiteral("printf hello")},
                           QDir::homePath(), QProcessEnvironment::systemEnvironment(), &error),
             qPrintable(error));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(session.isRunning());
    QCOMPARE(session.backend(), TerminalSession::Backend::Pty);

    QVERIFY(dataSpy.wait(5000));
    QByteArray output;
    for (const QList<QVariant> &args : dataSpy)
        output += args.at(0).toByteArray();
    QVERIFY2(output.contains("hello"), output.constData());
#else
    QSKIP("pty backend is Unix-only");
#endif
}

void TestTerminal::testSessionReportsExitCode()
{
#ifdef Q_OS_UNIX
    TerminalSession session;
    QSignalSpy finishedSpy(&session, &TerminalSession::finished);

    QString error;
    QVERIFY(session.start("/bin/sh", {QStringLiteral("-c"), QStringLiteral("exit 3")},
                          QDir::homePath(), QProcessEnvironment::systemEnvironment(), &error));
    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(finishedSpy.at(0).at(0).toInt(), 3);
    QVERIFY(session.hasExited());
    QCOMPARE(session.exitCode(), 3);
    QVERIFY(!session.isRunning());
#else
    QSKIP("pty backend is Unix-only");
#endif
}

void TestTerminal::testSessionFailureOnMissingProgram()
{
#ifdef Q_OS_UNIX
    TerminalSession session;
    QSignalSpy finishedSpy(&session, &TerminalSession::finished);

    QString error;
    // The fork itself succeeds, so the failure surfaces as the shell convention
    // "command not found": exit status 127 once the child gives up on exec().
    QVERIFY(session.start("/nonexistent/program/for/scriptura", QStringList(),
                          QDir::homePath(), QProcessEnvironment::systemEnvironment(), &error));
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(session.exitCode(), 127);
    QVERIFY(!session.isRunning());
#else
    QSKIP("pty backend is Unix-only");
#endif
}

void TestTerminal::testSessionDefaultShell()
{
    const QString shell = TerminalSession::defaultShell();
    QVERIFY(!shell.isEmpty());
    QVERIFY(QFileInfo::exists(shell));
    QVERIFY(TerminalSession::candidateShells().contains(shell));
}

// ── View ───────────────────────────────────────────────────────────────────

void TestTerminal::testViewFollowsOutput()
{
    TerminalEmulator emu;
    TerminalView view;
    view.setEmulator(&emu);
    view.resize(400, 200);

    // The view reports the grid its viewport can hold; the host adopts it.
    QSignalSpy resizeSpy(&view, &TerminalView::requestResize);
    connect(&view, &TerminalView::requestResize, &emu,
            [&emu](int columns, int rows) { emu.resize(rows, columns); });
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(emu.columns() > 10);
    QVERIFY(emu.rows() > 2);
    QCOMPARE(emu.columns(), resizeSpy.last().at(0).toInt());
    QCOMPARE(emu.rows(), resizeSpy.last().at(1).toInt());

    // Output beyond the screen scrolls the view's scroll range, not the grid.
    for (int i = 0; i < 200; ++i)
        emu.feed(QByteArray("row ") + QByteArray::number(i) + "\r\n");
    QVERIFY(!view.isScrolledBack());
    QCOMPARE(view.verticalScrollBar()->value(), 0);

    view.scrollLines(5);
    QVERIFY(view.isScrolledBack());
    QCOMPARE(view.verticalScrollBar()->value(), 5);
    view.scrollToBottom();
    QVERIFY(!view.isScrolledBack());

    // Clearing the scrollback keeps the live screen and empties the history.
    const int historyBefore = emu.buffer()->historyCount();
    QVERIFY(historyBefore > 0);
    view.clearScrollback();
    QCOMPARE(emu.buffer()->historyCount(), 0);

    view.resetBuffer();
    QCOMPARE(emu.buffer()->historyCount(), 0);
    QCOMPARE(emu.buffer()->cursor().row, 0);
}

void TestTerminal::testViewSelectionCopy()
{
    TerminalEmulator emu;
    TerminalView view;
    view.setEmulator(&emu);
    // Adopt the grid the view reports, as the panel does: with a 24x80 default the
    // view would only ever show the bottom of the (blank) screen.
    connect(&view, &TerminalView::requestResize, &emu,
            [&emu](int columns, int rows) { emu.resize(rows, columns); });
    view.resize(400, 200);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    emu.feed("hello world");

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->clear();

    // A drag from the start of the line to the end copies the row. The events go
    // to the viewport, which is what actually receives pointer input.
    QWidget *target = view.viewport();
    QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, QPoint(6, 6));
    QTest::mouseMove(target, QPoint(200, 6));
    QTest::mouseRelease(target, Qt::LeftButton, Qt::NoModifier, QPoint(200, 6));

    QVERIFY(view.hasSelection());
    const QString selected = view.selectedText();
    QVERIFY2(selected.startsWith(QStringLiteral("hello world")), qPrintable(selected));
    view.copySelection();
    QCOMPARE(clipboard->text(), selected);
    QCOMPARE(selected.trimmed(), QStringLiteral("hello world"));

    view.clearSelection();
    QVERIFY(!view.hasSelection());
    QVERIFY(view.selectedText().isEmpty());
}

void TestTerminal::testViewFontSizeClamped()
{
    TerminalView view;
    QCOMPARE(view.fontSize(), 12);

    view.increaseFontSize();
    QCOMPARE(view.fontSize(), 13);
    view.decreaseFontSize();
    view.decreaseFontSize();
    QCOMPARE(view.fontSize(), 11);

    // The limits are hard bounds, not soft ones.
    view.setFontSize(999);
    QCOMPARE(view.fontSize(), 32);
    view.setFontSize(1);
    QCOMPARE(view.fontSize(), 6);
    QVERIFY(view.cellWidth() > 0);
    QVERIFY(view.cellHeight() > 0);
}

// ── Panel ──────────────────────────────────────────────────────────────────

void TestTerminal::testPanelRunsCommandInTerminal()
{
#ifdef Q_OS_UNIX
    TerminalPanel panel;
    panel.setShellProgram(QStringLiteral("/bin/sh"));
    panel.setWorkingDirectory(QDir::homePath());

    panel.createSession(QStringLiteral("/bin/sh"));
    QCOMPARE(panel.sessionCount(), 1);
    QVERIFY(panel.currentView() != nullptr);

    QVERIFY(panel.runCommand(QStringLiteral("printf panel-marker")));

    // The command is run by a real shell: its output arrives asynchronously.
    TerminalEmulator *emu = panel.currentView()->emulator();
    QTRY_VERIFY_WITH_TIMEOUT(screenText(emu->buffer()).contains(QStringLiteral("panel-marker")), 5000);
#else
    QSKIP("pty backend is Unix-only");
#endif
}

void TestTerminal::testPanelSessionLifecycle()
{
#ifdef Q_OS_UNIX
    TerminalPanel panel;
    panel.setShellProgram(QStringLiteral("/bin/sh"));

    // Nothing is spawned until the panel is asked for a terminal.
    QCOMPARE(panel.sessionCount(), 0);

    panel.createSession(QStringLiteral("/bin/sh"));
    QCOMPARE(panel.sessionCount(), 1);
    // Until the shell reports a title, the tab falls back to the program name.
    QVERIFY(panel.currentTitle().contains(QStringLiteral("sh")));

    panel.createSession(QStringLiteral("/bin/sh"));
    QCOMPARE(panel.sessionCount(), 2);

    // runCommand targets the active terminal.
    TerminalEmulator *second = panel.currentView()->emulator();
    QVERIFY(panel.runCommand(QStringLiteral("printf second-terminal")));
    QTRY_VERIFY_WITH_TIMEOUT(
        screenText(second->buffer()).contains(QStringLiteral("second-terminal")), 5000);

    panel.closeCurrentSession();
    QCOMPARE(panel.sessionCount(), 1);
    panel.closeCurrentSession();
    QCOMPARE(panel.sessionCount(), 0);

    // A command after every terminal was closed starts a fresh one.
    QVERIFY(panel.runCommand(QStringLiteral("printf reopened")));
    QCOMPARE(panel.sessionCount(), 1);
#else
    QSKIP("pty backend is Unix-only");
#endif
}