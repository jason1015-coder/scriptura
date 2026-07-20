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
// New test headers
#include "test_crashhandler.h"
#include "test_plugincrashhandler.h"
#include "test_plugincontext.h"
#include "test_servicelocator.h"
#include "test_versionfetcher.h"
#include "test_updater.h"
#include "test_findreplace.h"
#include "test_problempanel.h"
#include "test_pluginregistry.h"
#include "test_terminalpanel.h"
#include "test_todopanel.h"
#include "test_gitpanel.h"
#include "test_codeactionui.h"
#include "test_debugconfiguration.h"
#include "test_breadcrumb.h"
#include "test_splitmanager.h"
#include "test_commandpalette.h"
#include "test_minimap.h"
#include "test_gitbranchwidget.h"
#include "test_gitdiffwidget.h"
#include "test_gitmergewidget.h"
#include "test_aiinlinecompletion.h"
#include "test_sqliteviewer.h"
#include "test_debuggergutter.h"
#include "test_projectsearch.h"
#include "test_themeicons.h"
#include "test_thememanager.h"
#include "test_windowanimator.h"
#include "test_pluginupdater.h"
#include "test_rundialog.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    int status = 0;
    // Existing test suites
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
    // New test suites — Internals
    status |= QTest::qExec(new TestCrashHandler, argc, argv);
    status |= QTest::qExec(new TestPluginCrashHandler, argc, argv);
    status |= QTest::qExec(new TestPluginContext, argc, argv);
    status |= QTest::qExec(new TestServiceLocator, argc, argv);
    status |= QTest::qExec(new TestVersionFetcher, argc, argv);
    status |= QTest::qExec(new TestUpdater, argc, argv);
    // New test suites — Panels
    status |= QTest::qExec(new TestFindReplace, argc, argv);
    status |= QTest::qExec(new TestProblemPanel, argc, argv);
    status |= QTest::qExec(new TestTerminalPanel, argc, argv);
    status |= QTest::qExec(new TestTodoPanel, argc, argv);
    status |= QTest::qExec(new TestGitPanel, argc, argv);
    // New test suites — Plugins
    status |= QTest::qExec(new TestPluginRegistry, argc, argv);
    status |= QTest::qExec(new TestCodeActionUI, argc, argv);
    // New test suites — Services
    status |= QTest::qExec(new TestDebugConfiguration, argc, argv);
    // New test suites — Widgets
    status |= QTest::qExec(new TestBreadcrumb, argc, argv);
    status |= QTest::qExec(new TestSplitManager, argc, argv);
    status |= QTest::qExec(new TestCommandPalette, argc, argv);
    status |= QTest::qExec(new TestMinimap, argc, argv);
    // New test suites — Git panels
    status |= QTest::qExec(new TestGitBranchWidget, argc, argv);
    status |= QTest::qExec(new TestGitDiffWidget, argc, argv);
    status |= QTest::qExec(new TestGitMergeWidget, argc, argv);
    // New test suites — Plugins
    status |= QTest::qExec(new TestAiInlineCompletion, argc, argv);
    status |= QTest::qExec(new TestSqliteViewer, argc, argv);
    // New test suites — Services
    status |= QTest::qExec(new TestDebuggerGutter, argc, argv);
    status |= QTest::qExec(new TestProjectSearch, argc, argv);
    // New test suites — Core
    status |= QTest::qExec(new TestThemeIcons, argc, argv);
    status |= QTest::qExec(new TestThemeManager, argc, argv);
    status |= QTest::qExec(new TestWindowAnimator, argc, argv);
    // New test suites — Internals
    status |= QTest::qExec(new TestPluginUpdater, argc, argv);
    status |= QTest::qExec(new TestRunDialog, argc, argv);

    return status;
}
