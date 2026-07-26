#ifndef RUST_ADAPTER_H
#define RUST_ADAPTER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextEdit>
#include <QTimer>
#include <functional>
#include <memory>

#include "rust_backend.h"

// ─────────────────────────────────────────────────────────────────────
//  RustLspClientAdapter — bridges LSP protocol to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustLspClientAdapter : public QObject
{
    Q_OBJECT
public:
    struct Diagnostic {
        enum Severity { Error = 1, Warning = 2, Information = 3, Hint = 4 };
        Severity severity;
        int line;
        int column;
        int endLine;
        int endColumn;
        QString message;
        QString source;
        QString code;
    };

    struct SymbolInformation {
        QString name;
        QString kind;
        QString containerName;
    };

    explicit RustLspClientAdapter(QObject *parent = nullptr);
    ~RustLspClientAdapter() override;

    bool startServer(const QString &command, const QStringList &args,
                     const QString &rootUri);
    void stopServer();
    bool isRunning() const;

    void initialize(const QString &rootUri, const QString &languageId);
    void initialized();
    void didOpen(const QString &uri, const QString &languageId, const QString &text);
    void didChange(const QString &uri, const QString &text);
    void didClose(const QString &uri);
    void shutdown();
    void exit();
    int completion(const QString &uri, int line, int character);
    int definition(const QString &uri, int line, int character);
    int hover(const QString &uri, int line, int character);
    int references(const QString &uri, int line, int character);
    int rename(const QString &uri, int line, int character, const QString &newName);
    int codeAction(const QString &uri, int startLine, int startChar,
                   int endLine, int endChar);
    int documentSymbol(const QString &uri);
    int workspaceSymbol(const QString &query);
    int formatting(const QString &uri, const QJsonObject &options);
    int signatureHelp(const QString &uri, int line, int character);
    int declaration(const QString &uri, int line, int character);
    int typeDefinition(const QString &uri, int line, int character);
    int implementation(const QString &uri, int line, int character);
    void feedMessage(const QByteArray &data);

signals:
    void serverStarted();
    void serverFailed(const QString &error);
    void diagnosticsReceived(const QString &uri, const QJsonArray &diagnostics);
    void completionReceived(const QJsonArray &items, int requestId);
    void definitionReceived(const QJsonArray &locations, int requestId);
    void hoverReceived(const QJsonObject &contents, int requestId);

private:
    static void onServerStartedCb(const char *data, void *userData);
    static void onServerFailedCb(const char *data, void *userData);
    static void onDiagnosticsCb(const char *uri, const char *jsonDiags, void *userData);
    static void onLspResultCb(int requestId, const char *jsonResult, void *userData);

    RustLspClient *m_client = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustDapClientAdapter — bridges DAP protocol to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustDapClientAdapter : public QObject
{
    Q_OBJECT
public:
    struct StackFrame {
        int id;
        QString name;
        int line;
        QString sourcePath;
    };

    struct Variable {
        QString name;
        QString value;
        QString type;
        int variablesReference;
    };

    explicit RustDapClientAdapter(QObject *parent = nullptr);
    ~RustDapClientAdapter() override;

    bool startServer(const QString &command, const QStringList &args);
    void stopServer();
    bool isRunning() const;

    void initialize(const QString &program, const QStringList &args, const QString &cwd);
    void launch();
    void configurationDone();
    void setBreakpoints(const QString &sourcePath, const QList<int> &lines);
    void continueDebug();
    void next();
    void stepIn();
    void stepOut();
    void pause();
    void disconnect();
    void stackTrace(int threadId);
    void scopes(int frameId);
    void variables(int varRef);
    void evaluate(const QString &expression, int frameId, const QString &context);

signals:
    void serverStarted();
    void serverFailed(const QString &error);
    void initialized();
    void stopped(const QString &reason, int threadId);
    void continued();
    void breakpointUpdated(const QString &sourcePath, const QJsonArray &breakpoints);
    void stackTraceReceived(int threadId, const QJsonArray &frames);
    void scopesReceived(int frameId, const QJsonArray &scopes);
    void variablesReceived(int varRef, const QJsonArray &variables);
    void evaluationReceived(const QString &expression, const QString &result);

private:
    static void onDapStartedCb(const char *data, void *userData);
    static void onDapFailedCb(const char *data, void *userData);
    static void onDapInitializedCb(const char *data, void *userData);
    static void onDapStoppedCb(const char *reason, int threadId, void *userData);
    static void onDapContinuedCb(const char *data, void *userData);
    static void onDapBreakpointsCb(const char *source, const char *jsonBps, void *userData);
    static void onDapStackTraceCb(int threadId, const char *jsonFrames, void *userData);
    static void onDapScopesCb(int frameId, const char *jsonScopes, void *userData);
    static void onDapVariablesCb(int varRef, const char *jsonVars, void *userData);
    static void onDapEvalCb(const char *result, void *userData);

    RustDapClient *m_client = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustEventBusAdapter — bridges Rust EventBus to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustEventBusAdapter : public QObject
{
    Q_OBJECT
public:
    using SubscriptionId = quint64;

    explicit RustEventBusAdapter(QObject *parent = nullptr);
    ~RustEventBusAdapter() override;

    SubscriptionId subscribe(const QString &event,
                             std::function<void(const QString&)> callback);
    void unsubscribe(const QString &event, SubscriptionId id);
    void publish(const QString &event, const QJsonObject &data = {});
    bool hasSubscribers(const QString &event) const;

signals:
    void eventPublished(const QString &event, const QJsonObject &data);

private:
    static void onEventCb(const char *event, const char *jsonData, void *userData);

    struct SubscriptionEntry {
        SubscriptionId id;
        QString event;
        std::function<void(const QString&)> callback;
    };

    RustEventBus *m_bus = nullptr;
    QHash<QString, QList<SubscriptionEntry>> m_subscriptions;
};

// ─────────────────────────────────────────────────────────────────────
//  RustPluginManagerAdapter — bridges PluginManager to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustPluginManagerAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustPluginManagerAdapter(QObject *parent = nullptr);
    ~RustPluginManagerAdapter() override;

    bool loadPlugins(const QString &path);
    bool loadPlugin(const QString &filePath);
    void unloadPlugin(const QString &id);
    void unloadAll();
    bool isLoaded(const QString &id) const;
    QString pluginVersion(const QString &id) const;
    QStringList listLoaded() const;

signals:
    void pluginLoaded(const QString &id);
    void pluginUnloaded(const QString &id);
    void pluginError(const QString &id, const QString &error);

private:
    static void onPluginLoadedCb(const char *id, const char *data, void *userData);
    static void onPluginUnloadedCb(const char *id, const char *data, void *userData);
    static void onPluginErrorCb(const char *id, const char *data, void *userData);

    RustPluginManager *m_manager = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustWorkspaceAdapter — bridges Workspace to Qt
// ─────────────────────────────────────────────────────────────────────
class RustWorkspaceAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustWorkspaceAdapter(QObject *parent = nullptr);
    ~RustWorkspaceAdapter() override;

    bool load(const QString &path);
    bool save();
    bool saveAs(const QString &path);
    QStringList folders() const;
    void setFolders(const QStringList &folders);
    QJsonObject settings() const;
    void setSettings(const QJsonObject &settings);
    QStringList recentFiles() const;
    void addRecentFile(const QString &file);
    QString path() const;
    bool isLoaded() const;

private:
    RustWorkspace *m_workspace = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustTaskRunnerAdapter — bridges TaskRunner to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustTaskRunnerAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustTaskRunnerAdapter(QObject *parent = nullptr);
    ~RustTaskRunnerAdapter() override;

    bool loadTasks(const QString &jsonTasks);
    void runTask(const QString &name);
    void stopTask();
    QStringList availableTasks() const;

signals:
    void taskStarted(const QString &name);
    void taskFinished(const QString &name, int exitCode);
    void taskOutput(const QString &name, const QString &output);
    void taskError(const QString &name, const QString &error);

private:
    static void onTaskStartedCb(const char *data, void *userData);
    static void onTaskFinishedCb(const char *data, void *userData);
    static void onTaskOutputCb(const char *data, void *userData);
    static void onTaskErrorCb(const char *data, void *userData);

    RustTaskRunner *m_runner = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustUpdaterAdapter — bridges Rust Updater to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustUpdaterAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustUpdaterAdapter(QObject *parent = nullptr);
    ~RustUpdaterAdapter() override;

    void checkForUpdates(const QString &currentVersion, const QString &updateUrl);
    bool isUpdateAvailable() const;
    QString latestVersion() const;

signals:
    void updateAvailable(const QString &version, const QString &downloadUrl);

private:
    static void onUpdateAvailableCb(const char *data, void *userData);

    RustUpdater *m_updater = nullptr;
    QString m_latestVersion;
};

// ─────────────────────────────────────────────────────────────────────
//  RustConfigValidatorAdapter — bridges Rust ConfigValidator to Qt
// ─────────────────────────────────────────────────────────────────────
class RustConfigValidatorAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustConfigValidatorAdapter(QObject *parent = nullptr);
    ~RustConfigValidatorAdapter() override;

    QString validate(const QString &jsonConfig, const QString &schemaJson);

signals:
    void validationError(const QString &error);

private:
    static void onValidationErrorCb(const char *data, void *userData);

    RustConfigValidator *m_validator = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustPluginRegistryAdapter — bridges Rust PluginRegistry to Qt signals
// ─────────────────────────────────────────────────────────────────────
class RustPluginRegistryAdapter : public QObject
{
    Q_OBJECT
public:
    explicit RustPluginRegistryAdapter(QObject *parent = nullptr);
    ~RustPluginRegistryAdapter() override;

    void setRegistryUrl(const QString &url);
    QString registryUrl() const;
    void checkForUpdates();
    bool upgradeAvailable(const QString &pluginId, const QString &currentVersion) const;

signals:
    void registryUpdated(const QString &manifestJson);
    void installFailed(const QString &pluginId, const QString &error);

private:
    static void onRegistryUpdatedCb(const char *data, void *userData);
    static void onInstallFailedCb(const char *id, const char *error, void *userData);

    RustPluginRegistry *m_registry = nullptr;
};

// ─────────────────────────────────────────────────────────────────────
//  RustBackend — singleton root that owns all Rust backend instances
// ─────────────────────────────────────────────────────────────────────
class RustBackend : public QObject
{
    Q_OBJECT
public:
    static RustBackend* instance();
    static void destroyInstance();

    RustLspClientAdapter*             lspClient() const { return m_lsp; }
    RustDapClientAdapter*             dapClient() const { return m_dap; }
    RustEventBusAdapter*              eventBus() const { return m_eventBus; }
    RustPluginManagerAdapter*         pluginManager() const { return m_pluginManager; }
    RustWorkspaceAdapter*             workspace() const { return m_workspace; }
    RustTaskRunnerAdapter*            taskRunner() const { return m_taskRunner; }
    RustUpdaterAdapter*               updater() const { return m_updater; }
    RustConfigValidatorAdapter*       configValidator() const { return m_configValidator; }
    RustPluginRegistryAdapter*        pluginRegistry() const { return m_pluginRegistry; }

private:
    RustBackend(QObject *parent = nullptr);
    ~RustBackend() override;
    RustBackend(const RustBackend&) = delete;
    RustBackend& operator=(const RustBackend&) = delete;

    RustLspClientAdapter*             m_lsp = nullptr;
    RustDapClientAdapter*             m_dap = nullptr;
    RustEventBusAdapter*              m_eventBus = nullptr;
    RustPluginManagerAdapter*         m_pluginManager = nullptr;
    RustWorkspaceAdapter*             m_workspace = nullptr;
    RustTaskRunnerAdapter*            m_taskRunner = nullptr;
    RustUpdaterAdapter*               m_updater = nullptr;
    RustConfigValidatorAdapter*       m_configValidator = nullptr;
    RustPluginRegistryAdapter*        m_pluginRegistry = nullptr;

    static RustBackend* s_instance;
};

#endif // RUST_ADAPTER_H
