#ifndef TEST_GITBLAME_H
#define TEST_GITBLAME_H

#include <QObject>

class TestGitBlame : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetEnabled();
    void testRequestEmptyPath();
    void testRequestWhenDisabled();
    void testClear();
    void testBlameForLineEmpty();
    void testBlameForFileEmpty();
    void testParseBlameOutput();
};

#endif // TEST_GITBLAME_H
