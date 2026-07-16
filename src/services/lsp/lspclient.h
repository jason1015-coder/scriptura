#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTextEdit>
#include <QMap>
#include "lengthprefixedframer.h"

class LspClient : public QObject
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
        QList<QTextEdit::ExtraSelection> selections;
    };

    struct Position {
        int line;
        int character;
    };

    struct Range {
        Position start;
        Position end;
    };

    explicit LspClient(QObject *parent = nullptr);
    ~LspClient();

    bool startServer(const QString &command, const QStringList &args = QStringList(),
                     const QString &rootUri = QString());
    void stopServer();

    /// Parse a raw (framed) LSP message. Public so managers/tests can feed
    /// messages without spawning a real language server.
    void processMessage(const QByteArray &data);

    bool isRunning() const { return m_process && m_process->state() == QProcess::Running; }

    void initialize(const QString &rootUri, const QString &languageId);
    void initialized();
    void didOpen(const QString &uri, const QString &languageId, const QString &text);
    void didChange(const QString &uri, const QString &text);
    void didClose(const QString &uri);
    void shutdown();
    void exit();

    void completion(const QString &uri, const Position &pos);
    void definition(const QString &uri, const Position &pos);
    void hover(const QString &uri, const Position &pos);
    void references(const QString &uri, const Position &pos);
    void rename(const QString &uri, const Position &pos, const QString &newName);
    void codeAction(const QString &uri, const Range &range);
    void documentSymbol(const QString &uri);
    void workspaceSymbol(const QString &query);
    void formatting(const QString &uri, const QJsonObject &options = {});
    void rangeFormatting(const QString &uri, const Range &range);
    void signatureHelp(const QString &uri, const Position &pos);
    void declaration(const QString &uri, const Position &pos);
    void typeDefinition(const QString &uri, const Position &pos);
    void implementation(const QString &uri, const Position &pos);

    QList<Diagnostic> diagnosticsForFile(const QString &uri) const;
    void clearDiagnostics(const QString &uri);

    struct SymbolInformation {
        QString name;
        QString kind;
        QString containerName;
        Range range;
    };

    struct InlayHint {
        Position position;
        QString label;
        bool paddingLeft;
        bool paddingRight;
    };

signals:
    void serverStarted();
    void serverStopped();
    void serverFailed(const QString &error);

    void initializedReceived();
    void diagnosticsReceived(const QString &uri, const QList<Diagnostic> &diagnostics);
    void completionReceived(const QJsonArray &items, int requestId);
    void definitionReceived(const QJsonArray &locations, int requestId);
    void hoverReceived(const QJsonObject &contents, int requestId);
    void referencesReceived(const QJsonArray &locations, int requestId);
    void renameWorkspaceEdit(const QJsonObject &edit, int requestId);
    void codeActionReceived(const QJsonArray &actions, int requestId);
    void documentSymbolReceived(const QList<SymbolInformation> &symbols);
    void workspaceSymbolReceived(const QList<SymbolInformation> &symbols);
    void formattingReceived(const QString &uri, const QJsonArray &edits);
    void signatureHelpReceived(const QJsonObject &help, int requestId);
    void declarationReceived(const QJsonArray &locations, int requestId);
    void typeDefinitionReceived(const QJsonArray &locations, int requestId);
    void implementationReceived(const QJsonArray &locations, int requestId);

    void logMessage(const QString &msg);

private slots:
    void onProcessReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTimeout();

 private:
    void sendMessage(const QJsonObject &msg);
    void sendRequest(const QString &method, const QJsonObject &params, int id);
    void sendNotification(const QString &method, const QJsonObject &params);
    void handleResponse(const QJsonObject &obj);
    void handleNotification(const QJsonObject &obj);
    void handleRequest(const QJsonObject &obj);

    QJsonObject createRequest(const QString &method, const QJsonObject &params, int id);
    QJsonObject createNotification(const QString &method, const QJsonObject &params);
    QJsonObject createResponse(int id, const QJsonValue &result);
    QJsonObject createErrorResponse(int id, int code, const QString &message);

    Diagnostic parseDiagnostic(const QJsonObject &diag) const;
public:
    QList<QTextEdit::ExtraSelection> createDiagnosticSelections(const Diagnostic &diag,
                                                                const QTextDocument *doc) const;

    QProcess *m_process;
    LengthPrefixedFramer *m_framer;
    int m_requestId;
    int m_initialized;
    QString m_rootUri;
    QString m_currentUri;
    QString m_currentLanguageId;

    QMap<QString, QList<Diagnostic>> m_diagnostics;
    QMap<int, QString> m_pendingRequests;
    QTimer *m_timeoutTimer;
    QString m_lastFormattingUri;
};

#endif // LSPCLIENT_H
