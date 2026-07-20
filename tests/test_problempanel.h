#ifndef TEST_PROBLEMPANEL_H
#define TEST_PROBLEMPANEL_H

#include <QObject>

class TestProblemPanel : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testAddAndClearProblems();
};

#endif
