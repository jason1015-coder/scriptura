#ifndef TEST_DAPCLIENT_H
#define TEST_DAPCLIENT_H

#include <QObject>

class TestDapClient : public QObject
{
    Q_OBJECT
private slots:
    void testInitializeResponseEmitsInitialized();
    void testStackTraceResponseEmitsFrames();
    void testMalformedJsonDoesNotCrash();
};

#endif // TEST_DAPCLIENT_H
