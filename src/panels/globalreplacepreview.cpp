#include "globalreplacepreview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMessageBox>
#include <QApplication>

GlobalReplacePreview::GlobalReplacePreview(QWidget *parent)
    : QWidget(parent)
    , m_totalMatches(0)
    , m_selectedMatches(0)
{
    setupUI();
    connect(m_searchButton, &QPushButton::clicked, this, &GlobalReplacePreview::onSearchClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &GlobalReplacePreview::onApplyClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &GlobalReplacePreview::onCancelClicked);
    connect(m_selectAllButton, &QPushButton::clicked, this, &GlobalReplacePreview::onSelectAllClicked);
    connect(m_deselectAllButton, &QPushButton::clicked, this, &GlobalReplacePreview::onDeselectAllClicked);
    connect(m_resultsTree, &QTreeWidget::itemDoubleClicked, this, &GlobalReplacePreview::onItemDoubleClicked);
}

void GlobalReplacePreview::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Search/Replace inputs
    QGridLayout *inputLayout = new QGridLayout();
    
    QLabel *searchLabel = new QLabel(tr("Search:"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Enter search pattern..."));
    
    QLabel *replaceLabel = new QLabel(tr("Replace:"), this);
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Enter replacement text..."));
    
    inputLayout->addWidget(searchLabel, 0, 0);
    inputLayout->addWidget(m_searchEdit, 0, 1);
    inputLayout->addWidget(replaceLabel, 1, 0);
    inputLayout->addWidget(m_replaceEdit, 1, 1);
    
    mainLayout->addLayout(inputLayout);
    
    // Options
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    m_caseSensitive = new QCheckBox(tr("Case Sensitive"), this);
    m_useRegex = new QCheckBox(tr("Regex"), this);
    m_wholeWord = new QCheckBox(tr("Whole Word"), this);
    optionsLayout->addWidget(m_caseSensitive);
    optionsLayout->addWidget(m_useRegex);
    optionsLayout->addWidget(m_wholeWord);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);
    
    // Results tree
    m_resultsTree = new QTreeWidget(this);
    m_resultsTree->setHeaderLabels({tr("File"), tr("Line"), tr("Match"), tr("Replace"), tr("✓")});
    m_resultsTree->setColumnWidth(0, 200);
    m_resultsTree->setColumnWidth(1, 60);
    m_resultsTree->setColumnWidth(2, 150);
    m_resultsTree->setColumnWidth(3, 150);
    m_resultsTree->setColumnWidth(4, 30);
    m_resultsTree->setRootIsDecorated(true);
    m_resultsTree->setStyleSheet(R"(
        QTreeWidget {
            border: 1px solid palette(mid);
            background: palette(base);
        }
        QTreeWidget::item {
            padding: 2px;
        }
        QTreeWidget::item:selected {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
    )");
    mainLayout->addWidget(m_resultsTree, 1);
    
    // Status and actions
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("No matches"), this);
    m_statusLabel->setStyleSheet("color: palette(mid);");
    
    m_selectAllButton = new QPushButton(tr("Select All"), this);
    m_deselectAllButton = new QPushButton(tr("Deselect All"), this);
    m_searchButton = new QPushButton(tr("Search"), this);
    m_applyButton = new QPushButton(tr("Apply"), this);
    m_applyButton->setEnabled(false);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    
    bottomLayout->addWidget(m_statusLabel, 1);
    bottomLayout->addWidget(m_selectAllButton);
    bottomLayout->addWidget(m_deselectAllButton);
    bottomLayout->addSpacing(10);
    bottomLayout->addWidget(m_searchButton);
    bottomLayout->addWidget(m_applyButton);
    bottomLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(bottomLayout);
}

void GlobalReplacePreview::setSearchPattern(const QString &pattern)
{
    m_searchEdit->setText(pattern);
}

void GlobalReplacePreview::setReplaceText(const QString &replacement)
{
    m_replaceEdit->setText(replacement);
}

void GlobalReplacePreview::searchInProject(const QString &projectPath)
{
    m_projectPath = projectPath;
    performSearch();
}

void GlobalReplacePreview::applyReplacements()
{
    int applied = 0;
    
    for (int i = 0; i < m_resultsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *fileItem = m_resultsTree->topLevelItem(i);
        QString filePath = fileItem->data(0, Qt::UserRole).toString();
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        QString content = QTextStream(&file).readAll();
        file.close();
        
        // Collect replacements for this file
        QList<QPair<int, QString>> replacements;  // line -> original text
        
        for (int j = 0; j < fileItem->childCount(); ++j) {
            QTreeWidgetItem *matchItem = fileItem->child(j);
            if (matchItem->checkState(4) == Qt::Checked) {
                int line = matchItem->data(1, Qt::UserRole).toInt();
                QString original = matchItem->data(2, Qt::UserRole).toString();
                QString replacement = matchItem->data(3, Qt::UserRole).toString();
                replacements.append({line, original});
            }
        }
        
        // Apply replacements (in reverse order to maintain line numbers)
        QStringList lines = content.split('\n');
        std::sort(replacements.begin(), replacements.end(),
                  [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                      return a.first > b.first;
                  });
        
        for (const auto &rep : replacements) {
            int lineIdx = rep.first - 1;
            if (lineIdx >= 0 && lineIdx < lines.size()) {
                QString pattern = m_searchEdit->text();
                QString replacement = m_replaceEdit->text();
                
                if (m_useRegex->isChecked()) {
                    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
                    if (!m_caseSensitive->isChecked()) {
                        options |= QRegularExpression::CaseInsensitiveOption;
                    }
                    QRegularExpression regex(pattern, options);
                    lines[lineIdx].replace(regex, replacement);
                } else {
                    Qt::CaseSensitivity cs = m_caseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
                    lines[lineIdx].replace(pattern, replacement, cs);
                }
                applied++;
            }
        }
        
        // Write back
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&file) << lines.join('\n');
            file.close();
        }
    }
    
    clearResults();
    emit replacementApplied(applied);
    
    QMessageBox::information(this, tr("Replace Complete"),
                           tr("%1 replacement(s) applied.").arg(applied));
}

void GlobalReplacePreview::cancelReplacements()
{
    clearResults();
    emit replacementCancelled();
}

void GlobalReplacePreview::onSearchClicked()
{
    performSearch();
}

void GlobalReplacePreview::onApplyClicked()
{
    if (m_selectedMatches == 0) {
        QMessageBox::information(this, tr("No Selection"),
                               tr("No replacements selected. Please select at least one match."));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Apply Replacements"),
        tr("Apply %1 replacement(s)?").arg(m_selectedMatches),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        applyReplacements();
    }
}

void GlobalReplacePreview::onCancelClicked()
{
    cancelReplacements();
}

void GlobalReplacePreview::onSelectAllClicked()
{
    for (int i = 0; i < m_resultsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *fileItem = m_resultsTree->topLevelItem(i);
        for (int j = 0; j < fileItem->childCount(); ++j) {
            fileItem->child(j)->setCheckState(4, Qt::Checked);
        }
    }
    m_selectedMatches = m_totalMatches;
    m_statusLabel->setText(tr("%1 matches (all selected)").arg(m_totalMatches));
    m_applyButton->setEnabled(m_selectedMatches > 0);
}

void GlobalReplacePreview::onDeselectAllClicked()
{
    for (int i = 0; i < m_resultsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *fileItem = m_resultsTree->topLevelItem(i);
        for (int j = 0; j < fileItem->childCount(); ++j) {
            fileItem->child(j)->setCheckState(4, Qt::Unchecked);
        }
    }
    m_selectedMatches = 0;
    m_statusLabel->setText(tr("%1 matches (none selected)").arg(m_totalMatches));
    m_applyButton->setEnabled(false);
}

void GlobalReplacePreview::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    if (!item->parent()) return;  // Clicked on file item
    
    QString filePath = item->parent()->data(0, Qt::UserRole).toString();
    int line = item->data(1, Qt::UserRole).toInt();
    int col = item->data(2, Qt::UserRole + 1).toInt();
    
    emit matchSelected(filePath, line, col);
}

void GlobalReplacePreview::performSearch()
{
    clearResults();
    
    QString pattern = m_searchEdit->text();
    if (pattern.isEmpty() || m_projectPath.isEmpty()) {
        return;
    }
    
    QRegularExpression regex;
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!m_caseSensitive->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    
    if (m_useRegex->isChecked()) {
        regex.setPattern(pattern);
        regex.setPatternOptions(options);
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("Invalid regex: %1").arg(regex.errorString()));
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
    }
    
    // Search recursively
    QDir dir(m_projectPath);
    QStringList files = dir.entryList(QDir::Files | QDir::Readable, QDir::Name);
    
    m_totalMatches = 0;
    
    for (const QString &fileName : files) {
        QString filePath = dir.absoluteFilePath(fileName);
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        QTextStream stream(&file);
        int lineNum = 0;
        QTreeWidgetItem *fileItem = nullptr;
        
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            lineNum++;
            
            QList<int> matchPositions;
            
            if (m_useRegex->isChecked()) {
                QRegularExpressionMatchIterator it = regex.globalMatch(line);
                while (it.hasNext()) {
                    QRegularExpressionMatch match = it.next();
                    matchPositions.append(match.capturedStart());
                }
            } else {
                int pos = 0;
                Qt::CaseSensitivity cs = m_caseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
                while ((pos = line.indexOf(pattern, pos, cs)) != -1) {
                    matchPositions.append(pos);
                    pos += pattern.length();
                }
            }
            
            for (int matchPos : matchPositions) {
                if (!fileItem) {
                    fileItem = new QTreeWidgetItem(m_resultsTree);
                    fileItem->setText(0, fileName);
                    fileItem->setData(0, Qt::UserRole, filePath);
                    fileItem->setExpanded(true);
                }
                
                QTreeWidgetItem *matchItem = new QTreeWidgetItem(fileItem);
                matchItem->setText(1, QString::number(lineNum));
                matchItem->setText(2, line.mid(qMax(0, matchPos - 10), 30));
                matchItem->setText(3, m_replaceEdit->text());
                matchItem->setCheckState(4, Qt::Checked);
                matchItem->setData(1, Qt::UserRole, lineNum);
                matchItem->setData(2, Qt::UserRole, line.mid(matchPos, pattern.length()));
                matchItem->setData(3, Qt::UserRole, m_replaceEdit->text());
                
                m_totalMatches++;
            }
        }
        
        file.close();
    }
    
    m_selectedMatches = m_totalMatches;
    
    if (m_totalMatches > 0) {
        m_statusLabel->setText(tr("%1 matches found").arg(m_totalMatches));
        m_statusLabel->setStyleSheet("color: green;");
        m_applyButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("No matches found"));
        m_statusLabel->setStyleSheet("color: palette(mid);");
        m_applyButton->setEnabled(false);
    }
}

void GlobalReplacePreview::updatePreview()
{
    // Update preview of replacements
}

void GlobalReplacePreview::clearResults()
{
    m_resultsTree->clear();
    m_totalMatches = 0;
    m_selectedMatches = 0;
    m_applyButton->setEnabled(false);
}

void GlobalReplacePreview::setCaseSensitive(bool enabled)
{
    m_caseSensitive->setChecked(enabled);
}

void GlobalReplacePreview::setUseRegex(bool enabled)
{
    m_useRegex->setChecked(enabled);
}

void GlobalReplacePreview::setWholeWord(bool enabled)
{
    m_wholeWord->setChecked(enabled);
}
