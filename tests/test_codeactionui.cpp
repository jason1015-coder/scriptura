#include <QTest>
#include "codeactionui.h"
#include "test_codeactionui.h"

void TestCodeActionUI::testInitialState()
{
    CodeActionController ctrl;
    QVERIFY(!ctrl.isVisible());
}

void TestCodeActionUI::testCodeActionBar()
{
    CodeActionBar bar;
    QVERIFY(!bar.hasActions());

    QList<CodeActionItem> items;
    CodeActionItem item;
    item.title = "Fix";
    item.kind = "quickfix";
    item.isPreferred = true;
    items.append(item);

    bar.setActions(items);
    QVERIFY(bar.hasActions());

    bar.clearActions();
    QVERIFY(!bar.hasActions());
}
