#include "todopanel.h"
#include <QApplication>

TodoPanel::TodoPanel(QWidget *parent)
    : QWidget(parent)
    , m_textEdit(new QTextEdit(this))
    , m_mainLayout(new QVBoxLayout(this))
{
    setObjectName("todoPanel");

    m_textEdit->setPlaceholderText(tr("Enter your todo items here..."));
    m_textEdit->setStyleSheet(
        "QTextEdit { border: none; background: palette(base); }"
    );

    m_mainLayout->addWidget(m_textEdit);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);
}

TodoPanel::~TodoPanel()
{
}
