#include "lspclient.h"

#include <QJsonParseError>
#include <QRegularExpression>
#include <QDebug>
#include <QTextDocument>
#include <QTextBlock>
#include <QApplication>
#include <QPalette>

LspClient::LspClient(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_requestId(1)
    , m_initialized(0)
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000); // 30 second timeout

    connect(m_process, &QProcess::readyReadStandardOutput, this, &LspClient::onProcessReadyRead);
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            this, &LspClient::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LspClient::onProcessFinished);
    connect(m_timeoutTimer, &QTimer::timeout, this, &LspClient::onTimeout);
}

LspClient::~LspClient()
{
    if (isRunning()) {
        shutdown();
        exit();
        m_process->waitForFinished(1000);
    }
}

bool LspClient::startServer(const QString &command, const QStringList &args,
                            const QString &rootUri)
{
    if (isRunning())
        return false;

    m_rootUri = rootUri;
    m_process->start(command, args);

    if (!m_process->waitForStarted(5000)) {
        emit serverFailed(tr("Failed to start language server: %1").arg(command));
        return false;
    }

    m_timeoutTimer->start();
    emit serverStarted();
    emit logMessage(tr("Language server started: %1").arg(command));
    return true;
}

void LspClient::stopServer()
{
    if (isRunning()) {
        shutdown();
        exit();
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

void LspClient::initialize(const QString &rootUri, const QString &languageId)
{
    m_rootUri = rootUri;
    m_currentLanguageId = languageId;

    QJsonObject params;
    params["processId"] = QCoreApplication::applicationPid();
    params["rootUri"] = rootUri;
    params["capabilities"] = QJsonObject{
        {"textDocument", QJsonObject{
            {"completion", QJsonObject{{"completionItem", QJsonObject{{"snippetSupport", false}}}}},
            {"hover", QJsonObject{{"contentFormat", QJsonArray{"markdown", "plaintext"}}}},
            {"definition", QJsonObject{}},
            {"references", QJsonObject{}},
            {"rename", QJsonObject{}},
            {"codeAction", QJsonObject{}}
        }},
        {"workspace", QJsonObject{
            {"workspaceFolders", true},
            {"applyEdit", true}
        }}
    };

    sendRequest("initialize", params, m_requestId);
    m_pendingRequests[m_requestId] = "initialize";
    m_requestId++;
}

void LspClient::initialized()
{
    sendNotification("initialized", QJsonObject{});
}

void LspClient::didOpen(const QString &uri, const QString &languageId, const QString &text)
{
    m_currentUri = uri;
    m_currentLanguageId = languageId;

    QJsonObject params;
    params["textDocument"] = QJsonObject{
        {"uri", uri},
        {"languageId", languageId},
        {"version", 1},
        {"text", text}
    };
    sendNotification("textDocument/didOpen", params);
}

void LspClient::didChange(const QString &uri, const QString &text)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{
        {"uri", uri},
        {"version", 2}
    };
    params["contentChanges"] = QJsonArray{
        QJsonObject{{"text", text}}
    };
    sendNotification("textDocument/didChange", params);
}

void LspClient::didClose(const QString &uri)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    sendNotification("textDocument/didClose", params);
}

void LspClient::shutdown()
{
    sendRequest("shutdown", QJsonObject{}, m_requestId);
    m_pendingRequests[m_requestId] = "shutdown";
    m_requestId++;
}

void LspClient::exit()
{
    sendNotification("exit", QJsonObject{});
}

void LspClient::completion(const QString &uri, const Position &pos)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", pos.line}, {"character", pos.character}};
    int id = m_requestId;
    sendRequest("textDocument/completion", params, id);
    m_pendingRequests[id] = "completion";
    m_requestId++;
}

void LspClient::definition(const QString &uri, const Position &pos)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", pos.line}, {"character", pos.character}};
    int id = m_requestId;
    sendRequest("textDocument/definition", params, id);
    m_pendingRequests[id] = "definition";
    m_requestId++;
}

void LspClient::hover(const QString &uri, const Position &pos)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", pos.line}, {"character", pos.character}};
    int id = m_requestId;
    sendRequest("textDocument/hover", params, id);
    m_pendingRequests[id] = "hover";
    m_requestId++;
}

void LspClient::references(const QString &uri, const Position &pos)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", pos.line}, {"character", pos.character}};
    int id = m_requestId;
    sendRequest("textDocument/references", params, id);
    m_pendingRequests[id] = "references";
    m_requestId++;
}

void LspClient::rename(const QString &uri, const Position &pos, const QString &newName)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", pos.line}, {"character", pos.character}};
    params["newName"] = newName;
    int id = m_requestId;
    sendRequest("textDocument/rename", params, id);
    m_pendingRequests[id] = "rename";
    m_requestId++;
}

void LspClient::codeAction(const QString &uri, const Range &range)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["range"] = QJsonObject{
        {"start", QJsonObject{{"line", range.start.line}, {"character", range.start.character}}},
        {"end", QJsonObject{{"line", range.end.line}, {"character", range.end.character}}}
    };
    int id = m_requestId;
    sendRequest("textDocument/codeAction", params, id);
    m_pendingRequests[id] = "codeAction";
    m_requestId++;
}

QList<LspClient::Diagnostic> LspClient::diagnosticsForFile(const QString &uri) const
{
    return m_diagnostics.value(uri);
}

void LspClient::clearDiagnostics(const QString &uri)
{
    m_diagnostics.remove(uri);
}

void LspClient::onProcessReadyRead()
{
    QMutexLocker locker(&m_mutex);
    m_buffer.append(m_process->readAllStandardOutput());
    locker.unlock();

    m_timeoutTimer->start();

    while (true) {
        QByteArray msg = readMessage();
        if (msg.isEmpty())
            break;
        processMessage(msg);
    }
}

void LspClient::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    emit serverFailed(tr("Language server error: %1").arg(m_process->errorString()));
    emit logMessage(tr("LSP Error: %1").arg(m_process->errorString()));
}

void LspClient::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
    m_timeoutTimer->stop();
    emit serverStopped();
    emit logMessage(tr("Language server stopped"));
}

void LspClient::onTimeout()
{
    emit logMessage(tr("Language server timeout - no response"));
}

void LspClient::sendMessage(const QJsonObject &msg)
{
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray header = QString("Content-Length: %1\r\n\r\n").arg(data.size()).toUtf8();
    m_process->write(header + data);
}

void LspClient::sendRequest(const QString &method, const QJsonObject &params, int id)
{
    sendMessage(createRequest(method, params, id));
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    sendMessage(createNotification(method, params));
}

void LspClient::processMessage(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        emit logMessage(tr("LSP JSON parse error: %1").arg(error.errorString()));
        return;
    }

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("id")) {
            // Response to our request
            QMutexLocker locker(&m_mutex);
            handleResponse(obj);
        } else if (obj.contains("method")) {
            // Notification from server (e.g., textDocument/publishDiagnostics)
            QString method = obj["method"].toString();
            if (method.startsWith("$/")) {
                // Custom request from server (ignore for now)
            } else {
                handleNotification(obj);
            }
        } else {
            // Unknown message type
            emit logMessage(tr("LSP: Received unknown message type"));
        }
    }
}

void LspClient::handleResponse(const QJsonObject &obj)
{
    int id = obj["id"].toInt();
    QString method;
    {
        QMutexLocker locker(&m_mutex);
        method = m_pendingRequests.take(id);
    }

    if (obj.contains("error") && !obj["error"].isNull()) {
        QJsonObject err = obj["error"].toObject();
        emit logMessage(tr("LSP error for %1: %2").arg(method, err["message"].toString()));
        return;
    }

    if (method == "initialize") {
        m_initialized = 1;
        emit initializedReceived();
    } else if (method == "shutdown") {
        m_initialized = 0;
    } else if (method == "completion") {
        QJsonArray items = obj["result"].toObject()["items"].toArray();
        emit completionReceived(items, id);
    } else if (method == "definition") {
        QJsonArray locations = obj["result"].toArray();
        emit definitionReceived(locations, id);
    } else if (method == "hover") {
        QJsonObject contents = obj["result"].toObject();
        emit hoverReceived(contents, id);
    } else if (method == "references") {
        QJsonArray locations = obj["result"].toArray();
        emit referencesReceived(locations, id);
    } else if (method == "rename") {
        QJsonObject edit = obj["result"].toObject();
        emit renameWorkspaceEdit(edit, id);
    } else if (method == "codeAction") {
        QJsonArray actions = obj["result"].toArray();
        emit codeActionReceived(actions, id);
    }
}

void LspClient::handleNotification(const QJsonObject &obj)
{
    QString method = obj["method"].toString();

    if (method == "textDocument/publishDiagnostics") {
        QJsonObject params = obj["params"].toObject();
        QString uri = params["uri"].toString();
        QJsonArray diagnostics = params["diagnostics"].toArray();

        QList<Diagnostic> diags;
        for (const QJsonValue &val : diagnostics) {
            diags.append(parseDiagnostic(val.toObject()));
        }

        QMutexLocker locker(&m_mutex);
        m_diagnostics[uri] = diags;
        locker.unlock();

        emit diagnosticsReceived(uri, diags);
    } else if (method == "window/logMessage") {
        QJsonObject params = obj["params"].toObject();
        emit logMessage(params["message"].toString());
    } else if (method == "window/showMessage") {
        QJsonObject params = obj["params"].toObject();
        emit logMessage(tr("LSP Message: %1").arg(params["message"].toString()));
    }
}

void LspClient::handleRequest(const QJsonObject &obj)
{
    // Server requests - handle if needed
    QString method = obj["method"].toString();
    int id = obj["id"].toInt();

    if (method == "window/workDoneProgress/create") {
        sendMessage(createResponse(id, QJsonObject{{"_supported", true}}));
    }
}

QByteArray LspClient::readMessage()
{
    QMutexLocker locker(&m_mutex);

    // Parse Content-Length header
    QByteArray headerEnd = "\r\n\r\n";
    int headerEndPos = m_buffer.indexOf(headerEnd);
    if (headerEndPos == -1)
        return QByteArray();

    QByteArray header = m_buffer.left(headerEndPos);
    m_buffer.remove(0, headerEndPos + headerEnd.size());

    // Extract Content-Length
    QRegularExpression re("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(header);
    if (!match.hasMatch())
        return QByteArray();

    int contentLength = match.captured(1).toInt();
    if (m_buffer.size() < contentLength)
        return QByteArray();

    QByteArray msg = m_buffer.left(contentLength);
    m_buffer.remove(0, contentLength);
    return msg;
}

QJsonObject LspClient::createRequest(const QString &method, const QJsonObject &params, int id)
{
    QJsonObject obj;
    obj["jsonrpc"] = "2.0";
    obj["id"] = id;
    obj["method"] = method;
    obj["params"] = params;
    return obj;
}

QJsonObject LspClient::createNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject obj;
    obj["jsonrpc"] = "2.0";
    obj["method"] = method;
    obj["params"] = params;
    return obj;
}

QJsonObject LspClient::createResponse(int id, const QJsonValue &result)
{
    QJsonObject obj;
    obj["jsonrpc"] = "2.0";
    obj["id"] = id;
    obj["result"] = result;
    return obj;
}

QJsonObject LspClient::createErrorResponse(int id, int code, const QString &message)
{
    QJsonObject obj;
    obj["jsonrpc"] = "2.0";
    obj["id"] = id;
    obj["error"] = QJsonObject{{"code", code}, {"message", message}};
    return obj;
}

LspClient::Diagnostic LspClient::parseDiagnostic(const QJsonObject &diag) const
{
    Diagnostic result;
    result.severity = static_cast<Diagnostic::Severity>(diag["severity"].toInt(static_cast<int>(Diagnostic::Error)));
    result.line = diag["range"].toObject()["start"].toObject()["line"].toInt();
    result.column = diag["range"].toObject()["start"].toObject()["character"].toInt();
    result.endLine = diag["range"].toObject()["end"].toObject()["line"].toInt();
    result.endColumn = diag["range"].toObject()["end"].toObject()["character"].toInt();
    result.message = diag["message"].toString();
    result.source = diag["source"].toString();
    result.code = diag["code"].toString();
    return result;
}

QList<QTextEdit::ExtraSelection> LspClient::createDiagnosticSelections(const Diagnostic &diag,
                                                                      const QTextDocument *doc) const
{
    QList<QTextEdit::ExtraSelection> selections;

    if (!doc)
        return selections;

    QTextEdit::ExtraSelection sel;
    QColor color;

    switch (diag.severity) {
    case Diagnostic::Error:
        color = QColor(255, 80, 80, 80); // Red
        break;
    case Diagnostic::Warning:
        color = QColor(255, 200, 50, 80); // Yellow/Orange
        break;
    case Diagnostic::Information:
        color = QColor(50, 150, 255, 60); // Blue
        break;
    case Diagnostic::Hint:
        color = QColor(150, 150, 150, 40); // Gray
        break;
    }

    sel.format.setUnderlineColor(color);
    sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);

    // Create selection for the diagnostic range
    QTextCursor cursor(const_cast<QTextDocument*>(doc));
    QTextBlock startBlock = doc->findBlockByNumber(diag.line);
    QTextBlock endBlock = doc->findBlockByNumber(diag.endLine);

    if (startBlock.isValid()) {
        cursor = QTextCursor(startBlock);
        cursor.setPosition(cursor.position() + diag.column);

        if (endBlock.isValid() && endBlock != startBlock) {
            cursor.setPosition(doc->findBlockByNumber(diag.endLine).position() + diag.endColumn, QTextCursor::KeepAnchor);
        } else {
            cursor.setPosition(cursor.position() + (diag.endColumn - diag.column), QTextCursor::KeepAnchor);
        }

        sel.cursor = cursor;
        selections.append(sel);
    }

    return selections;
}