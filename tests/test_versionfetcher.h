#ifndef TEST_VERSIONFETCHER_H
#define TEST_VERSIONFETCHER_H

#include <QObject>

class TestVersionFetcher : public QObject
{
    Q_OBJECT
private slots:
    void testCoreVersion();
};

#endif
