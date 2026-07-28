#include "regextester.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QMessageBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QClipboard>

RegexTester::RegexTester(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    connect(m_patternEdit, &QLineEdit::textChanged, this, &RegexTester::onPatternChanged);
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &RegexTester::onTextChanged);
    connect(m_caseInsensitive, &QCheckBox::toggled, this, &RegexTester::onFlagChanged);
    connect(m_dotMatchesNewline, &QCheckBox::toggled, this, &RegexTester::onFlagChanged);
    connect(m_multiline, &QCheckBox::toggled, this, &RegexTester::onFlagChanged);
    connect(m_global, &QCheckBox::toggled, this, &RegexTester::onFlagChanged);
    connect(m_saveButton, &QPushButton::clicked, this, &RegexTester::onSavePattern);
    connect(m_loadButton, &QPushButton::clicked, this, &RegexTester::onLoadPattern);
    connect(m_copyButton, &QPushButton::clicked, this, &RegexTester::onCopyPattern);
}

void RegexTester::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Pattern input
    QHBoxLayout *patternLayout = new QHBoxLayout();
    QLabel *patternLabel = new QLabel(tr("Pattern:"), this);
    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setPlaceholderText(tr("Enter regex pattern..."));
    m_patternEdit->setStyleSheet("QLineEdit { font-family: monospace; padding: 4px; }");
    patternLayout->addWidget(patternLabel);
    patternLayout->addWidget(m_patternEdit, 1);
    mainLayout->addLayout(patternLayout);
    
    // Flags
    QHBoxLayout *flagsLayout = new QHBoxLayout();
    m_caseInsensitive = new QCheckBox(tr("Case Insensitive (i)"), this);
    m_dotMatchesNewline = new QCheckBox(tr("Dot Matches Newline (s)"), this);
    m_multiline = new QCheckBox(tr("Multiline (m)"), this);
    m_global = new QCheckBox(tr("Global (g)"), this);
    flagsLayout->addWidget(m_caseInsensitive);
    flagsLayout->addWidget(m_dotMatchesNewline);
    flagsLayout->addWidget(m_multiline);
    flagsLayout->addWidget(m_global);
    flagsLayout->addStretch();
    mainLayout->addLayout(flagsLayout);
    
    // Splitter for text and matches
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    
    // Sample text
    QWidget *textWidget = new QWidget(this);
    QVBoxLayout *textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *textLabel = new QLabel(tr("Sample Text:"), this);
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(tr("Enter text to test against..."));
    m_textEdit->setStyleSheet("QPlainTextEdit { font-family: monospace; }");
    textLayout->addWidget(textLabel);
    textLayout->addWidget(m_textEdit, 1);
    splitter->addWidget(textWidget);
    
    // Match list
    QWidget *matchWidget = new QWidget(this);
    QVBoxLayout *matchLayout = new QVBoxLayout(matchWidget);
    matchLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *matchLabel = new QLabel(tr("Matches:"), this);
    m_matchList = new QListWidget(this);
    m_matchList->setStyleSheet("QListWidget { font-family: monospace; }");
    matchLayout->addWidget(matchLabel);
    matchLayout->addWidget(m_matchList, 1);
    splitter->addWidget(matchWidget);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);
    
    // Status and actions
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("No matches"), this);
    m_statusLabel->setStyleSheet("QLabel { color: palette(mid); }");
    m_saveButton = new QPushButton(tr("Save Pattern"), this);
    m_loadButton = new QPushButton(tr("Load Pattern"), this);
    m_copyButton = new QPushButton(tr("Copy Pattern"), this);
    bottomLayout->addWidget(m_statusLabel, 1);
    bottomLayout->addWidget(m_saveButton);
    bottomLayout->addWidget(m_loadButton);
    bottomLayout->addWidget(m_copyButton);
    mainLayout->addLayout(bottomLayout);
}

void RegexTester::onPatternChanged()
{
    performMatch();
}

void RegexTester::onTextChanged()
{
    performMatch();
}

void RegexTester::onFlagChanged()
{
    performMatch();
}

void RegexTester::performMatch()
{
    QString pattern = m_patternEdit->text();
    QString text = m_textEdit->toPlainText();
    
    m_matches.clear();
    
    if (pattern.isEmpty() || text.isEmpty()) {
        highlightMatches();
        updateMatchList();
        updateStatus();
        return;
    }
    
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (m_caseInsensitive->isChecked()) options |= QRegularExpression::CaseInsensitiveOption;
    if (m_dotMatchesNewline->isChecked()) options |= QRegularExpression::DotMatchesEverythingOption;
    if (m_multiline->isChecked()) options |= QRegularExpression::MultilineOption;
    
    QRegularExpression regex(pattern, options);
    if (!regex.isValid()) {
        m_statusLabel->setText(tr("Invalid pattern: %1").arg(regex.errorString()));
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        return;
    }
    
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        Match m;
        m.start = match.capturedStart();
        m.length = match.capturedLength();
        m.text = match.captured();
        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
            m.groups.append(match.captured(i));
        }
        m_matches.append(m);
    }
    
    highlightMatches();
    updateMatchList();
    updateStatus();
}

void RegexTester::highlightMatches()
{
    QTextCursor cursor(m_textEdit->document());
    cursor.select(QTextCursor::Document);
    
    // Clear existing highlighting
    QTextCharFormat clearFormat;
    clearFormat.clearBackground();
    cursor.mergeCharFormat(clearFormat);
    
    // Highlight matches
    QColor matchColor(100, 149, 237, 80);  // Light blue with transparency
    for (const Match &m : m_matches) {
        QTextCursor matchCursor(m_textEdit->document());
        matchCursor.setPosition(m.start);
        matchCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m.length);
        
        QTextCharFormat format;
        format.setBackground(matchColor);
        matchCursor.mergeCharFormat(format);
    }
}

void RegexTester::updateMatchList()
{
    m_matchList->clear();
    
    for (int i = 0; i < m_matches.size(); ++i) {
        const Match &m = m_matches[i];
        QString itemText = QString("[%1] \"%2\"").arg(i + 1).arg(m.text);
        if (!m.groups.isEmpty()) {
            itemText += QString(" (Groups: %1)").arg(m.groups.join(", "));
        }
        m_matchList->addItem(itemText);
    }
}

void RegexTester::updateStatus()
{
    if (m_matches.isEmpty()) {
        m_statusLabel->setText(tr("No matches"));
        m_statusLabel->setStyleSheet("QLabel { color: palette(mid); }");
    } else {
        m_statusLabel->setText(tr("%1 match(es) found").arg(m_matches.size()));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
    }
}

void RegexTester::onSavePattern()
{
    QString pattern = m_patternEdit->text();
    if (pattern.isEmpty()) return;
    
    QSettings settings;
    QJsonArray patterns = QJsonDocument::fromJson(settings.value("regex/patterns").toByteArray()).array();
    
    QJsonObject obj;
    obj["pattern"] = pattern;
    obj["caseInsensitive"] = m_caseInsensitive->isChecked();
    obj["dotMatchesNewline"] = m_dotMatchesNewline->isChecked();
    obj["multiline"] = m_multiline->isChecked();
    obj["global"] = m_global->isChecked();
    patterns.append(obj);
    
    settings.setValue("regex/patterns", QJsonDocument(patterns).toJson());
    emit patternSaved(QString(), pattern);
}

void RegexTester::onLoadPattern()
{
    QSettings settings;
    QByteArray data = settings.value("regex/patterns").toByteArray();
    if (data.isEmpty()) {
        QMessageBox::information(this, tr("Load Pattern"), tr("No saved patterns found."));
        return;
    }
    
    QJsonArray patterns = QJsonDocument::fromJson(data).array();
    if (patterns.isEmpty()) return;
    
    // Load the most recent pattern
    QJsonObject last = patterns.last().toObject();
    m_patternEdit->setText(last["pattern"].toString());
    m_caseInsensitive->setChecked(last["caseInsensitive"].toBool());
    m_dotMatchesNewline->setChecked(last["dotMatchesNewline"].toBool());
    m_multiline->setChecked(last["multiline"].toBool());
    m_global->setChecked(last["global"].toBool());
}

void RegexTester::onCopyPattern()
{
    QString pattern = m_patternEdit->text();
    if (!pattern.isEmpty()) {
        QApplication::clipboard()->setText(pattern);
        emit regexCopied(pattern);
    }
}
