#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>

class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QLabel *nameLabel = new QLabel(tr("sc"), this);
        nameLabel->setObjectName("splashName");
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-size: 140px; color: white; font-weight: bold;");

        layout->addStretch();
        layout->addWidget(nameLabel, 0, Qt::AlignCenter);
        layout->addStretch();

        QSizePolicy sp = sizePolicy();
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
        setMinimumSize(560, 380);
        setMaximumSize(560, 380);
        setStyleSheet("background-color: black;");
    }

    void showWithDelay(int milliseconds = 2500)
    {
        adjustSize();
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int x = (screenGeometry.width() - width()) / 2;
            int y = (screenGeometry.height() - height()) / 2;
            move(x, y);
        }

        show();
        raise();
        QApplication::processEvents();
    }
};

#endif // SPLASHSCREEN_H