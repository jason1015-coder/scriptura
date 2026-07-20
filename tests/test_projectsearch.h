#ifndef TEST_PROJECTSEARCH_H
#define TEST_PROJECTSEARCH_H

#include <QObject>

class TestProjectSearch : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetRootPath();
    void testClearResults();
};

#endif
