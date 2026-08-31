#include "gitblame.h"
#include "rust_adapter.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

GitBlame::GitBlame(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitBlame::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GitBlame::onProcessError);
}

void GitBlame::requestBlame(const QString &filePath)
{
    if (!m_enabled || filePath.isEmpty()) return;

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    m_currentFile = filePath;
    m_blameData.clear();

    QString dir = QFileInfo(filePath).absolutePath();
    m_process->setWorkingDirectory(dir);

    QStringList args;
    args << "blame" << "--porcelain" << QFileInfo(filePath).fileName();
    m_process->start("git", args);
}

void GitBlame::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        emit blameFailed(m_process->errorString());
        return;
    }

    QByteArray output = m_process->readAllStandardOutput();
    parseBlameOutput(output);
    emit blameReceived(m_currentFile);
}

void GitBlame::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::Crashed) {
        emit blameFailed(m_process->errorString());
    }
}

void GitBlame::parseBlameOutput(const QByteArray &output)
{
    m_blameData.clear();
    if (output.isEmpty()) return;

    // Delegate porcelain parsing to the Rust blame engine.
    char *json = rust_blame_parse(output.constData());
    if (!json) return;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json));
    rust_free_string(json);

    if (!doc.isArray()) return;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject o = v.toObject();
        BlameLineInfo info;
        info.commitHash = o["commitHash"].toString();
        info.author = o["author"].toString();
        info.date = o["date"].toString();
        info.summary = o["summary"].toString();
        info.line = o["line"].toInt();
        m_blameData[info.line] = info;
    }
}

BlameLineInfo GitBlame::blameForLine(int line) const
{
    return m_blameData.value(line);
}

QList<BlameLineInfo> GitBlame::blameForFile() const
{
    QList<BlameLineInfo> result;
    auto it = m_blameData.constBegin();
    while (it != m_blameData.constEnd()) {
        result.append(it.value());
        ++it;
    }
    return result;
}

void GitBlame::clear()
{
    m_blameData.clear();
    m_currentFile.clear();
}
