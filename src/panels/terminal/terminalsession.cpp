#include "terminal/terminalsession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

QProcessEnvironment baseEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Advertise the capabilities the emulator actually implements, so that
    // programs pick the colour depth and terminfo entry they can rely on.
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    return env;
}

} // namespace

TerminalSession::TerminalSession(QObject *parent)
    : QObject(parent)
{
}

TerminalSession::~TerminalSession()
{
    cleanup();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool TerminalSession::start(const QString &program, const QStringList &args, const QString &cwd,
                            const QProcessEnvironment &env, QString *error)
{
    if (isRunning())
        return true;

    QString effectiveCwd = cwd;
    if (effectiveCwd.isEmpty() || !QFileInfo(effectiveCwd).isDir())
        effectiveCwd = QDir::homePath();

    QProcessEnvironment merged = env.isEmpty() ? baseEnvironment() : env;
    for (const QString &key : baseEnvironment().keys())
        if (!merged.contains(key))
            merged.insert(key, baseEnvironment().value(key));

    m_program = program;
    m_workingDirectory = effectiveCwd;
    m_finished = false;
    m_exitCode = 0;
    m_terminating = false;
    m_pendingOutput.clear();
    m_pendingInput.clear();

    bool ok = false;
#ifdef Q_OS_UNIX
    ok = startPty(program, args, effectiveCwd, merged, error);
#else
    ok = startPipes(program, args, effectiveCwd, merged, error);
#endif
    if (!ok) {
        cleanup();
        m_finished = true;
        if (error && error->isEmpty())
            *error = tr("Failed to start terminal process.");
    }
    return ok;
}

bool TerminalSession::isRunning() const
{
#ifdef Q_OS_UNIX
    return m_pid > 0 && !m_finished;
#else
    return m_process && m_process->state() != QProcess::NotRunning;
#endif
}

int TerminalSession::processId() const
{
#ifdef Q_OS_UNIX
    return static_cast<int>(m_pid);
#else
    return m_process ? static_cast<int>(m_process->processId()) : 0;
#endif
}

void TerminalSession::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    emit titleChanged(m_title);
}

QString TerminalSession::defaultShell()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    const QString comspec = env.value(QStringLiteral("COMSPEC"));
    if (!comspec.isEmpty())
        return comspec;
    for (const QString &candidate : {QStringLiteral("powershell.exe"), QStringLiteral("cmd.exe")}) {
        const QString resolved = QStandardPaths::findExecutable(candidate);
        if (!resolved.isEmpty())
            return resolved;
    }
    return QStringLiteral("cmd.exe");
#else
    const QString shell = env.value(QStringLiteral("SHELL"));
    if (!shell.isEmpty() && QFileInfo::exists(shell))
        return shell;
    for (const QString &candidate : {QStringLiteral("/bin/bash"), QStringLiteral("/bin/sh")}) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QStringLiteral("/bin/sh");
#endif
}

QStringList TerminalSession::candidateShells()
{
    QStringList shells;
#ifdef Q_OS_WIN
    const QString comspec = QProcessEnvironment::systemEnvironment().value(QStringLiteral("COMSPEC"));
    if (!comspec.isEmpty())
        shells << comspec;
    shells << QStringLiteral("powershell.exe") << QStringLiteral("pwsh.exe")
           << QStringLiteral("cmd.exe");
#else
    const QString envShell = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SHELL"));
    if (!envShell.isEmpty() && QFileInfo::exists(envShell))
        shells << envShell;
    for (const QString &candidate : {QStringLiteral("/bin/bash"), QStringLiteral("/bin/zsh"),
                                     QStringLiteral("/bin/fish"), QStringLiteral("/bin/sh"),
                                     QStringLiteral("/usr/bin/fish"), QStringLiteral("/usr/bin/zsh")}) {
        if (QFileInfo::exists(candidate) && !shells.contains(candidate))
            shells << candidate;
    }
#endif
    if (shells.isEmpty())
        shells << defaultShell();
    return shells;
}

// ── Input / output ─────────────────────────────────────────────────────────

void TerminalSession::write(const QByteArray &data)
{
    if (data.isEmpty() || !isRunning())
        return;
    m_pendingInput.append(data);
    flushPending();
}

void TerminalSession::flushPending()
{
    if (m_pendingInput.isEmpty() || !isRunning())
        return;

#ifdef Q_OS_UNIX
    if (m_masterFd < 0)
        return;
    const qint64 written = ::write(m_masterFd, m_pendingInput.constData(), m_pendingInput.size());
    if (written > 0)
        m_pendingInput.remove(0, static_cast<int>(written));
    if (m_pendingInput.isEmpty()) {
        if (m_writeNotifier)
            m_writeNotifier->setEnabled(false);
    } else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && m_writeNotifier) {
        // The pty input buffer is full; retry when it drains.
        m_writeNotifier->setEnabled(true);
    }
#else
    if (m_process)
        m_process->write(m_pendingInput);
    m_pendingInput.clear();
#endif
}

void TerminalSession::onReadyRead()
{
#ifdef Q_OS_UNIX
    if (m_masterFd < 0)
        return;
    char buffer[8192];
    for (;;) {
        const ssize_t n = ::read(m_masterFd, buffer, sizeof(buffer));
        if (n > 0) {
            m_pendingOutput.append(buffer, static_cast<int>(n));
            // Emit in chunks so a chatty process cannot starve the event loop.
            if (m_pendingOutput.size() >= 16384) {
                emit dataReceived(m_pendingOutput);
                m_pendingOutput.clear();
            }
            continue;
        }
        if (n == 0) {
            // EIO or EOF: the child closed its side of the pty.
            flushPendingOutput();
            if (m_readNotifier)
                m_readNotifier->setEnabled(false);
            return;
        }
        if (errno == EINTR)
            continue;
        // EAGAIN is the normal end of a burst. Hand the bytes over now: a shell
        // that writes a prompt and then waits for input would otherwise stay
        // invisible until something filled the 16 KiB buffer.
        flushPendingOutput();
        return;
    }
#else
    if (!m_process)
        return;
    m_pendingOutput.append(m_process->readAllStandardOutput());
    if (!m_pendingOutput.isEmpty()) {
        emit dataReceived(m_pendingOutput);
        m_pendingOutput.clear();
    }
#endif
}

void TerminalSession::flushPendingOutput()
{
    if (m_pendingOutput.isEmpty())
        return;
    emit dataReceived(m_pendingOutput);
    m_pendingOutput.clear();
}

void TerminalSession::drainMasterFd()
{
#ifdef Q_OS_UNIX
    if (m_masterFd < 0)
        return;
    char buffer[8192];
    for (;;) {
        const ssize_t n = ::read(m_masterFd, buffer, sizeof(buffer));
        if (n > 0) {
            m_pendingOutput.append(buffer, static_cast<int>(n));
            if (m_pendingOutput.size() >= 16384)
                break;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        break;
    }
#endif
}

void TerminalSession::resize(int columns, int rows)
{
    columns = qMax(1, columns);
    rows = qMax(1, rows);
#ifdef Q_OS_UNIX
    if (m_masterFd < 0)
        return;
    struct winsize ws;
    ws.ws_col = static_cast<unsigned short>(columns);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    if (::ioctl(m_masterFd, TIOCSWINSZ, &ws) == 0 && m_pid > 0) {
        // Signals to the foreground process group, exactly as a real tty does.
        ::kill(-static_cast<pid_t>(m_pid), SIGWINCH);
    }
#else
    Q_UNUSED(columns);
    Q_UNUSED(rows);
#endif
}

// ── Termination ────────────────────────────────────────────────────────────

void TerminalSession::terminate()
{
    if (!isRunning())
        return;
    m_terminating = true;
#ifdef Q_OS_UNIX
    // The child is a session leader (setsid), so its process group id equals its
    // pid: signalling the group reaches every process it spawned.
    ::kill(-static_cast<pid_t>(m_pid), SIGTERM);
    if (m_killTimer)
        m_killTimer->start();
#else
    if (m_process)
        m_process->terminate();
#endif
}

void TerminalSession::kill()
{
#ifdef Q_OS_UNIX
    if (m_pid > 0 && !m_finished) {
        ::kill(-static_cast<pid_t>(m_pid), SIGKILL);
        return;
    }
#else
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        return;
    }
#endif
    if (!m_finished)
        finish(-1);
}

void TerminalSession::finish(int exitCode)
{
    if (m_finished)
        return;
    m_finished = true;
    m_exitCode = exitCode;
    if (!m_pendingOutput.isEmpty()) {
        emit dataReceived(m_pendingOutput);
        m_pendingOutput.clear();
    }
    if (m_readNotifier)
        m_readNotifier->setEnabled(false);
    if (m_writeNotifier)
        m_writeNotifier->setEnabled(false);
    if (m_reapTimer)
        m_reapTimer->stop();
    if (m_killTimer)
        m_killTimer->stop();
    emit finished(m_exitCode);
}

void TerminalSession::cleanup()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
    }
    if (m_reapTimer) {
        m_reapTimer->stop();
        delete m_reapTimer;
        m_reapTimer = nullptr;
    }
    if (m_killTimer) {
        m_killTimer->stop();
        delete m_killTimer;
        m_killTimer = nullptr;
    }

#ifdef Q_OS_UNIX
    if (m_pid > 0 && !m_finished) {
        ::kill(-static_cast<pid_t>(m_pid), SIGHUP);
        // Give the child a moment to exit on its own, then make sure it is gone.
        // The wait is bounded so closing the editor never blocks the UI thread.
        bool reaped = false;
        for (int i = 0; i < 20 && !reaped; ++i) {
            int status = 0;
            const pid_t result = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
            reaped = (result == m_pid) || (result < 0 && errno == ECHILD);
            if (!reaped)
                ::usleep(5000);
        }
        if (!reaped) {
            ::kill(-static_cast<pid_t>(m_pid), SIGKILL);
            int status = 0;
            ::waitpid(static_cast<pid_t>(m_pid), &status, 0);
        }
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
    if (m_slaveFd >= 0) {
        ::close(m_slaveFd);
        m_slaveFd = -1;
    }
    m_pid = -1;
#endif

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(500);
        }
        delete m_process;
        m_process = nullptr;
    }
}

// ── Unix pty backend ───────────────────────────────────────────────────────

#ifdef Q_OS_UNIX

bool TerminalSession::startPty(const QString &program, const QStringList &args,
                               const QString &cwd, const QProcessEnvironment &env, QString *error)
{
    m_backend = Backend::Pty;

    // Allocate the master/slave pair directly rather than through forkpty(), so
    // the child is created with plain fork()/exec() and the parent keeps full
    // control over the file descriptors.
    m_masterFd = ::open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (m_masterFd < 0) {
        if (error)
            *error = tr("Could not open /dev/ptmx: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    int unlock = 0;
    if (::ioctl(m_masterFd, TIOCSPTLCK, &unlock) < 0) {
        if (error)
            *error = tr("Could not unlock the pseudo terminal: %1")
                         .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    const char *slaveName = ::ptsname(m_masterFd);
    if (!slaveName) {
        if (error)
            *error = tr("Could not resolve the pseudo terminal slave: %1")
                         .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    const QByteArray slavePath(slaveName);
    m_slaveFd = ::open(slavePath.constData(), O_RDWR | O_NOCTTY);
    if (m_slaveFd < 0) {
        if (error)
            *error = tr("Could not open %1: %2")
                         .arg(QString::fromLatin1(slavePath))
                         .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
    ::fcntl(m_masterFd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK);

    // Block every signal across the fork so the child inherits a clean mask and
    // no handler can run between fork() and exec().
    sigset_t blockAll;
    sigset_t previous;
    ::sigfillset(&blockAll);
    ::sigprocmask(SIG_BLOCK, &blockAll, &previous);

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::sigprocmask(SIG_SETMASK, &previous, nullptr);
        if (error)
            *error = tr("fork() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    if (pid == 0) {
        // ── Child ─────────────────────────────────────────────────────────
        ::sigprocmask(SIG_SETMASK, &previous, nullptr);

        // New session, then claim the slave as the controlling terminal so
        // job control and ^C delivery work.
        if (::setsid() < 0)
            ::_exit(126);
        if (::ioctl(m_slaveFd, TIOCSCTTY, 0) < 0)
            ::_exit(126);

        // The pty must be the child's own process group, otherwise ^C from the
        // terminal would be delivered to the editor instead.
        ::setpgid(0, 0);

        if (::dup2(m_slaveFd, STDIN_FILENO) < 0 ||
            ::dup2(m_slaveFd, STDOUT_FILENO) < 0 ||
            ::dup2(m_slaveFd, STDERR_FILENO) < 0)
            ::_exit(126);
        if (m_masterFd > STDERR_FILENO)
            ::close(m_masterFd);
        if (m_slaveFd > STDERR_FILENO)
            ::close(m_slaveFd);

        ::signal(SIGPIPE, SIG_DFL);
        ::signal(SIGQUIT, SIG_DFL);
        ::signal(SIGINT, SIG_DFL);
        ::signal(SIGTERM, SIG_DFL);
        ::signal(SIGHUP, SIG_DFL);

        if (::chdir(cwd.toLocal8Bit().constData()) != 0)
            ::_exit(126);

        QList<QByteArray> envList;
        for (const QString &key : env.keys()) {
            envList.append(key.toLocal8Bit() + '=' + env.value(key).toLocal8Bit());
        }
        QList<QByteArray> rawArgs;
        rawArgs.append(program.toLocal8Bit());
        for (const QString &arg : args)
            rawArgs.append(arg.toLocal8Bit());

        // Build the char* arrays on the stack: nothing here may allocate after
        // the fork, so the lists are converted before exec.
        QVector<char *> argv;
        argv.reserve(rawArgs.size() + 1);
        for (QByteArray &arg : rawArgs)
            argv.append(arg.data());
        argv.append(nullptr);

        QVector<char *> envp;
        envp.reserve(envList.size() + 1);
        for (QByteArray &entry : envList)
            envp.append(entry.data());
        envp.append(nullptr);

        // execv() needs a path, and QFileInfo::absoluteFilePath() would resolve a
        // bare program name against the child's cwd instead of PATH.
        QString exePath = program;
        if (!exePath.contains(QLatin1Char('/'))) {
            const QString found = QStandardPaths::findExecutable(exePath);
            if (!found.isEmpty())
                exePath = found;
        }
        const QByteArray exe = exePath.toLocal8Bit();
        ::execv(exe.constData(), argv.data());
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    // ── Parent ────────────────────────────────────────────────────────────
    ::sigprocmask(SIG_SETMASK, &previous, nullptr);
    // Deliberately no setpgid() here: it would race the child's setsid(), which
    // fails with EPERM when the caller is already a process group leader, and
    // the child would die with exit code 126 before ever exec'ing.
    m_pid = pid;
    ::close(m_slaveFd);
    m_slaveFd = -1;

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &TerminalSession::onReadyRead);
    m_readNotifier->setEnabled(true);

    m_writeNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, [this]() {
        flushPending();
        if (m_pendingInput.isEmpty() && m_writeNotifier)
            m_writeNotifier->setEnabled(false);
    });
    m_writeNotifier->setEnabled(false);

    // SIGCHLD cannot be hooked without disturbing the rest of the application,
    // so the child is reaped by a short poll. 50 ms is below the threshold at
    // which an exit notice becomes noticeable.
    m_reapTimer = new QTimer(this);
    m_reapTimer->setInterval(50);
    connect(m_reapTimer, &QTimer::timeout, this, [this]() {
        if (m_pid <= 0 || m_finished)
            return;
        int status = 0;
        const pid_t result = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
        if (result == m_pid) {
            // The child can write and exit inside a single poll interval, so the
            // read notifier may never have fired for the bytes still sitting in
            // the pty buffer. Drain them before announcing the exit.
            drainMasterFd();
            int code = 0;
            if (WIFEXITED(status))
                code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                code = 128 + WTERMSIG(status);
            finish(code);
        } else if (result < 0 && errno == ECHILD) {
            finish(0);
        }
    });
    m_reapTimer->start();

    m_killTimer = new QTimer(this);
    m_killTimer->setSingleShot(true);
    m_killTimer->setInterval(2000);
    connect(m_killTimer, &QTimer::timeout, this, [this]() {
        if (!m_finished)
            kill();
    });

    emit started(processId());
    return true;
}

#endif // Q_OS_UNIX

// ── Windows pipe backend ───────────────────────────────────────────────────

#ifndef Q_OS_UNIX

void TerminalSession::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (!m_pendingOutput.isEmpty()) {
        emit dataReceived(m_pendingOutput);
        m_pendingOutput.clear();
    }
    finish(exitCode);
}

void TerminalSession::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        emit errorOccurred(tr("Could not start '%1'.").arg(m_program));
        finish(-1);
    }
}

bool TerminalSession::startPipes(const QString &program, const QStringList &args,
                                 const QString &cwd, const QProcessEnvironment &env, QString *error)
{
    m_backend = Backend::Pipes;

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(cwd);
    // stderr is merged into stdout so the single pty-like stream keeps the
    // interleaved order the program produced.
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalSession::onReadyRead);
    connect(m_process, &QProcess::finished, this, &TerminalSession::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &TerminalSession::onProcessError);

    m_process->start(program, args);
    if (!m_process->waitForStarted(3000)) {
        if (error)
            *error = m_process->errorString();
        return false;
    }
    emit started(processId());
    return true;
}

#endif // !Q_OS_UNIX
