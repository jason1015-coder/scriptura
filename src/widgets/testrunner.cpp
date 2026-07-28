#include "testrunner.h"
#include "rust_adapter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

TestRunner::TestRunner(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_currentFramework(TestFramework::Unknown)
{
}

TestRunner::~TestRunner()
{
    stopTests();
}

// ── Framework Detection (delegates to Rust backend) ──────────────

TestFramework TestRunner::detectFramework(const QString &projectPath) const
{
    QByteArray pathBytes = projectPath.toUtf8();
    char *result = rust_test_detect_framework(pathBytes.constData());
    if (!result) return TestFramework::Unknown;

    QString fw = QString::fromUtf8(result);
    rust_free_string(result);

    if (fw == "cargo_test") return TestFramework::CargoTest;
    if (fw == "jest") return TestFramework::Jest;
    if (fw == "go_test") return TestFramework::GoTest;
    if (fw == "pytest") return TestFramework::Pytest;
    if (fw == "cmake_ctest") return TestFramework::CMakeCTest;
    return TestFramework::Unknown;
}

QString TestRunner::frameworkName(TestFramework fw) const
{
    switch (fw) {
    case TestFramework::Pytest:      return "pytest";
    case TestFramework::Jest:        return "jest";
    case TestFramework::CargoTest:   return "cargo test";
    case TestFramework::GoTest:      return "go test";
    case TestFramework::CMakeCTest:  return "ctest";
    case TestFramework::Unknown:
    default:                         return "unknown";
    }
}

// ── Command Building (delegates to Rust backend) ─────────────────

QString TestRunner::buildCommand(TestFramework fw, const QString &projectPath,
                                  const QString &filter) const
{
    QByteArray fwBytes = frameworkName(fw).toUtf8();
    QByteArray pathBytes = projectPath.toUtf8();
    QByteArray filterBytes = filter.toUtf8();

    char *result = rust_test_build_command(fwBytes.constData(), pathBytes.constData(), filterBytes.constData());
    if (!result) return QString();

    QString cmd = QString::fromUtf8(result);
    rust_free_string(result);
    return cmd;
}

// ── Test Execution ───────────────────────────────────────────────

void TestRunner::resetState()
{
    m_outputBuffer.clear();
    m_errorBuffer.clear();
}

void TestRunner::runAllTests(const QString &projectPath)
{
    m_projectPath = projectPath;
    m_currentFramework = detectFramework(projectPath);

    if (m_currentFramework == TestFramework::Unknown) {
        emit testOutput(tr("No test framework detected in %1").arg(projectPath));
        TestSuite suite;
        suite.framework = "unknown";
        emit testCompleted(suite);
        return;
    }

    QString cmd = buildCommand(m_currentFramework, projectPath);
    if (cmd.isEmpty()) return;

    resetState();
    emit testStarted(cmd);

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &TestRunner::onProcessReadyRead);
        connect(m_process, &QProcess::readyReadStandardError, this, &TestRunner::onProcessReadyReadStdErr);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &TestRunner::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &TestRunner::onProcessError);
    }

    m_process->setWorkingDirectory(projectPath);
    m_process->start("sh", {"-c", cmd});
}

void TestRunner::runTestFile(const QString &projectPath, const QString &testFile)
{
    m_projectPath = projectPath;
    m_currentFramework = detectFramework(projectPath);
    QString cmd = buildCommand(m_currentFramework, projectPath, testFile);
    if (cmd.isEmpty()) return;

    resetState();
    emit testStarted(cmd);

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &TestRunner::onProcessReadyRead);
        connect(m_process, &QProcess::readyReadStandardError, this, &TestRunner::onProcessReadyReadStdErr);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &TestRunner::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &TestRunner::onProcessError);
    }

    m_process->setWorkingDirectory(projectPath);
    m_process->start("sh", {"-c", cmd});
}

void TestRunner::runTestName(const QString &projectPath, const QString &filter)
{
    runAllTests(projectPath); // Use filter in buildCommand
    Q_UNUSED(filter);
}

void TestRunner::stopTests()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

// ── Process Slots (output parsing delegates to Rust) ─────────────

void TestRunner::onProcessReadyRead()
{
    QByteArray data = m_process->readAllStandardOutput();
    m_outputBuffer.append(data);

    const auto lines = QString::fromUtf8(data).split('\n');
    for (const auto &line : lines) {
        if (!line.trimmed().isEmpty()) {
            emit testOutput(line);
        }
    }
}

void TestRunner::onProcessReadyReadStdErr()
{
    QByteArray data = m_process->readAllStandardError();
    m_errorBuffer.append(data);

    const auto lines = QString::fromUtf8(data).split('\n');
    for (const auto &line : lines) {
        if (!line.trimmed().isEmpty()) {
            emit testOutput(line);
        }
    }
}

void TestRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);

    // Delegate output parsing to Rust backend
    QByteArray fwBytes = frameworkName(m_currentFramework).toUtf8();
    QByteArray outBytes = m_outputBuffer.toUtf8();

    char *jsonResult = rust_test_parse_output(fwBytes.constData(), outBytes.constData());

    TestSuite suite;
    suite.framework = frameworkName(m_currentFramework);

    if (jsonResult) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonResult));
        rust_free_string(jsonResult);

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            suite.name = obj["name"].toString();
            suite.framework = obj["framework"].toString();
            suite.passed = obj["passed"].toInt();
            suite.failed = obj["failed"].toInt();
            suite.skipped = obj["skipped"].toInt();
            suite.total = obj["total"].toInt();

            QJsonArray results = obj["results"].toArray();
            for (const auto &r : results) {
                QJsonObject ro = r.toObject();
                TestResult tr;
                tr.name = ro["name"].toString();
                tr.status = ro["status"].toString();
                tr.message = ro["message"].toString();
                tr.durationMs = ro["duration_ms"].toInt();
                suite.results.append(tr);
            }
        }
    }

    emit testCompleted(suite);
}

void TestRunner::onProcessError(QProcess::ProcessError error)
{
    QString errMsg;
    switch (error) {
    case QProcess::FailedToStart:   errMsg = tr("Failed to start test process"); break;
    case QProcess::Crashed:         errMsg = tr("Test process crashed"); break;
    case QProcess::Timedout:        errMsg = tr("Test process timed out"); break;
    case QProcess::WriteError:      errMsg = tr("Write error to test process"); break;
    case QProcess::ReadError:       errMsg = tr("Read error from test process"); break;
    default:                        errMsg = tr("Unknown test process error"); break;
    }

    emit testOutput(tr("ERROR: %1").arg(errMsg));
    TestSuite suite;
    suite.framework = frameworkName(m_currentFramework);
    suite.name = "Error";
    emit testCompleted(suite);
}
