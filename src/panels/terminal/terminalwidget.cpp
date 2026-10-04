#include "terminal/terminalwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace {

// Characters that make up a "word" for double-click selection, following the
// shell convention of treating path and identifier punctuation as part of it.
// Takes a QStringView because a cell can hold an astral code point.
bool isWordChar(QStringView text)
{
    if (text.size() != 1)
        return false;
    const QChar c = text.at(0);
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-')
           || c == QLatin1Char('.') || c == QLatin1Char('/') || c == QLatin1Char('~')
           || c == QLatin1Char(':') || c == QLatin1Char('$') || c == QLatin1Char('@')
           || c == QLatin1Char('%') || c == QLatin1Char('+') || c == QLatin1Char('=');
}

} // namespace

// ── Construction / theming ─────────────────────────────────────────────────

TerminalView::TerminalView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    // Enables preedit (IME) text so input methods compose into the terminal.
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setCursor(Qt::IBeamCursor);

    verticalScrollBar()->setSingleStep(1);

    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &TerminalView::onScrollBarValueChanged);

    applyStyle();
    syncScrollRange();
}

TerminalView::~TerminalView() = default;

void TerminalView::setEmulator(TerminalEmulator *emulator)
{
    if (m_emulator == emulator)
        return;

    if (m_emulator)
        disconnect(m_emulator, nullptr, this, nullptr);

    m_emulator = emulator;
    if (m_emulator) {
        connect(m_emulator, &TerminalEmulator::screenChanged,
                this, &TerminalView::onScreenChanged);
        // The panel may delete the emulator before the view does; drop the
        // pointer instead of dangling.
        connect(m_emulator, &QObject::destroyed, this, [this]() { m_emulator = nullptr; });
        m_pendingResize = true;
    }

    applyStyle();
    syncEmulatorSize();
    syncScrollRange();
    viewport()->update();
}

void TerminalView::applyStyle()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace, QFont::PreferMatch);
    font.setPointSize(m_fontPointSize);
    font.setFixedPitch(true);
    // Bold and italic must be available for SGR 1 / SGR 3 to render at all.
    font.setWeight(QFont::Normal);
    font.setItalic(false);
    setFont(font);

    recalculateMetrics();
    applyThemePalette();
}

void TerminalView::applyThemePalette()
{
    const QPalette pal = palette();
    if (m_emulator) {
        m_defaultForeground = m_emulator->defaultForeground();
        m_defaultBackground = m_emulator->defaultBackground();
    } else {
        m_defaultForeground = pal.color(QPalette::WindowText);
        m_defaultBackground = pal.color(QPalette::Base);
    }
    m_selectionBackground = pal.color(QPalette::Highlight);
    m_selectionForeground = pal.color(QPalette::HighlightedText);
    m_cursorColor = pal.color(QPalette::Text);
    viewport()->update();
}

void TerminalView::changeEvent(QEvent *event)
{
    QAbstractScrollArea::changeEvent(event);
    switch (event->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
    case QEvent::FontChange:
        applyThemePalette();
        break;
    default:
        break;
    }
}

void TerminalView::recalculateMetrics()
{
    const QFontMetricsF metrics(font());
    m_cellWidth = qMax(1, static_cast<int>(qCeil(metrics.horizontalAdvance(QLatin1Char('M')))));
    m_cellHeight = qMax(1, static_cast<int>(qCeil(metrics.height())));
    m_paddingX = 4;
    m_paddingY = 2;
    verticalScrollBar()->setPageStep(visibleRows());
    m_pendingResize = true;
}

// ── Geometry ───────────────────────────────────────────────────────────────

int TerminalView::visibleRows() const
{
    const int usable = viewport()->height() - 2 * m_paddingY;
    return qMax(1, usable / qMax(1, m_cellHeight));
}

int TerminalView::topLine() const
{
    if (!m_emulator)
        return 0;
    const int total = m_emulator->buffer()->totalLines();
    return qMax(0, total - visibleRows() - verticalScrollBar()->value());
}

QPoint TerminalView::positionAt(const QPoint &viewportPos) const
{
    const int col = (viewportPos.x() - m_paddingX) / qMax(1, m_cellWidth);
    const int row = (viewportPos.y() - m_paddingY) / qMax(1, m_cellHeight);
    return QPoint(col, row);
}

QSize TerminalView::sizeHint() const
{
    const int columns = m_emulator ? m_emulator->columns() : 80;
    const int rows = m_emulator ? m_emulator->rows() : 24;
    return QSize(columns * m_cellWidth + 2 * m_paddingX + verticalScrollBar()->sizeHint().width(),
                 rows * m_cellHeight + 2 * m_paddingY);
}

// ── Scrolling ──────────────────────────────────────────────────────────────

void TerminalView::syncScrollRange()
{
    const int total = m_emulator ? m_emulator->buffer()->totalLines() : 0;
    QScrollBar *bar = verticalScrollBar();
    bar->setPageStep(visibleRows());
    bar->setRange(0, qMax(0, total - visibleRows()));
}

void TerminalView::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->minimum());
}

void TerminalView::scrollLines(int lines)
{
    verticalScrollBar()->setValue(verticalScrollBar()->value() + lines);
}

bool TerminalView::isScrolledBack() const
{
    return verticalScrollBar()->value() > verticalScrollBar()->minimum();
}

void TerminalView::onScreenChanged()
{
    syncScrollRange();
    viewport()->update();
}

void TerminalView::onScrollBarValueChanged(int value)
{
    Q_UNUSED(value);
    viewport()->update();
}

void TerminalView::resetBuffer()
{
    if (!m_emulator)
        return;
    m_emulator->reset();
    clearSelection();
    scrollToBottom();
    syncScrollRange();
    viewport()->update();
}

void TerminalView::clearScrollback()
{
    if (!m_emulator)
        return;
    m_emulator->buffer()->clearScrollback();
    scrollToBottom();
    syncScrollRange();
    viewport()->update();
}

void TerminalView::syncEmulatorSize()
{
    if (!m_emulator)
        return;

    const int columns = qMax(2, (viewport()->width() - 2 * m_paddingX) / qMax(1, m_cellWidth));
    const int rows = visibleRows();
    m_pendingResize = false;
    if (m_emulator->columns() == columns && m_emulator->rows() == rows)
        return;
    emit requestResize(columns, rows);
}

// ── Painting ───────────────────────────────────────────────────────────────

void TerminalView::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), m_defaultBackground);

    if (!m_emulator)
        return;

    const TerminalBuffer *buffer = m_emulator->buffer();
    const int top = topLine();
    const int rows = visibleRows();
    const int columns = m_emulator->columns();
    const QRect dirty = event->rect();

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font());

    // Where the selection starts and ends, in viewport row/column space.
    QPoint selStart = m_selectionAnchor;
    QPoint selEnd = m_selectionHead;
    if (selEnd.y() < selStart.y() || (selEnd.y() == selStart.y() && selEnd.x() < selStart.x()))
        std::swap(selStart, selEnd);
    const bool selecting = m_selecting || hasSelection();

    const int firstRow = qMax(0, dirty.top() / m_cellHeight);
    const int lastRow = qMin(rows - 1, dirty.bottom() / m_cellHeight);

    for (int r = firstRow; r <= lastRow; ++r) {
        const int combined = top + r;
        if (combined < 0 || combined >= buffer->totalLines())
            continue;
        const TermLine &line = buffer->line(combined);
        const int y = m_paddingY + r * m_cellHeight;

        // A selection that starts or ends mid-row only covers part of it.
        int selFrom = 0;
        int selTo = -1;
        if (selecting && r >= selStart.y() && r <= selEnd.y()) {
            selFrom = (r == selStart.y()) ? selStart.x() : 0;
            selTo = (r == selEnd.y()) ? selEnd.x() : columns;
            selFrom = qBound(0, selFrom, columns);
            selTo = qBound(selFrom, selTo, columns);
        }

        // Runs of identically formatted cells are drawn as one drawText() call:
        // a full-screen repaint would otherwise issue thousands of them.
        int runStart = 0;
        while (runStart < columns) {
            const TermCell &first = line.cells.value(runStart, TerminalBuffer::blankCell());
            if (first.isContinuation()) {
                ++runStart;
                continue;
            }

            int runEnd = runStart + 1;
            while (runEnd < columns) {
                const TermCell &next = line.cells.value(runEnd, TerminalBuffer::blankCell());
                if (next.isContinuation() || next.attrs != first.attrs || next.fg != first.fg
                    || next.bg != first.bg)
                    break;
                ++runEnd;
            }

            const QRect cellRect(m_paddingX + runStart * m_cellWidth, y,
                                 (runEnd - runStart) * m_cellWidth, m_cellHeight);

            // Backgrounds first: drawText would otherwise be clipped by them.
            if (selTo > selFrom) {
                const int from = qMax(runStart, selFrom);
                const int to = qMin(runEnd, selTo);
                if (to > from)
                    painter.fillRect(QRect(m_paddingX + from * m_cellWidth, y,
                                           (to - from) * m_cellWidth, m_cellHeight),
                                    m_selectionBackground);
            }
            if (first.bg != kColorDefault
                || (first.attrs & AttrReverse) != 0
                || m_emulator->reverseVideo()) {
                if (selTo <= selFrom || runEnd <= selFrom || runStart >= selTo)
                    painter.fillRect(cellRect, m_emulator->cellBackground(first));
            }

            QString text;
            for (int c = runStart; c < runEnd; ++c) {
                const char32_t cp = line.cells.value(c, TerminalBuffer::blankCell()).ch;
                text.append(QChar::fromUcs4(cp == U'\0' ? U' ' : cp));
            }

            const bool selected = selTo > selFrom && runEnd > selFrom && runStart < selTo;
            painter.setPen(selected ? m_selectionForeground : m_emulator->cellForeground(first));
            const bool bold = first.attrs & AttrBold;
            const bool italic = first.attrs & AttrItalic;
            if (bold || italic) {
                QFont styled = font();
                if (bold)
                    styled.setWeight(QFont::Bold);
                styled.setItalic(italic);
                painter.setFont(styled);
            }
            painter.drawText(cellRect, Qt::AlignLeft | Qt::AlignVCenter, text);
            if (bold || italic)
                painter.setFont(font());

            if (first.attrs & AttrUnderline) {
                painter.fillRect(QRect(m_paddingX + runStart * m_cellWidth, y + m_cellHeight - 2,
                                       (runEnd - runStart) * m_cellWidth, 1),
                                 selected ? m_selectionForeground
                                          : m_emulator->cellForeground(first));
            }
            if (first.attrs & AttrStrike) {
                const int mid = y + m_cellHeight / 2;
                painter.fillRect(QRect(m_paddingX + runStart * m_cellWidth, mid,
                                       (runEnd - runStart) * m_cellWidth, 1),
                                 selected ? m_selectionForeground
                                          : m_emulator->cellForeground(first));
            }

            runStart = runEnd;
        }
    }

    // The cursor is drawn last so it is never covered by a run's background.
    if (m_emulator->cursorVisible() && hasFocus()) {
        const int cursorRow = buffer->cursor().row;
        const int cursorCol = buffer->cursor().col;
        const int combined = buffer->historyCount() + cursorRow;
        const int viewportRow = combined - top;
        if (viewportRow >= 0 && viewportRow < rows) {
            const TermLine &line = buffer->line(combined);
            const TermCell cell = line.cells.value(cursorCol, TerminalBuffer::blankCell());
            const int width = cell.isContinuation() ? 1 : qMax(1, static_cast<int>(cell.width));
            const QRect cursorRect(m_paddingX + cursorCol * m_cellWidth,
                                   m_paddingY + viewportRow * m_cellHeight,
                                   width * m_cellWidth, m_cellHeight);
            painter.fillRect(cursorRect, m_cursorColor);
            if (cell.ch != U' ' && cell.ch != U'\0') {
                painter.setPen(m_defaultBackground);
                painter.drawText(cursorRect, Qt::AlignLeft | Qt::AlignVCenter,
                                 QString::fromUcs4(&cell.ch, 1));
            }
        }
    }
}

// ── Selection ──────────────────────────────────────────────────────────────

bool TerminalView::hasSelection() const
{
    return m_selectionAnchor != m_selectionHead;
}

QRect TerminalView::selectionRect(const QPoint &anchor, const QPoint &head) const
{
    QPoint start = anchor;
    QPoint end = head;
    if (end.y() < start.y() || (end.y() == start.y() && end.x() < start.x()))
        std::swap(start, end);
    return QRect(m_paddingX + start.x() * m_cellWidth, m_paddingY + start.y() * m_cellHeight,
                 (end.x() - start.x() + 1) * m_cellWidth, m_cellHeight);
}

QString TerminalView::selectionText(const QPoint &anchor, const QPoint &head) const
{
    if (!m_emulator)
        return QString();

    QPoint start = anchor;
    QPoint end = head;
    if (end.y() < start.y() || (end.y() == start.y() && end.x() < start.x()))
        std::swap(start, end);

    const TerminalBuffer *buffer = m_emulator->buffer();
    const int top = topLine();
    QString out;
    bool first = true;

    for (int r = start.y(); r <= end.y(); ++r) {
        const int combined = top + r;
        if (combined < 0 || combined >= buffer->totalLines())
            continue;
        const TermLine &line = buffer->line(combined);
        const int last = qMax(0, line.cells.size() - 1);
        const int from = qBound(0, r == start.y() ? start.x() : 0, last);
        const int to = qBound(from, r == end.y() ? end.x() : last, last);
        QString text;
        for (int c = from; c <= to && c < line.cells.size(); ++c) {
            const TermCell &cell = line.cells.at(c);
            if (cell.isContinuation())
                continue;
            text.append(QChar::fromUcs4(cell.ch == U'\0' ? U' ' : cell.ch));
        }
        if (!first && !line.wrapped)
            out += QLatin1Char('\n');
        out += text;
        first = false;
    }
    return out;
}

QString TerminalView::selectedText() const
{
    if (!hasSelection())
        return QString();
    return selectionText(m_selectionAnchor, m_selectionHead);
}

void TerminalView::copySelection()
{
    const QString text = selectedText();
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
}

void TerminalView::clearSelection()
{
    if (!hasSelection())
        return;
    m_selectionAnchor = m_selectionHead;
    viewport()->update();
}

void TerminalView::pasteFromClipboard()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    QByteArray data = text.toUtf8();
    if (m_emulator && m_emulator->bracketedPaste())
        data = "\x1b[200~" + data + "\x1b[201~";
    emit dataToSend(data);
}

void TerminalView::selectAll()
{
    const int columns = m_emulator ? m_emulator->columns() : 80;
    m_selectionAnchor = QPoint(0, 0);
    m_selectionHead = QPoint(qMax(0, columns - 1), qMax(0, visibleRows() - 1));
    viewport()->update();
}

// ── Input ──────────────────────────────────────────────────────────────────

void TerminalView::sendKey(QKeyEvent *event)
{
    if (!m_emulator)
        return;

    bool handled = true;
    const QByteArray data = m_emulator->encodeKey(event->key(), event->modifiers(),
                                                  event->text(), &handled);
    if (!handled || data.isEmpty())
        return;
    emit dataToSend(data);
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if (!m_emulator) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);

    // Editing shortcuts that must never reach the shell. A live selection wins
    // over Ctrl+C, which is what every other terminal does.
    if (ctrl && shift && event->key() == Qt::Key_C) {
        copySelection();
        return;
    }
    if (ctrl && shift && event->key() == Qt::Key_V) {
        pasteFromClipboard();
        return;
    }
    if (ctrl && event->key() == Qt::Key_C && hasSelection()) {
        copySelection();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        return;
    }
    if (event->key() == Qt::Key_Escape && hasSelection()) {
        clearSelection();
        return;
    }
    if (!ctrl && event->key() == Qt::Key_PageUp) {
        scrollLines(-qMax(1, visibleRows() - 1));
        viewport()->update();
        return;
    }
    if (!ctrl && event->key() == Qt::Key_PageDown) {
        scrollLines(qMax(1, visibleRows() - 1));
        viewport()->update();
        return;
    }

    sendKey(event);
    event->accept();
}

void TerminalView::sendMouseReport(int button, const QPoint &viewportPos, bool press, bool release)
{
    if (!m_emulator || m_emulator->mouseMode() == 0)
        return;

    const TerminalBuffer *buffer = m_emulator->buffer();
    const QPoint pos = positionAt(viewportPos);
    // Mouse coordinates are 1-based and always relative to the live screen,
    // never the scrollback, so the viewport row is mapped back through it.
    const int screenRow = pos.y() - (topLine() - buffer->historyCount());
    const int col = qBound(0, pos.x(), m_emulator->columns() - 1);
    const int row = qBound(0, screenRow, m_emulator->rows() - 1);

    const QByteArray data = m_emulator->encodeMouse(button, col + 1, row + 1, press, release,
                                                    QApplication::keyboardModifiers());
    if (!data.isEmpty())
        emit dataToSend(data);
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    setFocus(Qt::MouseFocusReason);

    const bool mouseMode = m_emulator && m_emulator->mouseMode() != 0;
    if (mouseMode) {
        if (event->button() == Qt::MiddleButton) {
            pasteFromClipboard();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
            if (m_mouseTracking) {
                m_mouseTracking = false;
                sendMouseReport(0, pos, false, true);
            }
            const int button = event->button() == Qt::LeftButton ? 0 : 2;
            m_mouseTracking = true;
            viewport()->setCursor(Qt::BlankCursor);
            sendMouseReport(button, pos, true, false);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        m_selectionAnchor = positionAt(pos);
        m_selectionHead = m_selectionAnchor;
        m_selecting = true;
        viewport()->update();
        event->accept();
        return;
    }

    QAbstractScrollArea::mousePressEvent(event);
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();

    if (m_mouseTracking) {
        // Button 3 with no press/release flag is "motion with no button held",
        // which is what both the legacy and SGR encodings use for dragging.
        if (m_emulator && m_emulator->mouseMode() >= 1002)
            sendMouseReport(3, pos, true, false);
        event->accept();
        return;
    }

    if (m_selecting) {
        m_selectionHead = positionAt(pos);
        viewport()->update();
        event->accept();
        return;
    }

    QAbstractScrollArea::mouseMoveEvent(event);
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();

    if (m_selecting) {
        m_selecting = false;
        if (hasSelection() && QGuiApplication::clipboard()->supportsSelection()) {
            QGuiApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
        }
        viewport()->update();
        event->accept();
        return;
    }

    if (m_mouseTracking) {
        m_mouseTracking = false;
        const int button = event->button() == Qt::LeftButton ? 0
                          : event->button() == Qt::RightButton ? 2 : 1;
        viewport()->unsetCursor();
        sendMouseReport(button, pos, false, true);
        event->accept();
        return;
    }

    QAbstractScrollArea::mouseReleaseEvent(event);
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_emulator || (m_emulator && m_emulator->mouseMode() != 0)) {
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }

    const QPoint pos = positionAt(event->position().toPoint());
    const int combined = topLine() + pos.y();
    if (combined < 0 || combined >= m_emulator->buffer()->totalLines()) {
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }

    const TermLine &line = m_emulator->buffer()->line(combined);
    const int columns = line.cells.size();
    int from = qBound(0, pos.x(), qMax(0, columns - 1));
    int to = from;
    while (from > 0 && isWordChar(QChar::fromUcs4(line.cells.at(from - 1).ch)))
        --from;
    while (to + 1 < columns && isWordChar(QChar::fromUcs4(line.cells.at(to + 1).ch)))
        ++to;

    m_selectionAnchor = QPoint(from, pos.y());
    m_selectionHead = QPoint(to, pos.y());
    viewport()->update();
    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);

    // Ctrl+wheel zooms, matching every other terminal; a plain wheel scrolls the
    // scrollback and does not move the keyboard focus away from the shell.
    if (ctrl) {
        const int steps = event->angleDelta().y();
        if (steps > 0)
            increaseFontSize();
        else if (steps < 0)
            decreaseFontSize();
        event->accept();
        return;
    }

    int lines = 0;
    if (!event->pixelDelta().isNull())
        lines = event->pixelDelta().y() / qMax(1, m_cellHeight);
    else
        lines = event->angleDelta().y() / 20;

    if (lines != 0) {
        scrollLines(lines);
        viewport()->update();
    }
    event->accept();
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    // Both the scroll range and the grid size follow the viewport, so the child
    // process is told about the new size and can redraw its own layout.
    syncScrollRange();
    syncEmulatorSize();
    viewport()->update();
}

void TerminalView::focusInEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusInEvent(event);
    if (m_emulator && m_emulator->focusEventsEnabled())
        emit dataToSend(QByteArray("\x1b[I"));
    viewport()->update();
}

void TerminalView::focusOutEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusOutEvent(event);
    if (m_emulator && m_emulator->focusEventsEnabled())
        emit dataToSend(QByteArray("\x1b[O"));
    viewport()->update();
}

void TerminalView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *copy = menu.addAction(tr("Copy"));
    copy->setEnabled(hasSelection());
    QAction *paste = menu.addAction(tr("Paste"));
    QAction *selectAllAction = menu.addAction(tr("Select All"));
    menu.addSeparator();

    QAction *clear = menu.addAction(tr("Clear Scrollback"));
    QAction *reset = menu.addAction(tr("Reset Terminal"));
    menu.addSeparator();

    QAction *zoomIn = menu.addAction(tr("Increase Font Size"));
    QAction *zoomOut = menu.addAction(tr("Decrease Font Size"));

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;

    if (chosen == copy)
        copySelection();
    else if (chosen == paste)
        pasteFromClipboard();
    else if (chosen == selectAllAction)
        selectAll();
    else if (chosen == clear)
        emit requestClearScrollback();
    else if (chosen == reset)
        emit requestReset();
    else if (chosen == zoomIn)
        increaseFontSize();
    else if (chosen == zoomOut)
        decreaseFontSize();
}

bool TerminalView::eventFilter(QObject *watched, QEvent *event)
{
    // Focus changes arrive here as well as through focusInEvent()/focusOutEvent()
    // (QAbstractScrollArea forwards the viewport's focus events to QFrame::event),
    // so only the plain pass-through is left to avoid reporting focus twice.
    return QAbstractScrollArea::eventFilter(watched, event);
}

// ── Font size ──────────────────────────────────────────────────────────────

void TerminalView::setFontSize(int pointSize)
{
    const int clamped = qBound(6, pointSize, 32);
    if (clamped == m_fontPointSize)
        return;
    m_fontPointSize = clamped;
    applyStyle();
    syncEmulatorSize();
    syncScrollRange();
    viewport()->update();
}

void TerminalView::increaseFontSize()
{
    setFontSize(m_fontPointSize + 1);
}

void TerminalView::decreaseFontSize()
{
    setFontSize(m_fontPointSize - 1);
}