#ifndef TEST_RUNDIALOG_H
#define TEST_RUNDIALOG_H

#include <QObject>

class TestRunDialog : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetConfigurations();
    void testEmptyConfigurations();
};

#endif
