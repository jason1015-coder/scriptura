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
    QPushButton* inspectorToggleButton;
    QLineEdit* searchField;

signals:
    void windowMoveRequested();
    void maximizeRequest();
    void minimizeRequest();
    void closeRequest();
    void sidebarToggleClicked();
    void inspectorToggleClicked();
    void searchRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_isDragging = false;
    QPoint m_dragPosition;

    void styleButtons();
    void setupLayout();
    void paintWindowControls(QPainter &p, const QRect &buttonRect, bool hovered, bool pressed, const QString &glyph);
};

#endif // CUSTOMTITLEBAR_H
