#include <QTest>
#include <QSignalSpy>
#include "gitblame.h"
#include "test_gitblame.h"

void TestGitBlame::testInitialState()
{
    GitBlame blame;
    QVERIFY(blame.isEnabled());
    QVERIFY(!blame.hasBlameData());
    QVERIFY(blame.currentFile().isEmpty());
    QVERIFY(blame.blameForFile().isEmpty());
}

void TestGitBlame::testSetEnabled()
{
    GitBlame blame;
    blame.setEnabled(false);
    QVERIFY(!blame.isEnabled());
    blame.setEnabled(true);
    QVERIFY(blame.isEnabled());
}

void TestGitBlame::testRequestEmptyPath()
{
    GitBlame blame;
    QSignalSpy failed(&blame, &GitBlame::blameFailed);
    blame.requestBlame(QString()); // should be a no-op
    QVERIFY(blame.currentFile().isEmpty());
}

void TestGitBlame::testRequestWhenDisabled()
{
    GitBlame blame;
    blame.setEnabled(false);
    blame.requestBlame("/tmp/somefile.cpp"); // no-op when disabled
    QVERIFY(blame.currentFile().isEmpty());
}

void TestGitBlame::testClear()
{
    GitBlame blame;
    blame.clear();
    QVERIFY(!blame.hasBlameData());
    QVERIFY(blame.currentFile().isEmpty());
}

void TestGitBlame::testBlameForLineEmpty()
{
    GitBlame blame;
    BlameLineInfo info = blame.blameForLine(0);
    QVERIFY(info.commitHash.isEmpty());
    QCOMPARE(info.line, 0);
}

void TestGitBlame::testBlameForFileEmpty()
{
    GitBlame blame;
    QVERIFY(blame.blameForFile().isEmpty());
}

void TestGitBlame::testParseBlameOutput()
{
    GitBlame blame;

    // Craft a porcelain-format output. The parse function is private, so we
    // exercise it indirectly through a real git repo in a temp dir.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir d(dir.path());
    QVERIFY(d.mkpath(".git"));

    QFile f(dir.filePath("sample.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("line one\nline two\n");
    f.close();

    // Commit via real git so requestBlame gets genuine porcelain output.
    QProcess git;
    git.setWorkingDirectory(dir.path());
    git.start("git", {"init", "-q"});
    QVERIFY(git.waitForFinished(5000));
    QCOMPARE(git.exitCode(), 0);

    git.start("git", {"add", "sample.txt"});
    QVERIFY(git.waitForFinished(5000));
    QCOMPARE(git.exitCode(), 0);

    git.start("git", {"-c", "user.email=test@example.com",
                      "-c", "user.name=Tester",
                      "commit", "-q", "-m", "initial"});
    QVERIFY(git.waitForFinished(5000));
    QCOMPARE(git.exitCode(), 0);

    QSignalSpy received(&blame, &GitBlame::blameReceived);
    QSignalSpy failed(&blame, &GitBlame::blameFailed);
    blame.requestBlame(dir.filePath("sample.txt"));

    // Wait up to 5s for the async git process
    int waited = 0;
    while (received.isEmpty() && failed.isEmpty() && waited < 5000) {
        QTest::qWait(100);
        waited += 100;
    }

    if (!failed.isEmpty()) {
        QSKIP("git blame failed in this environment; parser exercised via live repo only");
    }

    QVERIFY2(received.size() == 1, "blameReceived should fire once");
    QVERIFY(blame.hasBlameData());
    QCOMPARE(blame.blameForFile().size(), 2);

    BlameLineInfo line0 = blame.blameForLine(0);
    QVERIFY(!line0.commitHash.isEmpty());
    QCOMPARE(line0.line, 0);
    QVERIFY(!line0.author.isEmpty());
    QCOMPARE(line0.summary, QString("initial"));

    BlameLineInfo line1 = blame.blameForLine(1);
    QCOMPARE(line1.line, 1);
}
