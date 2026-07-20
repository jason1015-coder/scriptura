#ifndef TEST_SERVICELOCATOR_H
#define TEST_SERVICELOCATOR_H

#include <QObject>
#include <QString>

// A simple test service for ServiceLocator registration
class TestService : public QObject
{
    Q_OBJECT
public:
    explicit TestService(QObject *parent = nullptr) : QObject(parent) {}
    QString value() const { return m_value; }
    void setValue(const QString &v) { m_value = v; }
private:
    QString m_value;
};

class TestServiceLocator : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testSingletonInstance();
    void testRegisterAndGetService();
    void testUnregisterService();
    void testGetNonexistentService();
    void testRegisteredServices();
};

#endif
