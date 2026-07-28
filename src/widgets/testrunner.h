#ifndef TESTRUNNER_H
#define TESTRUNNER_H

#include <QObject>
#include <QProcess>
#include <QMap>
#include <QStringList>

struct TestResult {
    QString name;
    QString className;
    QString status;  // "passed", "failed", "skipped", "error"
    QString message; // error/failure message
    int durationMs = 0;
};

struct TestSuite {
    QString name;
    QString framework; // pytest, jest, cargo, go, cmake
    QList<TestResult> results;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int total = 0;
};

enum class TestFramework {
    Unknown,
    Pytest,
    Jest,
    CargoTest,
    GoTest,
    CMakeCTest
};

class TestRunner : public QObject
{
    Q_OBJECT
public:
    explicit TestRunner(QObject *parent = nullptr);
    ~TestRunner();

    // Detect test framework from project files
    TestFramework detectFramework(const QString &projectPath) const;
    QString frameworkName(TestFramework fw) const;

    // Build and run tests
    void runAllTests(const QString &projectPath);
    void runTestFile(const QString &projectPath, const QString &testFile);
    void runTestName(const QString &projectPath, const QString &filter);
    void stopTests();

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }
    TestFramework currentFramework() const { return m_currentFramework; }

signals:
    void testStarted(const QString &command);
    void testOutput(const QString &line);
    void testCompleted(const TestSuite &suite);
    void testProgress(int current, int total);

private slots:
    void onProcessReadyRead();
    void onProcessReadyReadStdErr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    TestFramework m_currentFramework;
    QString m_outputBuffer;
    QString m_errorBuffer;
    QString m_projectPath;

    // Framework detection helpers
    bool hasPackageJson(const QString &path) const;
    bool hasCargoToml(const QString &path) const;
    bool hasGoMod(const QString &path) const;
    bool hasCMakeLists(const QString &path) const;
    bool hasPytestConfig(const QString &path) const;

    // Command builders
    QString buildCommand(TestFramework fw, const QString &projectPath,
                         const QString &filter = QString()) const;

    // Output parsers
    TestSuite parsePytestOutput(const QString &output);
    TestSuite parseJestOutput(const QString &output);
    TestSuite parseCargoTestOutput(const QString &output);
    TestSuite parseGoTestOutput(const QString &output);

    void resetState();
};

#endif // TESTRUNNER_H
