#include <QTest>
#include <QApplication>
#include "codeactionui.h"
#include "test_codeactionui.h"

void TestCodeActionUI::testInitialState()
{
    CodeActionController ctrl;
    QVERIFY(!ctrl.isVisible());
}

void TestCodeActionUI::testReattachAfterEditorDestroyed()
{
    // The CodeActionBar is parented to the CodeEditor. Once a tab/editor is
    // destroyed Qt deletes the bar as a child; re-attaching the controller to
    // another editor must not dereference the freed bar (previously segfaulted
    // in m_bar->hide()).
    CodeActionController ctrl;

    CodeEditor *editorA = new CodeEditor;
    ctrl.attach(editorA, nullptr);
    delete editorA; // destroys the parented CodeActionBar

    CodeEditor editorB;
    ctrl.attach(&editorB, nullptr); // must not crash
    QVERIFY(true);

    CodeEditor editorC;
    ctrl.attach(&editorC, nullptr); // re-attach again
    QVERIFY(true);
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
