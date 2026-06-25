#include "crashhandler.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef Q_OS_MAC
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

static QString getDumpPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    return dir + "/scriptura-crash-" + timestamp + ".dmp";
}

void CrashHandler::install()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS *exception) -> LONG {
        QString path = getDumpPath();
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
        return EXCEPTION_CONTINUE_SEARCH;
    });
#endif

#ifdef Q_OS_LINUX
    signal(SIGSEGV, [](int) {
        QString path = getDumpPath();
        void *buffer[100];
        int nptrs = backtrace(buffer, 100);
        char **strings = backtrace_symbols(buffer, nptrs);
        
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "Scriptura Crash Report\n";
            out << "Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
            out << "Stack trace:\n";
            for (int i = 0; i < nptrs; i++) {
                out << strings[i] << "\n";
            }
            file.close();
        }
        if (strings) free(strings);
        exit(1);
    });
#endif

#ifdef Q_OS_MAC
    signal(SIGSEGV, [](int) {
        QString path = getDumpPath();
        void *buffer[100];
        int nptrs = backtrace(buffer, 100);
        char **strings = backtrace_symbols(buffer, nptrs);
        
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "Scriptura Crash Report\n";
            out << "Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
            out << "Stack trace:\n";
            for (int i = 0; i < nptrs; i++) {
                out << strings[i] << "\n";
            }
            file.close();
        }
        if (strings) free(strings);
        exit(1);
    });
#endif
}