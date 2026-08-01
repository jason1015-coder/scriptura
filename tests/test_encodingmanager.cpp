#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "encodingmanager.h"
#include "test_encodingmanager.h"

namespace {
QString writeTempFile(const QTemporaryDir &dir, const QByteArray &data)
{
    QString path = dir.filePath("enc_test.bin");
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(data);
    f.close();
    return path;
}
}

void TestEncodingManager::testSupportedEncodings()
{
    EncodingManager mgr;
    QStringList enc = mgr.supportedEncodings();
    QVERIFY(!enc.isEmpty());
    QVERIFY(enc.contains("UTF-8"));
    QVERIFY(enc.contains("UTF-16LE"));
    QVERIFY(enc.contains("ISO-8859-1"));
}

void TestEncodingManager::testEncodingDisplayName()
{
    EncodingManager mgr;
    QVERIFY(!mgr.encodingDisplayName("UTF-8").isEmpty());
    QVERIFY(mgr.encodingDisplayName("UTF-8").contains("UTF-8"));
}

void TestEncodingManager::testDetectUtf8()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "hello world");
    QCOMPARE(mgr.detectEncoding(path), QString("UTF-8"));
}

void TestEncodingManager::testDetectUtf8Bom()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, QByteArray("\xEF\xBB\xBFhello"));
    QCOMPARE(mgr.detectEncoding(path), QString("UTF-8"));
}

void TestEncodingManager::testDetectUtf16LE()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, QByteArray("\xFF\xFEh\x00i\x00"));
    QCOMPARE(mgr.detectEncoding(path), QString("UTF-16LE"));
}

void TestEncodingManager::testDetectUtf16BE()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, QByteArray("\xFE\xFF\x00h\x00i"));
    QCOMPARE(mgr.detectEncoding(path), QString("UTF-16BE"));
}

void TestEncodingManager::testDetectMissingFile()
{
    EncodingManager mgr;
    QCOMPARE(mgr.detectEncoding("/nonexistent/file.bin"), QString("UTF-8"));
}

void TestEncodingManager::testDetectNonUtf8()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // 0xFF 0xFE-ish invalid UTF-8 bytes (not a BOM at position 0, so decode fails)
    QString path = writeTempFile(dir, QByteArray("\xE9\xE8\xED\xE4")); // Latin-1 accented
    QCOMPARE(mgr.detectEncoding(path), QString("ISO-8859-1"));
}

void TestEncodingManager::testReadWriteUtf8()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "plain");
    QString content = mgr.readFileWithEncoding(path, "UTF-8");
    QCOMPARE(content, QString("plain"));
    QVERIFY(mgr.writeFileWithEncoding(path, "written", "UTF-8"));
    QCOMPARE(mgr.readFileWithEncoding(path, "UTF-8"), QString("written"));
}

void TestEncodingManager::testReadWriteUtf16()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("utf16.txt");
    QVERIFY(mgr.writeFileWithEncoding(path, "héllo", "UTF-16LE"));
    QString content = mgr.readFileWithEncoding(path, "UTF-16LE");
    QCOMPARE(content, QString("héllo"));
}

void TestEncodingManager::testConvertEncoding()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "convert me");
    QVERIFY(mgr.convertEncoding(path, "UTF-8", "UTF-16LE"));
    // UTF-16LE is written without a BOM, so detection falls back to UTF-8
    // validation; the important property is that the content round-trips.
    QCOMPARE(mgr.readFileWithEncoding(path, "UTF-16LE"), QString("convert me"));
    QVERIFY(mgr.readFileWithEncoding(path, "UTF-8") != QString("convert me")); // bytes changed
}

void TestEncodingManager::testConvertMissingFile()
{
    EncodingManager mgr;
    QVERIFY(!mgr.convertEncoding("/nonexistent/file", "UTF-8", "UTF-16LE"));
}

void TestEncodingManager::testDetectLineEndingLf()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "a\nb\nc");
    QCOMPARE(mgr.detectLineEnding(path), QString("LF"));
}

void TestEncodingManager::testDetectLineEndingCrlf()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "a\r\nb\r\nc");
    QCOMPARE(mgr.detectLineEnding(path), QString("CRLF"));
}

void TestEncodingManager::testDetectLineEndingCr()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "a\rb\rc");
    QCOMPARE(mgr.detectLineEnding(path), QString("CR"));
}

void TestEncodingManager::testDetectLineEndingEmpty()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "");
    QCOMPARE(mgr.detectLineEnding(path), QString("LF"));
}

void TestEncodingManager::testConvertLineEndingsToCrlf()
{
    EncodingManager mgr;
    QString out = mgr.convertLineEndings("a\nb\nc", "LF", "CRLF");
    QCOMPARE(out, QString("a\r\nb\r\nc"));
}

void TestEncodingManager::testConvertLineEndingsToCr()
{
    EncodingManager mgr;
    QString out = mgr.convertLineEndings("a\nb\nc", "LF", "CR");
    QCOMPARE(out, QString("a\rb\rc"));
}

void TestEncodingManager::testConvertLineEndingsFromCr()
{
    EncodingManager mgr;
    QString out = mgr.convertLineEndings("a\rb\rc", "CR", "LF");
    QCOMPARE(out, QString("a\nb\nc"));
}

void TestEncodingManager::testHasBomTrue()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // NOTE: must keep the hex escapes and "data" separate, otherwise
    // \xBFd would be parsed as a single hex escape (0xBFD).
    QString path = writeTempFile(dir, QByteArray("\xEF\xBB\xBF") + "data");
    QVERIFY(mgr.hasBOM(path));
}

void TestEncodingManager::testHasBomFalse()
{
    EncodingManager mgr;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir, "no bom here");
    QVERIFY(!mgr.hasBOM(path));
}

void TestEncodingManager::testHasBomMissingFile()
{
    EncodingManager mgr;
    QVERIFY(!mgr.hasBOM("/nonexistent/file"));
}
