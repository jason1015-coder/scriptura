#ifndef TEST_GITPANEL_H
#define TEST_GITPANEL_H

#include <QObject>

class TestGitPanel : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
};

#endif
