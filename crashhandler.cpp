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

// Async-signal-safe crash dump writing for Linux/macOS
#ifdef Q_OS_LINUX
static void writeCrashDumpSafe(int sig) {
    void *buffer[100];
    int nptrs = backtrace(buffer, 100);
    char **strings = backtrace_symbols(buffer, nptrs);

    // Use async-signal-safe functions only
    const char *path = "/tmp/scriptura-crash.log";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *header = "Scriptura Crash Report\nStack trace:\n";
        write(fd, header, strlen(header));
        for (int i = 0; i < nptrs && strings; i++) {
            write(fd, strings[i], strlen(strings[i]));
            write(fd, "\n", 1);
        }
        close(fd);
    }
    if (strings) free(strings);

    // Restore default handler and re-raise
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

#ifdef Q_OS_MAC
static void writeCrashDumpSafe(int sig) {
    void *buffer[100];
    int nptrs = backtrace(buffer, 100);
    char **strings = backtrace_symbols(buffer, nptrs);

    const char *path = "/tmp/scriptura-crash.log";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *header = "Scriptura Crash Report\nStack trace:\n";
        write(fd, header, strlen(header));
        for (int i = 0; i < nptrs && strings; i++) {
            write(fd, strings[i], strlen(strings[i]));
            write(fd, "\n", 1);
        }
        close(fd);
    }
    if (strings) free(strings);

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