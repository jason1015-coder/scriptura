#ifndef DEBUGSESSION_H
#define DEBUGSESSION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "dapclient.h"

/**
 * @brief Owns a DapClient and the debug-session lifecycle/state machine.
 *
 * Moves the debugger orchestration out of MainWindow into a single, testable
 * owner. It is the choke point that forwards raw DAP messages to the parser, so
 * a malformed debug-adapter frame cannot crash the editor.
 */
class DebugSession : public QObject
{
    Q_OBJECT
public:
    explicit DebugSession(QObject *parent = nullptr);
    ~DebugSession() override;

    bool start(const QString &command, const QStringList &args);
    void stop();
    bool isRunning() const;

    void initialize(const QString &program, const QStringList &args, const QString &cwd);
    void launch();
    void setBreakpoints(const QString &sourcePath, const QList<int> &lines);
    void continueDebug();
    void stepOver();
    void stepInto();
    void stepOut();

    /// Feed a raw (framed) DAP message into the parser. Public for testing.
    void handleRawMessage(const QByteArray &data);

    DapClient *client() const { return m_client; }

signals:
    void initialized();
    void stopped(const QString &reason);
    void continued();
    void stackTraceReceived(int threadId, const QList<DapClient::StackFrame> &frames);
    void scopesReceived(int frameId, const QList<DapClient::Scope> &scopes);
    void variablesReceived(int variablesReference, const QList<DapClient::Variable> &variables);
    void evaluationReceived(const QString &expression, const QString &result, const QString &context);
    void serverFailed(const QString &error);
    void logMessage(const QString &msg);

private:
    DapClient *m_client;
};

#endif // DEBUGSESSION_H
