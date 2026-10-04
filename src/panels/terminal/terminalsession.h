#ifndef SCRIPTURA_TERMINALSESSION_H
#define SCRIPTURA_TERMINALSESSION_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class QSocketNotifier;
class QTimer;

/**
 * A child process attached to a pseudo terminal.
 *
 * On Unix the child is started on a real pty (openpty via /dev/ptmx), so
 * isatty() is true, colours and line editing work, job control and curses
 * programs run, and the terminal size can be pushed with TIOCSWINSZ.
 *
 * Windows has no pty API reachable from Qt Widgets, so the session falls back to
 * QProcess pipes running cmd.exe / PowerShell. Output still renders, but child
 * programs see a pipe rather than a console: no colours, no readline, no curses.
 */
class TerminalSession : public QObject
{
    Q_OBJECT
public:
    enum class Backend {
        Pty,     ///< real pseudo terminal (Unix)
        Pipes,   ///< QProcess with pipes (Windows fallback)
    };

    explicit TerminalSession(QObject *parent = nullptr);
    ~TerminalSession() override;

    /**
     * Starts the child on a pty (or as a piped process).
     *
     * @param program  executable, usually the user's login shell
     * @param args     arguments; a login shell is started interactively
     * @param cwd      working directory; falls back to the home directory
     * @param env      extra environment variables layered on top of the editor's
     * @param error    set to a human-readable message when the call fails
     */
    bool start(const QString &program, const QStringList &args, const QString &cwd,
               const QProcessEnvironment &env, QString *error = nullptr);

    /// Sends bytes to the child. Safe to call from the GUI thread.
    void write(const QByteArray &data);

    /// Pushes the window size to the child (TIOCSWINSZ + SIGWINCH on Unix).
    void resize(int columns, int rows);

    /// Asks the child to quit: SIGTERM to its process group on Unix.
    void terminate();

    /// Kills the child immediately: SIGKILL to its process group on Unix.
    void kill();

    bool isRunning() const;
    Backend backend() const { return m_backend; }
    bool isPty() const { return m_backend == Backend::Pty; }
    int processId() const;
    QString title() const { return m_title; }
    int exitCode() const { return m_exitCode; }
    bool hasExited() const { return m_finished; }
    QString program() const { return m_program; }
    QString workingDirectory() const { return m_workingDirectory; }

    /// The user's login shell, or a sensible platform default.
    static QString defaultShell();

    /// Shells offered in the panel's shell picker.
    static QStringList candidateShells();

    void setTitle(const QString &title);

signals:
    void dataReceived(QByteArray data);
    void started(int pid);
    void finished(int exitCode);
    void titleChanged(const QString &title);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    // Only the pipe backend (Windows) reports through QProcess; the pty backend
    // polls waitpid() itself, so these must not exist elsewhere — moc emits a
    // reference to every declared slot.
#ifndef Q_OS_UNIX
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
#endif

private:
    bool startPty(const QString &program, const QStringList &args, const QString &cwd,
                  const QProcessEnvironment &env, QString *error);
    bool startPipes(const QString &program, const QStringList &args, const QString &cwd,
                    const QProcessEnvironment &env, QString *error);
    void flushPending();
    void flushPendingOutput();
    void drainMasterFd();
    void finish(int exitCode);
    void cleanup();

    Backend m_backend = Backend::Pty;

    // Unix
    int m_masterFd = -1;
    int m_slaveFd = -1;
    qint64 m_pid = -1;

    // Windows fallback
    QProcess *m_process = nullptr;

    QByteArray m_pendingOutput;
    QByteArray m_pendingInput;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer *m_reapTimer = nullptr;
    QTimer *m_killTimer = nullptr;

    bool m_finished = true;
    bool m_terminating = false;
    int m_exitCode = 0;
    QString m_program;
    QString m_workingDirectory;
    QString m_title;
};

#endif // SCRIPTURA_TERMINALSESSION_H
