#ifndef TESTPANEL_H
#define TESTPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCursor>

class TestRunner;

class TestPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TestPanel(QWidget *parent = nullptr);

    void setProjectPath(const QString &path);
    QString projectPath() const { return m_projectPath; }

    // Run tests
    void runAllTests();
    void runSelectedTest();

signals:
    void testRunStarted(const QString &command);
    void testRunFinished(int passed, int failed, int skipped);
    void jumpToTest(const QString &file, int line);

private slots:
    void onRunClicked();
    void onRunSelectedClicked();
    void onStopClicked();
    void onClearClicked();
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onTestStarted(const QString &command);
    void onTestOutput(const QString &line);
    void onTestCompleted(const QByteArray &suiteJson);
    void populateTree(const QByteArray &suiteJson);

private:
    void setupUI();
    void populateTree(const QVariant &suite);
    void updateSummary(int passed, int failed, int skipped);
    void addOutputLine(const QString &line, bool isError = false);

    QTreeWidget *m_testTree;
    QPushButton *m_runBtn;
    QPushButton *m_runSelectedBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;
    QLineEdit *m_filterEdit;
    QLabel *m_summaryLabel;
    QPlainTextEdit *m_outputEdit;

    TestRunner *m_runner;
    QString m_projectPath;
};

#endif // TESTPANEL_H
