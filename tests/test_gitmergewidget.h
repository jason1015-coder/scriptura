#ifndef TEST_GITMERGEWIDGET_H
#define TEST_GITMERGEWIDGET_H

#include <QObject>

class TestGitMergeWidget : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetConflict();
};

#endif
