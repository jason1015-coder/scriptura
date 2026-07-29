#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QJsonObject>

class ScripturaApplication;
class ApplicationContext;
class PluginManagerDialog;

/**
 * @file applicationmanager.h
 * @brief Manages lifecycle of Scriptura "Applications"
 *
 * Applications are self-contained UI panels that get:
 * - An icon in the floating dock
 * - A bottom-panel tab for their content
 * - Simple lifecycle (initialize / shutdown)
 *
 * This manager handles:
 * - Discovery from plugin directories (plugin.json with "type": "application")
 * - Loading and initialization
 * - Registration in the bottom panel
 * - Dock icon creation
 */
class ApplicationManager : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationManager(QObject* parent = nullptr);
    ~ApplicationManager() override;

    // ── Discovery & Loading ──────────────────────────────────────

    /**
     * @brief Scan a directory for applications
     * @param path Directory to scan (contains subdirs with plugin.json)
     * @return Number of applications found
     */
    int scanDirectory(const QString& path);

    /**
     * @brief Load a single application from a directory
     * @param appDir Path to application directory
     * @return true on success
     */
    bool loadApplication(const QString& appDir);

    /**
     * @brief Unload an application by ID
     */
    void unloadApplication(const QString& id);

    /**
     * @brief Load all discovered applications
     * Must be called after scanDirectory().
     */
    void loadAll();

    // ── Query ────────────────────────────────────────────────────

    struct AppInfo {
        QString id;
        QString name;
        QString version;
        QString author;
        QString description;
        QString iconPath;
        QString tooltip;
        QString appDir;
        bool loaded = false;
        bool enabled = true;
    };

    bool isLoaded(const QString& id) const;
    AppInfo appInfo(const QString& id) const;
    QList<AppInfo> allApplications() const;
    QStringList loadedIds() const;
    int count() const;

    // ── UI Integration ───────────────────────────────────────────

    /**
     * @brief Get the widget for a loaded application
     * Creates it on first access via createWidget()
     */
    QWidget* widget(const QString& id);

    /**
     * @brief Get the tab title for an application
     */
    QString tabTitle(const QString& id) const;

    // ── Default Applications ─────────────────────────────────────

    /**
     * @brief Register built-in applications that ship with Scriptura
     *
     * These are registered from the mainwindow's existing panels:
     * - Git, HTTP Client, Database, Regex, Format, Preview, Replace, Diff, Stash, Test
     */
    void registerBuiltins();

signals:
    void applicationLoaded(const QString& id, const QString& name, const QString& iconPath);
    void applicationUnloaded(const QString& id);
    void applicationError(const QString& id, const QString& error);

private:
    struct LoadedApp {
        AppInfo info;
        ScripturaApplication* instance = nullptr;
        ApplicationContext* context = nullptr;
        QWidget* widget = nullptr;
        bool widgetCreated = false;
    };

    bool loadAppMetadata(const QString& jsonPath, QJsonObject& metadata);
    LoadedApp* findLoaded(const QString& id);

    QHash<QString, LoadedApp> m_apps;
    QStringList m_scanPaths;
};

#endif // APPLICATIONMANAGER_H
