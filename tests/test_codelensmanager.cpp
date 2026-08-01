#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonObject>
#include "codelensmanager.h"
#include "test_codelensmanager.h"

namespace {
QJsonObject makeLens(int line, int col, const QString &title, const QString &command)
{
    QJsonObject start;
    start["line"] = line;
    start["character"] = col;
    QJsonObject range;
    range["start"] = start;
    QJsonObject cmd;
    cmd["title"] = title;
    cmd["command"] = command;
    cmd["arguments"] = QJsonArray();
    QJsonObject lens;
    lens["range"] = range;
    lens["command"] = cmd;
    return lens;
}
}

void TestCodeLensManager::testInitialState()
{
    CodeLensManager mgr;
    QVERIFY(mgr.isEnabled());
    QVERIFY(mgr.itemsForDocument("file:///a.cpp").isEmpty());
}

void TestCodeLensManager::testSetEnabledOffClears()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(0, 0, "1 ref", "doIt"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///a.cpp"));
    QCOMPARE(mgr.itemsForDocument("file:///a.cpp").size(), 1);

    mgr.setEnabled(false);
    QVERIFY(!mgr.isEnabled());
    QVERIFY(mgr.itemsForDocument("file:///a.cpp").isEmpty());
}

void TestCodeLensManager::testSetEnabledSameValue()
{
    CodeLensManager mgr;
    mgr.setEnabled(true); // already enabled: no-op
    QVERIFY(mgr.isEnabled());
}

void TestCodeLensManager::testClearAll()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(0, 0, "1 ref", "doIt"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///a.cpp"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///b.cpp"));
    mgr.clearAll();
    QVERIFY(mgr.itemsForDocument("file:///a.cpp").isEmpty());
    QVERIFY(mgr.itemsForDocument("file:///b.cpp").isEmpty());
}

void TestCodeLensManager::testClearDocument()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(0, 0, "1 ref", "doIt"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///a.cpp"));
    QSignalSpy spy(&mgr, &CodeLensManager::codeLensUpdated);
    mgr.clearDocument("file:///a.cpp");
    QVERIFY(mgr.itemsForDocument("file:///a.cpp").isEmpty());
    QCOMPARE(spy.count(), 1);
}

void TestCodeLensManager::testClearDocumentNonexistent()
{
    CodeLensManager mgr;
    QSignalSpy spy(&mgr, &CodeLensManager::codeLensUpdated);
    mgr.clearDocument("file:///missing.cpp");
    QCOMPARE(spy.count(), 0);
}

void TestCodeLensManager::testItemsAtLine()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(3, 0, "a", "cmdA"));
    arr.append(makeLens(3, 5, "b", "cmdB"));
    arr.append(makeLens(9, 0, "c", "cmdC"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///a.cpp"));
    QList<CodeLensItem> items = mgr.itemsAtLine("file:///a.cpp", 3);
    QCOMPARE(items.size(), 2);
}

void TestCodeLensManager::testItemsAtLineNone()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(3, 0, "a", "cmdA"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///a.cpp"));
    QVERIFY(mgr.itemsAtLine("file:///a.cpp", 99).isEmpty());
}

void TestCodeLensManager::testRequestCodeLensDisabled()
{
    CodeLensManager mgr;
    mgr.setEnabled(false);
    // Should early-return without touching anything
    mgr.requestCodeLens(nullptr, nullptr, "file:///a.cpp");
    QVERIFY(mgr.itemsForDocument("file:///a.cpp").isEmpty());
}

void TestCodeLensManager::testParseCodeLensFull()
{
    CodeLensManager mgr;
    QJsonArray arr;
    QJsonObject start;
    start["line"] = 7;
    start["character"] = 2;
    QJsonObject range;
    range["start"] = start;
    QJsonObject cmd;
    cmd["title"] = "3 references";
    cmd["command"] = "editor.action.showReferences";
    QJsonArray args;
    QJsonObject arg;
    arg["uri"] = "file:///x.cpp";
    args.append(arg);
    cmd["arguments"] = args;
    QJsonObject lens;
    lens["range"] = range;
    lens["command"] = cmd;
    arr.append(lens);

    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///x.cpp"));
    QList<CodeLensItem> items = mgr.itemsForDocument("file:///x.cpp");
    QCOMPARE(items.size(), 1);
    const CodeLensItem &item = items.first();
    QCOMPARE(item.line, 7);
    QCOMPARE(item.column, 2);
    QCOMPARE(item.title, QString("3 references"));
    QCOMPARE(item.command, QString("editor.action.showReferences"));
}

void TestCodeLensManager::testParseCodeLensMissingRange()
{
    CodeLensManager mgr;
    QJsonArray arr;
    QJsonObject lens; // no range, no command
    arr.append(lens);
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///m.cpp"));
    QList<CodeLensItem> items = mgr.itemsForDocument("file:///m.cpp");
    // Title is empty => filtered out
    QCOMPARE(items.size(), 0);
}

void TestCodeLensManager::testReceiveLensFiltersEmptyTitles()
{
    CodeLensManager mgr;
    QJsonArray arr;
    arr.append(makeLens(0, 0, "", "cmd"));
    arr.append(makeLens(1, 0, "valid", "cmd2"));
    QMetaObject::invokeMethod(&mgr, "onCodeLensReceived",
                              Q_ARG(QJsonArray, arr), Q_ARG(QString, "file:///f.cpp"));
    QCOMPARE(mgr.itemsForDocument("file:///f.cpp").size(), 1);
}
