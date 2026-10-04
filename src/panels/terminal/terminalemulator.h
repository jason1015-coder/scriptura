#ifndef SCRIPTURA_TERMINALEMULATOR_H
#define SCRIPTURA_TERMINALEMULATOR_H

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <cstdint>

// ── Colours ────────────────────────────────────────────────────────────────
// A terminal colour is either:
//   kColorDefault         the terminal default (taken from the editor theme)
//   0 .. 255              an index into the standard xterm-256 palette
//   kColorTrue | 0xRRGGBB a 24-bit direct colour (SGR 38;2 / 48;2)
using TermColor = qint32;
static constexpr TermColor kColorDefault = -1;
static constexpr TermColor kColorTrue = 0x01000000;

inline bool isTrueColor(TermColor c) { return (c & kColorTrue) != 0; }
inline QColor trueColorOf(TermColor c)
{
    return QColor(static_cast<int>((c >> 16) & 0xFF),
                  static_cast<int>((c >> 8) & 0xFF),
                  static_cast<int>(c & 0xFF));
}

// ── Cell attributes (SGR) ──────────────────────────────────────────────────
enum TermAttr : quint8 {
    AttrNone      = 0,
    AttrBold      = 1 << 0,
    AttrDim       = 1 << 1,
    AttrItalic    = 1 << 2,
    AttrUnderline = 1 << 3,
    AttrBlink     = 1 << 4,
    AttrReverse   = 1 << 5,
    AttrHidden    = 1 << 6,
    AttrStrike    = 1 << 7,
};

/**
 * One screen cell. `width` is how many columns the glyph occupies: 2 for
 * East-Asian wide and emoji code points, 1 normally, and 0 for the trailing
 * half of a wide glyph (such cells are skipped by the renderer and by text
 * export).
 */
struct TermCell {
    char32_t ch  = U' ';
    TermColor fg = kColorDefault;
    TermColor bg = kColorDefault;
    quint8 attrs = AttrNone;
    quint8 width = 1;

    bool isContinuation() const { return width == 0; }
    bool operator==(const TermCell &o) const
    {
        return ch == o.ch && fg == o.fg && bg == o.bg && attrs == o.attrs && width == o.width;
    }
    bool operator!=(const TermCell &o) const { return !(*this == o); }
};

/** Current pen (SGR) state. */
struct TermPen {
    TermColor fg = kColorDefault;
    TermColor bg = kColorDefault;
    quint8 attrs = AttrNone;

    void reset() { *this = TermPen(); }
};

struct TermCursor {
    int row = 0;
    int col = 0;
    TermPen pen;
};

/** A single row of the grid or of the scrollback. */
struct TermLine {
    QVector<TermCell> cells;
    bool wrapped = false; ///< the next logical line continues this one

    void assign(int columns, const TermCell &fill);
    QString toText() const;
};

// ── Text helpers (exposed so tests can assert on them directly) ────────────
int termCharWidth(char32_t cp);
QByteArray termDecodeUtf8(const QByteArray &bytes);

// ── TerminalBuffer ─────────────────────────────────────────────────────────

/**
 * The character grid plus its scrollback.
 *
 * Lines are addressed through a single "combined" index space so the view can
 * treat scrollback and screen as one continuous stream:
 *
 *     [0 .. historyCount()-1]                        -> scrollback
 *     [historyCount() .. historyCount()+rows-1]     -> screen
 *
 * The cursor, the scroll region and the saved-cursor slot live here as well,
 * because they are screen state that has to be saved and restored together when
 * the terminal switches to the alternate buffer.
 */
class TerminalBuffer
{
public:
    TerminalBuffer();

    void resize(int rows, int columns);
    void clear();
    void clearScrollback();

    QVector<TermLine> history() const { return m_history; }
    /// Replace the scrollback wholesale (used when leaving the alternate screen).
    void setHistory(const QVector<TermLine> &lines);

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int historyCount() const { return m_history.size(); }
    int totalLines() const { return m_history.size() + m_rows; }
    int historyLimit() const { return m_historyLimit; }
    void setHistoryLimit(int limit);

    /// Line lookup in combined index space. Never returns a null reference:
    /// out-of-range indices yield a scratch line of spaces.
    const TermLine &line(int combinedIndex) const;

    TermCell cellAt(int row, int col) const;
    TermLine &mutableLine(int row);
    QString textAt(int combinedIndex) const;

    // ── Cursor ────────────────────────────────────────────────────────────
    TermCursor &cursor() { return m_cursor; }
    const TermCursor &cursor() const { return m_cursor; }
    void setCursorPosition(int row, int col);
    TermCursor &savedCursor() { return m_savedCursor; }

    // ── Scroll region (0-based, inclusive) ───────────────────────────────
    int scrollTop() const { return m_scrollTop; }
    int scrollBottom() const { return m_scrollBottom; }
    void setScrollRegion(int top, int bottom);
    void resetScrollRegion() { setScrollRegion(0, m_rows - 1); }

    // ── Editing primitives ───────────────────────────────────────────────
    void putCell(char32_t cp, int width, const TermPen &pen);
    void lineFeed();
    void reverseLineFeed();
    void carriageReturn();
    void moveCursor(int row, int col);
    void moveCursorRelative(int dRow, int dCol);
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void insertLines(int count);
    void deleteLines(int count);
    void insertChars(int count);
    void deleteChars(int count);
    void eraseChars(int count);
    void scrollUp(int count);
    void scrollDown(int count);
    void tabToNextStop(int count);
    void backTab(int count);
    void setTabStop(int col);
    void clearTabStop(int col);
    void clearAllTabStops();

    /// Deferred autowrap (DECAWM): the cursor sticks to the last column and only
    /// moves to the next row once the following glyph arrives.
    void setWrapPending(bool pending) { m_wrapPending = pending; }
    bool wrapPending() const { return m_wrapPending; }
    bool autoWrap() const { return m_autoWrap; }
    void setAutoWrap(bool on) { m_autoWrap = on; }

    static TermCell blankCell();

private:
    void scrollRegionUp(int top, int bottom, int count, bool pushHistory);
    void scrollRegionDown(int top, int bottom, int count);
    void clampCursor();
    void pushHistory(const TermLine &line);
    void rebuildTabStops();

    QVector<TermLine> m_lines;
    QVector<TermLine> m_history;
    int m_rows = 24;
    int m_columns = 80;
    int m_historyLimit = 5000;
    int m_scrollTop = 0;
    int m_scrollBottom = 23;
    bool m_wrapPending = false;
    bool m_autoWrap = true;
    QList<bool> m_tabs;
    TermCursor m_cursor;
    TermCursor m_savedCursor;
    mutable TermLine m_scratch;
};

// ── TerminalEmulator ───────────────────────────────────────────────────────

/**
 * A VT100 / xterm-subset terminal emulator: a byte-level escape-sequence
 * parser driving a TerminalBuffer.
 *
 * It knows nothing about processes or widgets. Output arrives through feed(),
 * and the host reacts to the signals below: write the reply back to the pty,
 * retitle the tab, flash the bell, repaint.
 */
class TerminalEmulator : public QObject
{
    Q_OBJECT
public:
    explicit TerminalEmulator(QObject *parent = nullptr);

    void reset();
    void feed(const QByteArray &data);
    void feed(char byte) { feed(QByteArray(1, byte)); }

    void resize(int rows, int columns);

    TerminalBuffer *buffer() { return &m_buffer; }
    const TerminalBuffer *buffer() const { return &m_buffer; }

    int rows() const { return m_buffer.rows(); }
    int columns() const { return m_buffer.columns(); }

    // ── Modes the host and renderer need to know about ───────────────────
    bool cursorVisible() const { return m_cursorVisible; }
    bool applicationCursorKeys() const { return m_appCursorKeys; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    bool applicationKeypad() const { return m_appKeypad; }
    int mouseMode() const { return m_mouseMode; }
    bool mouseSgr() const { return m_mouseSgr; }
    bool focusEventsEnabled() const { return m_focusEvents; }
    bool alternateScreen() const { return m_altScreen; }
    bool reverseVideo() const { return m_reverseVideo; }

    // ── Colours ──────────────────────────────────────────────────────────
    static QColor paletteColor(int index);
    QColor resolveForeground(TermColor c) const;
    QColor resolveBackground(TermColor c) const;
    void setDefaultForeground(const QColor &c) { m_defaultFg = c; }
    void setDefaultBackground(const QColor &c) { m_defaultBg = c; }
    QColor defaultForeground() const { return m_defaultFg; }
    QColor defaultBackground() const { return m_defaultBg; }
    /// Cell colours, with reverse video already applied.
    QColor cellForeground(const TermCell &cell) const;
    QColor cellBackground(const TermCell &cell) const;

    // ── Input encoding ──────────────────────────────────────────────────
    /**
     * Bytes a key press should send. Sets *handled to false for keys the
     * terminal swallows (e.g. an unhandled function key), in which case the
     * caller should not send anything.
     */
    QByteArray encodeKey(int qtKey, Qt::KeyboardModifiers mods,
                         const QString &text, bool *handled = nullptr) const;

    /// Escape sequence reporting a mouse event at 1-based (col, row).
    QByteArray encodeMouse(int button, int col, int row, bool press, bool release,
                           Qt::KeyboardModifiers mods) const;

    QString currentTitle() const { return m_title; }

    /// Parse a CSI parameter string (";" separated, ":" as a sub-parameter).
    static QList<int> parseParams(const QByteArray &raw);

signals:
    void titleChanged(const QString &title);
    void bell();
    void requestWrite(const QByteArray &data);
    void resetRequested();
    void screenChanged();

private:
    enum class State {
        Ground,
        Escape,
        EscapeIntermediate,
        CsiEntry,
        CsiIntermediate,
        OscString,
        DcsEntry,
        DcsPassthrough,
        DcsIgnore,
    };

    void processByte(quint8 b);
    void handleC0(quint8 b);
    void handleEscapeByte(quint8 b);
    void handleCsiByte(quint8 b);
    void beginUtf8(quint8 b);
    void continueUtf8(quint8 b);
    void printCodepoint(char32_t cp);

    void dispatchEscape(quint8 b);
    void dispatchCsi(quint8 b);
    void dispatchOsc();
    void finishStringSequence();

    void applySgr(const QList<int> &params);
    void applyPrivateMode(const QList<int> &params, bool set);
    void applyAnsiMode(const QList<int> &params, bool set);
    void eraseInDisplay(const QList<int> &params);
    void reportDeviceStatus(const QList<int> &params);

    void enterAlternateScreen();
    void leaveAlternateScreen();

    void reply(const QByteArray &data);
    void reportCursorPosition(bool decPrivate);

    // ── Parser state ─────────────────────────────────────────────────────
    TerminalBuffer m_buffer;
    State m_state = State::Ground;
    QByteArray m_csiParams;
    quint8 m_csiPrivate = 0;
    QByteArray m_csiIntermediate;
    QByteArray m_oscBuffer;
    QByteArray m_dcsPayload;
    bool m_stringEsc = false; ///< saw ESC while inside OSC/DCS
    quint8 m_escapeIntermediate = 0;

    quint8 m_utf8Buf[4] = {0, 0, 0, 0};
    int m_utf8Len = 0;
    int m_utf8Need = 0;

    // ── Terminal state ───────────────────────────────────────────────────
    bool m_cursorVisible = true;
    bool m_appCursorKeys = false;
    bool m_appKeypad = false;
    bool m_autoWrap = true;
    bool m_bracketedPaste = false;
    bool m_focusEvents = false;
    bool m_altScreen = false;
    bool m_originMode = false;
    bool m_newlineMode = false;
    int m_mouseMode = 0;  ///< 0 none, 1000 press, 1002 drag, 1003 any-motion
    bool m_mouseSgr = false;
    bool m_reverseVideo = false;
    QString m_title;

    // Saved primary screen for the alternate buffer.
    QVector<TermLine> m_savedScreen;
    QVector<TermLine> m_savedHistory;
    TermCursor m_savedPrimaryCursor;
    int m_savedPrimaryTop = 0;
    int m_savedPrimaryBottom = 0;
    bool m_haveSavedScreen = false;

    QColor m_defaultFg = QColor(0xD8, 0xD8, 0xD8);
    QColor m_defaultBg = QColor(0x1E, 0x1E, 0x1E);
};

#endif // SCRIPTURA_TERMINALEMULATOR_H
