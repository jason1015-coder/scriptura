#ifndef PLUGINUAPI_H
#define PLUGINUAPI_H

#include <QObject>
#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QToolButton>
#include <QIcon>
#include <QKeySequence>
#include <functional>

class MainWindow;

/**
 * @file uiapi.h
 * @brief Plugin UI API — menus, toolbars, panels, status bar, sidebar buttons
 *
 * Exposes fine-grained control of the Scriptura user interface to plugins.
 * All methods are permission-gated via the owning plugin's declared permissions.
 */

class PluginUIApi : public QObject
{
    Q_OBJECT

public:
    /** Location where a custom panel can be registered. */
    enum class PanelLocation {
        BottomPanel,   ///< Register in the bottom panel stack
        SidePanel,     ///< Register in the side panel stack (editor area)
    };

    explicit PluginUIApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginUIApi() override;

    // ── Menu API ───────────────────────────────────────────────

    /**
     * @brief Add an action to an existing menu.
     * @param menuPath  Path to the menu, e.g. "File" or "Code/Format"
     * @param label     Visible label for the action
     * @param callback  Callback invoked when the action is triggered
     * @param shortcut  Optional keyboard shortcut (e.g. QKeySequence("Ctrl+Alt+H"))
     * @return The newly created QAction (owned by the menu)
     */
    QAction* addMenuAction(const QString &menuPath, const QString &label,
                           std::function<void()> callback,
                           const QKeySequence &shortcut = {});

    /**
     * @brief Add a submenu under an existing menu.
     * @param parentPath  Parent menu path, e.g. "Tools"
     * @param title       Title of the new submenu
     * @return The newly created QMenu
     */
    QMenu* addMenu(const QString &parentPath, const QString &title);

    /**
     * @brief Add a separator to an existing menu.
     * @param menuPath  Path to the menu, e.g. "File"
     */
    void addMenuSeparator(const QString &menuPath);

    // ── Toolbar API ────────────────────────────────────────────

    /**
     * @brief Add an action button to a toolbar.
     * @param toolbarName  Name of the target toolbar (created if missing)
     * @param label        Button label / tooltip text
     * @param callback     Callback invoked when clicked
     * @param icon         Optional icon
     * @return The newly created QAction
     */
    QAction* addToolbarAction(const QString &toolbarName, const QString &label,
                              std::function<void()> callback,
                              const QIcon &icon = {});

    // ── Status Bar API ─────────────────────────────────────────

    /**
     * @brief Add a widget to the status bar.
     * @param text       Initial text
     * @param permanent  If true, widget is always visible; otherwise it's temporary
     * @return The QLabel widget (plugin should keep a pointer to update text)
     */
    QLabel* addStatusBarWidget(const QString &text, bool permanent = false);

    // ── Panel API ──────────────────────────────────────────────

    /**
     * @brief Register a custom widget as a panel in the UI.
     * @param id        Unique panel identifier (e.g. "com.example.my-panel")
     * @param title     Title shown on the panel tab
     * @param widget    The panel widget (ownership transferred to the panel stack)
     * @param location  Where to place the panel
     */
    void registerPanel(const QString &id, const QString &title,
                       QWidget *widget, PanelLocation location);

    /**
     * @brief Show a previously registered panel.
     * @param id  Panel identifier passed to registerPanel()
     */
    void showPanel(const QString &id);

    /**
     * @brief Hide a previously registered panel.
     * @param id  Panel identifier passed to registerPanel()
     */
    void hidePanel(const QString &id);

    /**
     * @brief Unregister and remove a previously registered panel.
     * @param id  Panel identifier
     */
    void unregisterPanel(const QString &id);

    // ── Sidebar Button API ─────────────────────────────────────

    /**
     * @brief Add a button to the sidebar icon bar.
     * @param id       Unique button identifier
     * @param icon     Button icon
     * @param tooltip  Tooltip text
     * @param checkable  Whether the button acts as a toggle
     * @return The QToolButton (plugin can connect to its signals)
     */
    QToolButton* addSidebarButton(const QString &id, const QIcon &icon,
                                  const QString &tooltip, bool checkable = true);

    /**
     * @brief Remove a previously added sidebar button.
     * @param id  Button identifier passed to addSidebarButton()
     */
    void removeSidebarButton(const QString &id);

private:
    MainWindow *m_mainWindow;
};

#endif // PLUGINUAPI_H
