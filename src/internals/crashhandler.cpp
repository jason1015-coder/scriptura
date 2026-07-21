#include "crashhandler.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#ifdef Q_OS_MAC
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

QString CrashHandler::dumpPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/scriptura-crash.log";
}

// Async-signal-safe crash dump writing for Linux/macOS
//
// WARNING: backtrace_symbols() and free() are NOT async-signal-safe because
// they call malloc() internally.  We must use backtrace_symbols_fd() instead,
// which writes directly to a file descriptor without allocating memory.

#ifdef Q_OS_LINUX
static void writeCrashDumpSafe(int sig) {
    void *buffer[100];
    int nptrs = backtrace(buffer, 100);

    const char *path = "/tmp/scriptura-crash.log";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *header = "Scriptura Crash Report\nStack trace:\n";
        ssize_t written = write(fd, header, strlen(header));
        Q_UNUSED(written)

        // backtrace_symbols_fd() is async-signal-safe – it formats the
        // addresses directly into the fd without calling malloc.
        backtrace_symbols_fd(buffer, nptrs, fd);

        written = write(fd, "\n", 1);
        Q_UNUSED(written)
        close(fd);
    }

    // Restore default handler and re-raise
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

#ifdef Q_OS_MAC
static void writeCrashDumpSafe(int sig) {
    void *buffer[100];
    int nptrs = backtrace(buffer, 100);

    const char *path = "/tmp/scriptura-crash.log";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *header = "Scriptura Crash Report\nStack trace:\n";
        ssize_t written = write(fd, header, strlen(header));
        Q_UNUSED(written)

        backtrace_symbols_fd(buffer, nptrs, fd);

        written = write(fd, "\n", 1);
        Q_UNUSED(written)
        close(fd);
    }

    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

void CrashHandler::install()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS *exception) -> LONG {
        QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/scriptura-crash.dmp";
        HANDLE hFile = CreateFileA(path.toLocal8Bit().data(),
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei{};
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = exception;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                MiniDumpNormal, &mei, nullptr, nullptr);
            CloseHandle(hFile);
            qDebug() << "Crash dump written to:" << path;
        }
        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif

#ifdef Q_OS_LINUX
    signal(SIGSEGV, writeCrashDumpSafe);
    signal(SIGABRT, writeCrashDumpSafe);
    signal(SIGFPE, writeCrashDumpSafe);
    signal(SIGILL, writeCrashDumpSafe);
    signal(SIGBUS, writeCrashDumpSafe);
#endif

#ifdef Q_OS_MAC
    signal(SIGSEGV, writeCrashDumpSafe);
    signal(SIGABRT, writeCrashDumpSafe);
    signal(SIGFPE, writeCrashDumpSafe);
    signal(SIGILL, writeCrashDumpSafe);
    signal(SIGBUS, writeCrashDumpSafe);
#endif
}