#ifndef TERMINALPANEL_H
#define TERMINALPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>

class TerminalPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel();

    void startShell(const QString &workingDir = QString());
    void stopShell();
    bool isRunning() const;

signals:
    void commandExecuted(const QString &command);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReturnPressed();
    void onClearClicked();
    void onWorkingDirChanged(const QString &dir);

private:
    QPlainTextEdit *m_outputEdit;
    QLineEdit *m_inputEdit;
    QPushButton *m_clearButton;
    QLabel *m_statusLabel;
    QVBoxLayout *m_mainLayout;
    QProcess *m_process;
    QString m_workingDir;
    bool m_processRunning;

    QString findShell() const;
    void appendOutput(const QString &text, bool isError = false);
    void processAnsiCodes(QString &text);
    QStringList m_commandHistory;
    int m_historyIndex;
};

#endif // TERMINALPANEL_H
