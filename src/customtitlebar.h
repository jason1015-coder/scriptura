#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QPainter>
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
    QPushButton* settingsButton;
    QPushButton* inspectorToggleButton;
    QLineEdit* searchField;

signals:
    void windowMoveRequested();
    void maximizeRequest();
    void minimizeRequest();
    void closeRequest();
    void sidebarToggleClicked();
    void settingsClicked();
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

    void styleButtons();
    void setupLayout();
    void paintWindowControls(QPainter &p, QPushButton *button, const QString &glyph);
    // Theme text colour when it contrasts with the title bar background,
    // otherwise black/white — guarantees the glyphs are always visible.
    QColor windowControlForeground() const;
};

#endif // CUSTOMTITLEBAR_H
