#include "applicationdock.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QDebug>
#include <QtMath>
#include <QApplication>

ApplicationDock::ApplicationDock(QWidget* parent)
    : QWidget(parent)
    , m_easingCurve(QEasingCurve::OutCubic)
{
    setObjectName("applicationDock");
    setFixedHeight(64);
    setMinimumWidth(100);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);

    // Frosted glass backdrop
    setStyleSheet(R"(
        QWidget#applicationDock {
            background-color: rgba(30, 30, 30, 180);
            border: 1px solid rgba(255, 255, 255, 20);
            border-radius: 16px;
        }
    )");
}

ApplicationDock::~ApplicationDock()
{
    for (auto* btn : std::as_const(m_entries)) {
        delete btn;
    }
    m_entries.clear();
}

// ── App Management ───────────────────────────────────────────────

int ApplicationDock::addEntry(const QString& id, const QString& iconPath,
                               const QString& tooltip, const QColor& accentColor)
{
    if (hasEntry(id)) return -1;

    auto* entry = new DockButton();
    entry->entry.appId = id;
    entry->entry.iconPath = iconPath;
    entry->entry.tooltip = tooltip;
    entry->entry.accentColor = accentColor;

    entry->button = new QToolButton(this);
    entry->button->setIcon(QIcon(iconPath));
    entry->button->setIconSize(QSize(m_iconBaseSize, m_iconBaseSize));
    entry->button->setFixedSize(m_iconBaseSize + 16, m_iconBaseSize + 16);
    entry->button->setToolTip(tooltip);
    entry->button->setCursor(Qt::PointingHandCursor);
    entry->button->setStyleSheet(R"(
        QToolButton {
            border: none;
            border-radius: 12px;
            background: transparent;
            padding: 4px;
        }
        QToolButton:hover {
            background: rgba(255, 255, 255, 30);
        }
        QToolButton:pressed {
            background: rgba(255, 255, 255, 50);
        }
    )");

    connect(entry->button, &QToolButton::clicked, this, [this, id]() {
        emit appClicked(id);
    });

    m_entries.append(entry);
    layoutButtons();
    return m_entries.size() - 1;
}

void ApplicationDock::removeEntry(const QString& id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i]->entry.appId == id) {
            auto* btn = m_entries.takeAt(i);
            btn->button->deleteLater();
            delete btn;
            layoutButtons();
            return;
        }
    }
}

void ApplicationDock::setActiveApp(const QString& id)
{
    for (auto* btn : std::as_const(m_entries)) {
        btn->entry.active = (btn->entry.appId == id);
    }
    update();
}

bool ApplicationDock::hasEntry(const QString& id) const
{
    for (const auto* btn : std::as_const(m_entries)) {
        if (btn->entry.appId == id) return true;
    }
    return false;
}

// ── Magnification Property ───────────────────────────────────────

void ApplicationDock::setMagnification(float value)
{
    if (qFuzzyCompare(m_magnification, value)) return;
    m_magnification = value;
    update();
}

// ── Layout ───────────────────────────────────────────────────────

void ApplicationDock::layoutButtons()
{
    if (m_entries.isEmpty()) {
        setFixedWidth(0);
        hide();
        return;
    }

    show();

    int totalWidth = m_padding * 2;
    for (const auto* btn : std::as_const(m_entries)) {
        totalWidth += btn->button->width() + m_spacing;
    }
    totalWidth -= m_spacing; // no trailing spacing

    setFixedWidth(totalWidth);

    int x = m_padding;
    int centerY = (height() - m_iconBaseSize - 16) / 2;

    for (auto* btn : m_entries) {
        btn->button->move(x, centerY);
        x += btn->button->width() + m_spacing;
    }
}

// ── Mouse Tracking ───────────────────────────────────────────────

void ApplicationDock::updateMagnification(const QPoint& mousePos)
{
    if (m_entries.isEmpty()) return;

    int centerX = width() / 2;

    // Distance from mouse to each button center
    for (auto* btn : m_entries) {
        QRect btnRect = btn->button->geometry();
        int btnCenterX = btnRect.center().x();
        float dist = qAbs(mousePos.x() - btnCenterX);

        // Magnification falloff: closer = bigger
        float range = m_iconMaxSize * 1.5f;
        float factor = 1.0f - qBound(0.0f, dist / range, 1.0f);

        // Apply easing for smoother falloff
        factor = m_easingCurve.valueForProgress(factor);

        float targetScale = 1.0f + factor * (m_maxScale - 1.0f);

        // Smooth animation to target scale
        float currentScale = btn->hoverScale;
        float newScale = currentScale + (targetScale - currentScale) * 0.3f;

        if (!qFuzzyCompare(currentScale, newScale)) {
            btn->hoverScale = newScale;

            int iconSize = qRound(m_iconBaseSize * newScale);
            btn->button->setIconSize(QSize(iconSize, iconSize));
            int btnSize = iconSize + 16;
            btn->button->setFixedSize(btnSize, btnSize);
        }
    }

    layoutButtons();
}

void ApplicationDock::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw frosted glass background
    QPainterPath path;
    path.addRoundedRect(rect(), m_borderRadius, m_borderRadius);

    // Gradient backdrop
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0, QColor(40, 40, 40, 200));
    grad.setColorAt(1, QColor(20, 20, 20, 220));
    painter.fillPath(path, grad);

    // Subtle border
    QPen borderPen(QColor(255, 255, 255, 30));
    borderPen.setWidthF(0.5);
    painter.setPen(borderPen);
    painter.drawPath(path);

    // Draw active indicator dots below active app icons
    for (const auto* btn : std::as_const(m_entries)) {
        if (btn->entry.active) {
            QRect btnRect = btn->button->geometry();
            int dotX = btnRect.center().x() - 2;
            int dotY = btnRect.bottom() + 2;

            QColor dotColor = btn->entry.accentColor.isValid()
                                ? btn->entry.accentColor
                                : QColor(0, 122, 255);

            painter.setBrush(dotColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(dotX, dotY, 4, 4);

            // Glow effect around dot
            QRadialGradient glow(QPointF(dotX + 2, dotY + 2), 8);
            glow.setColorAt(0, QColor(dotColor.red(), dotColor.green(), dotColor.blue(), 60));
            glow.setColorAt(1, Qt::transparent);
            painter.setBrush(glow);
            painter.drawEllipse(dotX - 6, dotY - 6, 16, 16);
        }
    }
}

void ApplicationDock::mouseMoveEvent(QMouseEvent* event)
{
    updateMagnification(event->pos());
    QWidget::mouseMoveEvent(event);
}

void ApplicationDock::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)

    // Reset all icons to base size with smooth animation
    for (auto* btn : m_entries) {
        btn->hoverScale = 1.0f;
        btn->button->setIconSize(QSize(m_iconBaseSize, m_iconBaseSize));
        btn->button->setFixedSize(m_iconBaseSize + 16, m_iconBaseSize + 16);
    }

    layoutButtons();
    update();
}
