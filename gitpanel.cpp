#include "gitpanel.h"
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>

GitPanel::GitPanel(QWidget *parent)
    : QWidget(parent)
    , m_outputEdit(new QTextEdit(this))
    , m_mainLayout(new QVBoxLayout(this))
    , m_commitButton(new QPushButton(tr("Commit"), this))
    , m_pushButton(new QPushButton(tr("Push"), this))
{
    setObjectName("gitPanel");

    // Header with action buttons
    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(4);

    headerLayout->addWidget(m_commitButton);
    headerLayout->addWidget(m_pushButton);
    headerLayout->addStretch();

    // Output area
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setPlaceholderText(tr("Git output will appear here..."));
    m_outputEdit->setStyleSheet(
        "QTextEdit { border: none; background: palette(base); font-family: monospace; }"
    );

    m_mainLayout->addWidget(headerWidget);
    m_mainLayout->addWidget(m_outputEdit);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    connect(m_commitButton, &QPushButton::clicked, this, &GitPanel::onCommitClicked);
    connect(m_pushButton, &QPushButton::clicked, this, &GitPanel::onPushClicked);
}

GitPanel::~GitPanel()
{
}

void GitPanel::setOutput(const QString &text)
{
    m_outputEdit->setPlainText(text);
}

void GitPanel::onCommitClicked()
{
    emit commitRequested();
}

void GitPanel::onPushClicked()
{
    emit pushRequested();
}
