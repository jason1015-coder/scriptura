#ifndef APPLICATIONDOCK_H
#define APPLICATIONDOCK_H

#include <QWidget>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QIcon>
#include <QColor>
#include <QPointF>
#include <QEasingCurve>

/**
 * @file applicationdock.h
 * @brief Floating dock widget for application icons
 *
 * A macOS-dock-style floating bar at the bottom of the screen.
 * Features:
 * - Icons enlarge on hover with smooth animation
 * - Magnification effect (nearby icons also enlarge slightly)
 * - Frosted glass / acrylic backdrop
 * - Click to toggle application tab
 * - Active application has a glowing indicator dot
 *
 * Creates a "Scriptura as an OS" feel.
 */
class ApplicationDock : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float magnification READ magnification WRITE setMagnification)

public:
    explicit ApplicationDock(QWidget* parent = nullptr);
    ~ApplicationDock() override;

    // ── App Management ───────────────────────────────────────────

    struct DockEntry {
        QString appId;
        QString tooltip;
        QString iconPath;
        QColor accentColor;
        bool active = false;
    };

    /**
     * @brief Add an application to the dock
     * @param id Unique application ID
     * @param iconPath SVG icon path
     * @param tooltip Hover tooltip text
     * @param accentColor Glow color (invalid = theme default)
     * @return Index of the added entry
     */
    int addEntry(const QString& id, const QString& iconPath,
                 const QString& tooltip, const QColor& accentColor = QColor());

    /**
     * @brief Remove an application from the dock
     */
    void removeEntry(const QString& id);

    /**
     * @brief Set which application is currently active
     */
    void setActiveApp(const QString& id);

    /**
     * @brief Check if an app is in the dock
     */
    bool hasEntry(const QString& id) const;

    /**
     * @brief Number of entries in the dock
     */
    int entryCount() const { return m_entries.size(); }

    // ── Magnification Property ───────────────────────────────────

    float magnification() const { return m_magnification; }
    void setMagnification(float value);

signals:
    void appClicked(const QString& appId);

private:
    struct DockButton {
        DockEntry entry;
        QToolButton* button = nullptr;
        QPropertyAnimation* hoverAnim = nullptr;
        float hoverScale = 1.0f;
        bool isHovered = false;
    };

    void layoutButtons();
    void updateMagnification(const QPoint& mousePos);
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

    QList<DockButton*> m_entries;
    float m_magnification = 1.0f;
    float m_maxScale = 1.8f;
    float m_nearScale = 1.3f;
    int m_iconBaseSize = 28;
    int m_iconMaxSize = 48;
    int m_iconNearSize = 38;
    int m_spacing = 4;
    int m_padding = 12;
    int m_borderRadius = 16;
    QEasingCurve m_easingCurve;
};

#endif // APPLICATIONDOCK_H
