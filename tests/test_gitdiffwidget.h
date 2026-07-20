#ifndef TEST_GITDIFFWIDGET_H
#define TEST_GITDIFFWIDGET_H

#include <QObject>

class TestGitDiffWidget : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetDiff();
};

#endif
