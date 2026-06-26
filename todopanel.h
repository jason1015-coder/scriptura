#ifndef TODOPANEL_H
#define TODOPANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>

class TodoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TodoPanel(QWidget *parent = nullptr);
    ~TodoPanel();

private:
    QTextEdit *m_textEdit;
    QVBoxLayout *m_mainLayout;
};

#endif // TODOPANEL_H
