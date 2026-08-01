#ifndef TEST_ENCODINGMANAGER_H
#define TEST_ENCODINGMANAGER_H

#include <QObject>

class TestEncodingManager : public QObject
{
    Q_OBJECT
private slots:
    void testSupportedEncodings();
    void testEncodingDisplayName();
    void testDetectUtf8();
    void testDetectUtf8Bom();
    void testDetectUtf16LE();
    void testDetectUtf16BE();
    void testDetectMissingFile();
    void testDetectNonUtf8();
    void testReadWriteUtf8();
    void testReadWriteUtf16();
    void testConvertEncoding();
    void testConvertMissingFile();
    void testDetectLineEndingLf();
    void testDetectLineEndingCrlf();
    void testDetectLineEndingCr();
    void testDetectLineEndingEmpty();
    void testConvertLineEndingsToCrlf();
    void testConvertLineEndingsToCr();
    void testConvertLineEndingsFromCr();
    void testHasBomTrue();
    void testHasBomFalse();
    void testHasBomMissingFile();
};

#endif // TEST_ENCODINGMANAGER_H
