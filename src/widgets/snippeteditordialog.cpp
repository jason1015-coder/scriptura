#include "snippeteditordialog.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SnippetEditorDialog::SnippetEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Snippet Manager"));
    resize(700, 500);
    setupUI();
}

void SnippetEditorDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Header
    QLabel *title = new QLabel(tr("Code Snippets"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    QHBoxLayout *topLayout = new QHBoxLayout();

    // Snippet tree
    m_snippetTree = new QTreeWidget(this);
    m_snippetTree->setHeaderLabels({tr("Prefix"), tr("Description")});
    m_snippetTree->setColumnWidth(0, 150);
    m_snippetTree->setColumnWidth(1, 250);
    m_snippetTree->setAlternatingRowColors(true);
    connect(m_snippetTree, &QTreeWidget::currentItemChanged, this, [this]() {
        onSnippetSelected(-1);
    });
    topLayout->addWidget(m_snippetTree, 1);

    // Editor panel
    QVBoxLayout *editorLayout = new QVBoxLayout();

    QLabel *prefixLabel = new QLabel(tr("Prefix:"), this);
    editorLayout->addWidget(prefixLabel);
    m_prefixEdit = new QLineEdit(this);
    m_prefixEdit->setPlaceholderText(tr("e.g. div, fn, for"));
    editorLayout->addWidget(m_prefixEdit);

    QLabel *descLabel = new QLabel(tr("Description:"), this);
    editorLayout->addWidget(descLabel);
    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("e.g. Create a div element"));
    editorLayout->addWidget(m_descriptionEdit);

    QLabel *bodyLabel = new QLabel(tr("Body (use $1, $2 for tab stops):"), this);
    editorLayout->addWidget(bodyLabel);
    m_bodyEdit = new QPlainTextEdit(this);
    m_bodyEdit->setPlaceholderText(tr("<div>\n\t$1\n</div>"));
    m_bodyEdit->setTabStopDistance(20);
    QFont monoFont("monospace");
    monoFont.setStyleHint(QFont::Monospace);
    m_bodyEdit->setFont(monoFont);
    editorLayout->addWidget(m_bodyEdit, 1);

    QLabel *insertLabel = new QLabel(tr("Insert Text Preview:"), this);
    editorLayout->addWidget(insertLabel);
    m_insertTextEdit = new QPlainTextEdit(this);
    m_insertTextEdit->setReadOnly(true);
    m_insertTextEdit->setMaximumHeight(60);
    m_insertTextEdit->setFont(monoFont);
    editorLayout->addWidget(m_insertTextEdit);

    topLayout->addLayout(editorLayout, 1);
    mainLayout->addLayout(topLayout, 1);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_newBtn = new QPushButton(tr("New"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_saveBtn = new QPushButton(tr("Save"), this);
    m_importBtn = new QPushButton(tr("Import..."), this);
    m_exportBtn = new QPushButton(tr("Export..."), this);
    btnLayout->addWidget(m_newBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_importBtn);
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    connect(m_newBtn, &QPushButton::clicked, this, &SnippetEditorDialog::onNewClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SnippetEditorDialog::onDeleteClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &SnippetEditorDialog::onSaveClicked);
    connect(m_importBtn, &QPushButton::clicked, this, &SnippetEditorDialog::onImportClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &SnippetEditorDialog::onExportClicked);
}

void SnippetEditorDialog::loadSnippets(const QJsonObject &snippets)
{
    m_snippets = snippets;
    populateSnippetList();
}

void SnippetEditorDialog::populateSnippetList()
{
    m_snippetTree->clear();

    QStringList languages = m_snippets.keys();
    languages.sort();

    for (const QString &lang : languages) {
        QTreeWidgetItem *langItem = new QTreeWidgetItem(m_snippetTree);
        langItem->setText(0, lang);
        QFont font = langItem->font(0);
        font.setBold(true);
        langItem->setFont(0, font);

        QJsonObject langSnippets = m_snippets[lang].toObject();
        QStringList snippetNames = langSnippets.keys();
        snippetNames.sort();

        for (const QString &name : snippetNames) {
            QJsonObject snippet = langSnippets[name].toObject();
            QTreeWidgetItem *item = new QTreeWidgetItem(langItem);
            item->setText(0, snippet["prefix"].toString());
            item->setText(1, snippet["description"].toString());
            item->setData(0, Qt::UserRole, lang);
            item->setData(1, Qt::UserRole, name);
        }
    }

    m_snippetTree->expandAll();
    m_statusLabel->setText(tr("Loaded snippets for %1 languages").arg(languages.size()));
}

void SnippetEditorDialog::onSnippetSelected(int)
{
    QTreeWidgetItem *item = m_snippetTree->currentItem();
    if (!item || !item->parent()) {
        m_prefixEdit->clear();
        m_descriptionEdit->clear();
        m_bodyEdit->clear();
        m_insertTextEdit->clear();
        m_currentLanguage.clear();
        m_currentSnippetName.clear();
        return;
    }

    m_currentLanguage = item->data(0, Qt::UserRole).toString();
    m_currentSnippetName = item->data(1, Qt::UserRole).toString();

    QJsonObject langSnippets = m_snippets[m_currentLanguage].toObject();
    QJsonObject snippet = langSnippets[m_currentSnippetName].toObject();

    m_prefixEdit->setText(snippet["prefix"].toString());
    m_descriptionEdit->setText(snippet["description"].toString());

    QJsonArray bodyArray = snippet["body"].toArray();
    QStringList bodyLines;
    for (const QJsonValue &v : bodyArray) {
        bodyLines.append(v.toString());
    }
    m_bodyEdit->setPlainText(bodyLines.join("\n"));

    // Preview insert text (replace $1, $2 etc with placeholders)
    QString preview = bodyLines.join("\n");
    int tabStop = 1;
    while (preview.contains(QString("$%1").arg(tabStop))) {
        preview.replace(QString("$%1").arg(tabStop), QString("[%1]").arg(tabStop));
        tabStop++;
    }
    preview.replace("$0", "[cursor]");
    m_insertTextEdit->setPlainText(preview);
}

void SnippetEditorDialog::onNewClicked()
{
    // Create a new snippet in the current language or "global"
    QString lang = m_currentLanguage.isEmpty() ? "global" : m_currentLanguage;
    QString name = "new_snippet";

    QJsonObject langSnippets = m_snippets[lang].toObject();
    int counter = 1;
    while (langSnippets.contains(name)) {
        name = QString("new_snippet_%1").arg(counter++);
    }

    QJsonObject snippet;
    snippet["prefix"] = "";
    snippet["description"] = "New snippet";
    snippet["body"] = QJsonArray({"$1"});
    langSnippets[name] = snippet;
    m_snippets[lang] = langSnippets;

    populateSnippetList();
    m_currentLanguage = lang;
    m_currentSnippetName = name;
    m_statusLabel->setText(tr("Created new snippet \"%1\"").arg(name));
}

void SnippetEditorDialog::onDeleteClicked()
{
    if (m_currentLanguage.isEmpty() || m_currentSnippetName.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Snippet"),
        tr("Delete snippet \"%1\"?").arg(m_currentSnippetName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QJsonObject langSnippets = m_snippets[m_currentLanguage].toObject();
        langSnippets.remove(m_currentSnippetName);
        if (langSnippets.isEmpty()) {
            m_snippets.remove(m_currentLanguage);
        } else {
            m_snippets[m_currentLanguage] = langSnippets;
        }
        m_currentSnippetName.clear();
        m_currentLanguage.clear();
        populateSnippetList();
        m_statusLabel->setText(tr("Snippet deleted"));
    }
}

void SnippetEditorDialog::onSaveClicked()
{
    saveCurrentSnippet();
}

void SnippetEditorDialog::saveCurrentSnippet()
{
    if (m_currentLanguage.isEmpty() || m_currentSnippetName.isEmpty()) return;

    QJsonObject snippet;
    snippet["prefix"] = m_prefixEdit->text();
    snippet["description"] = m_descriptionEdit->text();

    QStringList bodyLines = m_bodyEdit->toPlainText().split("\n");
    QJsonArray bodyArray;
    for (const QString &line : bodyLines) {
        bodyArray.append(line);
    }
    snippet["body"] = bodyArray;

    QJsonObject langSnippets = m_snippets[m_currentLanguage].toObject();
    langSnippets[m_currentSnippetName] = snippet;
    m_snippets[m_currentLanguage] = langSnippets;

    emit snippetsChanged(m_snippets);
    m_statusLabel->setText(tr("Snippet saved"));
}

void SnippetEditorDialog::onImportClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Import Snippets"),
        QString(),
        tr("JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import Error"), tr("Could not open file: %1").arg(filePath));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isObject()) {
        QJsonObject imported = doc.object();
        // Merge into existing snippets
        for (auto it = imported.begin(); it != imported.end(); ++it) {
            if (m_snippets.contains(it.key())) {
                QJsonObject existing = m_snippets[it.key()].toObject();
                QJsonObject newSnippets = it.value().toObject();
                for (auto nit = newSnippets.begin(); nit != newSnippets.end(); ++nit) {
                    existing[nit.key()] = nit.value();
                }
                m_snippets[it.key()] = existing;
            } else {
                m_snippets[it.key()] = it.value();
            }
        }
        populateSnippetList();
        m_statusLabel->setText(tr("Imported snippets from %1").arg(QFileInfo(filePath).fileName()));
        emit snippetsChanged(m_snippets);
    } else {
        QMessageBox::warning(this, tr("Import Error"), tr("Invalid snippet file format"));
    }
}

void SnippetEditorDialog::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Snippets"),
        "snippets.json",
        tr("JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export Error"), tr("Could not write to file: %1").arg(filePath));
        return;
    }

    file.write(QJsonDocument(m_snippets).toJson());
    file.close();
    m_statusLabel->setText(tr("Exported snippets to %1").arg(QFileInfo(filePath).fileName()));
}

void SnippetEditorDialog::onLanguageChanged(const QString &language)
{
    m_currentLanguage = language;
}
