#ifndef TEST_ARCHIVEEXTRACTOR_H
#define TEST_ARCHIVEEXTRACTOR_H

#include <QObject>

class TestArchiveExtractor : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testExtractValidPluginArchive();
    void testExtractNestedDirectories();
    void testRejectsZipSlip();
    void testMissingArchiveReturnsFalse();

private:
    QString m_fixturesDir;
};

#endif // TEST_ARCHIVEEXTRACTOR_H
