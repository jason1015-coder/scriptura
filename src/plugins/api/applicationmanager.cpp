#include "applicationmanager.h"
#include "applicationinterface.h"
#include "applicationcontext.h"
#include "mainwindow.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QStandardPaths>
#include <QApplication>

ApplicationManager::ApplicationManager(QObject* parent)
    : QObject(parent)
{
}

ApplicationManager::~ApplicationManager()
{
    for (auto it = m_apps.begin(); it != m_apps.end(); ++it) {
        if (it->instance) {
            it->instance->shutdown();
            it->instance->deleteLater();
        }
        if (it->context) it->context->deleteLater();
    }
    m_apps.clear();
}

// ── Discovery & Loading ──────────────────────────────────────────

int ApplicationManager::scanDirectory(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) return 0;

    m_scanPaths.append(path);
    int found = 0;

    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : dir.entryInfoList()) {
        if (!entry.isDir()) continue;

        QString jsonPath = entry.absoluteFilePath() + "/plugin.json";
        if (!QFile::exists(jsonPath)) continue;

        QJsonObject meta;
        if (!loadAppMetadata(jsonPath, meta)) continue;

        QString type = meta.value("type").toString();
        if (type != "application") continue;

        QString id = meta.value("id").toString();
        if (id.isEmpty()) continue;

        if (!m_apps.contains(id)) {
            AppInfo info;
            info.id = id;
            info.name = meta.value("name").toString().isEmpty() ? id : meta.value("name").toString();
            info.version = meta.value("version").toString();
            info.author = meta.value("author").toString();
            info.description = meta.value("description").toString();
            info.iconPath = meta.value("icon").toString();
            info.tooltip = meta.value("tooltip").toString();
            if (info.tooltip.isEmpty()) info.tooltip = info.name;
            info.appDir = entry.absoluteFilePath();
            info.loaded = false;
            info.enabled = true;

            LoadedApp loaded;
            loaded.info = info;
            m_apps[id] = loaded;
            found++;
        }
    }

    qDebug() << "ApplicationManager: found" << found << "applications in" << path;
    return found;
}

bool ApplicationManager::loadApplication(const QString& appDir)
{
    QString jsonPath = appDir + "/plugin.json";
    QJsonObject meta;
    if (!loadAppMetadata(jsonPath, meta)) return false;

    QString id = meta.value("id").toString();
    if (id.isEmpty()) return false;

    // Check it's an application
    if (meta.value("type").toString() != "application") return false;

    // Find the library
    QString libName = meta.value("library").toString();
    if (libName.isEmpty()) {
        // Try to find .so/.dll/.dylib in the directory
        QDir dir(appDir);
        QStringList candidates = dir.entryList(QStringList() << "*.so" << "*.dll" << "*.dylib", QDir::Files);
        if (candidates.isEmpty()) {
            emit applicationError(id, "No library found in " + appDir);
            return false;
        }
        libName = candidates.first();
    }

    // Load the shared library
    QString libPath = appDir + "/" + libName;
    QPluginLoader* loader = new QPluginLoader(libPath, this);

    if (!loader->load()) {
        emit applicationError(id, loader->errorString());
        loader->deleteLater();
        return false;
    }

    QObject* obj = loader->instance();
    if (!obj) {
        emit applicationError(id, "Failed to create instance");
        loader->unload();
        loader->deleteLater();
        return false;
    }

    ScripturaApplication* app = qobject_cast<ScripturaApplication*>(obj);
    if (!app) {
        emit applicationError(id, "Library does not implement ScripturaApplication");
        loader->unload();
        loader->deleteLater();
        return false;
    }

    // Update info
    if (m_apps.contains(id)) {
        m_apps[id].info.loaded = true;
        m_apps[id].info.iconPath = app->iconPath();
        m_apps[id].info.tooltip = app->tooltip();
    } else {
        AppInfo info;
        info.id = id;
        info.name = app->name();
        info.version = app->version();
        info.author = app->author();
        info.description = app->description();
        info.iconPath = app->iconPath();
        info.tooltip = app->tooltip();
        info.appDir = appDir;
        info.loaded = true;
        info.enabled = true;
        LoadedApp loaded;
        loaded.info = info;
        m_apps[id] = loaded;
    }

    m_apps[id].instance = app;

    // Create context
    MainWindow* mainWindow = qobject_cast<MainWindow*>(qApp->activeWindow());
    m_apps[id].context = new ApplicationContext(mainWindow, this);

    // Initialize
    if (!app->initialize(m_apps[id].context)) {
        emit applicationError(id, "initialize() returned false");
        app->shutdown();
        app->deleteLater();
        m_apps[id].instance = nullptr;
        m_apps[id].info.loaded = false;
        return false;
    }

    emit applicationLoaded(id, app->name(), app->iconPath());
    return true;
}

void ApplicationManager::unloadApplication(const QString& id)
{
    auto it = m_apps.find(id);
    if (it == m_apps.end()) return;

    if (it->widget) {
        it->widget->hide();
        it->widget->deleteLater();
        it->widget = nullptr;
        it->widgetCreated = false;
    }

    if (it->instance) {
        it->instance->shutdown();
        it->instance->deleteLater();
        it->instance = nullptr;
    }

    if (it->context) {
        it->context->deleteLater();
        it->context = nullptr;
    }

    it->info.loaded = false;
    emit applicationUnloaded(id);
}

void ApplicationManager::loadAll()
{
    for (auto it = m_apps.begin(); it != m_apps.end(); ++it) {
        if (!it->info.loaded && it->info.enabled) {
            loadApplication(it->info.appDir);
        }
    }
}

// ── Query ────────────────────────────────────────────────────────

bool ApplicationManager::isLoaded(const QString& id) const
{
    auto it = m_apps.constFind(id);
    return it != m_apps.constEnd() && it->info.loaded;
}

ApplicationManager::AppInfo ApplicationManager::appInfo(const QString& id) const
{
    auto it = m_apps.constFind(id);
    if (it != m_apps.constEnd()) return it->info;
    return {};
}

QList<ApplicationManager::AppInfo> ApplicationManager::allApplications() const
{
    QList<AppInfo> result;
    for (const auto& app : std::as_const(m_apps)) {
        result.append(app.info);
    }
    return result;
}

QStringList ApplicationManager::loadedIds() const
{
    QStringList ids;
    for (auto it = m_apps.constBegin(); it != m_apps.constEnd(); ++it) {
        if (it->info.loaded) ids.append(it->info.id);
    }
    return ids;
}

int ApplicationManager::count() const
{
    return m_apps.size();
}

// ── UI Integration ───────────────────────────────────────────────

QWidget* ApplicationManager::widget(const QString& id)
{
    auto it = m_apps.find(id);
    if (it == m_apps.end() || !it->instance) return nullptr;

    if (!it->widgetCreated && it->instance) {
        it->widget = it->instance->createWidget(nullptr);
        it->widgetCreated = true;
    }

    return it->widget;
}

QString ApplicationManager::tabTitle(const QString& id) const
{
    auto it = m_apps.constFind(id);
    if (it != m_apps.constEnd()) return it->info.name;
    return id;
}

// ── Internal ─────────────────────────────────────────────────────

bool ApplicationManager::loadAppMetadata(const QString& jsonPath, QJsonObject& metadata)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray content = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

    metadata = doc.object();
    return true;
}

ApplicationManager::LoadedApp* ApplicationManager::findLoaded(const QString& id)
{
    auto it = m_apps.find(id);
    return (it != m_apps.end()) ? &(*it) : nullptr;
}

// ── Default Applications ─────────────────────────────────────────

void ApplicationManager::registerBuiltins()
{
    // Built-in applications are registered with metadata only
    // They are loaded by MainWindow when it creates their widgets

    auto registerBuiltin = [this](const QString& id, const QString& name,
                                   const QString& icon, const QString& tooltip,
                                   const QString& desc) {
        if (m_apps.contains(id)) return;
        AppInfo info;
        info.id = id;
        info.name = name;
        info.iconPath = icon;
        info.tooltip = tooltip;
        info.description = desc;
        info.loaded = false;
        info.enabled = true;
        LoadedApp loaded;
        loaded.info = info;
        m_apps[id] = loaded;
    };

    registerBuiltin("com.scriptura.git",       "Git",        ":/icons/git.svg",
                     "Git",         "Git version control integration");
    registerBuiltin("com.scriptura.httpclient", "HTTP Client", ":/icons/http.svg",
                     "HTTP Client", "REST API client for testing endpoints");
    registerBuiltin("com.scriptura.database",   "Database",    ":/icons/database.svg",
                     "Database",    "SQLite database viewer");
    registerBuiltin("com.scriptura.regex",      "Regex",       ":/icons/regex.svg",
                     "Regex Tester","Regular expression testing tool");
    registerBuiltin("com.scriptura.formatter",  "Format",      ":/icons/format.svg",
                     "Data Formatter","JSON, YAML, XML formatter");
    registerBuiltin("com.scriptura.preview",    "Preview",     ":/icons/preview.svg",
                     "Markdown Preview","Live markdown preview");
    registerBuiltin("com.scriptura.replacer",   "Replace",     ":/icons/replace.svg",
                     "Global Replace","Find and replace with preview");
    registerBuiltin("com.scriptura.diff",       "Diff",        ":/icons/diff.svg",
                     "Diff Viewer","Side-by-side file comparison");
    registerBuiltin("com.scriptura.stash",      "Stash",       ":/icons/stash.svg",
                     "Git Stash","Git stash management");
    registerBuiltin("com.scriptura.test",       "Test",        ":/icons/test.svg",
                     "Test Runner","Run and view test results");
}
