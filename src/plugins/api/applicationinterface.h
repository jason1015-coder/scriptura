#ifndef APPLICATIONINTERFACE_H
#define APPLICATIONINTERFACE_H

#include <QtPlugin>
#include <QObject>
#include <QString>
#include <QWidget>

class ApplicationContext;

/**
 * @file applicationinterface.h
 * @brief Defines the interface for Scriptura "Applications"
 *
 * An Application is a self-contained UI panel that:
 * - Has an enforced SVG icon for the floating dock
 * - Creates a content widget shown in a bottom-panel tab
 * - Is simpler than a full Plugin (no menu/toolbar/editor APIs needed)
 *
 * The core automatically:
 * - Detects applications from plugin.json with "type": "application"
 * - Creates a sidebar icon in the floating dock
 * - Manages the tab lifecycle
 *
 * Examples: Git, HTTP Client, Database Viewer, Regex Tester
 */
class ScripturaApplication : public QObject
{
    Q_OBJECT
public:
    explicit ScripturaApplication(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    virtual ~ScripturaApplication() = default;

    // ── Lifecycle ────────────────────────────────────────────────

    /**
     * @brief Initialize the application
     * @param context Application context providing access to core services
     * @return true on success
     */
    virtual bool initialize(ApplicationContext* context) = 0;

    /**
     * @brief Shutdown and clean up
     */
    virtual void shutdown() = 0;

    // ── Metadata ─────────────────────────────────────────────────

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString author() const = 0;
    virtual QString description() const = 0;

    // ── Visual Identity ──────────────────────────────────────────

    /**
     * @brief SVG icon path (enforced — must be an .svg file)
     *
     * The icon is displayed in the floating dock at the bottom of the screen.
     * It should be a clean, recognizable icon at 20x20 base size.
     * The dock handles enlargement on hover automatically.
     *
     * @return Path to SVG icon (e.g., ":/icons/git.svg" or "path/to/icon.svg")
     */
    virtual QString iconPath() const = 0;

    /**
     * @brief Tooltip text shown when hovering over the dock icon
     * @return Tooltip string
     */
    virtual QString tooltip() const { return name(); }

    /**
     * @brief Accent color for the dock icon glow effect (optional)
     *
     * If invalid, the theme's accent color is used.
     *
     * @return QColor for the icon's hover glow, or invalid for default
     */
    virtual QColor accentColor() const { return QColor(); }

    // ── Content ──────────────────────────────────────────────────

    /**
     * @brief Create the main content widget for this application
     *
     * Called once when the application is first opened.
     * The widget is placed in a bottom-panel tab managed by the core.
     *
     * @param parent Parent widget (the panel container)
     * @return A new QWidget owned by the application
     */
    virtual QWidget* createWidget(QWidget* parent) = 0;

    /**
     * @brief Whether this application supports multiple instances (tabs)
     *
     * Default: false (single instance, toggled on/off).
     */
    virtual bool multiInstance() const { return false; }
};

Q_DECLARE_INTERFACE(ScripturaApplication, "com.scriptura.application/1.0")

#endif // APPLICATIONINTERFACE_H
