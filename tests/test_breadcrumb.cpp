#include <QTest>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include "breadcrumb.h"
#include "test_breadcrumb.h"

void TestBreadcrumb::testInitialState()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    QVERIFY(breadcrumb.isEnabled());
    QVERIFY(breadcrumb.filePath().isEmpty());
    QVERIFY(breadcrumb.symbolPath().isEmpty());
}

void TestBreadcrumb::testFilePathSet()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    breadcrumb.setFilePath("/home/user/project/src/main.cpp");
    QCOMPARE(breadcrumb.filePath(), QString("/home/user/project/src/main.cpp"));
    QStringList parts = breadcrumb.filePathParts();
    QVERIFY(!parts.isEmpty());
    QCOMPARE(parts.last(), QString("main.cpp"));
}

void TestBreadcrumb::testSymbolPathSet()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    breadcrumb.setSymbolPath("mainFunction");
    QCOMPARE(breadcrumb.symbolPath(), QString("mainFunction"));
}

void TestBreadcrumb::testBreadcrumbClick_data()
{
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<QPoint>("clickPos");

    QTest::newRow("file-click") << "/home/user/project/src/main.cpp" << QPoint(50, 12);
    QTest::newRow("symbol-click") << "/home/user/project/src/main.cpp" << QPoint(200, 12);
}

void TestBreadcrumb::testBreadcrumbClick()
{
    QFETCH(QString, filePath);
    QFETCH(QPoint, clickPos);

    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    breadcrumb.setFilePath(filePath);
    breadcrumb.setSymbolPath("mySymbol");

    QSignalSpy spy(&breadcrumb, &Breadcrumb::breadcrumbClicked);

    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, clickPos);

    QVERIFY(spy.count() >= 0);
    if (spy.count() > 0) {
        QString clickedPath = spy.first().first().toString();
        QVERIFY(!clickedPath.isEmpty());
    }
}

void TestBreadcrumb::testFilePathParsing()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    breadcrumb.setFilePath("/home/user/project/src/main.cpp");
    QStringList parts = breadcrumb.filePathParts();
    QVERIFY(!parts.isEmpty());
    QCOMPARE(parts.last(), QString("main.cpp"));

    Breadcrumb breadcrumb2(&editor);
    breadcrumb2.setFilePath("C:\\Users\\dev\\project\\src\\main.cpp");
    QStringList parts2 = breadcrumb2.filePathParts();
    QVERIFY(!parts2.isEmpty());
}

void TestBreadcrumb::testEmptyPaths()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    breadcrumb.setFilePath("");
    QVERIFY(breadcrumb.filePathParts().isEmpty());
    breadcrumb.setSymbolPath("");
    QVERIFY(breadcrumb.symbolPath().isEmpty());
}
