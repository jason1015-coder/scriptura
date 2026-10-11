#ifndef TEST_THEMEMANAGER_H
#define TEST_THEMEMANAGER_H

#include <QObject>

class TestThemeManager : public QObject
{
    Q_OBJECT
private slots:
    void testDefaultTheme();
    void testApplyTheme();
    void testThemeProperties();
    void testPluginResolution();
    void testPluginEnableToggle();
};

#endif
