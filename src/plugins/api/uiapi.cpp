#include "uiapi.h"
#include "mainwindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QDebug>

PluginUIApi::PluginUIApi(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

PluginUIApi::~PluginUIApi() = default;

// ── Menu Resolution ─────────────────────────────────────────────

QMenu* PluginUIApi::resolveMenu(const QString &path)
{
    if (!m_mainWindow)
        return nullptr;

    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return nullptr;

    QMenuBar *menuBar = m_mainWindow->menuBar();
    if (!menuBar)
        return nullptr;

    // Find top-level menu
    QMenu *current = nullptr;
    for (QAction *action : menuBar->actions()) {
        if (QMenu *menu = action->menu()) {
            if (menu->title().remove('&').trimmed() == parts[0]) {
                current = menu;
                break;
            }
        }
    }
    if (!current)
        return nullptr;

    // Navigate submenus
    for (int i = 1; i < parts.size(); ++i) {
        QMenu *found = nullptr;
        for (QAction *action : current->actions()) {
            if (action->menu()) {
                QString title = action->menu()->title().remove('&').trimmed();
                if (title == parts[i]) {
                    found = action->menu();
                    break;
                }
            }
        }
        if (!found)
            return nullptr;
        current = found;
    }

    return current;
}

// ── Menu API ────────────────────────────────────────────────────

QAction* PluginUIApi::addMenuAction(const QString &menuPath, const QString &label,
                                     std::function<void()> callback,
                                     const QKeySequence &shortcut)
{
    QMenu *menu = resolveMenu(menuPath);
    if (!menu) {
        qWarning() << "PluginUIApi: menu not found:" << menuPath;
        return nullptr;
    }

    QAction *action = menu->addAction(label);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    if (callback) {
        connect(action, &QAction::triggered, this, std::move(callback));
    }
    m_lastMenuAction = action;
    return action;
}

QMenu* PluginUIApi::addMenu(const QString &parentPath, const QString &title)
{
    QMenu *parent = resolveMenu(parentPath);
    if (!parent) {
        qWarning() << "PluginUIApi: parent menu not found:" << parentPath;
        return nullptr;
    }
    return parent->addMenu(title);
}

void PluginUIApi::addMenuSeparator(const QString &menuPath)
{
    QMenu *menu = resolveMenu(menuPath);
    if (menu)
        menu->addSeparator();
}

// ── Toolbar API ─────────────────────────────────────────────────

QAction* PluginUIApi::addToolbarAction(const QString &toolbarName,
                                        const QString &label,
                                        std::function<void()> callback,
                                        const QIcon &icon)
{
    if (!m_mainWindow)
        return nullptr;

    // Find or create the toolbar
    QToolBar *toolbar = m_mainWindow->findChild<QToolBar*>(toolbarName);
    if (!toolbar) {
        toolbar = m_mainWindow->addToolBar(toolbarName);
        toolbar->setObjectName(toolbarName);
        toolbar->setMovable(false);
    }

    QAction *action = toolbar->addAction(icon, label);
    if (callback)
        connect(action, &QAction::triggered, this, std::move(callback));
    return action;
}

// ── Status Bar API ──────────────────────────────────────────────

QLabel* PluginUIApi::addStatusBarWidget(const QString &text, bool permanent)
{
    if (!m_mainWindow || !m_mainWindow->statusBar())
        return nullptr;

    QLabel *label = new QLabel(text, m_mainWindow->statusBar());
    label->setContentsMargins(4, 0, 4, 0);
    if (permanent)
        m_mainWindow->statusBar()->addPermanentWidget(label);
    else
        m_mainWindow->statusBar()->addWidget(label);
    return label;
}

// ── Panel API ───────────────────────────────────────────────────

void PluginUIApi::registerPanel(const QString &id, const QString &title,
                                 QWidget *widget, PanelLocation location)
{
    if (!m_mainWindow || !widget)
        return;

    // Prevent double-registration
    if (m_panels.contains(id)) {
        qWarning() << "PluginUIApi: panel already registered:" << id;
        return;
    }

    PanelEntry entry;
    entry.widget = widget;
    entry.location = location;

    if (location == PanelLocation::BottomPanel) {
        // Add to the bottom panel button bar & stack
        QStackedWidget *stack = m_mainWindow->findChild<QStackedWidget*>(QStringLiteral("bottomPanelStack"));

        if (stack) {
            entry.tabIndex = m_mainWindow->addBottomPanelButton(":/icons/settings.svg", title, title);
            widget->setParent(stack);
            stack->addWidget(widget);
            widget->hide();
        }
    } else {
        // SidePanel — add to editor stack
        QStackedWidget *editorStack = m_mainWindow->findChild<QStackedWidget*>(QStringLiteral("editorStack"));
        if (editorStack) {
            editorStack->addWidget(widget);
            widget->hide();
        }
    }

    m_panels[id] = entry;
}

void PluginUIApi::showPanel(const QString &id)
{
    if (!m_panels.contains(id))
        return;

    PanelEntry &entry = m_panels[id];
    if (entry.widget)
        entry.widget->show();

    // Switch to the right panel if in bottom panel
    if (entry.location == PanelLocation::BottomPanel && entry.tabIndex >= 0) {
        m_mainWindow->showBottomPanelIndex(entry.tabIndex);
    }
}

void PluginUIApi::hidePanel(const QString &id)
{
    if (!m_panels.contains(id))
        return;

    PanelEntry &entry = m_panels[id];
    if (entry.widget)
        entry.widget->hide();
}

void PluginUIApi::unregisterPanel(const QString &id)
{
    if (!m_panels.contains(id))
        return;

    PanelEntry entry = m_panels.take(id);
    if (entry.widget) {
        entry.widget->hide();
        entry.widget->deleteLater();
    }
}

// ── Sidebar Button API ──────────────────────────────────────────

QToolButton* PluginUIApi::addSidebarButton(const QString &id, const QIcon &icon,
                                            const QString &tooltip, bool checkable)
{
    if (!m_mainWindow || m_sidebarButtons.contains(id))
        return nullptr;

    // Find the sidebar icon bar and add the button to its layout
    QWidget *iconBar = m_mainWindow->findChild<QWidget*>("sidebarIconBar");
    if (!iconBar)
        return nullptr;

    QToolButton *button = new QToolButton(iconBar);
    button->setIcon(icon);
    button->setIconSize(QSize(20, 20));
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    button->setFixedSize(32, 32);
    button->setObjectName("pluginSidebarBtn_" + id);

    // Insert before stretch or append
    QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(iconBar->layout());
    if (lay) {
        lay->insertWidget(lay->count() - 1, button); // before the stretch
    }

    m_sidebarButtons[id] = button;
    return button;
}

void PluginUIApi::removeSidebarButton(const QString &id)
{
    if (!m_sidebarButtons.contains(id))
        return;
    QToolButton *btn = m_sidebarButtons.take(id);
    if (btn) {
        btn->hide();
        btn->deleteLater();
    }
}
