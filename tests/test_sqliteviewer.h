#ifndef TEST_SQLITEVIEWER_H
#define TEST_SQLITEVIEWER_H

#include <QObject>

class TestSqliteViewer : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testRefreshWithoutDb();
};

#endif
