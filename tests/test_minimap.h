#ifndef TEST_MINIMAP_H
#define TEST_MINIMAP_H

#include <QObject>

class TestMinimap : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testDocumentSet();
    void testScrollPosition_data();
    void testScrollPosition();
    void testNoDocument();
    void testDocumentChangeUpdate();
};

#endif
