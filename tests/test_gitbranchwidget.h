#ifndef TEST_GITBRANCHWIDGET_H
#define TEST_GITBRANCHWIDGET_H

#include <QObject>

class TestGitBranchWidget : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetBranches();
};

#endif
