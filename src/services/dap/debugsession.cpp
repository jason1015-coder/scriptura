#include "debugsession.h"

DebugSession::DebugSession(QObject *parent)
    : QObject(parent)
    , m_client(new DapClient(this))
{
    connect(m_client, &DapClient::initialized, this, &DebugSession::initialized);
    connect(m_client, &DapClient::stopped, this, &DebugSession::stopped);
    connect(m_client, &DapClient::continued, this, &DebugSession::continued);
    connect(m_client, &DapClient::stackTraceReceived,
            this, &DebugSession::stackTraceReceived);
    connect(m_client, &DapClient::scopesReceived, this, &DebugSession::scopesReceived);
    connect(m_client, &DapClient::variablesReceived,
            this, &DebugSession::variablesReceived);
    connect(m_client, &DapClient::evaluationReceived,
            this, &DebugSession::evaluationReceived);
    connect(m_client, &DapClient::serverFailed, this, &DebugSession::serverFailed);
    connect(m_client, &DapClient::logMessage, this, &DebugSession::logMessage);
}

DebugSession::~DebugSession() = default;

bool DebugSession::start(const QString &command, const QStringList &args)
{
    return m_client->startServer(command, args);
}

void DebugSession::stop()
{
    m_client->stopServer();
}

bool DebugSession::isRunning() const
{
    return m_client->isRunning();
}

void DebugSession::initialize(const QString &program,
                              const QStringList &args,
                              const QString &cwd)
{
    m_client->initialize(program, args, cwd);
}

void DebugSession::launch()
{
    m_client->launch();
}

void DebugSession::setBreakpoints(const QString &sourcePath, const QList<int> &lines)
{
    m_client->setBreakpoints(sourcePath, lines);
}

void DebugSession::continueDebug()
{
    m_client->continueDebug();
}

void DebugSession::stepOver()
{
    m_client->next();
}

void DebugSession::stepInto()
{
    m_client->stepIn();
}

void DebugSession::stepOut()
{
    m_client->stepOut();
}

void DebugSession::handleRawMessage(const QByteArray &data)
{
    m_client->processMessage(data);
}
