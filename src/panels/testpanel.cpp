#include "testpanel.h"
#include "testrunner.h"
#include <QHeaderView>
#include <QSplitter>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCursor>

TestPanel::TestPanel(QWidget *parent)
    : QWidget(parent)
    , m_projectPath("")
{
    m_runner = new TestRunner(this);
    setupUI();

    connect(m_runner, &TestRunner::testStarted, this, &TestPanel::onTestStarted);
    connect(m_runner, &TestRunner::testOutput, this, &TestPanel::onTestOutput);
    connect(m_runner, &TestRunner::testCompleted, this, [this](const TestSuite &s) {
        QJsonArray resultsArr;
        for (const auto &r : s.results) {
            resultsArr.append(QJsonObject{
                {"name", r.name},
                {"status", r.status},
                {"message", r.message},
                {"duration_ms", r.durationMs}
            });
        }
        QJsonObject obj{
            {"name", s.name},
            {"framework", s.framework},
            {"passed", s.passed},
            {"failed", s.failed},
            {"skipped", s.skipped},
            {"total", s.total},
            {"results", resultsArr}
        };
        onTestCompleted(QJsonDocument(obj).toJson());
    });
}

void TestPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Header with title and buttons
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *title = new QLabel(tr("Tests"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_runBtn = new QPushButton(tr("Run All"), this);
    m_runSelectedBtn = new QPushButton(tr("Run Selected"), this);
    m_stopBtn = new QPushButton(tr("Stop"), this);
    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_stopBtn->setEnabled(false);

    headerLayout->addWidget(m_runBtn);
    headerLayout->addWidget(m_runSelectedBtn);
    headerLayout->addWidget(m_stopBtn);
    headerLayout->addWidget(m_clearBtn);
    mainLayout->addLayout(headerLayout);

    // Filter
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter tests..."));
    mainLayout->addWidget(m_filterEdit);

    // Splitter: test tree on top, output on bottom
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    // Test tree
    m_testTree = new QTreeWidget(this);
    m_testTree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
    m_testTree->setColumnWidth(0, 400);
    m_testTree->setColumnWidth(1, 80);
    m_testTree->setColumnWidth(2, 80);
    m_testTree->setAlternatingRowColors(true);
    m_testTree->setRootIsDecorated(true);
    splitter->addWidget(m_testTree);

    // Output panel
    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMaximumBlockCount(10000);
    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);
    m_outputEdit->setFont(monoFont);
    splitter->addWidget(m_outputEdit);

    splitter->setSizes({400, 200});
    mainLayout->addWidget(splitter, 1);

    // Summary label
    m_summaryLabel = new QLabel(tr("No tests run"), this);
    mainLayout->addWidget(m_summaryLabel);

    // Connections
    connect(m_runBtn, &QPushButton::clicked, this, &TestPanel::onRunClicked);
    connect(m_runSelectedBtn, &QPushButton::clicked, this, &TestPanel::onRunSelectedClicked);
    connect(m_stopBtn, &QPushButton::clicked, m_runner, &TestRunner::stopTests);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        m_runBtn->setEnabled(true);
        m_runSelectedBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
    });
    connect(m_clearBtn, &QPushButton::clicked, this, &TestPanel::onClearClicked);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &TestPanel::onFilterChanged);
    connect(m_testTree, &QTreeWidget::itemDoubleClicked, this, &TestPanel::onItemDoubleClicked);
}

void TestPanel::setProjectPath(const QString &path)
{
    m_projectPath = path;
}

void TestPanel::runAllTests()
{
    if (m_projectPath.isEmpty()) return;

    m_testTree->clear();
    m_outputEdit->clear();
    m_summaryLabel->setText(tr("Running tests..."));

    m_runBtn->setEnabled(false);
    m_runSelectedBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_runner->runAllTests(m_projectPath);
}

void TestPanel::runSelectedTest()
{
    QTreeWidgetItem *item = m_testTree->currentItem();
    if (!item) return;

    QString testName = item->text(0);
    if (testName.isEmpty()) return;

    m_testTree->clear();
    m_outputEdit->clear();
    m_summaryLabel->setText(tr("Running test: %1...").arg(testName));

    m_runBtn->setEnabled(false);
    m_runSelectedBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_runner->runTestName(m_projectPath, testName);
}

void TestPanel::onRunClicked()
{
    runAllTests();
}

void TestPanel::onRunSelectedClicked()
{
    runSelectedTest();
}

void TestPanel::onStopClicked()
{
    m_runner->stopTests();
    m_runBtn->setEnabled(true);
    m_runSelectedBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_summaryLabel->setText(tr("Tests stopped"));
}

void TestPanel::onClearClicked()
{
    m_testTree->clear();
    m_outputEdit->clear();
    m_summaryLabel->setText(tr("No tests run"));
}

void TestPanel::onFilterChanged(const QString &text)
{
    for (int i = 0; i < m_testTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = m_testTree->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem *child = category->child(j);
            bool match = text.isEmpty() ||
                         child->text(0).contains(text, Qt::CaseInsensitive) ||
                         child->text(1).contains(text, Qt::CaseInsensitive);
            child->setHidden(!match);
            if (match) anyVisible = true;
        }
        category->setHidden(!anyVisible);
    }
}

void TestPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    QString testName = item->text(0);
    if (!testName.isEmpty()) {
        emit jumpToTest(m_projectPath, 0); // TODO: extract file/line from test name
    }
}

void TestPanel::onTestStarted(const QString &command)
{
    addOutputLine(tr("Running: %1").arg(command));
    emit testRunStarted(command);
}

void TestPanel::onTestOutput(const QString &line)
{
    addOutputLine(line);
}

void TestPanel::onTestCompleted(const QByteArray &suiteJson)
{
    m_runBtn->setEnabled(true);
    m_runSelectedBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    // Parse the suite from JSON
    QJsonDocument doc = QJsonDocument::fromJson(suiteJson);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    int passed = obj["passed"].toInt();
    int failed = obj["failed"].toInt();
    int skipped = obj["skipped"].toInt();

    populateTree(suiteJson);
    updateSummary(passed, failed, skipped);
    emit testRunFinished(passed, failed, skipped);
}

void TestPanel::populateTree(const QByteArray &suiteJson)
{
    m_testTree->clear();

    QJsonDocument doc = QJsonDocument::fromJson(suiteJson);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString framework = obj["framework"].toString();
    QJsonArray results = obj["results"].toArray();

    QTreeWidgetItem *categoryItem = new QTreeWidgetItem(m_testTree);
    categoryItem->setText(0, QString("[%1] %2 tests").arg(framework).arg(results.size()));
    categoryItem->setExpanded(true);
    QFont font = categoryItem->font(0);
    font.setBold(true);
    categoryItem->setFont(0, font);

    for (const auto &result : results) {
        QJsonObject r = result.toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(categoryItem);
        item->setText(0, r["name"].toString());
        item->setText(1, r["status"].toString());

        QString status = r["status"].toString();
        if (status == "passed") {
            item->setForeground(1, QColor(76, 175, 80)); // Green
        } else if (status == "failed") {
            item->setForeground(1, QColor(244, 67, 54)); // Red
        } else if (status == "skipped") {
            item->setForeground(1, QColor(255, 193, 7)); // Yellow
        }

        if (r["duration_ms"].toInt() > 0) {
            item->setText(2, QString("%1 ms").arg(r["duration_ms"].toInt()));
        }
    }
}

void TestPanel::updateSummary(int passed, int failed, int skipped)
{
    int total = passed + failed + skipped;
    QString summary = tr("Total: %1 | Passed: %2 | Failed: %3 | Skipped: %4")
                      .arg(total).arg(passed).arg(failed).arg(skipped);

    if (failed > 0) {
        m_summaryLabel->setStyleSheet("color: red; font-weight: bold;");
    } else if (passed > 0) {
        m_summaryLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_summaryLabel->setStyleSheet("");
    }

    m_summaryLabel->setText(summary);
}

void TestPanel::addOutputLine(const QString &line, bool isError)
{
    QTextCharFormat format;
    if (isError) {
        format.setForeground(QColor(244, 67, 54)); // Red
    }

    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(line + "\n");
    cursor.movePosition(QTextCursor::End);
    m_outputEdit->setTextCursor(cursor);
    m_outputEdit->ensureCursorVisible();
}
