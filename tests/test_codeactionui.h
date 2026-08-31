#ifndef TEST_CODEACTIONUI_H
#define TEST_CODEACTIONUI_H

#include <QObject>

class TestCodeActionUI : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testCodeActionBar();
    void testReattachAfterEditorDestroyed();
};

#endif
