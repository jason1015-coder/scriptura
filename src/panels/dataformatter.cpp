#include "dataformatter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QApplication>

DataFormatter::DataFormatter(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    connect(m_formatButton, &QPushButton::clicked, this, &DataFormatter::onFormatClicked);
    connect(m_minifyButton, &QPushButton::clicked, this, &DataFormatter::onMinifyClicked);
    connect(m_validateButton, &QPushButton::clicked, this, &DataFormatter::onValidateClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &DataFormatter::onClearClicked);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataFormatter::onFormatChanged);
}

void DataFormatter::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Format selector and actions
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *formatLabel = new QLabel(tr("Format:"), this);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems({"JSON", "YAML", "XML"});
    m_formatButton = new QPushButton(tr("Format"), this);
    m_minifyButton = new QPushButton(tr("Minify"), this);
    m_validateButton = new QPushButton(tr("Validate"), this);
    m_clearButton = new QPushButton(tr("Clear"), this);
    
    topLayout->addWidget(formatLabel);
    topLayout->addWidget(m_formatCombo);
    topLayout->addSpacing(10);
    topLayout->addWidget(m_formatButton);
    topLayout->addWidget(m_minifyButton);
    topLayout->addWidget(m_validateButton);
    topLayout->addStretch();
    topLayout->addWidget(m_clearButton);
    mainLayout->addLayout(topLayout);
    
    // Splitter for input/output
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    
    // Input
    QWidget *inputWidget = new QWidget(this);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *inputLabel = new QLabel(tr("Input:"), this);
    m_inputEdit = new QPlainTextEdit(this);
    m_inputEdit->setPlaceholderText(tr("Paste JSON, YAML, or XML here..."));
    m_inputEdit->setStyleSheet("QPlainTextEdit { font-family: monospace; }");
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(m_inputEdit, 1);
    splitter->addWidget(inputWidget);
    
    // Output
    QWidget *outputWidget = new QWidget(this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *outputLabel = new QLabel(tr("Output:"), this);
    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setStyleSheet("QPlainTextEdit { font-family: monospace; }");
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(m_outputEdit, 1);
    splitter->addWidget(outputWidget);
    
    mainLayout->addWidget(splitter, 1);
    
    // Status
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("QLabel { color: palette(mid); }");
    mainLayout->addWidget(m_statusLabel);
}

void DataFormatter::onFormatClicked()
{
    QString input = m_inputEdit->toPlainText();
    if (input.isEmpty()) return;
    
    QString output;
    QString format = m_formatCombo->currentText();
    
    if (format == "JSON") {
        output = formatJson(input);
    } else if (format == "YAML") {
        output = formatYaml(input);
    } else if (format == "XML") {
        output = formatXml(input);
    }
    
    if (!output.isEmpty()) {
        m_outputEdit->setPlainText(output);
        m_statusLabel->setText(tr("Formatted successfully"));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
        emit formatted(format);
    }
}

void DataFormatter::onMinifyClicked()
{
    QString input = m_inputEdit->toPlainText();
    if (input.isEmpty()) return;
    
    QString output;
    QString format = m_formatCombo->currentText();
    
    if (format == "JSON") {
        output = minifyJson(input);
    } else if (format == "YAML") {
        output = minifyYaml(input);
    } else if (format == "XML") {
        output = minifyXml(input);
    }
    
    if (!output.isEmpty()) {
        m_outputEdit->setPlainText(output);
        m_statusLabel->setText(tr("Minified successfully"));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
    }
}

void DataFormatter::onValidateClicked()
{
    QString input = m_inputEdit->toPlainText();
    if (input.isEmpty()) {
        m_statusLabel->setText(tr("No input to validate"));
        m_statusLabel->setStyleSheet("QLabel { color: palette(mid); }");
        return;
    }
    
    bool valid = false;
    QString format = m_formatCombo->currentText();
    
    if (format == "JSON") {
        valid = validateJson(input);
    } else if (format == "YAML") {
        valid = validateYaml(input);
    } else if (format == "XML") {
        valid = validateXml(input);
    }
    
    if (valid) {
        m_statusLabel->setText(tr("Valid %1").arg(format));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        m_statusLabel->setText(tr("Invalid %1").arg(format));
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
    }
}

void DataFormatter::onClearClicked()
{
    m_inputEdit->clear();
    m_outputEdit->clear();
    m_statusLabel->clear();
}

void DataFormatter::onFormatChanged(int index)
{
    Q_UNUSED(index);
    m_outputEdit->clear();
    m_statusLabel->clear();
}

QString DataFormatter::formatJson(const QString &input, bool indent)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        m_statusLabel->setText(tr("JSON Error: %1").arg(error.errorString()));
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        return QString();
    }
    
    if (indent) {
        return doc.toJson(QJsonDocument::Indented);
    } else {
        return doc.toJson(QJsonDocument::Compact);
    }
}

QString DataFormatter::formatYaml(const QString &input)
{
    // Simple YAML formatting - in production, use a proper YAML library
    // For now, just add proper indentation
    QStringList lines = input.split('\n');
    QStringList result;
    int indent = 0;
    
    for (QString line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            result.append("");
            continue;
        }
        
        if (trimmed.startsWith('-') || trimmed.endsWith(':')) {
            result.append(QString(indent * 2, ' ') + trimmed);
            if (trimmed.endsWith(':')) indent++;
        } else {
            result.append(QString(indent * 2, ' ') + trimmed);
        }
    }
    
    return result.join('\n');
}

QString DataFormatter::formatXml(const QString &input, bool indent)
{
    // Simple XML formatting
    QString result;
    int indentLevel = 0;
    bool inTag = false;
    
    for (int i = 0; i < input.length(); ++i) {
        QChar c = input.at(i);
        
        if (c == '<') {
            if (i + 1 < input.length() && input.at(i + 1) == '/') {
                // Closing tag
                indentLevel--;
                if (indent && !result.isEmpty() && !result.endsWith('\n')) {
                    result.append('\n');
                }
                if (indent) result.append(QString(indentLevel * 2, ' '));
            } else {
                // Opening tag
                if (indent && !result.isEmpty() && !result.endsWith('\n')) {
                    result.append('\n');
                }
                if (indent) result.append(QString(indentLevel * 2, ' '));
                indentLevel++;
            }
            inTag = true;
        }
        
        result.append(c);
        
        if (c == '>' && !inTag) {
            // End of tag
        } else if (c == '>' && i > 0 && input.at(i - 1) == '/') {
            // Self-closing tag
            indentLevel--;
        } else if (c == '>' && inTag) {
            inTag = false;
        }
    }
    
    return result;
}

QString DataFormatter::minifyJson(const QString &input)
{
    return formatJson(input, false);
}

QString DataFormatter::minifyYaml(const QString &input)
{
    // Remove comments and extra whitespace
    QStringList lines = input.split('\n');
    QStringList result;
    
    for (QString line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
        result.append(trimmed);
    }
    
    return result.join('\n');
}

QString DataFormatter::minifyXml(const QString &input)
{
    QString result = input;
    result.remove('\n');
    result.remove('\r');
    result.remove('\t');
    
    // Remove extra spaces between tags
    QRegularExpression re(">\\s+<");
    result.replace(re, "><");
    
    return result.trimmed();
}

bool DataFormatter::validateJson(const QString &input)
{
    QJsonParseError error;
    QJsonDocument::fromJson(input.toUtf8(), &error);
    return error.error == QJsonParseError::NoError;
}

bool DataFormatter::validateYaml(const QString &input)
{
    // Simple YAML validation - check for basic syntax
    // In production, use a proper YAML parser
    return !input.isEmpty();
}

bool DataFormatter::validateXml(const QString &input)
{
    // Simple XML validation - check for balanced tags
    int openCount = 0;
    bool inTag = false;
    
    for (int i = 0; i < input.length(); ++i) {
        QChar c = input.at(i);
        
        if (c == '<') {
            if (i + 1 < input.length() && input.at(i + 1) == '/') {
                openCount--;
            } else if (i + 1 < input.length() && input.at(i + 1) == '!') {
                // Comment
                continue;
            } else {
                openCount++;
            }
            inTag = true;
        } else if (c == '>' && inTag) {
            inTag = false;
        }
    }
    
    return openCount == 0;
}
