#include "rust_adapter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>
#include <QDebug>

// ═══════════════════════════════════════════════════════════════════════
//  RustLspClientAdapter
// ═══════════════════════════════════════════════════════════════════════

RustLspClientAdapter::RustLspClientAdapter(QObject *parent)
    : QObject(parent)
{
    m_client = rust_lsp_client_new();

    // Set up callbacks
    rust_lsp_on_server_started(m_client, &RustLspClientAdapter::onServerStartedCb, this);
    rust_lsp_on_server_failed(m_client, &RustLspClientAdapter::onServerFailedCb, this);
    rust_lsp_on_diagnostics(m_client, &RustLspClientAdapter::onDiagnosticsCb, this);
    rust_lsp_on_completion(m_client, &RustLspClientAdapter::onLspResultCb, this);
    rust_lsp_on_definition(m_client, &RustLspClientAdapter::onLspResultCb, this);
    rust_lsp_on_hover(m_client, &RustLspClientAdapter::onLspResultCb, this);
    rust_lsp_on_references(m_client, &RustLspClientAdapter::onLspResultCb, this);
    rust_lsp_on_code_action(m_client, &RustLspClientAdapter::onLspResultCb, this);
}

RustLspClientAdapter::~RustLspClientAdapter()
{
    stopServer();
    rust_lsp_client_free(m_client);
}

bool RustLspClientAdapter::startServer(const QString &command, const QStringList &args,
                                        const QString &rootUri)
{
    QByteArray cmdBytes = command.toUtf8();
    QByteArray uriBytes = rootUri.toUtf8();

    std::vector<QByteArray> argBytes;
    std::vector<const char*> argPtrs;
    for (const auto &a : args) {
        argBytes.push_back(a.toUtf8());
        argPtrs.push_back(argBytes.back().constData());
    }

    return rust_lsp_start_server(m_client, cmdBytes.constData(),
                                  argPtrs.data(), argPtrs.size(),
                                  uriBytes.constData());
}

void RustLspClientAdapter::stopServer()
{
    rust_lsp_stop_server(m_client);
}

bool RustLspClientAdapter::isRunning() const
{
    return rust_lsp_is_running(m_client);
}

void RustLspClientAdapter::initialize(const QString &rootUri, const QString &languageId)
{
    QByteArray uri = rootUri.toUtf8();
    QByteArray lang = languageId.toUtf8();
    rust_lsp_initialize(m_client, uri.constData(), lang.constData());
}

void RustLspClientAdapter::initialized()
{
    rust_lsp_initialized(m_client);
}

void RustLspClientAdapter::didOpen(const QString &uri, const QString &languageId,
                                    const QString &text)
{
    QByteArray u = uri.toUtf8();
    QByteArray l = languageId.toUtf8();
    QByteArray t = text.toUtf8();
    rust_lsp_did_open(m_client, u.constData(), l.constData(), t.constData());
}

void RustLspClientAdapter::didChange(const QString &uri, const QString &text)
{
    QByteArray u = uri.toUtf8();
    QByteArray t = text.toUtf8();
    rust_lsp_did_change(m_client, u.constData(), t.constData());
}

void RustLspClientAdapter::didClose(const QString &uri)
{
    QByteArray u = uri.toUtf8();
    rust_lsp_did_close(m_client, u.constData());
}

void RustLspClientAdapter::shutdown()
{
    rust_lsp_shutdown(m_client);
}

void RustLspClientAdapter::exit()
{
    rust_lsp_exit(m_client);
}

int RustLspClientAdapter::completion(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_completion(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::definition(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_definition(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::hover(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_hover(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::references(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_references(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::rename(const QString &uri, int line, int character,
                                  const QString &newName)
{
    QByteArray u = uri.toUtf8();
    QByteArray n = newName.toUtf8();
    return rust_lsp_rename(m_client, u.constData(), line, character, n.constData());
}

int RustLspClientAdapter::codeAction(const QString &uri, int startLine, int startChar,
                                      int endLine, int endChar)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_code_action(m_client, u.constData(), startLine, startChar,
                                 endLine, endChar);
}

int RustLspClientAdapter::documentSymbol(const QString &uri)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_document_symbol(m_client, u.constData());
}

int RustLspClientAdapter::workspaceSymbol(const QString &query)
{
    QByteArray q = query.toUtf8();
    return rust_lsp_workspace_symbol(m_client, q.constData());
}

int RustLspClientAdapter::formatting(const QString &uri, const QJsonObject &options)
{
    QByteArray u = uri.toUtf8();
    QByteArray o = QJsonDocument(options).toJson(QJsonDocument::Compact);
    return rust_lsp_formatting(m_client, u.constData(), o.constData());
}

int RustLspClientAdapter::signatureHelp(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_signature_help(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::declaration(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_declaration(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::typeDefinition(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_type_definition(m_client, u.constData(), line, character);
}

int RustLspClientAdapter::implementation(const QString &uri, int line, int character)
{
    QByteArray u = uri.toUtf8();
    return rust_lsp_implementation(m_client, u.constData(), line, character);
}

void RustLspClientAdapter::feedMessage(const QByteArray &data)
{
    rust_lsp_feed_message(m_client, data.constData());
}

// ── Static callbacks ──────────────────────────────────────────────

void RustLspClientAdapter::onServerStartedCb(const char *, void *userData)
{
    auto *self = static_cast<RustLspClientAdapter*>(userData);
    QMetaObject::invokeMethod(self, "serverStarted", Qt::QueuedConnection);
}

void RustLspClientAdapter::onServerFailedCb(const char *data, void *userData)
{
    auto *self = static_cast<RustLspClientAdapter*>(userData);
    QString error = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, error]() {
        emit self->serverFailed(error);
    }, Qt::QueuedConnection);
}

void RustLspClientAdapter::onDiagnosticsCb(const char *uri, const char *jsonDiags,
                                             void *userData)
{
    auto *self = static_cast<RustLspClientAdapter*>(userData);
    QString uriStr = QString::fromUtf8(uri);
    QJsonArray diags = QJsonDocument::fromJson(QByteArray(jsonDiags)).array();
    QMetaObject::invokeMethod(self, [self, uriStr, diags]() {
        emit self->diagnosticsReceived(uriStr, diags);
    }, Qt::QueuedConnection);
}

void RustLspClientAdapter::onLspResultCb(int requestId, const char *jsonResult,
                                           void *userData)
{
    auto *self = static_cast<RustLspClientAdapter*>(userData);
    QJsonArray items = QJsonDocument::fromJson(QByteArray(jsonResult)).array();
    QMetaObject::invokeMethod(self, [self, requestId, items]() {
        emit self->completionReceived(items, requestId);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustDapClientAdapter
// ═══════════════════════════════════════════════════════════════════════

RustDapClientAdapter::RustDapClientAdapter(QObject *parent)
    : QObject(parent)
{
    m_client = rust_dap_client_new();

    rust_dap_on_server_started(m_client, &onDapStartedCb, this);
    rust_dap_on_server_failed(m_client, &onDapFailedCb, this);
    rust_dap_on_initialized(m_client, &onDapInitializedCb, this);
    rust_dap_on_stopped(m_client, &onDapStoppedCb, this);
    rust_dap_on_continued(m_client, &onDapContinuedCb, this);
    rust_dap_on_breakpoints(m_client, &onDapBreakpointsCb, this);
    rust_dap_on_stack_trace(m_client, &onDapStackTraceCb, this);
    rust_dap_on_scopes(m_client, &onDapScopesCb, this);
    rust_dap_on_variables(m_client, &onDapVariablesCb, this);
    rust_dap_on_evaluation(m_client, &onDapEvalCb, this);
}

RustDapClientAdapter::~RustDapClientAdapter()
{
    stopServer();
    rust_dap_client_free(m_client);
}

bool RustDapClientAdapter::startServer(const QString &command, const QStringList &args)
{
    QByteArray cmd = command.toUtf8();
    std::vector<QByteArray> argBytes;
    std::vector<const char*> argPtrs;
    for (const auto &a : args) {
        argBytes.push_back(a.toUtf8());
        argPtrs.push_back(argBytes.back().constData());
    }
    return rust_dap_start_server(m_client, cmd.constData(), argPtrs.data(), argPtrs.size());
}

void RustDapClientAdapter::stopServer() { rust_dap_stop_server(m_client); }
bool RustDapClientAdapter::isRunning() const { return rust_dap_is_running(m_client); }

void RustDapClientAdapter::initialize(const QString &program, const QStringList &args,
                                       const QString &cwd)
{
    QByteArray p = program.toUtf8();
    QByteArray c = cwd.toUtf8();
    std::vector<QByteArray> argBytes;
    std::vector<const char*> argPtrs;
    for (const auto &a : args) {
        argBytes.push_back(a.toUtf8());
        argPtrs.push_back(argBytes.back().constData());
    }
    rust_dap_initialize(m_client, p.constData(), argPtrs.data(), argPtrs.size(), c.constData());
}

void RustDapClientAdapter::launch() { rust_dap_launch(m_client); }
void RustDapClientAdapter::configurationDone() { rust_dap_configuration_done(m_client); }

void RustDapClientAdapter::setBreakpoints(const QString &sourcePath, const QList<int> &lines)
{
    QByteArray path = sourcePath.toUtf8();
    std::vector<int> linesVec;
    for (int l : lines) linesVec.push_back(l);
    rust_dap_set_breakpoints(m_client, path.constData(), linesVec.data(), linesVec.size());
}

void RustDapClientAdapter::continueDebug() { rust_dap_continue(m_client); }
void RustDapClientAdapter::next() { rust_dap_next(m_client); }
void RustDapClientAdapter::stepIn() { rust_dap_step_in(m_client); }
void RustDapClientAdapter::stepOut() { rust_dap_step_out(m_client); }
void RustDapClientAdapter::pause() { rust_dap_pause(m_client); }
void RustDapClientAdapter::disconnect() { rust_dap_disconnect(m_client); }
void RustDapClientAdapter::stackTrace(int threadId) { rust_dap_stack_trace(m_client, threadId); }
void RustDapClientAdapter::scopes(int frameId) { rust_dap_scopes(m_client, frameId); }
void RustDapClientAdapter::variables(int varRef) { rust_dap_variables(m_client, varRef); }

void RustDapClientAdapter::evaluate(const QString &expression, int frameId,
                                     const QString &context)
{
    QByteArray e = expression.toUtf8();
    QByteArray c = context.toUtf8();
    rust_dap_evaluate(m_client, e.constData(), frameId, c.constData());
}

// ── Static callbacks ──────────────────────────────────────────────

void RustDapClientAdapter::onDapStartedCb(const char *, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QMetaObject::invokeMethod(self, [self]() {
        emit self->serverStarted();
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapFailedCb(const char *data, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QString d = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, d]() {
        emit self->serverFailed(d);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapInitializedCb(const char *, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QMetaObject::invokeMethod(self, [self]() {
        emit self->initialized();
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapContinuedCb(const char *, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QMetaObject::invokeMethod(self, [self]() {
        emit self->continued();
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapStoppedCb(const char *reason, int threadId, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QString r = QString::fromUtf8(reason);
    QMetaObject::invokeMethod(self, [self, r, threadId]() {
        emit self->stopped(r, threadId);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapBreakpointsCb(const char *source, const char *jsonBps,
                                                void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QString src = QString::fromUtf8(source);
    QJsonArray bps = QJsonDocument::fromJson(QByteArray(jsonBps)).array();
    QMetaObject::invokeMethod(self, [self, src, bps]() {
        emit self->breakpointUpdated(src, bps);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapStackTraceCb(int threadId, const char *jsonFrames,
                                              void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QJsonArray frames = QJsonDocument::fromJson(QByteArray(jsonFrames)).array();
    QMetaObject::invokeMethod(self, [self, threadId, frames]() {
        emit self->stackTraceReceived(threadId, frames);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapScopesCb(int frameId, const char *jsonScopes, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QJsonArray scopes = QJsonDocument::fromJson(QByteArray(jsonScopes)).array();
    QMetaObject::invokeMethod(self, [self, frameId, scopes]() {
        emit self->scopesReceived(frameId, scopes);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapVariablesCb(int varRef, const char *jsonVars, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QJsonArray vars = QJsonDocument::fromJson(QByteArray(jsonVars)).array();
    QMetaObject::invokeMethod(self, [self, varRef, vars]() {
        emit self->variablesReceived(varRef, vars);
    }, Qt::QueuedConnection);
}

void RustDapClientAdapter::onDapEvalCb(const char *result, void *userData)
{
    auto *self = static_cast<RustDapClientAdapter*>(userData);
    QString r = QString::fromUtf8(result);
    QMetaObject::invokeMethod(self, [self, r]() {
        emit self->evaluationReceived(QString(), r);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustEventBusAdapter
// ═══════════════════════════════════════════════════════════════════════

RustEventBusAdapter::RustEventBusAdapter(QObject *parent)
    : QObject(parent)
{
    m_bus = rust_eventbus_new();
    // Subscribe to all events from Rust and forward to Qt signal
    // (Actually Rust callbacks fire per-event; we handle this in onEventCb)
}

RustEventBusAdapter::~RustEventBusAdapter()
{
    rust_eventbus_free(m_bus);
}

auto RustEventBusAdapter::subscribe(const QString &event,
                                     std::function<void(const QString&)> callback)
    -> SubscriptionId
{
    QByteArray e = event.toUtf8();
    SubscriptionId id = rust_eventbus_subscribe(m_bus, e.constData(),
                                                 &onEventCb, this);

    // Track subscription for management
    m_subscriptions[event].append({id, event, callback});
    return id;
}

void RustEventBusAdapter::unsubscribe(const QString &event, SubscriptionId id)
{
    QByteArray e = event.toUtf8();
    rust_eventbus_unsubscribe(m_bus, e.constData(), id);

    if (m_subscriptions.contains(event)) {
        auto &list = m_subscriptions[event];
        list.erase(std::remove_if(list.begin(), list.end(),
                                   [id](const SubscriptionEntry &s) { return s.id == id; }),
                   list.end());
        if (list.isEmpty())
            m_subscriptions.remove(event);
    }
}

void RustEventBusAdapter::publish(const QString &event, const QJsonObject &data)
{
    QByteArray e = event.toUtf8();
    QByteArray d = QJsonDocument(data).toJson(QJsonDocument::Compact);
    rust_eventbus_publish(m_bus, e.constData(), d.constData());
}

bool RustEventBusAdapter::hasSubscribers(const QString &event) const
{
    QByteArray e = event.toUtf8();
    return rust_eventbus_has_subscribers(m_bus, e.constData());
}

void RustEventBusAdapter::onEventCb(const char *event, const char *jsonData, void *userData)
{
    auto *self = static_cast<RustEventBusAdapter*>(userData);
    QString ev = QString::fromUtf8(event);
    QJsonObject data = QJsonDocument::fromJson(QByteArray(jsonData)).object();

    // Forward to Qt signal
    QMetaObject::invokeMethod(self, [self, ev, data]() {
        emit self->eventPublished(ev, data);
    }, Qt::QueuedConnection);

    // Also call tracked C++ callbacks
    if (self->m_subscriptions.contains(ev)) {
        QString dataStr = QString::fromUtf8(jsonData);
        for (const auto &sub : self->m_subscriptions[ev]) {
            if (sub.callback)
                sub.callback(dataStr);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  RustPluginManagerAdapter
// ═══════════════════════════════════════════════════════════════════════

RustPluginManagerAdapter::RustPluginManagerAdapter(QObject *parent)
    : QObject(parent)
{
    m_manager = rust_plugin_manager_new();
    rust_pm_on_plugin_loaded(m_manager, &onPluginLoadedCb, this);
    rust_pm_on_plugin_unloaded(m_manager, &onPluginUnloadedCb, this);
    rust_pm_on_plugin_error(m_manager, &onPluginErrorCb, this);
}

RustPluginManagerAdapter::~RustPluginManagerAdapter()
{
    rust_plugin_manager_free(m_manager);
}

bool RustPluginManagerAdapter::loadPlugins(const QString &path)
{
    QByteArray p = path.toUtf8();
    return rust_pm_load_plugins(m_manager, p.constData());
}

bool RustPluginManagerAdapter::loadPlugin(const QString &filePath)
{
    QByteArray p = filePath.toUtf8();
    return rust_pm_load_plugin(m_manager, p.constData());
}

void RustPluginManagerAdapter::unloadPlugin(const QString &id)
{
    QByteArray i = id.toUtf8();
    rust_pm_unload_plugin(m_manager, i.constData());
}

void RustPluginManagerAdapter::unloadAll()
{
    rust_pm_unload_all(m_manager);
}

bool RustPluginManagerAdapter::isLoaded(const QString &id) const
{
    QByteArray i = id.toUtf8();
    return rust_pm_is_loaded(m_manager, i.constData());
}

QString RustPluginManagerAdapter::pluginVersion(const QString &id) const
{
    QByteArray i = id.toUtf8();
    char *ver = rust_pm_plugin_version(m_manager, i.constData());
    if (!ver) return {};
    QString result = QString::fromUtf8(ver);
    rust_free_string(ver);
    return result;
}

QStringList RustPluginManagerAdapter::listLoaded() const
{
    size_t len = 0;
    char **list = rust_pm_list_loaded(m_manager, &len);
    QStringList result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(list[i]);
    }
    rust_pm_free_strings(list, len);
    return result;
}

void RustPluginManagerAdapter::onPluginLoadedCb(const char *id, const char *, void *userData)
{
    auto *self = static_cast<RustPluginManagerAdapter*>(userData);
    QString pluginId = QString::fromUtf8(id);
    QMetaObject::invokeMethod(self, [self, pluginId]() {
        emit self->pluginLoaded(pluginId);
    }, Qt::QueuedConnection);
}

void RustPluginManagerAdapter::onPluginUnloadedCb(const char *id, const char *, void *userData)
{
    auto *self = static_cast<RustPluginManagerAdapter*>(userData);
    QString pluginId = QString::fromUtf8(id);
    QMetaObject::invokeMethod(self, [self, pluginId]() {
        emit self->pluginUnloaded(pluginId);
    }, Qt::QueuedConnection);
}

void RustPluginManagerAdapter::onPluginErrorCb(const char *id, const char *data,
                                                 void *userData)
{
    auto *self = static_cast<RustPluginManagerAdapter*>(userData);
    QString pluginId = QString::fromUtf8(id);
    QString error = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, pluginId, error]() {
        emit self->pluginError(pluginId, error);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustWorkspaceAdapter
// ═══════════════════════════════════════════════════════════════════════

RustWorkspaceAdapter::RustWorkspaceAdapter(QObject *parent)
    : QObject(parent)
{
    m_workspace = rust_workspace_new();
}

RustWorkspaceAdapter::~RustWorkspaceAdapter()
{
    rust_workspace_free(m_workspace);
}

bool RustWorkspaceAdapter::load(const QString &path)
{
    QByteArray p = path.toUtf8();
    return rust_workspace_load(m_workspace, p.constData());
}

bool RustWorkspaceAdapter::save()
{
    return rust_workspace_save(m_workspace);
}

bool RustWorkspaceAdapter::saveAs(const QString &path)
{
    QByteArray p = path.toUtf8();
    return rust_workspace_save_as(m_workspace, p.constData());
}

QStringList RustWorkspaceAdapter::folders() const
{
    size_t len = 0;
    char **folders = rust_workspace_folders(m_workspace, &len);
    QStringList result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(folders[i]);
    }
    rust_pm_free_strings(folders, len);
    return result;
}

void RustWorkspaceAdapter::setFolders(const QStringList &folders)
{
    std::vector<QByteArray> bytes;
    std::vector<const char*> ptrs;
    for (const auto &f : folders) {
        bytes.push_back(f.toUtf8());
        ptrs.push_back(bytes.back().constData());
    }
    rust_workspace_set_folders(m_workspace, ptrs.data(), ptrs.size());
}

QJsonObject RustWorkspaceAdapter::settings() const
{
    char *json = rust_workspace_get_settings(m_workspace);
    QJsonObject result = QJsonDocument::fromJson(QByteArray(json)).object();
    rust_free_string(json);
    return result;
}

void RustWorkspaceAdapter::setSettings(const QJsonObject &settings)
{
    QByteArray json = QJsonDocument(settings).toJson(QJsonDocument::Compact);
    rust_workspace_set_settings(m_workspace, json.constData());
}

QStringList RustWorkspaceAdapter::recentFiles() const
{
    size_t len = 0;
    char **files = rust_workspace_recent_files(m_workspace, &len);
    QStringList result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(files[i]);
    }
    rust_pm_free_strings(files, len);
    return result;
}

void RustWorkspaceAdapter::addRecentFile(const QString &file)
{
    QByteArray f = file.toUtf8();
    rust_workspace_add_recent(m_workspace, f.constData());
}

QString RustWorkspaceAdapter::path() const
{
    char *p = rust_workspace_path(m_workspace);
    QString result = QString::fromUtf8(p);
    rust_free_string(p);
    return result;
}

bool RustWorkspaceAdapter::isLoaded() const
{
    return rust_workspace_is_loaded(m_workspace);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustTaskRunnerAdapter
// ═══════════════════════════════════════════════════════════════════════

RustTaskRunnerAdapter::RustTaskRunnerAdapter(QObject *parent)
    : QObject(parent)
{
    m_runner = rust_task_runner_new();
    rust_task_runner_on_started(m_runner, &onTaskStartedCb, this);
    rust_task_runner_on_finished(m_runner, &onTaskFinishedCb, this);
    rust_task_runner_on_output(m_runner, &onTaskOutputCb, this);
    rust_task_runner_on_error(m_runner, &onTaskErrorCb, this);
}

RustTaskRunnerAdapter::~RustTaskRunnerAdapter()
{
    stopTask();
    rust_task_runner_free(m_runner);
}

bool RustTaskRunnerAdapter::loadTasks(const QString &jsonTasks)
{
    QByteArray j = jsonTasks.toUtf8();
    return rust_task_runner_load(m_runner, j.constData());
}

void RustTaskRunnerAdapter::runTask(const QString &name)
{
    QByteArray n = name.toUtf8();
    rust_task_runner_run(m_runner, n.constData());
}

void RustTaskRunnerAdapter::stopTask()
{
    rust_task_runner_stop(m_runner);
}

QStringList RustTaskRunnerAdapter::availableTasks() const
{
    size_t len = 0;
    char **tasks = rust_task_runner_available(m_runner, &len);
    QStringList result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(tasks[i]);
    }
    rust_pm_free_strings(tasks, len);
    return result;
}

void RustTaskRunnerAdapter::onTaskStartedCb(const char *data, void *userData)
{
    auto *self = static_cast<RustTaskRunnerAdapter*>(userData);
    QString name = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, name]() {
        emit self->taskStarted(name);
    }, Qt::QueuedConnection);
}

void RustTaskRunnerAdapter::onTaskFinishedCb(const char *data, void *userData)
{
    auto *self = static_cast<RustTaskRunnerAdapter*>(userData);
    QString info = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, info]() {
        emit self->taskFinished(info, 0);
    }, Qt::QueuedConnection);
}

void RustTaskRunnerAdapter::onTaskOutputCb(const char *data, void *userData)
{
    auto *self = static_cast<RustTaskRunnerAdapter*>(userData);
    QString output = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, output]() {
        emit self->taskOutput(QString(), output);
    }, Qt::QueuedConnection);
}

void RustTaskRunnerAdapter::onTaskErrorCb(const char *data, void *userData)
{
    auto *self = static_cast<RustTaskRunnerAdapter*>(userData);
    QString error = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, error]() {
        emit self->taskError(QString(), error);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustBackend singleton
// ═══════════════════════════════════════════════════════════════════════

RustBackend* RustBackend::s_instance = nullptr;

RustBackend::RustBackend(QObject *parent)
    : QObject(parent)
{
    m_eventBus = new RustEventBusAdapter(this);
    m_lsp = new RustLspClientAdapter(this);
    m_dap = new RustDapClientAdapter(this);
    m_pluginManager = new RustPluginManagerAdapter(this);
    m_workspace = new RustWorkspaceAdapter(this);
    m_taskRunner = new RustTaskRunnerAdapter(this);
}

RustBackend::~RustBackend()
{
    // Children are cleaned up by Qt parent-child mechanism
}

RustBackend* RustBackend::instance()
{
    if (!s_instance) {
        s_instance = new RustBackend();
    }
    return s_instance;
}

void RustBackend::destroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}
