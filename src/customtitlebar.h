#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>

class CustomTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget *parent = nullptr);

    void handleMousePress(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void stopDrag();

    QPushButton* minimizeButton;
    QPushButton* maximizeButton;
    QPushButton* closeButton;
    QLabel* titleLabel;
    QPushButton* sidebarToggleButton;
    QPushButton* inspectorToggleButton;
    QLineEdit* searchField;

    // Mirror the whole control row: what sits at the leading edge in the
    // classic layout moves to the trailing edge, and vice versa.
    void setMirrored(bool mirrored);
    bool isMirrored() const { return m_mirrored; }

signals:
    void windowMoveRequested();
    void maximizeRequest();
    void minimizeRequest();
    void closeRequest();
    void sidebarToggleClicked();
    void inspectorToggleClicked();
    void searchRequested(const QString &query);

protected:
    void paintEvent(QPaintEvent *event) override;
    // Watches the (transparent) window control buttons so this bar can repaint
    // their hover/pressed visuals, which are drawn here in paintEvent().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_isDragging = false;
    QPoint m_dragPosition;
    bool m_mirrored = false;
    QHBoxLayout *m_layout = nullptr;
    int m_spacingXs = 4;
    int m_spacingSm = 8;

    void styleButtons();
    void setupLayout();
    // Paints the bar's opaque background with top corners rounded to match the
    // editor container, so its square edges don't poke out over the rounded
    // frame (or let the window's mid-coloured ring show through the corners).
    void paintTitleBarBackground(QPainter &p);
    void paintWindowControls(QPainter &p, QPushButton *button, const QString &glyph);
    // Theme text colour when it contrasts with the title bar background,
    // otherwise black/white — guarantees the glyphs are always visible.
    QColor windowControlForeground() const;
};

#endif // CUSTOMTITLEBAR_H
