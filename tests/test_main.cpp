#include <QTest>
#include <QApplication>
#include "test_httpclientpanel.h"
#include "test_dependencyresolver.h"
#include "test_applicationdock.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    int status = 0;
    status |= QTest::qExec(new TestDependencyResolver, argc, argv);
    status |= QTest::qExec(new TestHttpClientPanel, argc, argv);
    status |= QTest::qExec(new TestApplicationDock, argc, argv);

    return status;
}
