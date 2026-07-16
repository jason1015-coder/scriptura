#ifndef LANGUAGESERVERMANAGER_H
#define LANGUAGESERVERMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "lspclient.h"

/**
 * @brief Owns an LspClient and exposes a domain-level API to the rest of the app.
 *
 * Decoupling the editor/UI from the raw LSP protocol client reduces the size of
 * MainWindow and gives the language-server integration a single, testable owner.
 * It also centralizes malformed-message handling so a buggy language server
 * cannot crash the host application.
 */
class LanguageServerManager : public QObject
{
    Q_OBJECT
public:
    explicit LanguageServerManager(QObject *parent = nullptr);
    ~LanguageServerManager() override;

    bool start(const QString &command, const QStringList &args, const QString &rootUri);
    void stop();

    bool isRunning() const;

    /// Feed a raw (framed) LSP message into the parser. Public so tests can
    /// exercise the parser without spawning a real language server.
    void handleRawMessage(const QByteArray &data);

    // Domain passthroughs
    void didOpen(const QString &uri, const QString &languageId, const QString &text);
    void didChange(const QString &uri, const QString &text);
    void didClose(const QString &uri);
    void completion(const QString &uri, const LspClient::Position &pos);
    void definition(const QString &uri, const LspClient::Position &pos);
    void hover(const QString &uri, const LspClient::Position &pos);
    void documentSymbol(const QString &uri);

    LspClient *client() const { return m_client; }

signals:
    void diagnosticsReceived(const QString &uri, const QList<LspClient::Diagnostic> &diagnostics);
    void completionReceived(const QJsonArray &items, int requestId);
    void definitionReceived(const QJsonArray &locations, int requestId);
    void hoverReceived(const QJsonObject &contents, int requestId);
    void documentSymbolReceived(const QList<LspClient::SymbolInformation> &symbols);
    void serverFailed(const QString &error);
    void logMessage(const QString &msg);

private:
    LspClient *m_client;
};

#endif // LANGUAGESERVERMANAGER_H
