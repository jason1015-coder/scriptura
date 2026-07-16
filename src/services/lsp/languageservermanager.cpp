#include "languageservermanager.h"

#include <QJsonArray>

LanguageServerManager::LanguageServerManager(QObject *parent)
    : QObject(parent)
    , m_client(new LspClient(this))
{
    connect(m_client, &LspClient::diagnosticsReceived,
            this, &LanguageServerManager::diagnosticsReceived);
    connect(m_client, &LspClient::completionReceived,
            this, &LanguageServerManager::completionReceived);
    connect(m_client, &LspClient::definitionReceived,
            this, &LanguageServerManager::definitionReceived);
    connect(m_client, &LspClient::hoverReceived,
            this, &LanguageServerManager::hoverReceived);
    connect(m_client, &LspClient::documentSymbolReceived,
            this, &LanguageServerManager::documentSymbolReceived);
    connect(m_client, &LspClient::serverFailed,
            this, &LanguageServerManager::serverFailed);
    connect(m_client, &LspClient::logMessage,
            this, &LanguageServerManager::logMessage);
}

LanguageServerManager::~LanguageServerManager() = default;

bool LanguageServerManager::start(const QString &command,
                                  const QStringList &args,
                                  const QString &rootUri)
{
    return m_client->startServer(command, args, rootUri);
}

void LanguageServerManager::stop()
{
    m_client->stopServer();
}

bool LanguageServerManager::isRunning() const
{
    return m_client->isRunning();
}

void LanguageServerManager::handleRawMessage(const QByteArray &data)
{
    // The LspClient parser already guards against malformed JSON; this is the
    // single choke point where a bad server message is contained.
    m_client->processMessage(data);
}

void LanguageServerManager::didOpen(const QString &uri,
                                    const QString &languageId,
                                    const QString &text)
{
    m_client->didOpen(uri, languageId, text);
}

void LanguageServerManager::didChange(const QString &uri, const QString &text)
{
    m_client->didChange(uri, text);
}

void LanguageServerManager::didClose(const QString &uri)
{
    m_client->didClose(uri);
}

void LanguageServerManager::completion(const QString &uri, const LspClient::Position &pos)
{
    m_client->completion(uri, pos);
}

void LanguageServerManager::definition(const QString &uri, const LspClient::Position &pos)
{
    m_client->definition(uri, pos);
}

void LanguageServerManager::hover(const QString &uri, const LspClient::Position &pos)
{
    m_client->hover(uri, pos);
}

void LanguageServerManager::documentSymbol(const QString &uri)
{
    m_client->documentSymbol(uri);
}
