#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include "rust_adapter.h"
#include "test_dependencyresolver.h"

void TestDependencyResolver::testBuildGraph()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2;
    p1["id"] = "plugin-a";
    p1["dependencies"] = QJsonArray{"plugin-b"};
    p2["id"] = "plugin-b";
    p2["dependencies"] = QJsonArray{};

    QList<QJsonObject> plugins{p1, p2};
    QStringList result = resolver.topologicalSort(plugins);
    QVERIFY(result.contains("plugin-b"));
    QVERIFY(result.contains("plugin-a"));
    QVERIFY(result.indexOf("plugin-b") < result.indexOf("plugin-a"));
}

void TestDependencyResolver::testTopologicalSortSimple()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2, p3;
    p1["id"] = "a";
    p1["dependencies"] = QJsonArray{"b"};
    p2["id"] = "b";
    p2["dependencies"] = QJsonArray{"c"};
    p3["id"] = "c";
    p3["dependencies"] = QJsonArray{};

    QStringList expected{"c", "b", "a"};
    QStringList result = resolver.topologicalSort({p1, p2, p3});
    QCOMPARE(result, expected);
}

void TestDependencyResolver::testTopologicalSortMultipleDeps()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2, p3, p4;
    p1["id"] = "app";
    p1["dependencies"] = QJsonArray{"ui", "core"};
    p2["id"] = "ui";
    p2["dependencies"] = QJsonArray{"core"};
    p3["id"] = "core";
    p3["dependencies"] = QJsonArray{};
    p4["id"] = "utils";
    p4["dependencies"] = QJsonArray{};

    QStringList result = resolver.topologicalSort({p1, p2, p3, p4});
    QVERIFY(result.indexOf("core") < result.indexOf("ui"));
    QVERIFY(result.indexOf("core") < result.indexOf("app"));
    QVERIFY(result.indexOf("ui") < result.indexOf("app"));
}

void TestDependencyResolver::testHasCircularDependency()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2;
    p1["id"] = "a";
    p1["dependencies"] = QJsonArray{"b"};
    p2["id"] = "b";
    p2["dependencies"] = QJsonArray{"a"};

    QVERIFY(resolver.hasCircularDependency({p1, p2}));
}

void TestDependencyResolver::testNoCircularDependency()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2, p3;
    p1["id"] = "a";
    p1["dependencies"] = QJsonArray{"b"};
    p2["id"] = "b";
    p2["dependencies"] = QJsonArray{"c"};
    p3["id"] = "c";
    p3["dependencies"] = QJsonArray{};

    QVERIFY(!resolver.hasCircularDependency({p1, p2, p3}));
}

void TestDependencyResolver::testValidateMissingRequiredDependency()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1;
    p1["id"] = "plugin-a";
    p1["dependencies"] = QJsonArray{"missing-dep"};

    QSet<QString> loaded{"plugin-a"};
    QList<RustDependencyResolverAdapter::DependencyError> errors = resolver.validate({p1}, loaded);
    QCOMPARE(errors.size(), 1);
    QCOMPARE(errors.first().pluginId, QString("plugin-a"));
    QCOMPARE(errors.first().missingDependency, QString("missing-dep"));
    QVERIFY(!errors.first().isOptional);
}

void TestDependencyResolver::testValidateOptionalDependencyMissing()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1;
    p1["id"] = "plugin-a";
    p1["dependencies"] = QJsonArray{};
    p1["optionalDependencies"] = QJsonArray{"missing-opt"};

    QSet<QString> loaded{"plugin-a"};
    QList<RustDependencyResolverAdapter::DependencyError> errors = resolver.validate({p1}, loaded);
    QCOMPARE(errors.size(), 0);
}

void TestDependencyResolver::testValidateAllDependenciesPresent()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2;
    p1["id"] = "plugin-a";
    p1["dependencies"] = QJsonArray{"plugin-b"};
    p2["id"] = "plugin-b";
    p2["dependencies"] = QJsonArray{};

    QSet<QString> loaded{"plugin-a", "plugin-b"};
    QList<RustDependencyResolverAdapter::DependencyError> errors = resolver.validate({p1, p2}, loaded);
    QCOMPARE(errors.size(), 0);
}

void TestDependencyResolver::testTopologicalSortReturnsEmptyOnCircular()
{
    RustDependencyResolverAdapter resolver;
    QJsonObject p1, p2;
    p1["id"] = "a";
    p1["dependencies"] = QJsonArray{"b"};
    p2["id"] = "b";
    p2["dependencies"] = QJsonArray{"a"};

    QStringList result = resolver.topologicalSort({p1, p2});
    QVERIFY(result.isEmpty());
}
