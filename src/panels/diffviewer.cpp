#include "diffviewer.h"
#include <QSplitter>
#include <QTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QTextStream>

DiffViewerWidget::DiffViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Navigation bar
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setContentsMargins(8, 4, 8, 4);

    QLabel *infoLabel = new QLabel(tr("Differences: 0"), this);
    infoLabel->setObjectName("diffInfoLabel");
    navLayout->addWidget(infoLabel);

    navLayout->addStretch();

    QPushButton *prevBtn = new QPushButton("◀", this);
    prevBtn->setFixedSize(28, 24);
    prevBtn->setToolTip(tr("Previous change"));
    connect(prevBtn, &QPushButton::clicked, this, &DiffViewerWidget::previousChange);
    navLayout->addWidget(prevBtn);

    QLabel *posLabel = new QLabel("0/0", this);
    posLabel->setObjectName("diffPosLabel");
    navLayout->addWidget(posLabel);

    QPushButton *nextBtn = new QPushButton("▶", this);
    nextBtn->setFixedSize(28, 24);
    nextBtn->setToolTip(tr("Next change"));
    connect(nextBtn, &QPushButton::clicked, this, &DiffViewerWidget::nextChange);
    navLayout->addWidget(nextBtn);

    mainLayout->addLayout(navLayout);

    // Splitter with two text edits
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_leftEdit = new QTextEdit(this);
    m_leftEdit->setReadOnly(true);
    m_leftEdit->setObjectName("diffLeftEdit");
    m_leftEdit->setLineWrapMode(QTextEdit::NoWrap);

    m_rightEdit = new QTextEdit(this);
    m_rightEdit->setReadOnly(true);
    m_rightEdit->setObjectName("diffRightEdit");
    m_rightEdit->setLineWrapMode(QTextEdit::NoWrap);

    m_splitter->addWidget(m_leftEdit);
    m_splitter->addWidget(m_rightEdit);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter, 1);

    // Synchronized scrolling
    connect(m_leftEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DiffViewerWidget::onLeftScrollBarChanged);
    connect(m_rightEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DiffViewerWidget::onRightScrollBarChanged);
}

void DiffViewerWidget::compareFiles(const QString &fileLeft, const QString &fileRight)
{
    auto readFile = [](const QString &path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        QTextStream stream(&f);
        return stream.readAll();
    };

    m_leftTitle = fileLeft;
    m_rightTitle = fileRight;
    compareTexts(readFile(fileLeft), readFile(fileRight), fileLeft, fileRight);
}

void DiffViewerWidget::compareTexts(const QString &left, const QString &right,
                                     const QString &leftTitle, const QString &rightTitle)
{
    m_leftTitle = leftTitle;
    m_rightTitle = rightTitle;
    computeDiff(left, right);
    renderDiff();
}

void DiffViewerWidget::computeDiff(const QString &left, const QString &right)
{
    m_hunks.clear();
    QStringList leftLines = left.split('\n');
    QStringList rightLines = right.split('\n');

    // Simple LCS-based diff
    int leftLen = leftLines.size();
    int rightLen = rightLines.size();

    // Build LCS table
    QVector<QVector<int>> dp(leftLen + 1, QVector<int>(rightLen + 1, 0));
    for (int i = 1; i <= leftLen; ++i) {
        for (int j = 1; j <= rightLen; ++j) {
            if (leftLines[i - 1] == rightLines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = qMax(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Trace back to find diff hunks
    QList<QPair<int, int>> changes; // (leftIdx, rightIdx) pairs
    int i = leftLen, j = rightLen;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && leftLines[i - 1] == rightLines[j - 1]) {
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            changes.prepend({-1, j - 1}); // added
            --j;
        } else {
            changes.prepend({i - 1, -1}); // removed
            --i;
        }
    }

    // Group changes into hunks
    DiffHunk currentHunk;
    bool inHunk = false;

    for (int idx = 0; idx < changes.size(); ++idx) {
        auto [leftIdx, rightIdx] = changes[idx];
        if (leftIdx >= 0 || rightIdx >= 0) {
            if (!inHunk) {
                currentHunk = DiffHunk();
                currentHunk.leftStart = qMax(0, (leftIdx >= 0 ? leftIdx : rightIdx) - 2);
                currentHunk.rightStart = qMax(0, (rightIdx >= 0 ? rightIdx : leftIdx) - 2);
                inHunk = true;
            }
            if (leftIdx >= 0) {
                currentHunk.leftLines.append(leftLines[leftIdx]);
                currentHunk.changeTypes.append("removed");
            } else {
                currentHunk.leftLines.append(QString());
                currentHunk.changeTypes.append("added");
            }
            if (rightIdx >= 0) {
                currentHunk.rightLines.append(rightLines[rightIdx]);
            } else {
                currentHunk.rightLines.append(QString());
            }
        } else {
            if (inHunk) {
                currentHunk.leftCount = currentHunk.leftLines.size();
                currentHunk.rightCount = currentHunk.rightLines.size();
                m_hunks.append(currentHunk);
                inHunk = false;
            }
        }
    }
    if (inHunk) {
        currentHunk.leftCount = currentHunk.leftLines.size();
        currentHunk.rightCount = currentHunk.rightLines.size();
        m_hunks.append(currentHunk);
    }
}

void DiffViewerWidget::renderDiff()
{
    m_leftEdit->clear();
    m_rightEdit->clear();

    QTextCharFormat unchangedFormat;
    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(144, 238, 144, 80));
    QTextCharFormat removedFormat;
    removedFormat.setBackground(QColor(255, 182, 193, 80));

    // Simple rendering: show all hunks
    for (const DiffHunk &hunk : m_hunks) {
        for (int k = 0; k < hunk.leftLines.size(); ++k) {
            QTextCursor leftCursor(m_leftEdit->document());
            leftCursor.movePosition(QTextCursor::End);
            if (hunk.changeTypes.value(k) == "removed") {
                leftCursor.insertText("- " + hunk.leftLines[k] + "\n");
            } else {
                leftCursor.insertText("  " + hunk.leftLines[k] + "\n");
            }
        }
        for (int k = 0; k < hunk.rightLines.size(); ++k) {
            QTextCursor rightCursor(m_rightEdit->document());
            rightCursor.movePosition(QTextCursor::End);
            if (k < hunk.leftLines.size() && hunk.leftLines[k].isEmpty()) {
                rightCursor.insertText("+ " + hunk.rightLines[k] + "\n");
            } else {
                rightCursor.insertText("  " + hunk.rightLines[k] + "\n");
            }
        }
    }

    m_currentHunkIndex = m_hunks.isEmpty() ? -1 : 0;
}

void DiffViewerWidget::nextChange()
{
    if (m_hunks.isEmpty()) return;
    m_currentHunkIndex = (m_currentHunkIndex + 1) % m_hunks.size();
    emit changeNavigated(m_currentHunkIndex, m_hunks.size());
}

void DiffViewerWidget::previousChange()
{
    if (m_hunks.isEmpty()) return;
    m_currentHunkIndex = (m_currentHunkIndex - 1 + m_hunks.size()) % m_hunks.size();
    emit changeNavigated(m_currentHunkIndex, m_hunks.size());
}

void DiffViewerWidget::onLeftScrollBarChanged(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_rightEdit->verticalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void DiffViewerWidget::onRightScrollBarChanged(int value)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    m_leftEdit->verticalScrollBar()->setValue(value);
    m_syncingScroll = false;
}

void DiffViewerWidget::highlightLine(QTextEdit *edit, int line, const QColor &color)
{
    QTextCursor cursor(edit->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setBackground(color);
    cursor.mergeCharFormat(fmt);
}

void DiffViewerWidget::syncScroll(QScrollBar *source, QScrollBar *target)
{
    if (m_syncingScroll) return;
    m_syncingScroll = true;
    int sourceMax = source->maximum();
    int targetMax = target->maximum();
    if (sourceMax > 0 && targetMax > 0) {
        double ratio = static_cast<double>(source->value()) / sourceMax;
        target->setValue(static_cast<int>(ratio * targetMax));
    } else {
        target->setValue(source->value());
    }
    m_syncingScroll = false;
}
