#include <QTest>
#include <QTextEdit>
#include "gitdiffwidget.h"
#include "test_gitdiffwidget.h"

void TestGitDiffWidget::testInitialState()
{
    GitDiffWidget widget;
    QVERIFY(widget.isEnabled());
}

void TestGitDiffWidget::testSetDiff()
{
    GitDiffWidget widget;
    QString diff = "--- a/file.cpp\n+++ b/file.cpp\n@@ -1,5 +1,6 @@\n int main() {\n+    return 0;\n }";
    widget.setDiff(diff);
    
    auto *edit = widget.findChild<QTextEdit*>();
    QVERIFY(edit != nullptr);
    QVERIFY(!edit->toHtml().isEmpty());
}
