#ifndef FINDREPLACEBAR_H
#define FINDREPLACEBAR_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QRegularExpression>

class FindReplaceBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindReplaceBar(QWidget *parent = nullptr);
    void setEditor(QPlainTextEdit *editor);
    void setReplaceVisible(bool visible);
    bool isReplaceVisible() const;
    void findNext();
    void findPrev();
    // Reverse the control row so the bar mirrors with the rest of the window
    // in the mirrored layout.
    void setMirrored(bool mirrored);

signals:
    void replaceAllComplete(int count);
    void closeRequested();
    void matchesUpdated(int current, int total);

private slots:
    void onFindTextChanged(const QString &text);
    void onNextClicked();
    void onPrevClicked();
    void onReplaceClicked();
    void onReplaceAllClicked();
    void onToggleReplace();
    void onReturnPressed();

private:
    enum Mode { Find, Replace };

    void performFind(bool forward);
    void updateMatches();
    QTextDocument::FindFlags findFlags() const;
    QRegularExpression regularExpression() const;
    QTextCursor findFrom(QTextDocument *doc, int from, QTextDocument::FindFlags flags) const;

    QLineEdit *m_findEdit;
    QLineEdit *m_replaceEdit;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_replaceBtn;
    QPushButton *m_replaceAllBtn;
    QPushButton *m_toggleReplaceBtn;
    QCheckBox *m_caseSensitive;
    QCheckBox *m_wholeWord;
    QCheckBox *m_regex;
    QLabel *m_countLabel;
    QHBoxLayout *m_layout = nullptr;
    bool m_mirrored = false;
    QPlainTextEdit *m_editor;
    Mode m_mode;
};

#endif // FINDREPLACEBAR_H
