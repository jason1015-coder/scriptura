#ifndef DATAFORMATTER_H
#define DATAFORMATTER_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

/**
 * Data Formatter panel for formatting JSON, YAML, and XML data.
 * Features:
 * - One-click formatting
 * - Minification
 * - Validation
 * - Format conversion (JSON <-> YAML)
 */
class DataFormatter : public QWidget
{
    Q_OBJECT
public:
    explicit DataFormatter(QWidget *parent = nullptr);

signals:
    void formatted(const QString &format);

private slots:
    void onFormatClicked();
    void onMinifyClicked();
    void onValidateClicked();
    void onClearClicked();
    void onFormatChanged(int index);

private:
    void setupUI();
    QString formatJson(const QString &input, bool indent = true);
    QString formatYaml(const QString &input);
    QString formatXml(const QString &input, bool indent = true);
    QString minifyJson(const QString &input);
    QString minifyYaml(const QString &input);
    QString minifyXml(const QString &input);
    bool validateJson(const QString &input);
    bool validateYaml(const QString &input);
    bool validateXml(const QString &input);
    
    QPlainTextEdit *m_inputEdit;
    QPlainTextEdit *m_outputEdit;
    QComboBox *m_formatCombo;
    QLabel *m_statusLabel;
    QPushButton *m_formatButton;
    QPushButton *m_minifyButton;
    QPushButton *m_validateButton;
    QPushButton *m_clearButton;
};

#endif // DATAFORMATTER_H
