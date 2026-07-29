#include "rust_adapter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>

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

    m_subscriptions[event].append({id, event, callback});
    return id;
}

auto RustEventBusAdapter::subscribe(const QString &event, QObject *receiver,
                                     std::function<void(const QVariant&)> callback)
    -> SubscriptionId
{
    QMutexLocker locker(&m_mutex);
    SubscriptionId id = m_nextId++;

    V8nSubscriptionEntry entry;
    entry.id = id;
    entry.event = event;
    entry.callback = std::move(callback);
    entry.receiver = receiver;
    entry.hasReceiver = (receiver != nullptr);
    m_v8nSubscriptions[event].append(entry);

    if (receiver) {
        connect(receiver, &QObject::destroyed, this, [this, event, id]() {
            unsubscribe(event, id);
        });
    }

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

    QMutexLocker locker(&m_mutex);
    if (m_v8nSubscriptions.contains(event)) {
        auto &list = m_v8nSubscriptions[event];
        list.erase(std::remove_if(list.begin(), list.end(),
                                   [id](const V8nSubscriptionEntry &s) { return s.id == id; }),
                   list.end());
        if (list.isEmpty())
            m_v8nSubscriptions.remove(event);
    }
}

void RustEventBusAdapter::unsubscribeReceiver(QObject *receiver)
{
    if (!receiver) return;

    QMutexLocker locker(&m_mutex);
    for (auto it = m_v8nSubscriptions.begin(); it != m_v8nSubscriptions.end();) {
        auto &entries = it.value();
        for (auto eit = entries.begin(); eit != entries.end();) {
            if (eit->receiver.data() == receiver) {
                eit = entries.erase(eit);
            } else {
                ++eit;
            }
        }
        if (entries.isEmpty()) {
            it = m_v8nSubscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

void RustEventBusAdapter::publish(const QString &event, const QVariant &data)
{
    // Convert QVariant to JSON string
    QByteArray jsonData;
    if (data.isValid()) {
        if (data.canConvert<QJsonObject>()) {
            jsonData = QJsonDocument(data.toJsonObject()).toJson(QJsonDocument::Compact);
        } else if (data.canConvert<QJsonArray>()) {
            jsonData = QJsonDocument(data.toJsonArray()).toJson(QJsonDocument::Compact);
        } else if (data.canConvert<QString>()) {
            jsonData = data.toString().toUtf8();
        } else if (data.canConvert<int>()) {
            jsonData = QByteArray::number(data.toInt());
        } else {
            // Fallback: serialize as JSON
            QJsonValue val = QJsonValue::fromVariant(data);
            if (!val.isUndefined()) {
                jsonData = QJsonDocument(QJsonObject{{"value", val}}).toJson(QJsonDocument::Compact);
            }
        }
    }

    // Publish to RustEventBus
    QByteArray e = event.toUtf8();
    rust_eventbus_publish(m_bus, e.constData(), jsonData.constData());

    // Call local QVariant subscribers
    QList<V8nSubscriptionEntry> callbacks;
    {
        QMutexLocker locker(&m_mutex);
        if (m_v8nSubscriptions.contains(event)) {
            callbacks = m_v8nSubscriptions[event];
        }
    }

    for (const auto &entry : callbacks) {
        if (entry.hasReceiver && entry.receiver.isNull()) continue;
        try {
            entry.callback(data);
        } catch (const std::exception &e) {
            qWarning() << "EventBus: Exception in callback for" << event << ":" << e.what();
        } catch (...) {
            qWarning() << "EventBus: Unknown exception";
        }
    }
}

void RustEventBusAdapter::publishJson(const QString &event, const QJsonObject &data)
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

    QMetaObject::invokeMethod(self, [self, ev, data]() {
        emit self->eventPublished(ev, data);
    }, Qt::QueuedConnection);

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
//  RustUpdaterAdapter
// ═══════════════════════════════════════════════════════════════════════

RustUpdaterAdapter::RustUpdaterAdapter(QObject *parent)
    : QObject(parent)
{
    m_updater = rust_updater_new();
    rust_updater_on_update_available(m_updater, &onUpdateAvailableCb, this);
}

RustUpdaterAdapter::~RustUpdaterAdapter()
{
    rust_updater_free(m_updater);
}

void RustUpdaterAdapter::checkForUpdates(const QString &currentVersion, const QString &updateUrl)
{
    QByteArray ver = currentVersion.toUtf8();
    QByteArray url = updateUrl.toUtf8();
    rust_updater_check(m_updater, ver.constData(), url.constData());
}

bool RustUpdaterAdapter::isUpdateAvailable() const
{
    return rust_updater_is_update_available(m_updater);
}

QString RustUpdaterAdapter::latestVersion() const
{
    char *ver = rust_updater_latest_version(m_updater);
    QString result = QString::fromUtf8(ver);
    rust_free_string(ver);
    return result;
}

void RustUpdaterAdapter::onUpdateAvailableCb(const char *data, void *userData)
{
    auto *self = static_cast<RustUpdaterAdapter*>(userData);
    QString version = QString::fromUtf8(data);
    self->m_latestVersion = version;
    QMetaObject::invokeMethod(self, [self, version]() {
        emit self->updateAvailable(version, QString());
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustConfigValidatorAdapter
// ═══════════════════════════════════════════════════════════════════════

RustConfigValidatorAdapter::RustConfigValidatorAdapter(QObject *parent)
    : QObject(parent)
{
    m_validator = rust_config_validator_new();
    rust_config_validator_on_error(m_validator, &onValidationErrorCb, this);
}

RustConfigValidatorAdapter::~RustConfigValidatorAdapter()
{
    rust_config_validator_free(m_validator);
}

QString RustConfigValidatorAdapter::validate(const QString &jsonConfig, const QString &schemaJson)
{
    QByteArray config = jsonConfig.toUtf8();
    QByteArray schema = schemaJson.toUtf8();
    char *result = rust_config_validator_validate(m_validator, config.constData(), schema.constData());
    QString error = QString::fromUtf8(result);
    rust_free_string(result);
    return error;
}

void RustConfigValidatorAdapter::onValidationErrorCb(const char *data, void *userData)
{
    auto *self = static_cast<RustConfigValidatorAdapter*>(userData);
    QString error = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, error]() {
        emit self->validationError(error);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustPluginRegistryAdapter
// ═══════════════════════════════════════════════════════════════════════

RustPluginRegistryAdapter::RustPluginRegistryAdapter(QObject *parent)
    : QObject(parent)
{
    m_registry = rust_plugin_registry_new();
    rust_plugin_registry_on_update(m_registry, &onRegistryUpdatedCb, this);
    rust_plugin_registry_on_install_failed(m_registry, &onInstallFailedCb, this);
}

RustPluginRegistryAdapter::~RustPluginRegistryAdapter()
{
    rust_plugin_registry_free(m_registry);
}

void RustPluginRegistryAdapter::setRegistryUrl(const QString &url)
{
    QByteArray u = url.toUtf8();
    rust_plugin_registry_set_url(m_registry, u.constData());
}

QString RustPluginRegistryAdapter::registryUrl() const
{
    char *url = rust_plugin_registry_get_url(m_registry);
    QString result = QString::fromUtf8(url);
    rust_free_string(url);
    return result;
}

void RustPluginRegistryAdapter::checkForUpdates()
{
    rust_plugin_registry_check_updates(m_registry);
}

bool RustPluginRegistryAdapter::upgradeAvailable(const QString &pluginId, const QString &currentVersion) const
{
    QByteArray id = pluginId.toUtf8();
    QByteArray ver = currentVersion.toUtf8();
    return rust_plugin_registry_upgrade_available(m_registry, id.constData(), ver.constData());
}

void RustPluginRegistryAdapter::onRegistryUpdatedCb(const char *data, void *userData)
{
    auto *self = static_cast<RustPluginRegistryAdapter*>(userData);
    QString json = QString::fromUtf8(data);
    QMetaObject::invokeMethod(self, [self, json]() {
        emit self->registryUpdated(json);
    }, Qt::QueuedConnection);
}

void RustPluginRegistryAdapter::onInstallFailedCb(const char *id, const char *error, void *userData)
{
    auto *self = static_cast<RustPluginRegistryAdapter*>(userData);
    QString pluginId = QString::fromUtf8(id);
    QString err = QString::fromUtf8(error);
    QMetaObject::invokeMethod(self, [self, pluginId, err]() {
        emit self->installFailed(pluginId, err);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustPermissionManagerAdapter
// ═══════════════════════════════════════════════════════════════════════

RustPermissionManagerAdapter::RustPermissionManagerAdapter(QObject *parent)
    : QObject(parent)
    , m_rustPm(rust_permission_manager_new())
{
}

RustPermissionManagerAdapter::~RustPermissionManagerAdapter()
{
    if (m_rustPm) {
        rust_permission_manager_free(m_rustPm);
        m_rustPm = nullptr;
    }
}

bool RustPermissionManagerAdapter::checkPermission(const QString &pluginId, Permission permission)
{
    if (!m_rustPm) return false;
    QByteArray idBytes = pluginId.toUtf8();
    return rust_permission_manager_check(m_rustPm, idBytes.constData(), static_cast<int>(permission));
}

void RustPermissionManagerAdapter::requestPermission(const QString &pluginId, Permission permission)
{
    if (!m_rustPm) return;
    QByteArray idBytes = pluginId.toUtf8();
    rust_permission_manager_request(m_rustPm, idBytes.constData(), static_cast<int>(permission));
}

void RustPermissionManagerAdapter::grantPermission(const QString &pluginId, Permission permission)
{
    if (!m_rustPm) return;
    QByteArray idBytes = pluginId.toUtf8();
    rust_permission_manager_grant(m_rustPm, idBytes.constData(), static_cast<int>(permission));
}

void RustPermissionManagerAdapter::revokePermission(const QString &pluginId, Permission permission)
{
    if (!m_rustPm) return;
    QByteArray idBytes = pluginId.toUtf8();
    rust_permission_manager_revoke(m_rustPm, idBytes.constData(), static_cast<int>(permission));
}

QList<Permission> RustPermissionManagerAdapter::grantedPermissions(const QString &pluginId) const
{
    if (!m_declaredPermissions.contains(pluginId))
        return {};
    return m_declaredPermissions[pluginId];
}

void RustPermissionManagerAdapter::setDeclaredPermissions(const QString &pluginId, const QList<Permission> &permissions)
{
    m_declaredPermissions[pluginId] = permissions;
}

QList<Permission> RustPermissionManagerAdapter::declaredPermissions(const QString &pluginId) const
{
    if (!m_declaredPermissions.contains(pluginId))
        return {};
    return m_declaredPermissions[pluginId];
}

// ═══════════════════════════════════════════════════════════════════════
//  RustServiceLocatorAdapter
// ═══════════════════════════════════════════════════════════════════════

RustServiceLocatorAdapter::RustServiceLocatorAdapter(QObject *parent)
    : QObject(parent)
    , m_rustSl(rust_service_locator_new())
{
}

RustServiceLocatorAdapter::~RustServiceLocatorAdapter()
{
    if (m_rustSl) {
        rust_service_locator_free(m_rustSl);
        m_rustSl = nullptr;
    }
}

void RustServiceLocatorAdapter::unregisterService(const QString &id)
{
    QByteArray idBytes = id.toUtf8();
    rust_service_locator_unregister(m_rustSl, idBytes.constData());
}

bool RustServiceLocatorAdapter::hasService(const QString &id) const
{
    QByteArray idBytes = id.toUtf8();
    return rust_service_locator_has(m_rustSl, idBytes.constData());
}

QStringList RustServiceLocatorAdapter::registeredServices() const
{
    size_t len = 0;
    char **list = rust_service_locator_list(m_rustSl, &len);
    if (!list) return {};
    QStringList result;
    for (size_t i = 0; i < len; ++i)
        result << QString::fromUtf8(list[i]);
    rust_service_locator_free_list(list, len);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  RustPluginCrashHandlerAdapter
// ═══════════════════════════════════════════════════════════════════════

RustPluginCrashHandlerAdapter::RustPluginCrashHandlerAdapter(QObject *parent)
    : QObject(parent)
    , m_rustH(rust_crash_handler_new())
    , m_crashLogPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugin_crashes.log")
{
    QDir().mkpath(QFileInfo(m_crashLogPath).absolutePath());
    rust_crash_handler_on_crash(m_rustH, &RustPluginCrashHandlerAdapter::onCrashCb, this);
}

RustPluginCrashHandlerAdapter::~RustPluginCrashHandlerAdapter()
{
    if (m_rustH) {
        rust_crash_handler_free(m_rustH);
        m_rustH = nullptr;
    }
}

void RustPluginCrashHandlerAdapter::handleCrash(const QString &pluginId)
{
    QByteArray idBytes = pluginId.toUtf8();
    QString errorStr = QStringLiteral("Process crashed");
    QByteArray errorBytes = errorStr.toUtf8();
    rust_crash_handler_report_crash(m_rustH, idBytes.constData(), errorBytes.constData());

    CrashInfo info;
    info.pluginId = pluginId;
    info.timestamp = QDateTime::currentDateTime();
    info.errorType = errorStr;
    info.stackTrace = QString();
    info.autoDisabled = true;

    m_crashHistory.prepend(info);
    if (m_crashHistory.size() > 100) m_crashHistory.removeLast();

    QString logEntry = QString("[%1] Plugin crashed: %2\n")
                           .arg(info.timestamp.toString(Qt::ISODate))
                           .arg(pluginId);
    QFile logFile(m_crashLogPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << logEntry;
    }

    disablePlugin(pluginId);
    emit pluginCrashed(pluginId, info);
    qWarning() << "Plugin crashed:" << pluginId << "at" << info.timestamp;
}

void RustPluginCrashHandlerAdapter::disablePlugin(const QString &pluginId)
{
    m_disabledPlugins[pluginId] = true;
}

bool RustPluginCrashHandlerAdapter::isPluginDisabled(const QString &pluginId) const
{
    return m_disabledPlugins.value(pluginId, false);
}

void RustPluginCrashHandlerAdapter::enablePlugin(const QString &pluginId)
{
    m_disabledPlugins.remove(pluginId);
}

QList<CrashInfo> RustPluginCrashHandlerAdapter::recentCrashes(int limit) const
{
    if (limit <= 0 || limit >= m_crashHistory.size())
        return m_crashHistory;
    return m_crashHistory.mid(0, limit);
}

void RustPluginCrashHandlerAdapter::onCrashCb(const char *pluginId, const char *error, void *userData)
{
    auto *self = static_cast<RustPluginCrashHandlerAdapter*>(userData);
    if (!self) return;
    QString id = QString::fromUtf8(pluginId);
    QString err = QString::fromUtf8(error);
    QMetaObject::invokeMethod(self, [self, id, err]() {
        CrashInfo info;
        info.pluginId = id;
        info.timestamp = QDateTime::currentDateTime();
        info.errorType = err;
        info.stackTrace = QString();
        info.autoDisabled = true;
        self->m_crashHistory.prepend(info);
        if (self->m_crashHistory.size() > 100) self->m_crashHistory.removeLast();
        self->disablePlugin(id);
        emit self->pluginCrashed(id, info);
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════════════════
//  RustArchiveExtractorAdapter
// ═══════════════════════════════════════════════════════════════════════

RustArchiveExtractorAdapter::RustArchiveExtractorAdapter(QObject *parent)
    : QObject(parent)
{
    m_extractor = rust_archive_extractor_new();
}

RustArchiveExtractorAdapter::~RustArchiveExtractorAdapter()
{
    if (m_extractor) {
        rust_archive_extractor_free(m_extractor);
        m_extractor = nullptr;
    }
}

bool RustArchiveExtractorAdapter::extract(const QByteArray &archiveData, const QString &destDir)
{
    if (!m_extractor) return false;
    QByteArray dirBytes = destDir.toUtf8();
    return rust_archive_extractor_extract(m_extractor,
        reinterpret_cast<const uint8_t*>(archiveData.constData()),
        archiveData.size(),
        dirBytes.constData());
}

// ═══════════════════════════════════════════════════════════════════════
//  RustDependencyResolverAdapter — uses Rust FFI directly
// ═══════════════════════════════════════════════════════════════════════

RustDependencyResolverAdapter::RustDependencyResolverAdapter(QObject *parent)
    : QObject(parent)
    , m_resolver(rust_dep_resolver_new())
{
}

RustDependencyResolverAdapter::~RustDependencyResolverAdapter()
{
    if (m_resolver) {
        rust_dep_resolver_free(m_resolver);
        m_resolver = nullptr;
    }
}

/// Helper: add all plugins to the Rust resolver, clear first.
/// Returns false if any plugin metadata is invalid.
static bool addAllPlugins(RustDependencyResolver *resolver, const QList<QJsonObject> &plugins)
{
    rust_dep_resolver_clear(resolver);
    for (const QJsonObject &p : plugins) {
        QByteArray id = p["id"].toString().toUtf8();
        QByteArray meta = QJsonDocument(p).toJson(QJsonDocument::Compact);
        if (!rust_dep_resolver_add_plugin(resolver, id.constData(), meta.constData())) {
            return false;
        }
    }
    return true;
}

QList<RustDependencyResolverAdapter::DependencyError> RustDependencyResolverAdapter::validate(
    const QList<QJsonObject> &plugins, const QSet<QString> &actuallyLoaded)
{
    QList<DependencyError> errors;
    if (!m_resolver) return errors;

    // Try to resolve all plugins together
    if (!addAllPlugins(m_resolver, plugins)) {
        // Metadata parse failure — report all plugins as unparseable
        for (const QJsonObject &p : plugins) {
            DependencyError err;
            err.pluginId = p["id"].toString();
            err.missingDependency = QString();
            err.isOptional = false;
            errors.append(err);
        }
        return errors;
    }

    size_t len = 0;
    char **result = rust_dep_resolver_order(m_resolver, &len);

    // If order is empty AND we have plugins, resolution failed (missing deps or cycle)
    if ((!result || len == 0) && !plugins.isEmpty()) {
        // Check each plugin's dependencies against actuallyLoaded
        for (const QJsonObject &p : plugins) {
            QString pluginId = p["id"].toString();
            QJsonArray depArray = p["dependencies"].toArray();
            for (const QJsonValue &val : depArray) {
                QString depId = val.toString();
                if (!actuallyLoaded.contains(depId)) {
                    DependencyError err;
                    err.pluginId = pluginId;
                    err.missingDependency = depId;
                    err.isOptional = false;
                    errors.append(err);
                }
            }
        }
    }

    if (result) {
        rust_dep_resolver_free_order(result, len);
    }
    return errors;
}

QStringList RustDependencyResolverAdapter::topologicalSort(const QList<QJsonObject> &plugins)
{
    if (!m_resolver) return {};

    // Add all plugins to the resolver first
    if (!addAllPlugins(m_resolver, plugins)) {
        return {};
    }

    // Now resolve the order
    size_t len = 0;
    char **result = rust_dep_resolver_order(m_resolver, &len);
    QStringList sorted;
    if (result && len > 0) {
        for (size_t i = 0; i < len; ++i) {
            sorted.append(QString::fromUtf8(result[i]));
        }
        rust_dep_resolver_free_order(result, len);
    }
    return sorted;
}

bool RustDependencyResolverAdapter::hasCircularDependency(const QList<QJsonObject> &plugins)
{
    // Use topologicalSort — if it returns empty, there's a cycle
    return topologicalSort(plugins).isEmpty() && !plugins.isEmpty();
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
    m_updater = new RustUpdaterAdapter(this);
    m_configValidator = new RustConfigValidatorAdapter(this);
    m_pluginRegistry = new RustPluginRegistryAdapter(this);
    m_permissionManager = new RustPermissionManagerAdapter(this);
    m_serviceLocator = new RustServiceLocatorAdapter(this);
    m_crashHandler = new RustPluginCrashHandlerAdapter(this);
    m_dependencyResolver = new RustDependencyResolverAdapter(this);
    m_archiveExtractor = new RustArchiveExtractorAdapter(this);
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
