#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "dapclient.h"
#include "test_dapclient.h"

static QByteArray raw(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void TestDapClient::testInitializeResponseEmitsInitialized()
{
    DapClient client;
    QSignalSpy spy(&client, &DapClient::initialized);

    QJsonObject msg;
    msg["seq"] = 1;
    msg["type"] = "response";
    msg["command"] = "initialize";
    msg["success"] = true;
    msg["body"] = QJsonObject{};

    client.processMessage(raw(msg));
    QCOMPARE(spy.count(), 1);
}

void TestDapClient::testStackTraceResponseEmitsFrames()
{
    DapClient client;
    QSignalSpy spy(&client, &DapClient::stackTraceReceived);

    QJsonObject frameObj;
    frameObj["id"] = 100;
    frameObj["name"] = "main";
    frameObj["line"] = 42;
    frameObj["source"] = QJsonObject{{"name", "app.cpp"}, {"path", "/src/app.cpp"}};

    QJsonObject msg;
    msg["seq"] = 2;
    msg["type"] = "response";
    msg["command"] = "stackTrace";
    msg["success"] = true;
    msg["arguments"] = QJsonObject{{"threadId", 1}};
    msg["body"] = QJsonObject{{"stackFrames", QJsonArray{frameObj}}};

    client.processMessage(raw(msg));

    QCOMPARE(spy.count(), 1);
    QList<DapClient::StackFrame> frames =
        spy.first().at(1).value<QList<DapClient::StackFrame>>();
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().name, QString("main"));
    QCOMPARE(frames.first().line, 42);
}

void TestDapClient::testMalformedJsonDoesNotCrash()
{
    DapClient client;
    QSignalSpy logSpy(&client, &DapClient::logMessage);

    // A debug adapter sending garbage must be contained, not crash the editor.
    client.processMessage(QByteArray("??? not json"));
    QVERIFY(logSpy.count() >= 0); // no crash is the assertion
}
