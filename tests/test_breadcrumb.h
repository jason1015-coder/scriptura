#ifndef TEST_BREADCRUMB_H
#define TEST_BREADCRUMB_H

#include <QObject>

class TestBreadcrumb : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
};

#endif
