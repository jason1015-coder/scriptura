#include "gitblame.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>

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
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split('\n');

    int currentLine = 0;
    BlameLineInfo currentInfo;

    for (const QString &line : lines) {
        if (line.startsWith('\t')) {
            // This is the actual line content - record the blame info
            currentInfo.line = currentLine;
            m_blameData[currentLine] = currentInfo;
            currentLine++;
            currentInfo = BlameLineInfo(); // Reset for next line
            continue;
        }

        QStringList parts = line.split(' ');
        if (parts.isEmpty()) continue;

        // Header line: "<commit-hash> <original-line> <final-line> [<num-lines>]"
        if (parts.size() >= 3 && parts[0].length() == 40) {
            currentInfo.commitHash = parts[0];
            continue;
        }

        if (parts[0] == "author") {
            currentInfo.author = line.mid(7).trimmed();
        } else if (parts[0] == "author-time") {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(parts[1].toLongLong());
            currentInfo.date = dt.toString("yyyy-MM-dd hh:mm");
        } else if (parts[0] == "summary") {
            currentInfo.summary = line.mid(8).trimmed();
        }
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
