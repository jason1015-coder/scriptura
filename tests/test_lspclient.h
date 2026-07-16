#ifndef TEST_LSPCLIENT_H
#define TEST_LSPCLIENT_H

#include <QObject>

class TestLspClient : public QObject
{
    Q_OBJECT
private slots:
    void testPublishDiagnosticsEmitsSignal();
    void testMalformedJsonDoesNotCrash();
    void testUnknownMessageTypeIsIgnored();
};

#endif // TEST_LSPCLIENT_H
