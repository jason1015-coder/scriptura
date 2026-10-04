#ifndef SCRIPTURA_TERMINALWIDGET_H
#define SCRIPTURA_TERMINALWIDGET_H

#include "terminal/terminalemulator.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QString>

/**
 * Renders a TerminalEmulator and turns input back into bytes.
 *
 * The grid is painted directly rather than through a QTextDocument: the
 * emulator owns the authoritative buffer, and per-cell formatting in a document
 * would mean rewriting thousands of lines on every repaint.
 *
 * The vertical scrollbar's value is the distance from the live bottom of the
 * buffer, so 0 means "following output" and anything larger is scrollback.
 */
class TerminalView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit TerminalView(QWidget *parent = nullptr);
    ~TerminalView() override;

    /// The emulator is not owned: the panel keeps it alive alongside the view.
    void setEmulator(TerminalEmulator *emulator);
    TerminalEmulator *emulator() const { return m_emulator; }

    void resetBuffer();
    void clearScrollback();
    void scrollToBottom();
    void scrollLines(int lines);
    bool isScrolledBack() const;

    QString selectedText() const;
    bool hasSelection() const;
    void copySelection();
    void clearSelection();
    void pasteFromClipboard();

    int fontSize() const { return m_fontPointSize; }
    void setFontSize(int pointSize);
    void increaseFontSize();
    void decreaseFontSize();

    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }
    int visibleRows() const;

    /// Re-derives the font metrics and the colours from the current theme.
    void applyStyle();

    QSize sizeHint() const override;

signals:
    void dataToSend(QByteArray data);
    void requestResize(int columns, int rows);
    void requestReset();
    void requestClearScrollback();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onScreenChanged();
    void onScrollBarValueChanged(int value);

private:
    void applyThemePalette();
    void recalculateMetrics();
    void syncScrollRange();
    void syncEmulatorSize();

    int topLine() const;
    QPoint positionAt(const QPoint &viewportPos) const;
    QRect selectionRect(const QPoint &anchor, const QPoint &head) const;
    QString selectionText(const QPoint &anchor, const QPoint &head) const;
    void selectAll();

    void sendMouseReport(int button, const QPoint &viewportPos, bool press, bool release);
    void sendKey(QKeyEvent *event);

    TerminalEmulator *m_emulator = nullptr;

    QFont m_font;
    int m_fontPointSize = 12;
    int m_cellWidth = 8;
    int m_cellHeight = 16;
    int m_paddingX = 4;
    int m_paddingY = 3;

    QColor m_defaultBackground;
    QColor m_defaultForeground;
    QColor m_selectionBackground;
    QColor m_selectionForeground;
    QColor m_cursorColor;

    QPoint m_selectionAnchor;
    QPoint m_selectionHead;
    bool m_selecting = false;
    bool m_mouseTracking = false;

    bool m_pendingResize = true;
};

#endif // SCRIPTURA_TERMINALWIDGET_H
