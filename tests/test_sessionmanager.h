#ifndef TEST_SESSIONMANAGER_H
#define TEST_SESSIONMANAGER_H

#include <QObject>

class TestSessionManager : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void testDefaults();
    void testSaveSessionDisabled();
    void testSaveAndRestoreRoundTrip();
    void testSaveWithEditorState();
    void testRestoreNoSession();
    void testClearSession();
    void testHotExitRoundTrip();
    void testHotExitNoData();
    void testAutoSaveToggle();
};

#endif // TEST_SESSIONMANAGER_H
