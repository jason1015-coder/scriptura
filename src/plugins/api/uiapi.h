#ifndef SRC_PLUGINUAPI_H
#define SRC_PLUGINUAPI_H

#include <QObject>
#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QToolButton>
#include <QIcon>
#include <QKeySequence>
#include <QHash>
#include <functional>

class MainWindow;

class PluginUIApi : public QObject
{
    Q_OBJECT

public:
    enum class PanelLocation {
        BottomPanel,
        SidePanel,
    };

    explicit PluginUIApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginUIApi() override;

    // Menu API
    QAction* addMenuAction(const QString &menuPath, const QString &label,
                           std::function<void()> callback,
                           const QKeySequence &shortcut = {});
    QMenu* addMenu(const QString &parentPath, const QString &title);
    void addMenuSeparator(const QString &menuPath);

    // Toolbar API
    QAction* addToolbarAction(const QString &toolbarName, const QString &label,
                              std::function<void()> callback,
                              const QIcon &icon = {});

    // Status Bar API
    QLabel* addStatusBarWidget(const QString &text, bool permanent = false);

    // Panel API
    void registerPanel(const QString &id, const QString &title,
                       QWidget *widget, PanelLocation location);
    void showPanel(const QString &id);
    void hidePanel(const QString &id);

    // Panel API
    void unregisterPanel(const QString &id);

    // Sidebar Button API
    QToolButton* addSidebarButton(const QString &id, const QIcon &icon,
                                  const QString &tooltip, bool checkable = true);
    void removeSidebarButton(const QString &id);

private:
    QMenu* resolveMenu(const QString &path);
    QAction *m_lastMenuAction = nullptr;

    MainWindow *m_mainWindow;

    // Track registered panels for show/hide by id
    struct PanelEntry {
        QWidget *widget;
        PanelLocation location;
        int tabIndex = -1;
    };
    QHash<QString, PanelEntry> m_panels;

    // Track registered sidebar buttons
    QHash<QString, QToolButton*> m_sidebarButtons;
};

#endif // SRC_PLUGINUAPI_H
