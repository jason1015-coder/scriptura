#include <QTest>
#include "gitpanel.h"
#include "test_gitpanel.h"

void TestGitPanel::testInitialState()
{
    GitPanel panel;
    QVERIFY(panel.isEnabled());
}
