#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QListWidget>
#include "taskrunnerui.h"
#include "test_taskrunnerui.h"

void TestTaskRunnerUI::testInitialState()
{
    TaskRunnerUI ui;
    QCOMPARE(ui.taskCount(), 0);
    QVERIFY(ui.detectedTasks().isEmpty());
}

void TestTaskRunnerUI::testDetectPackageJsonTasks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    scripts["test"] = "jest";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QCOMPARE(ui.taskCount(), 2);

    DetectedTask t0 = ui.taskAt(0);
    QCOMPARE(t0.name, QString("build"));
    QCOMPARE(t0.command, QString("npm run build"));
    QCOMPARE(t0.source, QString("package.json"));

    DetectedTask t1 = ui.taskAt(1);
    QCOMPARE(t1.name, QString("test"));
    QCOMPARE(t1.command, QString("npm run test"));
    QCOMPARE(t1.source, QString("package.json"));

    QVERIFY(ui.taskAt(-1).name.isEmpty());
    QVERIFY(ui.taskAt(99).name.isEmpty());
}

void TestTaskRunnerUI::testDetectMakefileTasks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile makefile(dir.path() + "/Makefile");
    QVERIFY(makefile.open(QIODevice::WriteOnly | QIODevice::Text));
    makefile.write("build:\n");
    makefile.write("test:\n");
    makefile.write(".PHONY: build test\n");
    makefile.write("# comment line\n");
    makefile.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QCOMPARE(ui.taskCount(), 2);
    QCOMPARE(ui.taskAt(0).name, QString("build"));
    QCOMPARE(ui.taskAt(0).command, QString("make build"));
    QCOMPARE(ui.taskAt(0).source, QString("Makefile"));
    QCOMPARE(ui.taskAt(1).name, QString("test"));
    QCOMPARE(ui.taskAt(1).command, QString("make test"));
}

void TestTaskRunnerUI::testDetectCargoTasks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile cargo(dir.path() + "/Cargo.toml");
    QVERIFY(cargo.open(QIODevice::WriteOnly | QIODevice::Text));
    cargo.write("[package]\nname = \"test\"\n");
    cargo.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QCOMPARE(ui.taskCount(), 5);
    QCOMPARE(ui.taskAt(0).name, QString("build"));
    QCOMPARE(ui.taskAt(0).command, QString("cargo build"));
    QCOMPARE(ui.taskAt(4).name, QString("fmt"));
    QCOMPARE(ui.taskAt(4).command, QString("cargo fmt"));
    QCOMPARE(ui.taskAt(4).source, QString("Cargo.toml"));
}

void TestTaskRunnerUI::testTaskRunEmission()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QSignalSpy runSpy(&ui, &TaskRunnerUI::taskRun);
    QSignalSpy selectedSpy(&ui, &TaskRunnerUI::taskSelected);

    QListWidget *list = ui.findChild<QListWidget*>();
    QPushButton *btn = ui.findChild<QPushButton*>();
    QVERIFY(list != nullptr);
    QVERIFY(btn != nullptr);

    list->setCurrentRow(0);
    QTest::mouseClick(btn, Qt::LeftButton);

    QCOMPARE(runSpy.count(), 1);
    QCOMPARE(runSpy.first().first().toString(), QString("npm run build"));
    QCOMPARE(selectedSpy.count(), 1);
}

void TestTaskRunnerUI::testTaskDetectionSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    scripts["test"] = "jest";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    TaskRunnerUI ui;
    QSignalSpy detectedSpy(&ui, &TaskRunnerUI::tasksDetected);

    ui.detectTasks(dir.path());

    QCOMPARE(detectedSpy.count(), 1);
    QCOMPARE(detectedSpy.first().first().toInt(), 2);
    QCOMPARE(ui.taskCount(), 2);
}

void TestTaskRunnerUI::testTaskAtBounds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QVERIFY(ui.taskAt(-1).name.isEmpty());
    QVERIFY(ui.taskAt(0).name == "build");
    QVERIFY(ui.taskAt(99).name.isEmpty());
}

void TestTaskRunnerUI::testTaskRunNoSelection()
{
    TaskRunnerUI ui;
    QSignalSpy runSpy(&ui, &TaskRunnerUI::taskRun);

    ui.detectTasks("/tmp/nonexistent");

    QListWidget *list = ui.findChild<QListWidget*>();
    QPushButton *btn = ui.findChild<QPushButton*>();
    QVERIFY(list != nullptr);
    QVERIFY(btn != nullptr);

    QCOMPARE(list->currentRow(), -1);
    QTest::mouseClick(btn, Qt::LeftButton);

    QCOMPARE(runSpy.count(), 0);
}

void TestTaskRunnerUI::testTaskRunRefresh()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    TaskRunnerUI ui;
    ui.detectTasks(dir.path());

    QSignalSpy runSpy(&ui, &TaskRunnerUI::taskRun);

    QPushButton *refreshBtn = nullptr;
    QList<QPushButton*> buttons = ui.findChildren<QPushButton*>();
    for (QPushButton *btn : buttons) {
        if (btn->text() == "Refresh") {
            refreshBtn = btn;
            break;
        }
    }
    QVERIFY(refreshBtn != nullptr);
    QTest::mouseClick(refreshBtn, Qt::LeftButton);

    QCOMPARE(runSpy.count(), 1);
    QCOMPARE(runSpy.first().first().toString(), QString("refresh"));
}

void TestTaskRunnerUI::testTaskRunCommandParsing()
{
    TaskRunnerUI ui;

    QList<DetectedTask> tasks = ui.detectedTasks();
    QVERIFY(tasks.isEmpty());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile pkg(dir.path() + "/package.json");
    QVERIFY(pkg.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonDocument doc;
    QJsonObject root;
    QJsonObject scripts;
    scripts["build"] = "tsc";
    scripts["test"] = "jest";
    root["scripts"] = scripts;
    doc.setObject(root);
    pkg.write(QJsonDocument(doc).toJson());
    pkg.close();

    ui.detectTasks(dir.path());

    for (int i = 0; i < ui.taskCount(); ++i) {
        DetectedTask task = ui.taskAt(i);
        QVERIFY(!task.command.isEmpty());
        const QStringList parts = task.command.split(' ', Qt::SkipEmptyParts);
        QVERIFY(!parts.isEmpty());
        QVERIFY(!parts.first().isEmpty());
    }
}
