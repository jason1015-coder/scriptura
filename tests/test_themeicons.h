#ifndef TEST_THEMEICONS_H
#define TEST_THEMEICONS_H

#include <QObject>

class TestThemeIcons : public QObject
{
    Q_OBJECT
private slots:
    void testRoles();
    void testSingletonInstance();
    void testRecolorAll();
};

#endif
