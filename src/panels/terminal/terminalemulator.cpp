#include "terminal/terminalemulator.h"

#include <QStringDecoder>

namespace {

/// Mixes @a color towards @a towards by @a ratio, used to fade dim text.
QColor blend(const QColor &color, const QColor &towards, qreal ratio)
{
    return QColor::fromRgbF(color.redF() + (towards.redF() - color.redF()) * ratio,
                            color.greenF() + (towards.greenF() - color.greenF()) * ratio,
                            color.blueF() + (towards.blueF() - color.blueF()) * ratio);
}

} // namespace

// ── Text helpers ───────────────────────────────────────────────────────────

int termCharWidth(char32_t cp)
{
    if (cp == 0)
        return 0;

    // Combining marks occupy no cell of their own.
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
        (cp >= 0xFE20 && cp <= 0xFE2F))
        return 0;

    // East-Asian Wide / Fullwidth plus the emoji blocks that render double-wide
    // in monospace fonts.
    if ((cp >= 0x1100 && cp <= 0x115F) ||   // Hangul Jamo
        (cp >= 0x2E80 && cp <= 0x303E) ||   // CJK radicals .. CJK symbols
        (cp >= 0x3041 && cp <= 0x33FF) ||   // Kana, CJK compatibility
        (cp >= 0x3400 && cp <= 0x4DBF) ||   // CJK extension A
        (cp >= 0x4E00 && cp <= 0x9FFF) ||   // CJK unified ideographs
        (cp >= 0xA000 && cp <= 0xA4CF) ||   // Yi
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK compatibility ideographs
        (cp >= 0xFE30 && cp <= 0xFE6F) ||   // CJK compatibility forms
        (cp >= 0xFF00 && cp <= 0xFF60) ||   // Fullwidth forms
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1F300 && cp <= 0x1F64F) || // emoji
        (cp >= 0x1F680 && cp <= 0x1F6FF) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) ||
        (cp >= 0x20000 && cp <= 0x3FFFD))   // CJK extension B+
        return 2;

    return 1;
}

QByteArray termDecodeUtf8(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return QByteArray();
    // One-shot decode: this helper has no state between calls, so a sequence that
    // is still truncated at the end of the buffer is reported as invalid instead
    // of being held back for a next call that will never come. (TerminalEmulator
    // does its own stateful buffering, which is where split sequences survive.)
    return QString::fromUtf8(bytes.constData(), bytes.size()).toUtf8();
}

// ── TermLine ───────────────────────────────────────────────────────────────

void TermLine::assign(int columns, const TermCell &fill)
{
    if (columns < 0)
        columns = 0;
    // NOTE: Qt6 unified QVector with QList and removed std::vector-style
    // assign(). resize() + fill() is the Qt6-compatible equivalent.
    cells.resize(columns);
    cells.fill(fill);
    wrapped = false;
}

QString TermLine::toText() const
{
    QString out;
    out.reserve(cells.size());
    for (const TermCell &c : cells) {
        if (c.isContinuation())
            continue;
        out.append(QString::fromUcs4(&c.ch, 1));
    }
    return out;
}

// ── TerminalBuffer ─────────────────────────────────────────────────────────

TerminalBuffer::TerminalBuffer()
{
    m_lines.reserve(m_rows);
    for (int i = 0; i < m_rows; ++i)
        m_lines.append(TermLine());
    for (int r = 0; r < m_rows; ++r)
        m_lines[r].assign(m_columns, blankCell());
    rebuildTabStops();
}

TermCell TerminalBuffer::blankCell()
{
    return TermCell();
}

void TerminalBuffer::rebuildTabStops()
{
    m_tabs.resize(m_columns);
    m_tabs.fill(false);
    for (int i = 0; i < m_columns; i += 8)
        m_tabs[i] = true;
}

void TerminalBuffer::resize(int rows, int columns)
{
    if (rows < 1)
        rows = 1;
    if (columns < 1)
        columns = 1;
    if (rows == m_rows && columns == m_columns)
        return;

    const TermCell fill = blankCell();

    if (columns != m_columns) {
        for (TermLine &l : m_history)
            l.assign(columns, fill);
        for (TermLine &l : m_lines)
            l.assign(columns, fill);
        m_columns = columns;
        rebuildTabStops();
    }

    if (rows != m_rows) {
        // Growing appends blank rows at the bottom; shrinking pushes the removed
        // rows into the scrollback so no output is silently lost.
        while (m_lines.size() < rows) {
            TermLine blank;
            blank.assign(m_columns, fill);
            m_lines.append(blank);
        }
        while (m_lines.size() > rows) {
            if (m_historyLimit > 0)
                pushHistory(m_lines.last());
            m_lines.removeLast();
        }
        m_rows = rows;
    }

    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    clampCursor();
}

void TerminalBuffer::clear()
{
    const TermCell fill = blankCell();
    for (TermLine &l : m_lines)
        l.assign(m_columns, fill);
    m_history.clear();
    m_cursor = TermCursor();
    m_savedCursor = TermCursor();
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    m_wrapPending = false;
    rebuildTabStops();
}

void TerminalBuffer::clearScrollback()
{
    m_history.clear();
}

void TerminalBuffer::setHistoryLimit(int limit)
{
    m_historyLimit = qMax(0, limit);
    while (m_history.size() > m_historyLimit)
        m_history.removeFirst();
}

void TerminalBuffer::setHistory(const QVector<TermLine> &lines)
{
    m_history = lines;
    while (m_history.size() > m_historyLimit)
        m_history.removeFirst();
}

void TerminalBuffer::pushHistory(const TermLine &line)
{
    if (m_historyLimit <= 0)
        return;
    m_history.append(line);
    while (m_history.size() > m_historyLimit)
        m_history.removeFirst();
}

const TermLine &TerminalBuffer::line(int combinedIndex) const
{
    if (combinedIndex < 0) {
        m_scratch.wrapped = false;
        return m_scratch;
    }
    if (combinedIndex < m_history.size())
        return m_history.at(combinedIndex);
    const int row = combinedIndex - m_history.size();
    if (row < m_lines.size())
        return m_lines.at(row);
    m_scratch.wrapped = false;
    return m_scratch;
}

TermCell TerminalBuffer::cellAt(int row, int col) const
{
    if (row < 0 || row >= m_lines.size() || col < 0 || col >= m_columns)
        return blankCell();
    return m_lines.at(row).cells.at(col);
}

TermLine &TerminalBuffer::mutableLine(int row)
{
    if (row < 0 || row >= m_lines.size())
        return m_scratch;
    return m_lines[row];
}

QString TerminalBuffer::textAt(int combinedIndex) const
{
    return line(combinedIndex).toText();
}

void TerminalBuffer::clampCursor()
{
    m_cursor.row = qBound(0, m_cursor.row, m_rows - 1);
    m_cursor.col = qBound(0, m_cursor.col, m_columns - 1);
}

void TerminalBuffer::setCursorPosition(int row, int col)
{
    m_cursor.row = qBound(0, row, m_rows - 1);
    m_cursor.col = qBound(0, col, m_columns - 1);
    m_wrapPending = false;
}

void TerminalBuffer::moveCursor(int row, int col)
{
    setCursorPosition(row, col);
}

void TerminalBuffer::moveCursorRelative(int dRow, int dCol)
{
    m_cursor.row = qBound(0, m_cursor.row + dRow, m_rows - 1);
    m_cursor.col = qBound(0, m_cursor.col + dCol, m_columns - 1);
    m_wrapPending = false;
}

void TerminalBuffer::setScrollRegion(int top, int bottom)
{
    if (top < 0)
        top = 0;
    if (bottom >= m_rows)
        bottom = m_rows - 1;
    if (top >= bottom)
        return; ///< an inverted or empty region is ignored, as xterm does
    m_scrollTop = top;
    m_scrollBottom = bottom;
    m_cursor.row = m_scrollTop;
    m_cursor.col = 0;
    m_wrapPending = false;
}

void TerminalBuffer::carriageReturn()
{
    m_cursor.col = 0;
    m_wrapPending = false;
}

void TerminalBuffer::lineFeed()
{
    if (m_cursor.row == m_scrollBottom)
        scrollRegionUp(m_scrollTop, m_scrollBottom, 1, m_scrollTop == 0);
    else if (m_cursor.row < m_rows - 1)
        ++m_cursor.row;
    m_wrapPending = false;
}

void TerminalBuffer::reverseLineFeed()
{
    if (m_cursor.row == m_scrollTop)
        scrollRegionDown(m_scrollTop, m_scrollBottom, 1);
    else if (m_cursor.row > 0)
        --m_cursor.row;
    m_wrapPending = false;
}

void TerminalBuffer::scrollRegionUp(int top, int bottom, int count, bool toHistory)
{
    count = qBound(1, count, bottom - top + 1);
    const TermCell fill = blankCell();
    for (int i = 0; i < count; ++i) {
        if (toHistory)
            pushHistory(m_lines.at(top));
        for (int r = top; r < bottom; ++r)
            m_lines[r] = m_lines.at(r + 1);
        m_lines[bottom].assign(m_columns, fill);
    }
}

void TerminalBuffer::scrollUp(int count)
{
    // Only lines leaving the top of the full screen belong in the scrollback;
    // lines pushed out of a smaller region are discarded, as xterm does.
    scrollRegionUp(m_scrollTop, m_scrollBottom, count, m_scrollTop == 0);
}

void TerminalBuffer::scrollDown(int count)
{
    scrollRegionDown(m_scrollTop, m_scrollBottom, count);
}

void TerminalBuffer::scrollRegionDown(int top, int bottom, int count)
{
    count = qBound(1, count, bottom - top + 1);
    const TermCell fill = blankCell();
    for (int i = 0; i < count; ++i) {
        for (int r = bottom; r > top; --r)
            m_lines[r] = m_lines.at(r - 1);
        m_lines[top].assign(m_columns, fill);
    }
}

void TerminalBuffer::putCell(char32_t cp, int width, const TermPen &pen)
{
    if (width < 1)
        width = 1;

    // Deferred wrap: writing past the last column only rolls to a new row once
    // the next glyph actually arrives (DECAWM semantics).
    if (m_wrapPending && m_autoWrap) {
        m_lines[m_cursor.row].wrapped = true;
        m_cursor.col = 0;
        lineFeed();
        m_wrapPending = false;
    }

    if (m_cursor.col + width > m_columns) {
        if (m_autoWrap) {
            m_lines[m_cursor.row].wrapped = true;
            m_cursor.col = 0;
            lineFeed();
        } else {
            m_cursor.col = m_columns - width;
        }
    }

    TermLine &row = m_lines[m_cursor.row];
    TermCell &cell = row.cells[m_cursor.col];
    cell.ch = cp;
    cell.fg = pen.fg;
    cell.bg = pen.bg;
    cell.attrs = pen.attrs;
    cell.width = static_cast<quint8>(width);

    for (int i = 1; i < width && m_cursor.col + i < m_columns; ++i) {
        TermCell &tail = row.cells[m_cursor.col + i];
        tail = blankCell();
        tail.fg = pen.fg;
        tail.bg = pen.bg;
        tail.attrs = pen.attrs;
        tail.width = 0;
    }

    m_cursor.col += width;
    if (m_cursor.col >= m_columns) {
        m_cursor.col = m_columns - 1;
        m_wrapPending = true;
    }
}

void TerminalBuffer::eraseInLine(int mode)
{
    const int row = m_cursor.row;
    if (row < 0 || row >= m_lines.size())
        return;
    TermLine &line = m_lines[row];
    const TermCell fill = blankCell();
    int from = 0;
    int to = m_columns - 1;
    if (mode == 0)
        from = m_cursor.col;
    else if (mode == 1)
        to = m_cursor.col;
    else if (mode != 2)
        return;
    for (int c = from; c <= to && c < m_columns; ++c)
        line.cells[c] = fill;
    if (mode != 0)
        m_wrapPending = false;
}

void TerminalBuffer::eraseInDisplay(int mode)
{
    const TermCell fill = blankCell();
    switch (mode) {
    case 0:
        eraseInLine(0);
        for (int r = m_cursor.row + 1; r < m_lines.size(); ++r)
            m_lines[r].assign(m_columns, fill);
        break;
    case 1:
        eraseInLine(1);
        for (int r = 0; r < m_cursor.row && r < m_lines.size(); ++r)
            m_lines[r].assign(m_columns, fill);
        break;
    case 2:
        for (TermLine &l : m_lines)
            l.assign(m_columns, fill);
        break;
    default:
        break;
    }
}

void TerminalBuffer::insertLines(int count)
{
    if (m_cursor.row < m_scrollTop || m_cursor.row > m_scrollBottom)
        return;
    count = qBound(1, count, m_scrollBottom - m_cursor.row + 1);
    const TermCell fill = blankCell();
    for (int i = 0; i < count; ++i) {
        for (int r = m_scrollBottom; r > m_cursor.row; --r)
            m_lines[r] = m_lines.at(r - 1);
        m_lines[m_cursor.row].assign(m_columns, fill);
    }
    m_cursor.col = 0;
    m_wrapPending = false;
}

void TerminalBuffer::deleteLines(int count)
{
    if (m_cursor.row < m_scrollTop || m_cursor.row > m_scrollBottom)
        return;
    count = qBound(1, count, m_scrollBottom - m_cursor.row + 1);
    const TermCell fill = blankCell();
    for (int i = 0; i < count; ++i) {
        for (int r = m_cursor.row; r < m_scrollBottom; ++r)
            m_lines[r] = m_lines.at(r + 1);
        m_lines[m_scrollBottom].assign(m_columns, fill);
    }
    m_cursor.col = 0;
    m_wrapPending = false;
}

void TerminalBuffer::insertChars(int count)
{
    const int row = m_cursor.row;
    if (row < 0 || row >= m_lines.size())
        return;
    count = qBound(1, count, m_columns - m_cursor.col);
    TermLine &line = m_lines[row];
    for (int c = m_columns - 1; c >= m_cursor.col + count; --c)
        line.cells[c] = line.cells[c - count];
    const TermCell fill = blankCell();
    for (int c = m_cursor.col; c < m_cursor.col + count; ++c)
        line.cells[c] = fill;
}

void TerminalBuffer::deleteChars(int count)
{
    const int row = m_cursor.row;
    if (row < 0 || row >= m_lines.size())
        return;
    count = qBound(1, count, m_columns - m_cursor.col);
    TermLine &line = m_lines[row];
    for (int c = m_cursor.col; c < m_columns - count; ++c)
        line.cells[c] = line.cells[c + count];
    const TermCell fill = blankCell();
    for (int c = m_columns - count; c < m_columns; ++c)
        line.cells[c] = fill;
}

void TerminalBuffer::eraseChars(int count)
{
    const int row = m_cursor.row;
    if (row < 0 || row >= m_lines.size())
        return;
    count = qBound(1, count, m_columns - m_cursor.col);
    TermLine &line = m_lines[row];
    for (int c = m_cursor.col; c < m_cursor.col + count; ++c)
        line.cells[c] = blankCell();
}

void TerminalBuffer::setTabStop(int col)
{
    if (col >= 0 && col < m_tabs.size())
        m_tabs[col] = true;
}

void TerminalBuffer::clearTabStop(int col)
{
    if (col >= 0 && col < m_tabs.size())
        m_tabs[col] = false;
}

void TerminalBuffer::clearAllTabStops()
{
    m_tabs.fill(false);
}

void TerminalBuffer::tabToNextStop(int count)
{
    count = qMax(1, count);
    for (int i = 0; i < count; ++i) {
        int col = m_cursor.col + 1;
        while (col < m_columns - 1 && !m_tabs.value(col, false))
            ++col;
        m_cursor.col = qMin(col, m_columns - 1);
    }
    m_wrapPending = false;
}

void TerminalBuffer::backTab(int count)
{
    count = qMax(1, count);
    for (int i = 0; i < count; ++i) {
        int col = m_cursor.col - 1;
        while (col > 0 && !m_tabs.value(col, false))
            --col;
        m_cursor.col = qMax(col, 0);
    }
    m_wrapPending = false;
}

// ── TerminalEmulator ───────────────────────────────────────────────────────

TerminalEmulator::TerminalEmulator(QObject *parent)
    : QObject(parent)
{
    reset();
}

void TerminalEmulator::reset()
{
    m_buffer.clear();
    m_state = State::Ground;
    m_csiParams.clear();
    m_csiIntermediate.clear();
    m_csiPrivate = 0;
    m_oscBuffer.clear();
    m_dcsPayload.clear();
    m_stringEsc = false;
    m_escapeIntermediate = 0;
    m_utf8Len = 0;
    m_utf8Need = 0;
    m_cursorVisible = true;
    m_appCursorKeys = false;
    m_appKeypad = false;
    m_autoWrap = true;
    m_buffer.setAutoWrap(true);
    m_bracketedPaste = false;
    m_focusEvents = false;
    m_originMode = false;
    m_newlineMode = false;
    m_mouseMode = 0;
    m_mouseSgr = false;
    m_reverseVideo = false;
    m_savedScreen.clear();
    m_savedHistory.clear();
    m_haveSavedScreen = false;
    m_title.clear();
    emit screenChanged();
}

void TerminalEmulator::resize(int rows, int columns)
{
    if (m_buffer.rows() == rows && m_buffer.columns() == columns)
        return;
    m_buffer.resize(rows, columns);
    emit screenChanged();
}

QList<int> TerminalEmulator::parseParams(const QByteArray &raw)
{
    QList<int> out;
    if (raw.isEmpty())
        return out;
    int current = 0;
    bool hasDigit = false;
    for (int i = 0; i < raw.size(); ++i) {
        const char c = raw.at(i);
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            hasDigit = true;
        } else if (c == ';' || c == ':') {
            out.append(hasDigit ? current : 0);
            current = 0;
            hasDigit = false;
        } else {
            break;
        }
    }
    out.append(hasDigit ? current : 0);
    return out;
}

void TerminalEmulator::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    for (int i = 0; i < data.size(); ++i)
        processByte(static_cast<quint8>(data.at(i)));
    emit screenChanged();
}

void TerminalEmulator::processByte(quint8 b)
{
    // Inside OSC/DCS the payload is collected as raw bytes, so UTF-8 assembly is
    // bypassed there and the string is decoded once, on dispatch.
    const bool inString = (m_state == State::OscString || m_state == State::DcsEntry ||
                           m_state == State::DcsPassthrough);

    if (!inString) {
        if (m_utf8Need > 0) {
            continueUtf8(b);
            return;
        }
        if (b >= 0x80) {
            beginUtf8(b);
            return;
        }
    }

    switch (m_state) {
    case State::Ground:
        if (b == 0x1B) {
            m_state = State::Escape;
            m_escapeIntermediate = 0;
        } else if (b == 0x7F || b == 0x80) {
            // DEL (and a stray C1) are discarded.
        } else if (b < 0x20) {
            handleC0(b);
        } else {
            printCodepoint(b);
        }
        break;

    case State::Escape:
    case State::EscapeIntermediate:
        handleEscapeByte(b);
        break;

    case State::CsiEntry:
    case State::CsiIntermediate:
        handleCsiByte(b);
        break;

    case State::OscString:
        if (b == 0x07) { // BEL terminates OSC
            m_state = State::Ground;
            dispatchOsc();
        } else if (m_stringEsc) {
            m_stringEsc = false;
            if (b == '\\')
                finishStringSequence();
            else
                m_oscBuffer.append(static_cast<char>(b));
        } else if (b == 0x1B) {
            m_stringEsc = true;
        } else {
            m_oscBuffer.append(static_cast<char>(b));
            if (m_oscBuffer.size() > 8192)
                m_oscBuffer.remove(0, m_oscBuffer.size() - 8192);
        }
        break;

    case State::DcsEntry:
    case State::DcsPassthrough:
        if (m_stringEsc) {
            m_stringEsc = false;
            if (b == '\\')
                finishStringSequence();
        } else if (b == 0x1B) {
            m_stringEsc = true;
        } else if (b == 0x07) {
            m_state = State::Ground;
            m_dcsPayload.clear();
        } else {
            m_dcsPayload.append(static_cast<char>(b));
            if (m_dcsPayload.size() > 8192)
                m_dcsPayload.remove(0, m_dcsPayload.size() - 8192);
            m_state = State::DcsPassthrough;
        }
        break;

    case State::DcsIgnore:
        if (m_stringEsc) {
            m_stringEsc = false;
            if (b == '\\')
                m_state = State::Ground;
        } else if (b == 0x1B) {
            m_stringEsc = true;
        }
        break;
    }
}

void TerminalEmulator::handleC0(quint8 b)
{
    switch (b) {
    case 0x00: // NUL
    case 0x05: // ENQ (no answer-back configured)
    case 0x0E: // SO
    case 0x0F: // SI - charset switching is not supported
        break;
    case 0x07: // BEL
        emit bell();
        break;
    case 0x08: // BS
        if (m_buffer.cursor().col > 0) {
            --m_buffer.cursor().col;
            m_buffer.setWrapPending(false);
        }
        break;
    case 0x09: // HT
        m_buffer.tabToNextStop(1);
        break;
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
        m_buffer.lineFeed();
        if (m_newlineMode)
            m_buffer.carriageReturn();
        break;
    case 0x0D: // CR
        m_buffer.carriageReturn();
        break;
    case 0x18: // CAN
    case 0x1A: // SUB
        m_state = State::Ground;
        break;
    default:
        break;
    }
}

void TerminalEmulator::beginUtf8(quint8 b)
{
    m_utf8Len = 1;
    m_utf8Buf[0] = b;
    if (b >= 0xC2 && b <= 0xDF)
        m_utf8Need = 1;
    else if (b >= 0xE0 && b <= 0xEF)
        m_utf8Need = 2;
    else if (b >= 0xF0 && b <= 0xF4)
        m_utf8Need = 3;
    else {
        m_utf8Need = 0;
        m_utf8Len = 0;
        printCodepoint(0xFFFD); // invalid lead byte
    }
}

void TerminalEmulator::continueUtf8(quint8 b)
{
    if ((b & 0xC0) != 0x80) {
        m_utf8Need = 0;
        m_utf8Len = 0;
        printCodepoint(0xFFFD);
        processByte(b); // the offending byte may legitimately start something new
        return;
    }
    m_utf8Buf[m_utf8Len++] = b;
    if (m_utf8Len <= m_utf8Need)
        return;

    m_utf8Need = 0;
    const int len = m_utf8Len;
    m_utf8Len = 0;

    quint32 cp = 0;
    if (len == 2)
        cp = m_utf8Buf[0] & 0x1Fu;
    else if (len == 3)
        cp = m_utf8Buf[0] & 0x0Fu;
    else
        cp = m_utf8Buf[0] & 0x07u;
    for (int i = 1; i < len; ++i)
        cp = (cp << 6) | (m_utf8Buf[i] & 0x3Fu);

    // Reject overlong forms, surrogates and out-of-range code points.
    static const quint32 minimum[3] = {0x80, 0x800, 0x10000};
    if (cp < minimum[len - 2] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;

    printCodepoint(static_cast<char32_t>(cp));
}

void TerminalEmulator::printCodepoint(char32_t cp)
{
    const int width = termCharWidth(cp);
    if (width == 0)
        return; ///< combining marks have no cell of their own
    // DECSCNM only changes how the cells are painted (cellForeground() and
    // cellBackground() swap fore and back); it never stops the program writing.
    m_buffer.putCell(cp, width, m_buffer.cursor().pen);
}

// ── Escape sequences ───────────────────────────────────────────────────────

void TerminalEmulator::handleEscapeByte(quint8 b)
{
    if (b == 0x1B) {
        m_escapeIntermediate = 0;
        m_state = State::Escape;
        return;
    }
    if (b == 0x18 || b == 0x1A) {
        m_state = State::Ground;
        return;
    }
    if (b >= 0x20 && b <= 0x2F) {
        m_escapeIntermediate = b;
        m_state = State::EscapeIntermediate;
        return;
    }
    if (m_state == State::EscapeIntermediate) {
        // Intermediates followed by a final: nothing in the supported subset
        // uses them, so just consume the sequence.
        m_state = State::Ground;
        return;
    }

    switch (b) {
    case '[':
        m_state = State::CsiEntry;
        m_csiParams.clear();
        m_csiIntermediate.clear();
        m_csiPrivate = 0;
        break;
    case ']':
        m_state = State::OscString;
        m_oscBuffer.clear();
        m_stringEsc = false;
        break;
    case 'P': // DCS - payload ignored (DECRQSS and friends)
        m_state = State::DcsEntry;
        m_dcsPayload.clear();
        m_stringEsc = false;
        break;
    case 'X': // SOS
    case '^': // PM
    case '_': // APC
        m_state = State::DcsIgnore;
        m_stringEsc = false;
        break;
    default:
        dispatchEscape(b);
        break;
    }
}

void TerminalEmulator::dispatchEscape(quint8 b)
{
    m_state = State::Ground;
    m_escapeIntermediate = 0;
    switch (b) {
    case 'D': // IND
        m_buffer.lineFeed();
        break;
    case 'E': // NEL
        m_buffer.lineFeed();
        m_buffer.carriageReturn();
        break;
    case 'H': // HTS
        m_buffer.setTabStop(m_buffer.cursor().col);
        break;
    case 'M': // RI
        m_buffer.reverseLineFeed();
        break;
    case '7': // DECSC
        m_buffer.savedCursor() = m_buffer.cursor();
        break;
    case '8': { // DECRC
        const TermCursor saved = m_buffer.savedCursor();
        m_buffer.cursor() = saved;
        m_buffer.setCursorPosition(saved.row, saved.col);
        break;
    }
    case 'c': // RIS
        reset();
        emit resetRequested();
        break;
    case '=':
        m_appKeypad = true;
        break;
    case '>':
        m_appKeypad = false;
        break;
    case '\\': // stray ST
        break;
    default:
        break;
    }
}

void TerminalEmulator::handleCsiByte(quint8 b)
{
    if (b == 0x1B) {
        m_state = State::Escape;
        m_escapeIntermediate = 0;
        return;
    }
    if (b == 0x18 || b == 0x1A) {
        m_state = State::Ground;
        return;
    }
    if (b >= 0x40 && b <= 0x7E) {
        dispatchCsi(b);
        return;
    }
    if (b >= 0x20 && b <= 0x2F) {
        m_csiIntermediate.append(static_cast<char>(b));
        m_state = State::CsiIntermediate;
        return;
    }
    if (b >= 0x30 && b <= 0x3F) {
        // Only a leading '?', '<', '=' or '>' is meaningful as a private marker.
        if (m_csiPrivate == 0 && m_csiIntermediate.isEmpty() &&
            (b == '?' || b == '<' || b == '=' || b == '>')) {
            m_csiPrivate = b;
            return;
        }
        m_csiParams.append(static_cast<char>(b));
        m_state = State::CsiEntry;
        return;
    }
    // Bytes outside every CSI range are dropped without changing state.
}

void TerminalEmulator::dispatchCsi(quint8 b)
{
    const QList<int> p = parseParams(m_csiParams);

    if (m_csiPrivate == '?') {
        m_state = State::Ground;
        m_csiIntermediate.clear();
        m_csiPrivate = 0;
        if (b == 'h')
            applyPrivateMode(p, true);
        else if (b == 'l')
            applyPrivateMode(p, false);
        else if (b == 'n')
            reportCursorPosition(true); // DECXCPR
        return;
    }

    m_state = State::Ground;
    m_csiIntermediate.clear();
    m_csiPrivate = 0;

    // Defaulted parameter helper: an absent or zero parameter means `def`.
    auto arg = [&p](int index, int def) -> int {
        if (index >= p.size())
            return def;
        const int v = p.at(index);
        return v > 0 ? v : def;
    };

    switch (b) {
    case '@': // ICH
        m_buffer.insertChars(arg(0, 1));
        break;
    case 'A': // CUU
        m_buffer.moveCursorRelative(-arg(0, 1), 0);
        break;
    case 'B': // CUD
        m_buffer.moveCursorRelative(arg(0, 1), 0);
        break;
    case 'C': // CUF
        m_buffer.moveCursorRelative(0, arg(0, 1));
        break;
    case 'D': // CUB
        m_buffer.moveCursorRelative(0, -arg(0, 1));
        break;
    case 'E': // CNL
        m_buffer.moveCursorRelative(arg(0, 1), 0);
        m_buffer.carriageReturn();
        break;
    case 'F': // CPL
        m_buffer.moveCursorRelative(-arg(0, 1), 0);
        m_buffer.carriageReturn();
        break;
    case 'G': // CHA
    case '`': // HPA
        m_buffer.moveCursor(m_buffer.cursor().row, arg(0, 1) - 1);
        break;
    case 'H':  // CUP
    case 'f':  // HVP
    case 'e':  // VPR
        if (b == 'e') {
            m_buffer.moveCursorRelative(arg(0, 1), 0);
        } else {
            const int row = m_originMode ? m_buffer.scrollTop() + arg(0, 1) - 1 : arg(0, 1) - 1;
            m_buffer.moveCursor(row, arg(1, 1) - 1);
        }
        break;
    case 'I': // CHT
        m_buffer.tabToNextStop(arg(0, 1));
        break;
    case 'J': // ED
        eraseInDisplay(p);
        break;
    case 'K': // EL
        m_buffer.eraseInLine(p.isEmpty() ? 0 : p.at(0));
        break;
    case 'L': // IL
        m_buffer.insertLines(arg(0, 1));
        break;
    case 'M': // DL
        m_buffer.deleteLines(arg(0, 1));
        break;
    case 'P': // DCH
        m_buffer.deleteChars(arg(0, 1));
        break;
    case 'S': // SU
        m_buffer.scrollUp(arg(0, 1));
        break;
    case 'T': // SD
        m_buffer.scrollDown(arg(0, 1));
        break;
    case 'X': // ECH
        m_buffer.eraseChars(arg(0, 1));
        break;
    case 'Z': // CBT
        m_buffer.backTab(arg(0, 1));
        break;
    case 'a': // HPR
        m_buffer.moveCursorRelative(0, arg(0, 1));
        break;
    case 'd': // VPA
        m_buffer.moveCursor(m_originMode ? m_buffer.scrollTop() + arg(0, 1) - 1
                                         : arg(0, 1) - 1,
                            m_buffer.cursor().col);
        break;
    case 'g': { // TBC
        const int mode = p.isEmpty() ? 0 : p.at(0);
        if (mode == 3)
            m_buffer.clearAllTabStops();
        else if (mode == 0)
            m_buffer.clearTabStop(m_buffer.cursor().col);
        break;
    }
    case 'h': // SM
    case 'l': // RM
        applyAnsiMode(p, b == 'h');
        break;
    case 'm': // SGR
        applySgr(p);
        break;
    case 'n': // DSR
        reportDeviceStatus(p);
        break;
    case 'r': // DECSTBM
        m_buffer.setScrollRegion(arg(0, 1) - 1,
                                 p.size() >= 2 ? arg(1, m_buffer.rows()) - 1
                                              : m_buffer.rows() - 1);
        break;
    case 's': // SCOSC
        m_buffer.savedCursor() = m_buffer.cursor();
        break;
    case 'u': { // SCORC
        const TermCursor saved = m_buffer.savedCursor();
        m_buffer.cursor() = saved;
        m_buffer.setCursorPosition(saved.row, saved.col);
        break;
    }
    case 'c': // primary DA
        reply(QByteArray("\x1b[?1;2c"));
        break;
    case 'q': // cursor style - accepted and ignored
    case 't': // window manipulation - ignored
    default:
        break;
    }
}

// ── SGR ────────────────────────────────────────────────────────────────────

void TerminalEmulator::applySgr(const QList<int> &params)
{
    TermPen pen = m_buffer.cursor().pen;
    if (params.isEmpty()) {
        pen.reset();
        m_buffer.cursor().pen = pen;
        return;
    }

    // Reads one extended colour starting at `i`, returning the next index.
    auto readExtended = [&params](int i, TermColor &out) -> int {
        if (i >= params.size())
            return i;
        const int mode = params.at(i);
        if (mode == 5 && i + 1 < params.size()) {
            out = params.at(i + 1) & 0xFF;
            return i + 2;
        }
        if (mode == 2) {
            int base = i + 1;
            // The ITU form "38:2:<colour-space-id>:r:g:b" carries an extra field.
            if (base < params.size() && params.at(base) == 0 && params.size() >= i + 5)
                ++base;
            if (base + 2 < params.size()) {
                const int r = qBound(0, params.at(base), 255);
                const int g = qBound(0, params.at(base + 1), 255);
                const int b = qBound(0, params.at(base + 2), 255);
                out = kColorTrue | (r << 16) | (g << 8) | b;
                return base + 3;
            }
        }
        return i + 1;
    };

    int i = 0;
    while (i < params.size()) {
        const int v = params.at(i);
        switch (v) {
        case 0:
            pen.reset();
            break;
        case 1:
            pen.attrs |= AttrBold;
            break;
        case 2:
            pen.attrs |= AttrDim;
            break;
        case 3:
            pen.attrs |= AttrItalic;
            break;
        case 4:
            pen.attrs |= AttrUnderline;
            break;
        case 5:
        case 6:
            pen.attrs |= AttrBlink;
            break;
        case 7:
            pen.attrs |= AttrReverse;
            break;
        case 8:
            pen.attrs |= AttrHidden;
            break;
        case 9:
            pen.attrs |= AttrStrike;
            break;
        case 21:
        case 22:
            pen.attrs &= ~static_cast<quint8>(AttrBold | AttrDim);
            break;
        case 23:
            pen.attrs &= ~static_cast<quint8>(AttrItalic);
            break;
        case 24:
            pen.attrs &= ~static_cast<quint8>(AttrUnderline);
            break;
        case 25:
            pen.attrs &= ~static_cast<quint8>(AttrBlink);
            break;
        case 27:
            pen.attrs &= ~static_cast<quint8>(AttrReverse);
            break;
        case 28:
            pen.attrs &= ~static_cast<quint8>(AttrHidden);
            break;
        case 29:
            pen.attrs &= ~static_cast<quint8>(AttrStrike);
            break;
        case 38:
            i = readExtended(i + 1, pen.fg);
            continue;
        case 39:
            pen.fg = kColorDefault;
            break;
        case 48:
            i = readExtended(i + 1, pen.bg);
            continue;
        case 49:
            pen.bg = kColorDefault;
            break;
        default:
            if (v >= 30 && v <= 37)
                pen.fg = v - 30;
            else if (v >= 40 && v <= 47)
                pen.bg = v - 40;
            else if (v >= 90 && v <= 97)
                pen.fg = v - 90 + 8;
            else if (v >= 100 && v <= 107)
                pen.bg = v - 100 + 8;
            break;
        }
        ++i;
    }
    m_buffer.cursor().pen = pen;
}

// ── Modes ──────────────────────────────────────────────────────────────────

void TerminalEmulator::applyPrivateMode(const QList<int> &params, bool set)
{
    for (int value : params) {
        switch (value) {
        case 1: // DECCKM - application cursor keys
            m_appCursorKeys = set;
            break;
        case 5: // DECSCNM - reverse video
            m_reverseVideo = set;
            break;
        case 6: // DECOM - origin mode
            m_originMode = set;
            m_buffer.moveCursor(m_originMode ? m_buffer.scrollTop() : 0, 0);
            break;
        case 7: // DECAWM - autowrap
            m_autoWrap = set;
            m_buffer.setAutoWrap(set);
            break;
        case 12: // cursor blinking - cosmetic only
            break;
        case 25: // DECTCEM
            m_cursorVisible = set;
            break;
        case 1000:
        case 1002:
        case 1003:
            m_mouseMode = set ? value : 0;
            break;
        case 1004: // focus in/out reporting
            m_focusEvents = set;
            break;
        case 1006: // SGR mouse encoding
            m_mouseSgr = set;
            break;
        case 1005: // UTF-8 mouse - obsolete, the SGR form is used instead
        case 1015: // urxvt mouse - obsolete
            break;
        case 1047:
        case 1049:
            if (set)
                enterAlternateScreen();
            else
                leaveAlternateScreen();
            break;
        case 1048:
            if (set) {
                m_buffer.savedCursor() = m_buffer.cursor();
            } else {
                const TermCursor saved = m_buffer.savedCursor();
                m_buffer.cursor() = saved;
                m_buffer.setCursorPosition(saved.row, saved.col);
            }
            break;
        case 2004: // bracketed paste
            m_bracketedPaste = set;
            break;
        default:
            break;
        }
    }
}

void TerminalEmulator::applyAnsiMode(const QList<int> &params, bool set)
{
    for (int value : params) {
        switch (value) {
        case 4: // IRM - insert mode affects printing, not supported
            break;
        case 20: // LNM - newline mode
            m_newlineMode = set;
            break;
        default:
            break;
        }
    }
}

void TerminalEmulator::eraseInDisplay(const QList<int> &params)
{
    const int mode = params.isEmpty() ? 0 : params.at(0);
    if (mode == 3) {
        m_buffer.clearScrollback();
        return;
    }
    m_buffer.eraseInDisplay(mode);
}

void TerminalEmulator::reportDeviceStatus(const QList<int> &params)
{
    const int request = params.isEmpty() ? 0 : params.at(0);
    switch (request) {
    case 5:
        reply(QByteArray("\x1b[0n"));
        break;
    case 6:
        reportCursorPosition(false);
        break;
    default:
        break;
    }
}

// ── Alternate screen ───────────────────────────────────────────────────────

void TerminalEmulator::enterAlternateScreen()
{
    if (m_altScreen)
        return;

    m_savedScreen.clear();
    m_savedScreen.reserve(m_buffer.rows());
    for (int r = 0; r < m_buffer.rows(); ++r) {
        TermLine copy;
        copy.cells = m_buffer.mutableLine(r).cells;
        copy.wrapped = m_buffer.mutableLine(r).wrapped;
        m_savedScreen.append(copy);
    }
    m_savedHistory = m_buffer.history();
    m_savedPrimaryCursor = m_buffer.cursor();
    m_savedPrimaryTop = m_buffer.scrollTop();
    m_savedPrimaryBottom = m_buffer.scrollBottom();
    m_haveSavedScreen = true;

    m_buffer.clear();
    m_altScreen = true;
    emit screenChanged();
}

void TerminalEmulator::leaveAlternateScreen()
{
    if (!m_altScreen)
        return;

    const TermCell fill = TerminalBuffer::blankCell();
    const int cols = m_buffer.columns();

    m_buffer.clear();
    for (int r = 0; r < m_savedScreen.size() && r < m_buffer.rows(); ++r) {
        const TermLine &src = m_savedScreen.at(r);
        TermLine &dst = m_buffer.mutableLine(r);
        dst.cells = src.cells;
        dst.wrapped = src.wrapped;
        // The grid may have been resized while the alternate screen was up.
        if (dst.cells.size() < cols)
            dst.cells.append(QVector<TermCell>(cols - dst.cells.size(), fill));
        else if (dst.cells.size() > cols)
            dst.cells.resize(cols);
    }
    m_buffer.setHistory(m_savedHistory);

    const TermCursor saved = m_savedPrimaryCursor;
    m_buffer.cursor() = saved;
    m_buffer.setCursorPosition(saved.row, saved.col);
    m_buffer.setScrollRegion(m_savedPrimaryTop, m_savedPrimaryBottom);

    m_savedScreen.clear();
    m_savedHistory.clear();
    m_haveSavedScreen = false;
    m_altScreen = false;
    emit screenChanged();
}

// ── Replies ────────────────────────────────────────────────────────────────

void TerminalEmulator::reply(const QByteArray &data)
{
    if (!data.isEmpty())
        emit requestWrite(data);
}

void TerminalEmulator::reportCursorPosition(bool decPrivate)
{
    QByteArray data = "\x1b[";
    if (decPrivate)
        data += '?';
    data += QByteArray::number(m_buffer.cursor().row + 1);
    data += ';';
    data += QByteArray::number(m_buffer.cursor().col + 1);
    if (decPrivate)
        data += ";1";
    data += 'R';
    reply(data);
}

// ── OSC / DCS ──────────────────────────────────────────────────────────────

void TerminalEmulator::finishStringSequence()
{
    if (m_state == State::OscString)
        dispatchOsc();
    else
        m_state = State::Ground;
    m_stringEsc = false;
}

void TerminalEmulator::dispatchOsc()
{
    m_state = State::Ground;
    m_stringEsc = false;
    m_dcsPayload.clear();

    const QByteArray payload = m_oscBuffer;
    m_oscBuffer.clear();
    if (payload.isEmpty())
        return;

    const int sep = payload.indexOf(';');
    const int code = payload.left(sep < 0 ? payload.size() : sep).toInt();
    const QByteArray rest = sep < 0 ? QByteArray() : payload.mid(sep + 1);

    // "rgb:rrrr/gggg/bbbb", the reply format for the colour queries below.
    auto rgbAnswer = [](const QColor &c) {
        QByteArray out = "rgb:";
        for (int shift : {16, 8, 0})
            out += QByteArray::number((c.red() >> shift) & 0xFF, 16).rightJustified(2, '0');
        out[4] = '/';
        out[7] = '/';
        return out;
    };

    switch (code) {
    case 0:  // icon name + window title
    case 1:  // icon name
    case 2: { // window title
        const QString title = QString::fromUtf8(rest);
        if (title != m_title) {
            m_title = title;
            emit titleChanged(m_title);
        }
        break;
    }
    case 4: { // palette colour query "4;<n>?"
        QByteArray answer;
        const QList<QByteArray> parts = rest.split(';');
        for (const QByteArray &part : parts) {
            const int eq = part.indexOf('?');
            if (eq < 0)
                continue;
            const int index = part.left(eq).toInt();
            answer += "\x1b]4;" + QByteArray::number(index) + ";" + rgbAnswer(paletteColor(index))
                      + "\x1b\\";
        }
        reply(answer);
        break;
    }
    case 10: // default foreground query
    case 11: // default background query
        reply(QByteArray("\x1b]") + QByteArray::number(code) + ';' + rgbAnswer(code == 10 ? m_defaultFg : m_defaultBg) + "\x1b\\");
        break;
    default:
        // OSC 52 (clipboard write) is deliberately not honoured: a program in
        // the terminal must not be able to replace the user's clipboard silently.
        break;
    }
}

// ── Colours ────────────────────────────────────────────────────────────────

QColor TerminalEmulator::paletteColor(int index)
{
    static const int base[16][3] = {
        {0, 0, 0},       {128, 0, 0},     {0, 128, 0},     {128, 128, 0},
        {0, 0, 128},     {128, 0, 128},   {0, 128, 128},   {192, 192, 192},
        {128, 128, 128}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
        {0, 0, 255},     {255, 0, 255},   {0, 255, 255},   {255, 255, 255}
    };
    static const int cube[6] = {0, 95, 135, 175, 215, 255};

    if (index < 0 || index > 255)
        return QColor(0, 0, 0);
    if (index < 16)
        return QColor(base[index][0], base[index][1], base[index][2]);
    if (index < 232) {
        const int n = index - 16;
        return QColor(cube[n / 36], cube[(n / 6) % 6], cube[n % 6]);
    }
    const int v = 8 + (index - 232) * 10;
    return QColor(v, v, v);
}

QColor TerminalEmulator::resolveForeground(TermColor c) const
{
    if (c == kColorDefault)
        return m_defaultFg;
    if (isTrueColor(c))
        return trueColorOf(c);
    return paletteColor(c);
}

QColor TerminalEmulator::resolveBackground(TermColor c) const
{
    if (c == kColorDefault)
        return m_defaultBg;
    if (isTrueColor(c))
        return trueColorOf(c);
    return paletteColor(c);
}

QColor TerminalEmulator::cellForeground(const TermCell &cell) const
{
    QColor color;
    if (m_reverseVideo || (cell.attrs & AttrReverse))
        color = resolveBackground(cell.bg);
    else
        color = resolveForeground(cell.fg);

    // SGR 1 additionally promotes the eight ANSI colours to their bright
    // variants. True colours and the 256-colour palette are already as bright
    // as the program asked for, so they are left alone.
    if ((cell.attrs & AttrBold) && !isTrueColor(cell.fg) && cell.fg >= 0 && cell.fg < 8)
        color = paletteColor(cell.fg + 8);

    if (cell.attrs & AttrDim)
        color = blend(color, resolveBackground(cell.bg), 0.4);

    return color;
}

QColor TerminalEmulator::cellBackground(const TermCell &cell) const
{
    QColor color;
    if (m_reverseVideo || (cell.attrs & AttrReverse))
        color = resolveForeground(cell.fg);
    else
        color = resolveBackground(cell.bg);

    if (cell.attrs & AttrDim)
        color = blend(color, resolveForeground(cell.fg), 0.4);

    return color;
}

// ── Key encoding ───────────────────────────────────────────────────────────

QByteArray TerminalEmulator::encodeKey(int key, Qt::KeyboardModifiers mods,
                                        const QString &text, bool *handled) const
{
    if (handled)
        *handled = true;

    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool alt = mods.testFlag(Qt::AltModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);

    // xterm modifier parameter: 1 + shift(1) + alt(2) + ctrl(4)
    const int xmod = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);

    auto csiKey = [this, xmod](char final) -> QByteArray {
        if (xmod == 1) {
            if (m_appCursorKeys)
                return QByteArray("\x1bO") + final;
            return QByteArray("\x1b[") + final;
        }
        return QByteArray("\x1b[1;") + QByteArray::number(xmod) + final;
    };
    auto tildeKey = [xmod](int number) -> QByteArray {
        if (xmod == 1)
            return "\x1b[" + QByteArray::number(number) + "~";
        return "\x1b[" + QByteArray::number(number) + ";" + QByteArray::number(xmod) + "~";
    };
    auto ss3Key = [xmod](char final) -> QByteArray {
        if (xmod == 1)
            return QByteArray("\x1bO") + final;
        return QByteArray("\x1b[1;") + QByteArray::number(xmod) + final;
    };

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return "\r";
    case Qt::Key_Backspace:
        return ctrl ? QByteArray(1, '\x08') : QByteArray(1, '\x7f');
    case Qt::Key_Tab:
        return shift ? QByteArray("\x1b[Z") : QByteArray("\t");
    case Qt::Key_Backtab:
        return "\x1b[Z";
    case Qt::Key_Escape:
        return "\x1b";
    case Qt::Key_Up:
        return csiKey('A');
    case Qt::Key_Down:
        return csiKey('B');
    case Qt::Key_Right:
        return csiKey('C');
    case Qt::Key_Left:
        return csiKey('D');
    case Qt::Key_Home:
        return csiKey('H');
    case Qt::Key_End:
        return csiKey('F');
    case Qt::Key_Insert:
        return tildeKey(2);
    case Qt::Key_Delete:
        return tildeKey(3);
    case Qt::Key_PageUp:
        return tildeKey(5);
    case Qt::Key_PageDown:
        return tildeKey(6);
    case Qt::Key_F1:
        return ss3Key('P');
    case Qt::Key_F2:
        return ss3Key('Q');
    case Qt::Key_F3:
        return ss3Key('R');
    case Qt::Key_F4:
        return ss3Key('S');
    case Qt::Key_F5:
        return tildeKey(15);
    case Qt::Key_F6:
        return tildeKey(17);
    case Qt::Key_F7:
        return tildeKey(18);
    case Qt::Key_F8:
        return tildeKey(19);
    case Qt::Key_F9:
        return tildeKey(20);
    case Qt::Key_F10:
        return tildeKey(21);
    case Qt::Key_F11:
        return tildeKey(23);
    case Qt::Key_F12:
        return tildeKey(24);
    default:
        break;
    }

    if (ctrl && !text.isEmpty()) {
        const ushort u = text.at(0).unicode();
        if (u >= 'a' && u <= 'z')
            return QByteArray(1, static_cast<char>(u - 'a' + 1));
        if (u >= 'A' && u <= 'Z')
            return QByteArray(1, static_cast<char>(u - 'A' + 1));
        if (u == '@' || u == ' ' || u == '[' || u == '\\' || u == ']' || u == '^' || u == '_')
            return QByteArray(1, static_cast<char>(u - '@'));
        if (u == '?')
            return QByteArray(1, '\x7f');
    }

    if (text.isEmpty())
        return QByteArray();

    const QByteArray base = text.toUtf8();
    if (alt)
        return "\x1b" + base;
    return base;
}

QByteArray TerminalEmulator::encodeMouse(int button, int col, int row, bool press,
                                          bool release, Qt::KeyboardModifiers mods) const
{
    if (m_mouseMode == 0)
        return QByteArray();

    int code = button;
    if (mods.testFlag(Qt::ShiftModifier))
        code += 4;
    if (mods.testFlag(Qt::ControlModifier))
        code += 8;
    if (mods.testFlag(Qt::AltModifier))
        code += 16;
    // The legacy encodings cannot express "which button was released", so they
    // report every release as button 3.
    if (!press && !m_mouseSgr)
        code += 3;

    if (m_mouseSgr) {
        // SGR 1006: "ESC [ < button ; col ; row (M|m)", which unlike the legacy
        // encoding can name the button that was released.
        QByteArray data = "\x1b[<" + QByteArray::number(code) + ";"
                          + QByteArray::number(col) + ";" + QByteArray::number(row);
        data += release ? 'm' : 'M';
        return data;
    }

    return "\x1b[M" + QByteArray(1, static_cast<char>(code + 32))
           + QByteArray(1, static_cast<char>(col + 32))
           + QByteArray(1, static_cast<char>(row + 32));
}
