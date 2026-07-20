#include <QTest>
#include "gitmergewidget.h"
#include "test_gitmergewidget.h"

void TestGitMergeWidget::testInitialState()
{
    GitMergeWidget widget;
    QVERIFY(widget.isEnabled());
    QVERIFY(widget.resultText().isEmpty());
}

void TestGitMergeWidget::testSetConflict()
{
    GitMergeWidget widget;
    widget.setConflict("int a = 1;", "int a = 2;", "int a = 1;");
    QCOMPARE(widget.resultText(), QString("int a = 1;"));
}
