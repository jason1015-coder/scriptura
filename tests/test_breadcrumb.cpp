#include <QTest>
#include <QPlainTextEdit>
#include "breadcrumb.h"
#include "test_breadcrumb.h"

void TestBreadcrumb::testInitialState()
{
    QPlainTextEdit editor;
    Breadcrumb breadcrumb(&editor);
    QVERIFY(breadcrumb.isEnabled());
}
