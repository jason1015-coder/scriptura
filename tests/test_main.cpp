#include <QTest>
#include <QApplication>
#include "test_workspace.h"
#include "test_configvalidator.h"
#include "test_httpclientpanel.h"
#include "test_lengthprefixedframer.h"
#include "test_dependencyresolver.h"
#include "test_eventbus.h"
#include "test_permissionmanager.h"
#include "test_pluginsettings.h"
#include "test_taskrunner.h"
#include "test_archiveextractor.h"
#include "test_pluginmanager.h"
#include "test_lspclient.h"
#include "test_dapclient.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QApplication app(argc, argv);

    int status = 0;
    status |= QTest::qExec(new TestWorkspace, argc, argv);
    status |= QTest::qExec(new TestConfigValidator, argc, argv);
    status |= QTest::qExec(new TestHttpClientPanel, argc, argv);
    status |= QTest::qExec(new TestLengthPrefixedFramer, argc, argv);
    status |= QTest::qExec(new TestDependencyResolver, argc, argv);
    status |= QTest::qExec(new TestEventBus, argc, argv);
    status |= QTest::qExec(new TestPermissionManager, argc, argv);
    status |= QTest::qExec(new TestPluginSettings, argc, argv);
    status |= QTest::qExec(new TestTaskRunner, argc, argv);
    status |= QTest::qExec(new TestArchiveExtractor, argc, argv);
    status |= QTest::qExec(new TestPluginManager, argc, argv);
    status |= QTest::qExec(new TestLspClient, argc, argv);
    status |= QTest::qExec(new TestDapClient, argc, argv);

    return status;
}
