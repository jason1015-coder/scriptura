#include <QTest>
#include "servicelocator.h"
#include "test_servicelocator.h"

void TestServiceLocator::initTestCase()
{
    ServiceLocator::destroyInstance();
}

void TestServiceLocator::cleanupTestCase()
{
    ServiceLocator::destroyInstance();
}

void TestServiceLocator::testSingletonInstance()
{
    ServiceLocator *a = ServiceLocator::instance();
    ServiceLocator *b = ServiceLocator::instance();
    QVERIFY(a != nullptr);
    QCOMPARE(a, b);
}

void TestServiceLocator::testRegisterAndGetService()
{
    ServiceLocator *locator = ServiceLocator::instance();
    TestService *svc = new TestService();
    svc->setValue("test-value");

    locator->registerService<QObject>("test-svc", svc);
    QVERIFY(locator->hasService("test-svc"));

    TestService *retrieved = locator->getService<TestService>("test-svc");
    QVERIFY(retrieved != nullptr);
    QCOMPARE(retrieved->value(), QString("test-value"));
}

void TestServiceLocator::testUnregisterService()
{
    ServiceLocator *locator = ServiceLocator::instance();
    TestService *svc = new TestService();
    locator->registerService<QObject>("unreg-svc", svc);
    QVERIFY(locator->hasService("unreg-svc"));

    locator->unregisterService("unreg-svc");
    QVERIFY(!locator->hasService("unreg-svc"));
}

void TestServiceLocator::testGetNonexistentService()
{
    ServiceLocator *locator = ServiceLocator::instance();
    QObject *svc = locator->getService<QObject>("nonexistent");
    QVERIFY(svc == nullptr);
}

void TestServiceLocator::testRegisteredServices()
{
    ServiceLocator *locator = ServiceLocator::instance();
    QStringList before = locator->registeredServices();

    TestService *svc = new TestService();
    locator->registerService<QObject>("list-svc1", svc);
    locator->registerService<QObject>("list-svc2", new QObject());

    QStringList after = locator->registeredServices();
    QCOMPARE(after.size(), before.size() + 2);
    QVERIFY(after.contains("list-svc1"));
    QVERIFY(after.contains("list-svc2"));
}
