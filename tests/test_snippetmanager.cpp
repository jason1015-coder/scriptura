#include <QTest>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include "snippetmanager.h"
#include "test_snippetmanager.h"

void TestSnippetManager::init()
{
    // SnippetManager loads persisted snippets from QSettings in its
    // constructor; clear between tests so they don't leak into each other.
    QSettings().clear();
}

namespace {
Snippet makeSnippet(const QString &id, const QString &prefix, const QString &body,
                    const QString &language = QString(), int tabStops = 0)
{
    Snippet s;
    s.id = id;
    s.name = "Snippet " + id;
    s.prefix = prefix;
    s.body = body;
    s.description = "desc";
    s.language = language;
    s.tabStops = tabStops;
    return s;
}
}

void TestSnippetManager::testInitialState()
{
    SnippetManager mgr;
    QVERIFY(mgr.allSnippets().isEmpty());
    QVERIFY(!mgr.hasTabStops());
}

void TestSnippetManager::testAddSnippet()
{
    SnippetManager mgr;
    QSignalSpy added(&mgr, &SnippetManager::snippetAdded);
    QSignalSpy changed(&mgr, &SnippetManager::snippetsChanged);
    mgr.addSnippet(makeSnippet("s1", "for", "for (;;) {}"));
    QCOMPARE(mgr.allSnippets().size(), 1);
    QCOMPARE(added.count(), 1);
    QCOMPARE(changed.count(), 1);
}

void TestSnippetManager::testAddDuplicateIdIgnored()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    mgr.addSnippet(makeSnippet("s1", "for2", "b"));
    QCOMPARE(mgr.allSnippets().size(), 1);
}

void TestSnippetManager::testUpdateSnippet()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    Snippet updated = makeSnippet("s1", "for", "b");
    QSignalSpy changed(&mgr, &SnippetManager::snippetsChanged);
    mgr.updateSnippet(updated);
    QCOMPARE(mgr.snippetById("s1").body, QString("b"));
    QCOMPARE(changed.count(), 1);
}

void TestSnippetManager::testUpdateMissingSnippet()
{
    SnippetManager mgr;
    QSignalSpy changed(&mgr, &SnippetManager::snippetsChanged);
    mgr.updateSnippet(makeSnippet("ghost", "g", "x"));
    QCOMPARE(changed.count(), 0);
    QVERIFY(mgr.allSnippets().isEmpty());
}

void TestSnippetManager::testRemoveSnippet()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    QSignalSpy removed(&mgr, &SnippetManager::snippetRemoved);
    mgr.removeSnippet("s1");
    QVERIFY(mgr.allSnippets().isEmpty());
    QCOMPARE(removed.count(), 1);
}

void TestSnippetManager::testRemoveMissingSnippet()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    mgr.removeSnippet("nope");
    QCOMPARE(mgr.allSnippets().size(), 1);
}

void TestSnippetManager::testSnippetById()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    QCOMPARE(mgr.snippetById("s1").id, QString("s1"));
}

void TestSnippetManager::testSnippetByIdMissing()
{
    SnippetManager mgr;
    Snippet s = mgr.snippetById("missing");
    QVERIFY(s.id.isEmpty());
}

void TestSnippetManager::testSnippetsForLanguage()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "a", "x", "cpp"));
    mgr.addSnippet(makeSnippet("s2", "b", "y", "js"));
    mgr.addSnippet(makeSnippet("s3", "c", "z")); // global
    mgr.addSnippet(makeSnippet("s4", "d", "w", "*")); // global
    QCOMPARE(mgr.snippetsForLanguage("cpp").size(), 3);
    QCOMPARE(mgr.snippetsForLanguage("js").size(), 3);
}

void TestSnippetManager::testSnippetPrefixes()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    mgr.addSnippet(makeSnippet("s2", "", "b"));
    QStringList prefixes = mgr.snippetPrefixes();
    QCOMPARE(prefixes.size(), 1);
    QVERIFY(prefixes.contains("for"));
}

void TestSnippetManager::testHasSnippetForPrefix()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a", "cpp"));
    QVERIFY(mgr.hasSnippetForPrefix("for", "cpp"));
    QVERIFY(!mgr.hasSnippetForPrefix("for", "js")); // language mismatch => false
    QVERIFY(!mgr.hasSnippetForPrefix("while", "cpp"));
}

void TestSnippetManager::testSnippetForPrefix()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "body", "cpp"));
    QCOMPARE(mgr.snippetForPrefix("for", "cpp").id, QString("s1"));
    QVERIFY(mgr.snippetForPrefix("for", "js").id.isEmpty());
    QVERIFY(mgr.snippetForPrefix("zzz", "cpp").id.isEmpty());
}

void TestSnippetManager::testInsertSnippetNoEditor()
{
    SnippetManager mgr;
    mgr.insertSnippet(nullptr, makeSnippet("s1", "for", "x")); // no crash
    QVERIFY(true);
}

void TestSnippetManager::testInsertSnippetWithTabStops()
{
    SnippetManager mgr;
    QPlainTextEdit editor;
    // NOTE: only ${N:placeholder} stops are matched; the plain $N:name form
    // is greedily consumed as a single placeholder by the parser.
    mgr.insertSnippet(&editor, makeSnippet("s1", "for", "${1:name} ${2:other}"));

    QVERIFY(editor.toPlainText().contains("${1:name}"));
    QVERIFY(mgr.hasTabStops());
    // insertSnippet jumps to the 2nd stop, selecting its placeholder text
    QVERIFY(editor.textCursor().position() > 0);
}

void TestSnippetManager::testTabStopNavigation()
{
    SnippetManager mgr;
    QPlainTextEdit editor;
    mgr.insertSnippet(&editor, makeSnippet("s1", "for", "${1:a} ${2:b} ${3:c}"));
    QVERIFY(mgr.hasTabStops());

    // Advance through stops; the cursor should move forward each time.
    int lastPos = editor.textCursor().position();
    mgr.nextTabStop(&editor);
    QVERIFY(editor.textCursor().position() > lastPos);
    QVERIFY(mgr.hasTabStops());

    lastPos = editor.textCursor().position();
    mgr.nextTabStop(&editor); // past the last stop => clears
    QVERIFY(!mgr.hasTabStops());
    QCOMPARE(editor.textCursor().position(), lastPos); // no crash, stays put

    // Re-insert and navigate backwards.
    mgr.insertSnippet(&editor, makeSnippet("s2", "for", "${1:x} ${2:y}"));
    QVERIFY(mgr.hasTabStops());
    int p = editor.textCursor().position();
    mgr.previousTabStop(&editor);
    QVERIFY(editor.textCursor().position() <= p);
    QVERIFY(mgr.hasTabStops());
}

void TestSnippetManager::testClearTabStops()
{
    SnippetManager mgr;
    QPlainTextEdit editor;
    mgr.insertSnippet(&editor, makeSnippet("s1", "for", "$1 $2"));
    QVERIFY(mgr.hasTabStops());
    mgr.clearTabStops();
    QVERIFY(!mgr.hasTabStops());
}

void TestSnippetManager::testImportExport()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a", "cpp", 2));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("snippets.json");
    QVERIFY(mgr.exportSnippets(path));
    QVERIFY(QFile::exists(path));

    SnippetManager mgr2;
    QVERIFY(mgr2.importSnippets(path));
    QCOMPARE(mgr2.allSnippets().size(), 1);
    QCOMPARE(mgr2.snippetById("s1").body, QString("a"));

    // Importing again should not duplicate
    QVERIFY(mgr2.importSnippets(path));
    QCOMPARE(mgr2.allSnippets().size(), 1);
}

void TestSnippetManager::testImportBadFile()
{
    SnippetManager mgr;
    QVERIFY(!mgr.importSnippets("/nonexistent/snippets.json"));
}

void TestSnippetManager::testImportInvalidJson()
{
    SnippetManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("bad.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("this is { not json");
    f.close();
    QVERIFY(!mgr.importSnippets(path));
}

void TestSnippetManager::testExportToBadPath()
{
    SnippetManager mgr;
    mgr.addSnippet(makeSnippet("s1", "for", "a"));
    QVERIFY(!mgr.exportSnippets("/nonexistent/dir/file.json"));
}

void TestSnippetManager::testVariableSubstitution()
{
    SnippetManager mgr;
    QPlainTextEdit editor;
    mgr.insertSnippet(&editor, makeSnippet("s1", "for", "date=$CURRENT_YEAR"));

    QString text = editor.toPlainText();
    QVERIFY(text.startsWith("date="));
    QString year = text.mid(5);
    bool ok = false;
    year.toInt(&ok);
    QVERIFY(ok); // $CURRENT_YEAR substituted with a numeric year
    QVERIFY(year.length() == 4);
}
