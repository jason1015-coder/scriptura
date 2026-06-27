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

        // App icon
        QLabel *iconLabel = new QLabel(this);
        QPixmap pixmap(":/icon.png");
        if (pixmap.isNull()) {
            // Fallback: try loading from file system
            pixmap.load("icon.png");
        }
        if (!pixmap.isNull()) {
            iconLabel->setPixmap(pixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconLabel->setText("Scriptura");
            iconLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: palette(text);");
        }
        iconLabel->setAlignment(Qt::AlignCenter);

        // App name
        QLabel *nameLabel = new QLabel(tr("Scriptura"), this);
        nameLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: palette(text);");
        nameLabel->setAlignment(Qt::AlignCenter);

        layout->addStretch();
        layout->addWidget(iconLabel, 0, Qt::AlignCenter);
        layout->addWidget(nameLabel, 0, Qt::AlignCenter);
        layout->addStretch();

        // Set initial style (will be overridden by setThemeBackground)
        setStyleSheet(R"(
            SplashScreen {
                background-color: white;
                border: 1px solid palette(mid);
                border-radius: 16px;
            }
        )");
    }

    void setThemeBackground(const QColor &color)
    {
        setStyleSheet(QString(R"(
            SplashScreen {
                background-color: %1;
                border: 1px solid palette(mid);
                border-radius: 16px;
            }
        )").arg(color.name()));
    }

    void showWithDelay(int milliseconds = 4000)
    {
        // Adjust size to content and center on screen
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
        
        // Close after delay
        QTimer::singleShot(milliseconds, this, &QWidget::close);
    }
};

#endif // SPLASHSCREEN_H