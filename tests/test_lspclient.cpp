#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "lspclient.h"
#include "test_lspclient.h"

static QByteArray raw(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void TestLspClient::testPublishDiagnosticsEmitsSignal()
{
    LspClient client;
    QSignalSpy spy(&client, &LspClient::diagnosticsReceived);

    QJsonObject diag;
    diag["range"] = QJsonObject{
        {"start", QJsonObject{{"line", 1}, {"character", 0}}},
        {"end", QJsonObject{{"line", 1}, {"character", 5}}},
    };
    diag["severity"] = 1;
    diag["message"] = "syntax error";

    QJsonObject msg;
    msg["method"] = "textDocument/publishDiagnostics";
    msg["params"] = QJsonObject{
        {"uri", "file:///a.cpp"},
        {"diagnostics", QJsonArray{diag}},
    };

    client.processMessage(raw(msg));

    QCOMPARE(spy.count(), 1);
    QList<LspClient::Diagnostic> diags = spy.first().at(1).value<QList<LspClient::Diagnostic>>();
    QCOMPARE(diags.size(), 1);
    QCOMPARE(diags.first().message, QString("syntax error"));
    QCOMPARE(diags.first().severity, LspClient::Diagnostic::Error);
}

void TestLspClient::testMalformedJsonDoesNotCrash()
{
    LspClient client;
    QSignalSpy logSpy(&client, &LspClient::logMessage);

    // Truncated / invalid JSON must be contained, not crash the host.
    client.processMessage(QByteArray("{not valid json"));

    QVERIFY(logSpy.count() >= 1);
}

void TestLspClient::testUnknownMessageTypeIsIgnored()
{
    LspClient client;
    QSignalSpy diagSpy(&client, &LspClient::diagnosticsReceived);
    QSignalSpy logSpy(&client, &LspClient::logMessage);

    QJsonObject msg;
    msg["id"] = 7;                 // has id but neither result nor error nor method
    msg["someField"] = "x";

    client.processMessage(raw(msg));

    QCOMPARE(diagSpy.count(), 0); // nothing dispatched
    QVERIFY(logSpy.count() >= 1); // logged as unknown
}
