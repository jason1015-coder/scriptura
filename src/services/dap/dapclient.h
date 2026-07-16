#ifndef DAPCLIENT_H
#define DAPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include "lengthprefixedframer.h"

class DapClient : public QObject
{
    Q_OBJECT
public:
    struct Position {
        int line;
        int character;
    };

    struct Range {
        Position start;
        Position end;
    };

    struct Source {
        QString name;
        QString path;
        int sourceReference = 0;
    };

    struct Breakpoint {
        int id = 0;
        bool verified = false;
        int line = 0;
        QString message;
        QString condition;
        QString hitCondition;
        QString logMessage;
    };

    struct StackFrame {
        int id = 0;
        QString name;
        int line = 0;
        Source source;
    };

    struct Scope {
        QString name;
        int variablesReference = 0;
        Range variables;
    };

    struct Variable {
        QString name;
        QString value;
        QString type;
        int variablesReference = 0;
    };

    explicit DapClient(QObject *parent = nullptr);
    ~DapClient();

    bool startServer(const QString &command, const QStringList &args = QStringList());
    void stopServer();
    bool isRunning() const { return m_process && m_process->state() == QProcess::Running; }
    int currentThreadId() const { return m_currentThreadId; }

    /// Parse a raw (framed) DAP message. Public so managers/tests can feed
    /// messages without spawning a real debug adapter.
    void processMessage(const QByteArray &data);

    void initialize(const QString &program, const QStringList &args, const QString &cwd);
    void launch();
    void configurationDone();
    void setBreakpoints(const QString &sourcePath, const QList<int> &lines);
    void setBreakpointsWithConditions(const QString &sourcePath, const QList<Breakpoint> &breakpoints);
    void continueDebug();
    void next();
    void stepIn();
    void stepOut();
    void pause();
    void disconnect();
    void stackTrace(int threadId);
    void scopes(int frameId);
    void variables(int variablesReference);
    void evaluate(const QString &expression, int frameId, const QString &context = "repl");

signals:
    void serverStarted();
    void serverStopped();
    void serverFailed(const QString &error);
    void initialized();
    void stopped(const QString &reason);
    void continued();
    void breakpointUpdated(const QString &sourcePath, const QList<Breakpoint> &breakpoints);
    void stackTraceReceived(int threadId, const QList<StackFrame> &frames);
    void scopesReceived(int frameId, const QList<Scope> &scopes);
    void variablesReceived(int variablesReference, const QList<Variable> &variables);
    void evaluationReceived(const QString &expression, const QString &result, const QString &context = QString());
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
    void processEvent(const QJsonObject &event);

    QJsonObject createRequest(const QString &method, const QJsonObject &params, int id);
    QJsonObject createNotification(const QString &method, const QJsonObject &params);
    QJsonObject createResponse(int id, const QJsonValue &result);
    QJsonObject createErrorResponse(int id, int code, const QString &message);

    Breakpoint parseBreakpoint(const QJsonObject &obj) const;
    StackFrame parseStackFrame(const QJsonObject &obj) const;
    Scope parseScope(const QJsonObject &obj) const;
    Variable parseVariable(const QJsonObject &obj) const;

private:
    QProcess *m_process;
    LengthPrefixedFramer *m_framer;
    int m_requestId;
    int m_seq;
    QTimer *m_timeoutTimer;
    QString m_program;
    QStringList m_args;
    QString m_cwd;
    int m_currentThreadId;
    bool m_configurationDoneSent;
};

#endif // DAPCLIENT_H
