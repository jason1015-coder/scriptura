#ifndef TEST_AIINLINECOMPLETION_H
#define TEST_AIINLINECOMPLETION_H

#include <QObject>

class TestAiInlineCompletion : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetSettings();
    void testSetEditor();
};

#endif
