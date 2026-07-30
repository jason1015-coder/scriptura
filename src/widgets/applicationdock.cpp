#include "applicationdock.h"
#include "themeicons.h"
#include "thememanager.h"

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

    // Border radius is set via stylesheet; background is custom-painted in paintEvent
    // so it adapts to the current theme colors.
    setStyleSheet(R"(
        QWidget#applicationDock {
            border-radius: 16px;
        }
    )");

    // ── Theme fade animation ───────────────────────────────────────
    m_themeAnim = new QVariantAnimation(this);
    m_themeAnim->setDuration(300);
    m_themeAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_themeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        double t = value.toDouble();
        m_glassBg     = lerpColor(m_animSrcBg,     m_animTgtBg,     t);
        m_glassBgEnd  = lerpColor(m_animSrcBgEnd,  m_animTgtBgEnd,  t);
        m_borderColor = lerpColor(m_animSrcBorder, m_animTgtBorder, t);
        m_accentColor = lerpColor(m_animSrcAccent, m_animTgtAccent, t);
        update();
    });
    connect(m_themeAnim, &QVariantAnimation::finished, this, [this]() {
        // Snap to exact target colors on finish
        m_glassBg     = m_animTgtBg;
        m_glassBgEnd  = m_animTgtBgEnd;
        m_borderColor = m_animTgtBorder;
        m_accentColor = m_animTgtAccent;
        update();
    });
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
    // Use ThemeIcons for theme-aware SVG tinting — icons auto-recolor on theme change
    ThemeIcons::instance()->setIcon(entry->button, iconPath, ThemeIcons::Role::Svg);
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

    // Draw frosted glass background using theme-aware colors
    QPainterPath path;
    path.addRoundedRect(rect(), m_borderRadius, m_borderRadius);

    // Gradient backdrop
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0, m_glassBg);
    grad.setColorAt(1, m_glassBgEnd);
    painter.fillPath(path, grad);

    // Subtle border
    QPen borderPen(m_borderColor);
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
                                : m_accentColor;

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

// ── Theme Support ───────────────────────────────────────────────────

ApplicationDock::ThemeColors ApplicationDock::currentThemeColors() const
{
    return { m_glassBg, m_glassBgEnd, m_borderColor, m_accentColor, m_isDark };
}

void ApplicationDock::setThemeManager(ThemeManager* mgr)
{
    if (m_themeManager) {
        disconnect(m_themeManager, &ThemeManager::themeChanged,
                   this, &ApplicationDock::onThemeChanged);
    }

    m_themeManager = mgr;

    if (mgr) {
        connect(mgr, &ThemeManager::themeChanged,
                this, &ApplicationDock::onThemeChanged);
        // Apply the current theme immediately
        onThemeChanged();
    }
}

void ApplicationDock::onThemeChanged()
{
    if (!m_themeManager) return;

    auto theme = m_themeManager->currentTheme();
    bool isDark = theme.isDark();
    QColor accent = m_themeManager->accentColor();

    // Compute glass colors matching the pattern used in MainWindow::applyTheme
    QColor glassBg = isDark ? QColor(56, 58, 61, 230) : QColor(246, 246, 246, 230);
    QColor glassBorder = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 25);

    updateTheme(glassBg, glassBorder, accent, isDark);
}

void ApplicationDock::updateTheme(const QColor& bg, const QColor& border,
                                   const QColor& accent, bool isDark)
{
    m_isDark = isDark;

    // Compute the glassBgEnd from the base bg color
    QColor bgEnd = isDark ? bg.darker(130) : bg.darker(110);

    // Store current rendered colors as animation source
    m_animSrcBg     = m_glassBg;
    m_animSrcBgEnd  = m_glassBgEnd;
    m_animSrcBorder = m_borderColor;
    m_animSrcAccent = m_accentColor;

    // Store new colors as animation target
    m_animTgtBg     = bg;
    m_animTgtBgEnd  = bgEnd;
    m_animTgtBorder = border;
    m_animTgtAccent = accent;

    // Start the fade animation
    m_themeAnim->stop();
    m_themeAnim->setStartValue(0.0);
    m_themeAnim->setEndValue(1.0);
    m_themeAnim->start();

    // Update button hover/press colors to match the theme (instant, no animation needed)
    QString hoverBg  = isDark ? "rgba(255, 255, 255, 30)" : "rgba(0, 0, 0, 12)";
    QString pressBg  = isDark ? "rgba(255, 255, 255, 50)" : "rgba(0, 0, 0, 20)";

    for (auto* btn : m_entries) {
        btn->button->setStyleSheet(QString(R"(
            QToolButton {
                border: none;
                border-radius: 12px;
                background: transparent;
                padding: 4px;
            }
            QToolButton:hover {
                background: %1;
            }
            QToolButton:pressed {
                background: %2;
            }
        )").arg(hoverBg, pressBg));
    }
}

// ── Color interpolation helper ───────────────────────────────────────

QColor ApplicationDock::lerpColor(const QColor& from, const QColor& to, double t)
{
    if (t <= 0.0) return from;
    if (t >= 1.0) return to;
    return QColor(
        qRound(from.red()   + (to.red()   - from.red())   * t),
        qRound(from.green() + (to.green() - from.green()) * t),
        qRound(from.blue()  + (to.blue()  - from.blue())  * t),
        qRound(from.alpha() + (to.alpha() - from.alpha()) * t)
    );
}
