#ifndef GITBLAME_H
#define GITBLAME_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QProcess>
#include <QTimer>

class QPlainTextEdit;

/**
 * Represents blame info for a single line.
 */
struct BlameLineInfo {
    QString commitHash;
    QString author;
    QString date;
    QString summary;    // Commit message (first line)
    int line;           // 0-based line number
};

/**
 * Provides Git blame annotations for editor lines.
 * Runs `git blame --porcelain` and parses the output.
 */
class GitBlame : public QObject
{
    Q_OBJECT
public:
    explicit GitBlame(QObject *parent = nullptr);

    // Request blame info for a file
    void requestBlame(const QString &filePath);

    // Get blame info for a specific line (0-based)
    BlameLineInfo blameForLine(int line) const;

    // Get all blame data for a file
    QList<BlameLineInfo> blameForFile() const;

    // Check if blame data is available for a file
    bool hasBlameData() const { return !m_blameData.isEmpty(); }

    // Current file being blamed
    QString currentFile() const { return m_currentFile; }

    // Clear blame data
    void clear();

    // Enable/disable blame display
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

signals:
    void blameReceived(const QString &filePath);
    void blameFailed(const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseBlameOutput(const QByteArray &output);

    QProcess *m_process;
    QString m_currentFile;
    QMap<int, BlameLineInfo> m_blameData;  // line -> info
    bool m_enabled = true;
    bool m_pending = false;
};

#endif // GITBLAME_H
