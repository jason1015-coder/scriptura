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
#include <QVariantMap>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

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
    // Run the fetch on a background thread: the Rust side does a blocking
    // HTTP request (reqwest::blocking::get) which must never run on the GUI
    // thread. Results come back through the registered callbacks, which
    // marshal onto this thread via queued connections.
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([this]() {
        rust_plugin_registry_check_updates(m_registry);
    }));
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
//  UiActionBridge
// ═══════════════════════════════════════════════════════════════════════

UiActionBridge::UiActionBridge(QObject *parent)
    : QObject(parent)
{
    m_handler = rust_ui_actions_new();
}

UiActionBridge::~UiActionBridge()
{
    if (m_handler) {
        rust_ui_actions_free(m_handler);
        m_handler = nullptr;
    }
}

void UiActionBridge::handle(const QString &action, const QJsonObject &payload)
{
    if (!m_handler) return;

    QByteArray actionBytes = action.toUtf8();
    QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    char *result = rust_ui_actions_handle(m_handler, actionBytes.constData(),
                                          payloadBytes.constData());
    if (!result) return;

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(result));
    rust_free_string(result);

    const QJsonObject obj = doc.object();
    const QString error = obj["error"].toString();
    if (!error.isEmpty()) {
        emit actionError(action, error);
        return;
    }

    const QJsonArray commands = obj["commands"].toArray();
    for (const QJsonValue &v : commands) {
        dispatchCommand(v.toObject());
    }
}

void UiActionBridge::dispatchCommand(const QJsonObject &cmd)
{
    const QString name = cmd["cmd"].toString();
    if (name == UiCommands::WindowMinimize) {
        emit windowMinimizeRequested();
    } else if (name == UiCommands::WindowToggleMaximized) {
        emit windowToggleMaximizedRequested();
    } else if (name == UiCommands::WindowClose) {
        emit windowCloseRequested();
    } else if (name == UiCommands::SidebarToggle) {
        emit sidebarToggleRequested();
    } else if (name == UiCommands::InspectorToggle) {
        emit inspectorToggleRequested();
    } else if (name == UiCommands::SettingsOpen) {
        emit settingsOpenRequested();
    } else if (name == UiCommands::SearchOpen) {
        emit searchOpenRequested(cmd["query"].toString());
    } else if (name == UiCommands::ProjectPromptOpen) {
        emit projectPromptOpenRequested();
    } else if (name == UiCommands::ProjectOpen) {
        emit projectOpenRequested(cmd["path"].toString());
    } else if (name == UiCommands::GitClone) {
        emit gitCloneRequested(cmd["url"].toString());
    } else if (name == UiCommands::FileNew) {
        emit fileNewRequested();
    } else {
        qWarning() << "UiActionBridge: unknown command from Rust:" << name;
    }
}

QStringList UiActionBridge::auditLog() const
{
    QStringList result;
    if (!m_handler) return result;

    size_t len = 0;
    char **entries = rust_ui_actions_log(m_handler, &len);
    if (!entries) return result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(entries[i]);
    }
    rust_pm_free_strings(entries, len);
    return result;
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
    m_uiActions = new UiActionBridge(this);
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

// ═══════════════════════════════════════════════════════════════════════
//  RustTextBufferAdapter
// ═══════════════════════════════════════════════════════════════════════

namespace {

/// RAII guard for strings returned by the Rust FFI.
struct RustStr
{
    explicit RustStr(char *s) : m_s(s) {}
    ~RustStr() { if (m_s) rust_free_string(m_s); }
    RustStr(const RustStr&) = delete;
    RustStr& operator=(const RustStr&) = delete;
    bool isNull() const { return m_s == nullptr; }
    QString value() const { return m_s ? QString::fromUtf8(m_s) : QString(); }
private:
    char *m_s;
};

Snippet snippetFromJsonObj(const QJsonObject &o)
{
    Snippet s;
    s.id = o.value("id").toString();
    s.name = o.value("name").toString();
    s.prefix = o.value("prefix").toString();
    s.body = o.value("body").toString();
    s.description = o.value("description").toString();
    s.language = o.value("language").toString();
    s.tabStops = o.value("tabStops").toInt(0);
    return s;
}

QJsonObject snippetToJsonObj(const Snippet &s)
{
    QJsonObject o;
    o.insert("id", s.id);
    o.insert("name", s.name);
    o.insert("prefix", s.prefix);
    o.insert("body", s.body);
    o.insert("description", s.description);
    o.insert("language", s.language);
    o.insert("tabStops", s.tabStops);
    return o;
}

} // anonymous namespace

RustTextBufferAdapter::RustTextBufferAdapter(QObject *parent)
    : QObject(parent)
    , m_buffer(rust_text_buffer_new())
{
}

RustTextBufferAdapter::~RustTextBufferAdapter()
{
    if (m_buffer)
        rust_text_buffer_free(m_buffer);
}

void RustTextBufferAdapter::setText(const QString &utf8)
{
    if (!m_buffer) return;
    const QByteArray bytes = utf8.toUtf8();
    rust_text_set(m_buffer, bytes.constData());
}

QString RustTextBufferAdapter::text() const
{
    if (!m_buffer) return QString();
    RustStr s(rust_text_get(m_buffer));
    return s.value();
}

quint64 RustTextBufferAdapter::version() const
{
    return m_buffer ? rust_text_version(m_buffer) : 0;
}

int RustTextBufferAdapter::lineCount() const
{
    return m_buffer ? static_cast<int>(rust_text_line_count(m_buffer)) : 0;
}

QString RustTextBufferAdapter::line(uint32_t line) const
{
    if (!m_buffer) return QString();
    RustStr s(rust_text_line(m_buffer, line));
    return s.value();
}

QString RustTextBufferAdapter::smartIndent(const QString &lineText, uint32_t tabWidth)
{
    const QByteArray bytes = lineText.toUtf8();
    RustStr s(rust_edit_smart_indent(bytes.constData(), tabWidth));
    return s.value();
}

int RustTextBufferAdapter::bracketDecision(const QString &typed, const QString &next, bool hasNext)
{
    const QByteArray typedBytes = typed.toUtf8();
    const QByteArray nextBytes = next.toUtf8();
    return rust_edit_bracket_decision(typedBytes.constData(),
                                      nextBytes.constData(), hasNext);
}

QString RustTextBufferAdapter::bracketClose(const QString &typed)
{
    const QByteArray bytes = typed.toUtf8();
    char out[8] = {0};
    if (rust_edit_bracket_close(bytes.constData(), out, sizeof(out)))
        return QString::fromUtf8(out);
    return QString();
}

QPair<qint64, qint64> RustTextBufferAdapter::nextOccurrence(
    const QString &text, const QString &needle, size_t fromUtf16, bool *found)
{
    if (found) *found = false;
    const QByteArray textBytes = text.toUtf8();
    const QByteArray needleBytes = needle.toUtf8();
    size_t start = 0, end = 0;
    if (!rust_edit_next_occurrence_utf16(textBytes.constData(), needleBytes.constData(),
                                         fromUtf16, &start, &end))
        return {-1, -1};
    if (found) *found = true;
    return {static_cast<qint64>(start), static_cast<qint64>(end)};
}

QString RustTextBufferAdapter::allOccurrencesJson(const QString &text, const QString &needle)
{
    const QByteArray textBytes = text.toUtf8();
    const QByteArray needleBytes = needle.toUtf8();
    RustStr s(rust_edit_all_occurrences_utf16(textBytes.constData(), needleBytes.constData()));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

int RustTextBufferAdapter::fuzzyScore(const QString &pattern, const QString &text)
{
    const QByteArray pBytes = pattern.toUtf8();
    const QByteArray tBytes = text.toUtf8();
    return static_cast<int>(rust_search_fuzzy(pBytes.constData(), tBytes.constData()));
}

QString RustTextBufferAdapter::foldRanges(const QString &flatText, bool indentBased)
{
    const QByteArray bytes = flatText.toUtf8();
    RustStr s(rust_fold_compute(bytes.constData(), indentBased ? 1 : 0));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

QString RustTextBufferAdapter::bracketPairs(const QString &flatText)
{
    const QByteArray bytes = flatText.toUtf8();
    RustStr s(rust_brackets_compute(bytes.constData()));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

QString RustTextBufferAdapter::expandSnippet(const QString &body, const QString &filename)
{
    const QByteArray bodyBytes = body.toUtf8();
    const QByteArray fileBytes = filename.toUtf8();
    RustStr s(rust_snippet_expand(bodyBytes.constData(), fileBytes.constData()));
    return s.isNull() ? QStringLiteral("{}") : s.value();
}

// ═══════════════════════════════════════════════════════════════════════
//  RustBookmarkStoreAdapter
// ═══════════════════════════════════════════════════════════════════════

RustBookmarkStoreAdapter::RustBookmarkStoreAdapter(QObject *parent)
    : QObject(parent)
    , m_store(rust_bookmarks_new())
{
}

RustBookmarkStoreAdapter::~RustBookmarkStoreAdapter()
{
    if (m_store)
        rust_bookmarks_free(m_store);
}

int RustBookmarkStoreAdapter::toggle(const QString &file, quint32 line, const QString &text)
{
    if (!m_store) return -2;
    const QByteArray fileBytes = file.toUtf8();
    const QByteArray textBytes = text.toUtf8();
    return rust_bookmarks_toggle(m_store, fileBytes.constData(), line, textBytes.constData());
}

bool RustBookmarkStoreAdapter::remove(int id)
{
    return m_store && rust_bookmarks_remove(m_store, id);
}

void RustBookmarkStoreAdapter::clear()
{
    if (m_store) rust_bookmarks_clear(m_store);
}

void RustBookmarkStoreAdapter::clearFile(const QString &file)
{
    if (!m_store) return;
    const QByteArray fileBytes = file.toUtf8();
    rust_bookmarks_clear_file(m_store, fileBytes.constData());
}

bool RustBookmarkStoreAdapter::isBookmarked(const QString &file, quint32 line) const
{
    if (!m_store) return false;
    const QByteArray fileBytes = file.toUtf8();
    return rust_bookmarks_is_bookmarked(m_store, fileBytes.constData(), line);
}

int RustBookmarkStoreAdapter::bookmarkAt(const QString &file, quint32 line) const
{
    if (!m_store) return -1;
    const QByteArray fileBytes = file.toUtf8();
    return rust_bookmarks_at(m_store, fileBytes.constData(), line);
}

bool RustBookmarkStoreAdapter::parseNav(const QString &json, Nav *out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return false;
    const QJsonObject o = doc.object();
    if (o.isEmpty() || o.value("id").isNull()) return false;
    if (out) {
        out->id = o.value("id").toInt(-1);
        out->file = o.value("file").toString();
        out->line = static_cast<quint32>(o.value("line").toInt(0));
        out->text = o.value("text").toString();
    }
    return true;
}

bool RustBookmarkStoreAdapter::next(const QString &file, qint64 currentLine, Nav *out) const
{
    if (!m_store) return false;
    const QByteArray fileBytes = file.toUtf8();
    RustStr s(rust_bookmarks_next(m_store, fileBytes.constData(), currentLine));
    return parseNav(s.value(), out);
}

bool RustBookmarkStoreAdapter::prev(const QString &file, qint64 currentLine, Nav *out) const
{
    if (!m_store) return false;
    const QByteArray fileBytes = file.toUtf8();
    RustStr s(rust_bookmarks_prev(m_store, fileBytes.constData(), currentLine));
    return parseNav(s.value(), out);
}

QString RustBookmarkStoreAdapter::toJson() const
{
    if (!m_store) return QStringLiteral("[]");
    RustStr s(rust_bookmarks_json(m_store));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

QString RustBookmarkStoreAdapter::toQtJson() const
{
    if (!m_store) return QStringLiteral("[]");
    RustStr s(rust_bookmarks_to_qt_json(m_store));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

void RustBookmarkStoreAdapter::loadQtJson(const QString &json)
{
    if (!m_store) return;
    const QByteArray bytes = json.toUtf8();
    rust_bookmarks_load_qt_json(m_store, bytes.constData());
}

// ═══════════════════════════════════════════════════════════════════════
//  RustSnippetStoreAdapter
// ═══════════════════════════════════════════════════════════════════════

RustSnippetStoreAdapter::RustSnippetStoreAdapter(QObject *parent)
    : QObject(parent)
    , m_store(rust_snippet_store_new())
{
}

RustSnippetStoreAdapter::~RustSnippetStoreAdapter()
{
    if (m_store)
        rust_snippet_store_free(m_store);
}

Snippet RustSnippetStoreAdapter::snippetFromJson(const QString &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    return snippetFromJsonObj(doc.object());
}

QString RustSnippetStoreAdapter::snippetToJson(const Snippet &snippet)
{
    return QString::fromUtf8(
        QJsonDocument(snippetToJsonObj(snippet)).toJson(QJsonDocument::Compact));
}

bool RustSnippetStoreAdapter::add(const Snippet &snippet)
{
    if (!m_store) return false;
    const QByteArray bytes = snippetToJson(snippet).toUtf8();
    return rust_snippet_store_add(m_store, bytes.constData());
}

bool RustSnippetStoreAdapter::update(const Snippet &snippet)
{
    if (!m_store) return false;
    const QByteArray bytes = snippetToJson(snippet).toUtf8();
    return rust_snippet_store_update(m_store, bytes.constData());
}

bool RustSnippetStoreAdapter::remove(const QString &id)
{
    if (!m_store) return false;
    const QByteArray bytes = id.toUtf8();
    return rust_snippet_store_remove(m_store, bytes.constData());
}

Snippet RustSnippetStoreAdapter::get(const QString &id) const
{
    if (!m_store) return Snippet{};
    const QByteArray bytes = id.toUtf8();
    RustStr s(rust_snippet_store_get(m_store, bytes.constData()));
    return s.isNull() ? Snippet{} : snippetFromJson(s.value());
}

QList<Snippet> RustSnippetStoreAdapter::all() const
{
    QList<Snippet> out;
    if (!m_store) return out;
    size_t len = 0;
    char **strs = rust_snippet_store_all(m_store, &len);
    for (size_t i = 0; i < len; ++i)
        out.append(snippetFromJson(QString::fromUtf8(strs[i])));
    if (strs) rust_pm_free_strings(strs, len);
    return out;
}

QList<Snippet> RustSnippetStoreAdapter::forLanguage(const QString &language) const
{
    QList<Snippet> out;
    if (!m_store) return out;
    const QByteArray bytes = language.toUtf8();
    size_t len = 0;
    char **strs = rust_snippet_store_for_language(m_store, bytes.constData(), &len);
    for (size_t i = 0; i < len; ++i)
        out.append(snippetFromJson(QString::fromUtf8(strs[i])));
    if (strs) rust_pm_free_strings(strs, len);
    return out;
}

QStringList RustSnippetStoreAdapter::prefixes() const
{
    QStringList out;
    if (!m_store) return out;
    size_t len = 0;
    char **strs = rust_snippet_store_prefixes(m_store, &len);
    for (size_t i = 0; i < len; ++i)
        out.append(QString::fromUtf8(strs[i]));
    if (strs) rust_pm_free_strings(strs, len);
    return out;
}

bool RustSnippetStoreAdapter::hasPrefix(const QString &prefix, const QString &language) const
{
    if (!m_store) return false;
    const QByteArray pBytes = prefix.toUtf8();
    const QByteArray lBytes = language.toUtf8();
    return rust_snippet_store_has_prefix(m_store, pBytes.constData(), lBytes.constData());
}

Snippet RustSnippetStoreAdapter::findForPrefix(const QString &prefix, const QString &language) const
{
    if (!m_store) return Snippet{};
    const QByteArray pBytes = prefix.toUtf8();
    const QByteArray lBytes = language.toUtf8();
    RustStr s(rust_snippet_store_find(m_store, pBytes.constData(), lBytes.constData()));
    return s.isNull() ? Snippet{} : snippetFromJson(s.value());
}

QString RustSnippetStoreAdapter::save() const
{
    if (!m_store) return QStringLiteral("[]");
    RustStr s(rust_snippet_store_save(m_store));
    return s.isNull() ? QStringLiteral("[]") : s.value();
}

void RustSnippetStoreAdapter::load(const QString &json)
{
    if (!m_store) return;
    const QByteArray bytes = json.toUtf8();
    rust_snippet_store_load(m_store, bytes.constData());
}

size_t RustSnippetStoreAdapter::importSnippets(const QString &json)
{
    if (!m_store) return 0;
    const QByteArray bytes = json.toUtf8();
    return rust_snippet_store_import(m_store, bytes.constData());
}

QString RustSnippetStoreAdapter::substituteVariables(const QString &body, const QVariantMap &vars)
{
    // Rust expects an array of [key, value] pairs.
    QJsonArray pairs;
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) {
        pairs.append(QJsonArray{ it.key(), it.value().toString() });
    }
    const QByteArray bodyBytes = body.toUtf8();
    const QByteArray varsBytes = QString::fromUtf8(
        QJsonDocument(pairs).toJson(QJsonDocument::Compact)).toUtf8();
    RustStr s(rust_snippet_substitute_variables(bodyBytes.constData(), varsBytes.constData()));
    return s.isNull() ? body : s.value();
}
