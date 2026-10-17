#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QPalette>
#include <QColor>
#include <QSvgRenderer>
#include <QPainter>
#include "themeicons.h"

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
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(10);

        // App icon (SVG)
        QLabel *iconLabel = new QLabel(this);
        iconLabel->setObjectName("splashIcon");
        iconLabel->setAlignment(Qt::AlignCenter);

        // App name
        QLabel *nameLabel = new QLabel(tr("Scriptura"), this);
        nameLabel->setObjectName("splashName");
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-size: 24px; font-weight: bold;");

        layout->addStretch();
        layout->addWidget(iconLabel, 0, Qt::AlignCenter);
        layout->addWidget(nameLabel, 0, Qt::AlignCenter);
        layout->addStretch();

        // Set initial style (will be overridden by setThemeBackground)
        setStyleSheet(R"(
            SplashScreen {
                background-color: palette(window);
                border: none;
                border-radius: 14px;
            }
        )");
    }

    void setThemeBackground(const QColor &color)
    {
        double lum = 0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue();
        bool isDark = lum < 160.0;
        QColor textColor = isDark ? QColor(220, 220, 220) : QColor(30, 30, 32);

        setStyleSheet(QString(R"(
            SplashScreen {
                background-color: %1;
                border: none;
                border-radius: 14px;
            }
            QLabel {
                color: %2;
            }
        )").arg(color.name()).arg(textColor.name()));

        QLabel *iconLabel = findChild<QLabel*>("splashIcon");
        if (iconLabel) {
            iconLabel->setPixmap(ThemeIcons::instance()->pixmap(":/icons/app-icon.svg", textColor, 128));
        }
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