#ifndef TEST_COMMANDPALETTE_H
#define TEST_COMMANDPALETTE_H

#include <QObject>

class TestCommandPalette : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
};

#endif
