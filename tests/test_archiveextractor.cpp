#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "archiveextractor.h"
#include "test_archiveextractor.h"

void TestArchiveExtractor::initTestCase()
{
    // Fixtures live in tests/fixtures; resolve relative to the test binary.
    QString binDir = QCoreApplication::applicationDirPath();
    QStringList candidates{
        binDir + "/../../tests/fixtures",
        binDir + "/tests/fixtures",
        binDir + "/../tests/fixtures",
    };
    for (const QString &c : candidates) {
        if (QFile::exists(c + "/valid_plugin.zip")) {
            m_fixturesDir = QDir(c).absolutePath();
            break;
        }
    }
    QVERIFY2(!m_fixturesDir.isEmpty(), "Could not locate tests/fixtures directory");
}

void TestArchiveExtractor::testExtractValidPluginArchive()
{
    QTemporaryDir out;
    QVERIFY(out.isValid());

    QString error;
    bool ok = Scriptura::ArchiveExtractor::extract(
        m_fixturesDir + "/valid_plugin.zip", out.path(), &error);
    QVERIFY2(ok, qPrintable(error));

    QVERIFY(QFile::exists(out.path() + "/myplugin/plugin.json"));
    QVERIFY(QFile::exists(out.path() + "/myplugin/libdemo.so"));

    QFile meta(out.path() + "/myplugin/plugin.json");
    QVERIFY(meta.open(QIODevice::ReadOnly));
    QVERIFY(meta.readAll().contains("com.test.demo"));
}

void TestArchiveExtractor::testExtractNestedDirectories()
{
    QTemporaryDir out;
    QVERIFY(out.isValid());

    QString error;
    QVERIFY(Scriptura::ArchiveExtractor::extract(
        m_fixturesDir + "/valid_plugin.zip", out.path(), &error));

    QString nested = out.path() + "/myplugin/sub/nested.txt";
    QVERIFY(QFile::exists(nested));
    QFile f(nested);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("nested file content"));
}

void TestArchiveExtractor::testRejectsZipSlip()
{
    QTemporaryDir out;
    QVERIFY(out.isValid());

    QString error;
    bool ok = Scriptura::ArchiveExtractor::extract(
        m_fixturesDir + "/evil_plugin.zip", out.path(), &error);
    QVERIFY2(!ok, "Zip-slip archive must be rejected");

    // The traversal target must NOT have been written outside the output dir.
    QString escaped = QDir::cleanPath(out.path() + "/../../escape.txt");
    QVERIFY2(!QFile::exists(escaped),
             qPrintable("Unsafe entry escaped to: " + escaped));
}

void TestArchiveExtractor::testMissingArchiveReturnsFalse()
{
    QTemporaryDir out;
    QVERIFY(out.isValid());
    QString error;
    QVERIFY(!Scriptura::ArchiveExtractor::extract(
        out.path() + "/does_not_exist.zip", out.path(), &error));
}
