#include <QTest>
#include <QTreeView>
#include <QStandardItemModel>
#include "gitbranchwidget.h"
#include "test_gitbranchwidget.h"

void TestGitBranchWidget::testInitialState()
{
    GitBranchWidget widget;
    QVERIFY(widget.isEnabled());
}

void TestGitBranchWidget::testSetBranches()
{
    GitBranchWidget widget;
    QStringList local = {"main", "develop", "feature/new"};
    QStringList remote = {"origin/main", "origin/develop"};
    
    widget.setBranches(local, remote);
    
    // Find the tree view to verify data was set
    auto *tree = widget.findChild<QTreeView*>();
    QVERIFY(tree != nullptr);
    auto *model = qobject_cast<QStandardItemModel*>(tree->model());
    QVERIFY(model != nullptr);
    // Should have 2 root items: Local and Remote
    QCOMPARE(model->rowCount(), 2);
}
