#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QString>

class CrashHandler
{
public:
    static void install();
    static QString dumpPath();
};

#endif // CRASHHANDLER_H