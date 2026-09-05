#ifndef TEST_UI_ACTION_BRIDGE_H
#define TEST_UI_ACTION_BRIDGE_H

#include <QObject>

class TestUiActionBridge : public QObject
{
    Q_OBJECT
private slots:
    void minimizeRoutesThroughRust();
    void togglesRouteCommands();
    void searchQueryPassedThrough();
    void invalidPayloadRejected();
    void unknownActionRejected();
    void invalidProjectPathRejected();
    void auditLogRecordsActions();
};

#endif // TEST_UI_ACTION_BRIDGE_H