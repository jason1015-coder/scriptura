#include "pluginmanager.h"
#include "eventbus.h"
#include "servicelocator.h"
#include "versionfetcher.h"
#include "plugincrashhandler.h"
#include "plugincontext.h"
#include <QDebug>
#include <QJsonArray>
#include <QStandardPaths>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QMessageBox>

const QString PluginManager::DISABLED_PLUGINS_FILE = "disabled_plugins.json";

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_crashHandler(new PluginCrashHandler(this))
{
    QMutexLocker locker(&m_mutex);
    loadDisabledPlugins();

    connect(m_crashHandler.data(), &PluginCrashHandler::pluginCrashed,
            this, &PluginManager::onPluginCrashed);
}

PluginManager::~PluginManager()
{
    QMutexLocker locker(&m_mutex);
    saveDisabledPlugins();
    unloadAllPlugins();
}

void PluginManager::setContext(PluginContext* context)
{
    QMutexLocker locker(&m_mutex);
    m_context = context;
}

void PluginManager::setPermissionManager(PermissionManager* manager)
{
    QMutexLocker locker(&m_mutex);
    m_permissionManager = manager;
}

void PluginManager::setCrashHandler(PluginCrashHandler* handler)
{
    QMutexLocker locker(&m_mutex);
    m_crashHandler.reset(handler);
}

bool PluginManager::checkPermission(const QString& pluginId, Permission permission) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_permissionManager)
        return true;
    return m_permissionManager->checkPermission(pluginId, permission);
}

void PluginManager::onPluginCrashed(const QString& pluginId, const CrashInfo& info)
{
    QMutexLocker locker(&m_mutex);
    qWarning() << "Plugin" << pluginId << "crashed:" << info.errorType;
    disablePlugin(pluginId);
    emit pluginError(pluginId, QObject::tr("Plugin crashed: %1").arg(info.errorType));
}

bool PluginManager::loadPlugins(const QString& pluginPath)
{
    QMutexLocker locker(&m_mutex);
    
    QDir dir(pluginPath);
    if (!dir.exists()) {
        qWarning() << "Plugin directory does not exist:" << pluginPath;
        return false;
    }
    
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst);
    
    QList<QJsonObject> pluginMetadata;
    for (const QFileInfo& info : dir.entryInfoList()) {
        if (info.isDir()) {
            QString metadataFile = info.absoluteFilePath() + "/plugin.json";
            if (QFile::exists(metadataFile)) {
                QJsonObject metadata;
                if (loadPluginMetadata(info.absoluteFilePath(), metadata)) {
                    QString pluginId = metadata["id"].toString();
                    
                    if (m_disabledPlugins.contains(pluginId)) {
                        qDebug() << "Plugin" << pluginId << "is disabled, skipping";
                        continue;
                    }
                    
                    pluginMetadata.append(metadata);
                    if (metadata.contains("id")) {
                        QString pid = metadata["id"].toString();
                        m_pluginPaths[pid] = info.absoluteFilePath();
                        m_pluginVersions[pid] = metadata["version"].toString();
                    }
                }
            }
        }
    }
    
    QList<DependencyResolver::DependencyError> errors = m_resolver.validate(pluginMetadata, QSet<QString>());
    for (const auto& error : errors) {
        if (!error.isOptional) {
            qWarning() << "Missing required dependency:" << error.missingDependency
                       << "for plugin" << error.pluginId;
        }
    }
    
    if (!buildDependencyGraph(pluginMetadata)) {
        return false;
    }
    
    QStringList loadOrder = topologicalSort();
    QSet<QString> failedPlugins;
    
    for (const QString& pluginId : loadOrder) {
        if (!loadPluginById(pluginId)) {
            qWarning() << "Failed to load plugin:" << pluginId;
            failedPlugins.insert(pluginId);
        }
    }
    
    for (const QString& pluginId : loadOrder) {
        if (failedPlugins.contains(pluginId)) {
            continue;
        }
        
        if (m_plugins.contains(pluginId)) {
            const PluginInfo& info = m_plugins[pluginId];
            for (const QString& dep : info.dependencies) {
                if (failedPlugins.contains(dep)) {
                    qWarning() << "Skipping plugin" << pluginId << "due to failed dependency:" << dep;
                    unloadPluginImpl(pluginId);
                    failedPlugins.insert(pluginId);
                    break;
                }
            }
        }
    }
    
    QSet<QString> loadedIds;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value().instance) {
            loadedIds.insert(it.key());
        }
    }
    
    QList<DependencyResolver::DependencyError> finalErrors = m_resolver.validate(pluginMetadata, loadedIds);
    for (const auto& error : finalErrors) {
        if (!error.isOptional) {
            qWarning() << "Dependency not loaded:" << error.missingDependency
                       << "for plugin" << error.pluginId;
        }
    }
    
    initializePlugins();
    
    return failedPlugins.isEmpty();
}

bool PluginManager::loadPlugin(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    return loadPluginImpl(filePath);
}

bool PluginManager::loadPluginImpl(const QString& filePath)
{
    QDir pluginDir(filePath);
    if (!pluginDir.exists()) {
        return false;
    }
    
    QJsonObject metadata;
    if (!loadPluginMetadata(filePath, metadata)) {
        return false;
    }
    
    QString pluginId = metadata["id"].toString();
    if (pluginId.isEmpty()) {
        qWarning() << "Plugin metadata missing 'id' field";
        return false;
    }
    
    if (m_plugins.contains(pluginId)) {
        return true;
    }
    
    QString libraryName = metadata["library"].toString();
    if (libraryName.isEmpty()) {
        qWarning() << "Plugin" << pluginId << "has no library specified";
        return false;
    }
    
    QString libraryPath = pluginDir.absoluteFilePath(libraryName);
    if (!QFile::exists(libraryPath)) {
        QString baseName = libraryName;
        int lastDot = baseName.lastIndexOf('.');
        if (lastDot > 0) {
            baseName = baseName.left(lastDot);
        }
        #ifdef Q_OS_WIN
            QString platformLibrary = baseName + ".dll";
        #elif defined(Q_OS_MAC)
            QString platformLibrary = baseName + ".dylib";
        #else
            QString platformLibrary = baseName + ".so";
        #endif
        libraryPath = pluginDir.absoluteFilePath(platformLibrary);
        qDebug() << "Library not found at" << pluginDir.absoluteFilePath(libraryName) 
                 << ", trying platform-specific:" << libraryPath;
    }
    
    QSharedPointer<QPluginLoader> loader(new QPluginLoader(libraryPath));
    
    QObject* pluginObj = loader->instance();
    if (!pluginObj) {
        qWarning() << "Failed to load plugin library:" << libraryPath 
                   << loader->errorString();
        return false;
    }
    
    ScripturaPlugin* plugin = qobject_cast<ScripturaPlugin*>(pluginObj);
    if (!plugin) {
        qWarning() << "Plugin does not implement ScripturaPlugin interface:" << pluginId;
        return false;
    }
    
    if (!checkVersionCompatibility(metadata)) {
        qWarning() << "Plugin" << pluginId << "is not compatible with current Scriptura version";
        emit pluginIncompatible(pluginId, VersionFetcher::coreVersion());
        return false;
    }
    
    PluginInfo info;
    info.filePath = filePath;
    info.metadata = metadata;
    info.loader = loader;
    info.instance = plugin;
    info.initialized = false;
    
    if (metadata.contains("dependencies")) {
        QJsonArray depArray = metadata["dependencies"].toArray();
        for (const QJsonValue& val : depArray) {
            info.dependencies.append(val.toString());
        }
    }
    
    m_plugins[pluginId] = info;
    emit pluginLoaded(pluginId);
    
    return true;
}

void PluginManager::unloadPlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    unloadPluginImpl(id);
    emit pluginUnloaded(id);
}

void PluginManager::unloadPluginImpl(const QString& id)
{
    if (!m_plugins.contains(id)) {
        return;
    }
    
    PluginInfo& info = m_plugins[id];
    
    if (info.instance && info.initialized) {
        try {
            info.instance->shutdown();
        } catch (const std::exception& e) {
            qWarning() << "Exception during plugin shutdown:" << e.what();
        }
    }

    // 在卸載共享庫之前，先移除該插件註冊的事件處理器，
    // 避免後續 publishEvent / EventBus::publish 呼叫到已釋放/已取消映射的回調 (use-after-free / SIGSEGV)
    {
        QMutexLocker locker(&m_mutex);
        const QList<quint64> ids = m_pluginSubscriptions.value(id);
        for (quint64 subId : ids) {
            for (auto it = m_eventHandlers.begin(); it != m_eventHandlers.end();) {
                QList<Subscription>& subs = it.value();
                for (auto sit = subs.begin(); sit != subs.end();) {
                    if (sit->id == subId) {
                        sit = subs.erase(sit);
                    } else {
                        ++sit;
                    }
                }
                if (subs.isEmpty()) {
                    it = m_eventHandlers.erase(it);
                } else {
                    ++it;
                }
            }
        }
        m_pluginSubscriptions.remove(id);
    }

    // 同時移除該插件實例在 EventBus 上的所有訂閱 (不依賴插件是否傳入 owner)
    if (info.instance) {
        EventBus::instance()->unsubscribeReceiver(info.instance);
    }

    if (info.loader) {
        info.loader->unload();
        info.loader.reset();
    }

    m_plugins.remove(id);
}

void PluginManager::unloadAllPlugins()
{
    QMutexLocker locker(&m_mutex);
    QStringList ids = m_plugins.keys();
    for (int i = ids.size() - 1; i >= 0; --i) {
        unloadPluginImpl(ids[i]);
        emit pluginUnloaded(ids[i]);
    }
}

QList<ScripturaPlugin*> PluginManager::plugins() const
{
    QMutexLocker locker(&m_mutex);
    QList<ScripturaPlugin*> result;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value().instance) {
            result.append(it.value().instance);
        }
    }
    return result;
}

ScripturaPlugin* PluginManager::getPlugin(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_plugins.contains(id)) {
        return nullptr;
    }
    return m_plugins[id].instance;
}

bool PluginManager::isLoaded(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    return m_plugins.contains(id) && m_plugins[id].initialized;
}

QList<ScripturaPlugin*> PluginManager::pluginsWithFeature(PluginFeature feature) const
{
    QMutexLocker locker(&m_mutex);
    QList<ScripturaPlugin*> result;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        if (it.value().instance && it.value().instance->hasFeature(feature)) {
            result.append(it.value().instance);
        }
    }
    return result;
}

void PluginManager::publishEvent(const QString& event, const QVariant& data)
{
    QList<std::function<void(const QVariant&)>> callbacks;
    
    {
        QMutexLocker locker(&m_mutex);
        if (m_eventHandlers.contains(event)) {
            for (const auto& subscription : m_eventHandlers[event]) {
                callbacks.append(subscription.callback);
            }
        }
    }
    
    for (const auto& callback : callbacks) {
        try {
            callback(data);
        } catch (const std::exception& e) {
            qWarning() << "Exception in event handler for" << event << ":" << e.what();
        }
    }
    
    EventBus::instance()->publish(event, data);
}

quint64 PluginManager::subscribeToEvent(const QString& event, 
                                         std::function<void(const QVariant&)> callback)
{
    return subscribeToEvent(event, std::move(callback), QString());
}

quint64 PluginManager::subscribeToEvent(const QString& event,
                                         std::function<void(const QVariant&)> callback,
                                         const QString& ownerPluginId)
{
    QMutexLocker locker(&m_mutex);
    quint64 id = m_nextSubscriptionId++;
    m_eventHandlers[event].append({id, std::move(callback)});
    if (!ownerPluginId.isEmpty()) {
        m_pluginSubscriptions[ownerPluginId].append(id);
    }
    return id;
}

bool PluginManager::loadPluginMetadata(const QString& filePath, QJsonObject& metadata)
{
    QString metadataFile = filePath + "/plugin.json";
    
    QFile file(metadataFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open plugin metadata:" << metadataFile;
        return false;
    }
    
    QByteArray content = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse plugin metadata:" << error.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "Plugin metadata is not a JSON object";
        return false;
    }
    
    metadata = doc.object();
    return true;
}

bool PluginManager::checkDependencies(const QJsonObject& metadata)
{
    QMutexLocker locker(&m_mutex);
    if (!metadata.contains("dependencies")) {
        return true;
    }
    
    QJsonArray depArray = metadata["dependencies"].toArray();
    for (const QJsonValue& val : depArray) {
        QString depId = val.toString();
        if (!m_plugins.contains(depId)) {
            return false;
        }
    }
    
    return true;
}

bool PluginManager::resolveDependencies()
{
    // 這個方法在 buildDependencyGraph 中處理
    return true;
}

void PluginManager::initializePlugins()
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it->instance && !it->initialized) {
            if (m_context) {
                m_context->setCurrentPluginId(it.key());
                if (m_permissionManager) {
                    QList<Permission> declared = m_permissionManager->declaredPermissions(it.key());
                    for (Permission p : declared) {
                        m_permissionManager->grantPermission(it.key(), p);
                    }
                }
                bool ok = false;
                try {
                    ok = it->instance->initialize(m_context);
                } catch (const std::exception& e) {
                    qWarning() << "Exception during plugin initialization:" << it.key() << e.what();
                    ok = false;
                }
                m_context->setCurrentPluginId(QString());
                if (ok) {
                    it->initialized = true;
                    setupPluginConnections(it->instance);
                    qDebug() << "Plugin initialized:" << it.key();
                } else {
                    qWarning() << "Plugin initialization failed:" << it.key();
                    emit pluginError(it.key(), "Initialization failed");
                }
            } else {
                it->initialized = true;
            }
        }
    }
}

void PluginManager::setupPluginConnections(ScripturaPlugin* plugin)
{
    Q_UNUSED(plugin);
    // 未來可以設定插件信號連接
}

bool PluginManager::buildDependencyGraph(const QList<QJsonObject>& pluginMetadata)
{
    QMutexLocker locker(&m_mutex);
    m_dependencyGraph.clear();
    
    for (const QJsonObject& metadata : pluginMetadata) {
        QString id = metadata["id"].toString();
        if (id.isEmpty()) {
            continue;
        }
        
        QStringList deps;
        if (metadata.contains("dependencies")) {
            QJsonArray depArray = metadata["dependencies"].toArray();
            for (const QJsonValue& val : depArray) {
                deps.append(val.toString());
            }
        }
        
        m_dependencyGraph[id] = deps;
    }
    
    return true;
}

QStringList PluginManager::topologicalSort()
{
    QMutexLocker locker(&m_mutex);
    QList<QJsonObject> plugins;
    for (const QString& id : m_dependencyGraph.keys()) {
        QJsonObject obj;
        obj["id"] = id;
        QJsonArray deps;
        for (const QString& dep : m_dependencyGraph[id]) {
            deps.append(dep);
        }
        obj["dependencies"] = deps;
        plugins.append(obj);
    }
    
    return m_resolver.topologicalSort(plugins);
}

bool PluginManager::loadPluginById(const QString& pluginId)
{
    if (m_plugins.contains(pluginId)) {
        return true;
    }
    
    if (!m_allowedPlugins.isEmpty() && !m_allowedPlugins.contains(pluginId)) {
        qWarning() << "Plugin not allowed (install via Plugin Manager):" << pluginId;
        return false;
    }
    
    if (m_pluginPaths.contains(pluginId)) {
        return loadPluginImpl(m_pluginPaths[pluginId]);
    }
    
    qWarning() << "Plugin path not found for:" << pluginId;
    return false;
}

void PluginManager::setAllowedPlugins(const QSet<QString>& allowed)
{
    QMutexLocker locker(&m_mutex);
    m_allowedPlugins = allowed;
}

void PluginManager::addAllowedPlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    m_allowedPlugins.insert(id);
}

void PluginManager::removeAllowedPlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    m_allowedPlugins.remove(id);
}

bool PluginManager::isAllowed(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    if (m_allowedPlugins.isEmpty()) {
        return true;
    }
    return m_allowedPlugins.contains(id);
}

QSet<QString> PluginManager::allowedPlugins() const
{
    QMutexLocker locker(&m_mutex);
    return m_allowedPlugins;
}

bool PluginManager::checkVersionCompatibility(const QJsonObject& metadata) const
{
    if (!metadata.contains("incompatible_with")) {
        return true;  // 沒有標記不相容，視為相容
    }
    
    QString currentVersion = VersionFetcher::coreVersion();
    QJsonArray incompatibleArray = metadata["incompatible_with"].toArray();
    QStringList incompatibleVersions;
    for (const QJsonValue& val : incompatibleArray) {
        incompatibleVersions.append(val.toString());
    }
    
    return !incompatibleVersions.contains(currentVersion);
}

bool PluginManager::disablePlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_plugins.contains(id)) {
        return false;
    }
    
    PluginInfo& info = m_plugins[id];
    if (info.state == PluginState::Disabled) {
        return true;
    }
    
    if (info.instance && info.initialized) {
        try {
            info.instance->shutdown();
        } catch (const std::exception& e) {
            qWarning() << "Exception during plugin shutdown:" << e.what();
        }
    }
    
    if (info.loader) {
        info.loader->unload();
        info.loader.reset();
    }
    
    info.instance = nullptr;
    info.initialized = false;
    info.state = PluginState::Disabled;
    
    m_disabledPlugins.insert(id);
    saveDisabledPlugins();
    
    emit pluginUnloaded(id);
    return true;
}

bool PluginManager::enablePlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_disabledPlugins.contains(id)) {
        return false;
    }
    
    m_disabledPlugins.remove(id);
    saveDisabledPlugins();
    
    if (m_pluginPaths.contains(id)) {
        return loadPluginImpl(m_pluginPaths[id]);
    }
    
    return false;
}

bool PluginManager::reloadPlugin(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_plugins.contains(id)) {
        return false;
    }
    
    if (!disablePlugin(id)) {
        return false;
    }
    
    m_disabledPlugins.remove(id);
    saveDisabledPlugins();
    
    if (m_pluginPaths.contains(id)) {
        return loadPluginImpl(m_pluginPaths[id]);
    }
    
    return false;
}

bool PluginManager::isDisabled(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    return m_disabledPlugins.contains(id);
}

QSet<QString> PluginManager::disabledPlugins() const
{
    QMutexLocker locker(&m_mutex);
    return m_disabledPlugins;
}

void PluginManager::loadDisabledPlugins()
{
    QMutexLocker locker(&m_mutex);
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/" + DISABLED_PLUGINS_FILE;
    
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QByteArray content = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse disabled plugins file:" << error.errorString();
        return;
    }
    
    if (doc.isArray()) {
        QJsonArray array = doc.array();
        for (const QJsonValue& val : array) {
            m_disabledPlugins.insert(val.toString());
        }
    }
}

void PluginManager::saveDisabledPlugins() const
{
    QMutexLocker locker(&m_mutex);
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString filePath = configPath + "/" + DISABLED_PLUGINS_FILE;
    
    QJsonArray array;
    for (const QString& id : m_disabledPlugins) {
        array.append(id);
    }
    
    QJsonDocument doc(array);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

QString PluginManager::pluginsInstallDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
    QDir().mkpath(dir);
    return dir;
}

bool PluginManager::installPluginFromArchive(const QString &pluginId, const QByteArray &archiveData, QWidget* parentWidget)
{
    QMutexLocker locker(&m_mutex);
    
    if (pluginId.isEmpty() || archiveData.isEmpty())
        return false;

    QString targetDir = pluginsInstallDir() + "/" + pluginId;
    QDir(targetDir).removeRecursively();
    QDir().mkpath(targetDir);

    QString archivePath = targetDir + "/__download.zip";
    {
        QFile f(archivePath);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write(archiveData);
        f.close();
    }

    bool extracted = false;
    if (!QStandardPaths::findExecutable("unzip").isEmpty()) {
        QProcess unzip;
        unzip.setWorkingDirectory(targetDir);
        unzip.start("unzip", {"-o", archivePath});
        extracted = unzip.waitForFinished(30000) && unzip.exitStatus() == QProcess::NormalExit;
    }
    QFile::remove(archivePath);

    if (!extracted)
        return false;

    QJsonObject metadata;
    if (!loadPluginMetadata(targetDir, metadata)) {
        QDir(targetDir).removeRecursively();
        return false;
    }

    QString name = metadata["name"].toString(pluginId);
    QString version = metadata["version"].toString("?");
    QString author = metadata["author"].toString("?");
    QString description = metadata["description"].toString();
    QString permissions;
    if (metadata.contains("permissions")) {
        QJsonArray perms = metadata["permissions"].toArray();
        for (const QJsonValue &v : perms)
            permissions += "  - " + v.toString() + "\n";
    }
    if (permissions.isEmpty())
        permissions = "  (none declared)\n";

    QString info = QObject::tr("Plugin: %1\nVersion: %2\nAuthor: %3\n\nDescription:\n%4\n\nDeclared permissions:\n%5\n\nPlugins run with full access to your files and system. Only install plugins from sources you trust.")
                       .arg(name, version, author, description, permissions);

    QMessageBox::StandardButton trust = QMessageBox::question(
        parentWidget,
        QObject::tr("Install Plugin"),
        info,
        QMessageBox::Yes | QMessageBox::No);
    if (trust != QMessageBox::Yes) {
        QDir(targetDir).removeRecursively();
        return false;
    }

    addAllowedPlugin(pluginId);
    m_pluginPaths.remove(pluginId);

    bool loaded = loadPluginById(pluginId);
    if (!loaded) {
        QDir(targetDir).removeRecursively();
    }
    return loaded;
}
