#include "statusbarwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("statusBarWidget");
    setFixedHeight(28);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 0, 12, 0);
    mainLayout->setSpacing(16);

    // File name
    m_fileLabel = new QLabel(this);
    m_fileLabel->setObjectName("statusFileLabel");
    m_fileLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addWidget(m_fileLabel);

    // Modified indicator
    mainLayout->addStretch();

    // Language
    m_languageLabel = new QLabel(this);
    m_languageLabel->setObjectName("statusLanguageLabel");
    m_languageLabel->setAlignment(Qt::AlignCenter);
    m_languageLabel->setCursor(Qt::PointingHandCursor);
    m_languageLabel->setToolTip(tr("Click to change language"));
    connect(m_languageLabel, &QLabel::linkActivated, this, &StatusBarWidget::languageClicked);
    mainLayout->addWidget(m_languageLabel);

    // Encoding
    m_encodingLabel = new QLabel(this);
    m_encodingLabel->setObjectName("statusEncodingLabel");
    m_encodingLabel->setAlignment(Qt::AlignCenter);
    m_encodingLabel->setCursor(Qt::PointingHandCursor);
    m_encodingLabel->setToolTip(tr("Click to change encoding"));
    connect(m_encodingLabel, &QLabel::linkActivated, this, &StatusBarWidget::encodingClicked);
    mainLayout->addWidget(m_encodingLabel);

    // Line ending
    m_lineEndingLabel = new QLabel(this);
    m_lineEndingLabel->setObjectName("statusLineEndingLabel");
    m_lineEndingLabel->setAlignment(Qt::AlignCenter);
    m_lineEndingLabel->setCursor(Qt::PointingHandCursor);
    m_lineEndingLabel->setToolTip(tr("Click to change line ending"));
    connect(m_lineEndingLabel, &QLabel::linkActivated, this, &StatusBarWidget::lineEndingClicked);
    mainLayout->addWidget(m_lineEndingLabel);

    // Indentation
    m_indentLabel = new QLabel(this);
    m_indentLabel->setObjectName("statusIndentLabel");
    m_indentLabel->setAlignment(Qt::AlignCenter);
    m_indentLabel->setCursor(Qt::PointingHandCursor);
    m_indentLabel->setToolTip(tr("Click to change indentation"));
    connect(m_indentLabel, &QLabel::linkActivated, this, &StatusBarWidget::indentationClicked);
    mainLayout->addWidget(m_indentLabel);

    // Separator
    QFrame *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep1);

    // Git branch
    m_gitBranchLabel = new QLabel("main", this);
    m_gitBranchLabel->setObjectName("statusGitLabel");
    m_gitBranchLabel->setAlignment(Qt::AlignCenter);
    m_gitBranchLabel->setCursor(Qt::PointingHandCursor);
    m_gitBranchLabel->setToolTip(tr("Click for git options"));
    connect(m_gitBranchLabel, &QLabel::linkActivated, this, &StatusBarWidget::gitBranchClicked);
    mainLayout->addWidget(m_gitBranchLabel);

    // Separator
    QFrame *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep2);

    // Error/warning count
    m_errorLabel = new QLabel("0 ⚠ 0 ✗", this);
    m_errorLabel->setObjectName("statusErrorLabel");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setCursor(Qt::PointingHandCursor);
    connect(m_errorLabel, &QLabel::linkActivated, this, &StatusBarWidget::errorCountClicked);
    mainLayout->addWidget(m_errorLabel);

    // Line count
    m_lineCountLabel = new QLabel("Ln 1, Col 1", this);
    m_lineCountLabel->setObjectName("statusPositionLabel");
    m_lineCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_lineCountLabel);
}

void StatusBarWidget::setLanguage(const QString &language)
{
    m_languageLabel->setText(QStringLiteral("<a href='lang' style='color:inherit;text-decoration:none;'>%1</a>").arg(language));
}

void StatusBarWidget::setEncoding(const QString &encoding)
{
    m_encodingLabel->setText(QStringLiteral("<a href='enc' style='color:inherit;text-decoration:none;'>%1</a>").arg(encoding));
}

void StatusBarWidget::setLineEnding(const QString &ending)
{
    m_lineEndingLabel->setText(QStringLiteral("<a href='le' style='color:inherit;text-decoration:none;'>%1</a>").arg(ending));
}

void StatusBarWidget::setIndentation(const QString &indent)
{
    m_indentLabel->setText(QStringLiteral("<a href='indent' style='color:inherit;text-decoration:none;'>%1</a>").arg(indent));
}

void StatusBarWidget::setCursorPosition(int line, int column)
{
    m_lineCountLabel->setText(QString("Ln %1, Col %2").arg(line).arg(column));
}

void StatusBarWidget::setGitBranch(const QString &branch)
{
    m_gitBranchLabel->setText("⎇ " + branch);
}

void StatusBarWidget::setErrorCount(int errors, int warnings)
{
    m_errorLabel->setText(QString("%1 ⚠  %2 ✗").arg(warnings).arg(errors));
}

void StatusBarWidget::setLineCount(int lines)
{
    // Integrated into position label or as separate tooltip
    m_lineCountLabel->setToolTip(tr("%1 lines").arg(lines));
}

void StatusBarWidget::setFileName(const QString &name)
{
    m_fileLabel->setText(name);
}

void StatusBarWidget::setModified(bool modified)
{
    QString name = m_fileLabel->text();
    if (modified && !name.startsWith("● ")) {
        m_fileLabel->setText("● " + name);
    } else if (!modified && name.startsWith("● ")) {
        m_fileLabel->setText(name.mid(2));
    }
}
