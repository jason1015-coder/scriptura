#ifndef TEST_TODOPANEL_H
#define TEST_TODOPANEL_H

#include <QObject>

class TestTodoPanel : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
};

#endif
