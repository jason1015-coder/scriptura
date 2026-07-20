#include <QTest>
#include <QApplication>
#include "test_workspace.h"
#include "test_configvalidator.h"
#include "test_httpclientpanel.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    int status = 0;
    status |= QTest::qExec(new TestWorkspace, argc, argv);
    status |= QTest::qExec(new TestConfigValidator, argc, argv);
    status |= QTest::qExec(new TestHttpClientPanel, argc, argv);

    return status;
}
