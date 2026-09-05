#include <QTest>
#include <QSignalSpy>
#include "rust_adapter.h"
#include "scriptura_actions.h"
#include "test_ui_action_bridge.h"

// Verifies the Rust-owned UI action layer: every action is validated and
// decided in Rust (UiActionHandler), and the C++ bridge only translates the
// decided commands into Qt signals. Rejected actions emit actionError() and
// execute nothing.

void TestUiActionBridge::minimizeRoutesThroughRust()
{
    UiActionBridge bridge;
    QSignalSpy spyMin(&bridge, &UiActionBridge::windowMinimizeRequested);
    QSignalSpy spyErr(&bridge, &UiActionBridge::actionError);

    bridge.handle(UiActions::TitlebarMinimize);

    QCOMPARE(spyMin.count(), 1);
    QCOMPARE(spyErr.count(), 0);
}

void TestUiActionBridge::togglesRouteCommands()
{
    UiActionBridge bridge;
    QSignalSpy spyMax(&bridge, &UiActionBridge::windowToggleMaximizedRequested);
    QSignalSpy spySide(&bridge, &UiActionBridge::sidebarToggleRequested);
    QSignalSpy spyInsp(&bridge, &UiActionBridge::inspectorToggleRequested);

    bridge.handle(UiActions::TitlebarMaximize);
    bridge.handle(UiActions::TitlebarSidebarToggle);
    bridge.handle(UiActions::TitlebarInspectorToggle);

    QCOMPARE(spyMax.count(), 1);
    QCOMPARE(spySide.count(), 1);
    QCOMPARE(spyInsp.count(), 1);
}

void TestUiActionBridge::searchQueryPassedThrough()
{
    UiActionBridge bridge;
    QSignalSpy spy(&bridge, &UiActionBridge::searchOpenRequested);

    bridge.handle(UiActions::TitlebarSearch, {{QStringLiteral("query"), QStringLiteral("open project")}});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("open project"));
}

void TestUiActionBridge::invalidPayloadRejected()
{
    UiActionBridge bridge;
    QSignalSpy spySearch(&bridge, &UiActionBridge::searchOpenRequested);
    QSignalSpy spyErr(&bridge, &UiActionBridge::actionError);

    // Empty search query — Rust rejects it, no command is executed.
    bridge.handle(UiActions::TitlebarSearch, {{QStringLiteral("query"), QStringLiteral("   ")}});
    QCOMPARE(spySearch.count(), 0);
    QCOMPARE(spyErr.count(), 1);
    QCOMPARE(spyErr.takeFirst().at(0).toString(), QString(UiActions::TitlebarSearch));

    // Malformed JSON payload.
    bridge.handle(UiActions::TitlebarSearch, QJsonObject());
    QCOMPARE(spyErr.count(), 1);
}

void TestUiActionBridge::unknownActionRejected()
{
    UiActionBridge bridge;
    QSignalSpy spy(&bridge, &UiActionBridge::actionError);

    bridge.handle(QStringLiteral("ui.mystery.button"));

    QCOMPARE(spy.count(), 1);
}

void TestUiActionBridge::invalidProjectPathRejected()
{
    UiActionBridge bridge;
    QSignalSpy spyOpen(&bridge, &UiActionBridge::projectOpenRequested);
    QSignalSpy spyErr(&bridge, &UiActionBridge::actionError);

    bridge.handle(UiActions::WelcomeRecentProject,
                  {{QStringLiteral("path"), QStringLiteral("/no/such/dir/xyz")}});

    QCOMPARE(spyOpen.count(), 0);
    QCOMPARE(spyErr.count(), 1);
}

void TestUiActionBridge::auditLogRecordsActions()
{
    UiActionBridge bridge;
    bridge.handle(UiActions::TitlebarClose);
    bridge.handle(UiActions::TitlebarSettings);

    QStringList log = bridge.auditLog();
    QVERIFY(log.size() >= 2);
    QVERIFY(log.last().contains(UiActions::TitlebarSettings));
    QVERIFY(log.at(log.size() - 2).contains(UiActions::TitlebarClose));
}