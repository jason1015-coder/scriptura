#ifndef TEST_TERMINALPANEL_H
#define TEST_TERMINALPANEL_H

#include <QObject>

class TestTerminalPanel : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testWorkingDirectory();
};

#endif
