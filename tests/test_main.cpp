#include <QTest>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include "test_aiinlinecompletion.h"
#include "test_bookmarkmanager.h"
#include "test_bracketcolorizer.h"
#include "test_breadcrumb.h"
#include "test_codeactionui.h"
#include "test_codelensmanager.h"
#include "test_crashhandler.h"
#include "test_codeeditor_multicursor.h"
#include "test_dataformatter.h"
#include "test_debugconfiguration.h"
#include "test_debuggergutter.h"
#include "test_dependencyresolver.h"
#include "test_encodingmanager.h"
#include "test_filewatcher.h"
#include "test_findreplace.h"
#include "test_foldmanager.h"
#include "test_gitblame.h"
#include "test_gitbranchwidget.h"
#include "test_gitdiffwidget.h"
#include "test_gitmergewidget.h"
#include "test_httpclientpanel.h"
#include "test_largefilehandler.h"
#include "test_mainwindow_editing.h"
#include "test_minimap.h"
#include "test_multicursor.h"
#include "test_plugincontext.h"
#include "test_pluginregistry.h"
#include "test_projectsearch.h"
#include "test_rundialog.h"
#include "test_sessionmanager.h"
#include "test_snippetmanager.h"
#include "test_splitmanager.h"
#include "test_sqliteviewer.h"
#include "test_themeicons.h"
#include "test_thememanager.h"
#include "test_windowanimator.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    // Redirect QStandardPaths (AppDataLocation etc.) to a test-only location
    // so SessionManager/LargeFileHandler never touch real user data.
    QStandardPaths::setTestModeEnabled(true);

    QApplication app(argc, argv);

    // Isolate QSettings so unit tests never touch the developer's real
    // Scriptura configuration (SnippetManager/BookmarkManager/PluginContext
    // constructors load from QSettings).
    QCoreApplication::setOrganizationName(QStringLiteral("ScripturaTests"));
    QCoreApplication::setApplicationName(QStringLiteral("UnitTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    const QString settingsDir = QDir::tempPath() + QStringLiteral("/scriptura_unit_test_settings");
    QDir().mkpath(settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings().clear(); // start from a clean slate every run

    int status = 0;
    status |= QTest::qExec(new TestAiInlineCompletion, argc, argv);
    status |= QTest::qExec(new TestBookmarkManager, argc, argv);
    status |= QTest::qExec(new TestBracketColorizer, argc, argv);
    status |= QTest::qExec(new TestBreadcrumb, argc, argv);
    status |= QTest::qExec(new TestCodeActionUI, argc, argv);
    status |= QTest::qExec(new TestCodeLensManager, argc, argv);
    status |= QTest::qExec(new TestCrashHandler, argc, argv);
    status |= QTest::qExec(new TestDataFormatter, argc, argv);
    status |= QTest::qExec(new TestDebugConfiguration, argc, argv);
    status |= QTest::qExec(new TestDebuggerGutter, argc, argv);
    status |= QTest::qExec(new TestDependencyResolver, argc, argv);
    status |= QTest::qExec(new TestEncodingManager, argc, argv);
    status |= QTest::qExec(new TestFileWatcher, argc, argv);
    status |= QTest::qExec(new TestFindReplace, argc, argv);
    status |= QTest::qExec(new TestFoldManager, argc, argv);
    status |= QTest::qExec(new TestGitBlame, argc, argv);
    status |= QTest::qExec(new TestGitBranchWidget, argc, argv);
    status |= QTest::qExec(new TestGitDiffWidget, argc, argv);
    status |= QTest::qExec(new TestGitMergeWidget, argc, argv);
    status |= QTest::qExec(new TestHttpClientPanel, argc, argv);
    status |= QTest::qExec(new TestLargeFileHandler, argc, argv);
    status |= QTest::qExec(new TestMainWindowEditing, argc, argv);
    status |= QTest::qExec(new TestMinimap, argc, argv);
    status |= QTest::qExec(new TestMultiCursor, argc, argv);
    status |= QTest::qExec(new TestCodeEditorMultiCursor, argc, argv);
    status |= QTest::qExec(new TestPluginContext, argc, argv);
    status |= QTest::qExec(new TestPluginRegistry, argc, argv);
    status |= QTest::qExec(new TestProjectSearch, argc, argv);
    status |= QTest::qExec(new TestRunDialog, argc, argv);
    status |= QTest::qExec(new TestSessionManager, argc, argv);
    status |= QTest::qExec(new TestSnippetManager, argc, argv);
    status |= QTest::qExec(new TestSplitManager, argc, argv);
    status |= QTest::qExec(new TestSqliteViewer, argc, argv);
    status |= QTest::qExec(new TestThemeIcons, argc, argv);
    status |= QTest::qExec(new TestThemeManager, argc, argv);
    status |= QTest::qExec(new TestWindowAnimator, argc, argv);

    return status;
}
