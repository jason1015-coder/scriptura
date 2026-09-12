#include <QTest>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QScrollBar>
#include "minimap.h"
#include "test_minimap.h"

void TestMinimap::testInitialState()
{
    QPlainTextEdit editor;
    Minimap minimap(&editor);
    QVERIFY(minimap.isEnabled());
    QVERIFY(minimap.hasDocument());
    QCOMPARE(minimap.document(), editor.document());
}

void TestMinimap::testDocumentSet()
{
    QPlainTextEdit editor;
    editor.setPlainText("line1\nline2\nline3");
    Minimap minimap(&editor);
    QVERIFY(minimap.hasDocument());
    QCOMPARE(minimap.document(), editor.document());
}

void TestMinimap::testScrollPosition_data()
{
    QTest::addColumn<QString>("documentText");
    QTest::addColumn<int>("scrollValue");
    QTest::addColumn<int>("clickY");

    QTest::newRow("simple-scroll") << "line1\nline2\nline3" << 50 << 30;
    QTest::newRow("multi-line") << "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight" << 100 << 40;
    QTest::newRow("zero-scroll") << "a\nb\nc" << 0 << 10;
}

void TestMinimap::testScrollPosition()
{
    QFETCH(QString, documentText);
    QFETCH(int, scrollValue);
    QFETCH(int, clickY);

    QPlainTextEdit editor;
    editor.setPlainText(documentText);
    Minimap minimap(&editor);
    QVERIFY(minimap.hasDocument());

    QSignalSpy spy(&minimap, &Minimap::viewportRequested);

    editor.verticalScrollBar()->setValue(scrollValue);
    QTest::mouseClick(&minimap, Qt::LeftButton, Qt::NoModifier, QPoint(40, clickY));

    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.first();
    int requestedPos = args.at(0).toInt();
    QVERIFY(requestedPos >= 0);

    QCOMPARE(editor.verticalScrollBar()->value(), requestedPos);
}

void TestMinimap::testNoDocument()
{
    Minimap minimap(nullptr);
    QVERIFY(!minimap.hasDocument());
    QCOMPARE(minimap.document(), static_cast<QTextDocument*>(nullptr));
    QCOMPARE(minimap.visibleRegion(), QRect());
}

void TestMinimap::testDocumentChangeUpdate()
{
    QPlainTextEdit editor;
    editor.setPlainText("initial");
    Minimap minimap(&editor);
    QVERIFY(minimap.hasDocument());

    editor.setPlainText("updated text");
    QCOMPARE(minimap.document(), editor.document());
}
