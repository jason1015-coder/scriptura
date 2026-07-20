#include "findreplace.h"

#include <QHBoxLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QApplication>
#include <QKeyEvent>

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent)
    , m_findEdit(new QLineEdit(this))
    , m_replaceEdit(new QLineEdit(this))
    , m_prevBtn(new QPushButton(tr("\u25B2"), this))
    , m_nextBtn(new QPushButton(tr("\u25BC"), this))
    , m_replaceBtn(new QPushButton(tr("Replace"), this))
    , m_replaceAllBtn(new QPushButton(tr("Replace All"), this))
    , m_toggleReplaceBtn(new QPushButton(tr("\u21C5"), this))
    , m_caseSensitive(new QCheckBox(tr("Aa"), this))
    , m_wholeWord(new QCheckBox(tr("W"), this))
    , m_regex(new QCheckBox(tr(".*"), this))
    , m_countLabel(new QLabel(this))
    , m_editor(nullptr)
    , m_mode(Find)
{
    setObjectName("findReplaceBar");

    m_findEdit->setPlaceholderText(tr("Find..."));
    m_findEdit->setClearButtonEnabled(true);
    m_replaceEdit->setPlaceholderText(tr("Replace..."));
    m_replaceEdit->setClearButtonEnabled(true);

    m_prevBtn->setFixedSize(24, 24);
    m_prevBtn->setToolTip(tr("Previous match"));
    m_nextBtn->setFixedSize(24, 24);
    m_nextBtn->setToolTip(tr("Next match"));
    m_replaceBtn->setToolTip(tr("Replace current match"));
    m_replaceAllBtn->setToolTip(tr("Replace all matches"));
    m_toggleReplaceBtn->setFixedSize(24, 24);
    m_toggleReplaceBtn->setToolTip(tr("Toggle replace field"));
    m_caseSensitive->setToolTip(tr("Case sensitive"));
    m_wholeWord->setToolTip(tr("Whole word"));
    m_regex->setToolTip(tr("Regular expression"));

    m_caseSensitive->setFixedSize(24, 24);
    m_wholeWord->setFixedSize(24, 24);
    m_regex->setFixedSize(24, 24);

    m_countLabel->setFixedWidth(60);
    m_countLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_findEdit, 1);
    layout->addWidget(m_replaceEdit, 1);
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_nextBtn);
    layout->addWidget(m_toggleReplaceBtn);
    layout->addWidget(m_replaceBtn);
    layout->addWidget(m_replaceAllBtn);
    layout->addWidget(m_caseSensitive);
    layout->addWidget(m_wholeWord);
    layout->addWidget(m_regex);
    layout->addWidget(m_countLabel);

    setReplaceVisible(false);

    connect(m_findEdit, &QLineEdit::textChanged, this, &FindReplaceBar::onFindTextChanged);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::onReturnPressed);
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::onReturnPressed);
    connect(m_prevBtn, &QPushButton::clicked, this, &FindReplaceBar::findPrev);
    connect(m_nextBtn, &QPushButton::clicked, this, &FindReplaceBar::findNext);
    connect(m_replaceBtn, &QPushButton::clicked, this, &FindReplaceBar::onReplaceClicked);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceBar::onReplaceAllClicked);
    connect(m_toggleReplaceBtn, &QPushButton::clicked, this, &FindReplaceBar::onToggleReplace);
    connect(m_caseSensitive, &QCheckBox::checkStateChanged, this, &FindReplaceBar::updateMatches);
    connect(m_wholeWord, &QCheckBox::checkStateChanged, this, &FindReplaceBar::updateMatches);
    connect(m_regex, &QCheckBox::checkStateChanged, this, &FindReplaceBar::updateMatches);
}

void FindReplaceBar::setEditor(QPlainTextEdit *editor)
{
    m_editor = editor;
    m_findEdit->clear();
    m_replaceEdit->clear();
    m_countLabel->clear();
    if (m_editor)
        m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
}

void FindReplaceBar::setReplaceVisible(bool visible)
{
    m_replaceEdit->setVisible(visible);
    m_replaceBtn->setVisible(visible);
    m_replaceAllBtn->setVisible(visible);
    if (visible)
        m_mode = Replace;
    else
        m_mode = Find;
}

bool FindReplaceBar::isReplaceVisible() const
{
    return m_mode == Replace;
}

void FindReplaceBar::findNext()
{
    if (!m_editor)
        return;
    performFind(true);
}

void FindReplaceBar::findPrev()
{
    if (!m_editor)
        return;
    performFind(false);
}

void FindReplaceBar::onFindTextChanged(const QString &text)
{
    Q_UNUSED(text)
    if (!m_editor)
        return;
    updateMatches();
}

void FindReplaceBar::onNextClicked()
{
    findNext();
}

void FindReplaceBar::onPrevClicked()
{
    findPrev();
}

void FindReplaceBar::onReplaceClicked()
{
    if (!m_editor || m_mode != Replace)
        return;

    QString findText = m_findEdit->text();
    if (findText.isEmpty())
        return;

    QTextDocument *doc = m_editor->document();
    QTextCursor cursor = m_editor->textCursor();

    QTextDocument::FindFlags flags = findFlags();
    bool matches = false;
    if (cursor.hasSelection()) {
        QTextCursor match = findFrom(doc, cursor.selectionStart(), flags);
        if (!match.isNull() && match.selectionStart() == cursor.selectionStart()
                && match.selectionEnd() == cursor.selectionEnd()) {
            matches = true;
        }
    }
    if (!matches && !m_regex->isChecked()) {
        matches = (cursor.selectedText() == findText);
    }

    if (matches) {
        cursor.insertText(m_replaceEdit->text());
        m_editor->setTextCursor(cursor);
    }

    performFind(true);
}

void FindReplaceBar::onReplaceAllClicked()
{
    if (!m_editor || m_mode != Replace)
        return;

    QString findText = m_findEdit->text();
    if (findText.isEmpty())
        return;

    QTextDocument *doc = m_editor->document();
    QTextDocument::FindFlags flags = findFlags();

    int count = 0;
    int pos = 0;
    QTextCursor cursor;
    while (true) {
        cursor = findFrom(doc, pos, flags);
        if (cursor.isNull())
            break;
        cursor.insertText(m_replaceEdit->text());
        count++;
        pos = cursor.position();
    }

    emit replaceAllComplete(count);
    updateMatches();
}

void FindReplaceBar::onToggleReplace()
{
    setReplaceVisible(!isReplaceVisible());
}

void FindReplaceBar::onReturnPressed()
{
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
        findPrev();
    else
        findNext();
}

void FindReplaceBar::performFind(bool forward)
{
    if (!m_editor)
        return;

    QString findText = m_findEdit->text();
    if (findText.isEmpty()) {
        m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
        m_countLabel->clear();
        return;
    }

    QTextDocument *doc = m_editor->document();
    QTextCursor cursor = m_editor->textCursor();
    QTextDocument::FindFlags flags = findFlags();
    if (!forward)
        flags |= QTextDocument::FindBackward;

    int pos = cursor.selectionStart();
    cursor = findFrom(doc, pos, flags);
    if (cursor.isNull()) {
        if (forward) {
            cursor = findFrom(doc, 0, flags);
        } else {
            cursor = findFrom(doc, doc->characterCount(), flags);
        }
    }

    if (!cursor.isNull()) {
        m_editor->setTextCursor(cursor);
        m_editor->centerCursor();
    }

    updateMatches();
}

void FindReplaceBar::updateMatches()
{
    if (!m_editor)
        return;

    QString findText = m_findEdit->text();
    if (findText.isEmpty()) {
        m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
        m_countLabel->setText("0/0");
        emit matchesUpdated(0, 0);
        return;
    }

    QTextDocument *doc = m_editor->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    QTextDocument::FindFlags flags = findFlags();

    QList<QTextEdit::ExtraSelection> selections;
    int matchIndex = 0;
    int activeIndex = 0;
    QTextCursor activeCursor = m_editor->textCursor();

    while (true) {
        cursor = findFrom(doc, cursor.position(), flags);
        if (cursor.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = cursor;
        QColor highlightColor = qApp->palette().color(QPalette::Highlight);
        if (cursor.selectionStart() == activeCursor.selectionStart() ||
            cursor.selectionEnd() == activeCursor.selectionEnd()) {
            activeIndex = matchIndex;
            sel.format.setBackground(QColor(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 200));
            sel.format.setForeground(qApp->palette().color(QPalette::HighlightedText));
        } else {
            sel.format.setBackground(QColor(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 70));
        }
        selections.append(sel);
        matchIndex++;
        cursor.setPosition(cursor.selectionEnd());
    }

    if (selections.isEmpty()) {
        m_countLabel->setText("0/0");
        emit matchesUpdated(0, 0);
    } else {
        m_countLabel->setText(QString("%1/%2").arg(activeIndex + 1).arg(selections.size()));
        emit matchesUpdated(activeIndex + 1, selections.size());
    }

    m_editor->setExtraSelections(selections);
}

QTextDocument::FindFlags FindReplaceBar::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (m_caseSensitive->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWord->isChecked())
        flags |= QTextDocument::FindWholeWords;
    return flags;
}

QTextCursor FindReplaceBar::findFrom(QTextDocument *doc, int from, QTextDocument::FindFlags flags) const
{
    if (m_regex->isChecked()) {
        QRegularExpression re = regularExpression();
        if (re.isValid()) {
            QTextCursor cursor(doc);
            cursor.setPosition(from);
            return doc->find(re, cursor, flags);
        }
    }
    QString text = m_findEdit->text();
    if (text.isEmpty())
        return QTextCursor();
    QTextCursor cursor(doc);
    cursor.setPosition(from);
    return doc->find(text, cursor, flags);
}

QRegularExpression FindReplaceBar::regularExpression() const
{
    if (m_regex->isChecked()) {
        QString pattern = m_findEdit->text();
        QRegularExpression::PatternOptions opts;
        if (!m_caseSensitive->isChecked())
            opts |= QRegularExpression::CaseInsensitiveOption;
        return QRegularExpression(pattern, opts);
    }
    return QRegularExpression();
}
