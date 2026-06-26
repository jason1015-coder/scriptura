#ifndef GITPANEL_H
#define GITPANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

class GitPanel : public QWidget
{
    Q_OBJECT
public:
    explicit GitPanel(QWidget *parent = nullptr);
    ~GitPanel();

    void setOutput(const QString &text);

signals:
    void commitRequested();
    void pushRequested();

private slots:
    void onCommitClicked();
    void onPushClicked();

private:
    QTextEdit *m_outputEdit;
    QVBoxLayout *m_mainLayout;
    QPushButton *m_commitButton;
    QPushButton *m_pushButton;
};

#endif // GITPANEL_H
