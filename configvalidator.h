#ifndef CONFIGVALIDATOR_H
#define CONFIGVALIDATOR_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QMap>
#include <QVariant>
#include <functional>

class ConfigValidator : public QObject
{
    Q_OBJECT
public:
    struct ValidationRule {
        QString key;
        QVariant defaultValue;
        std::function<bool(const QVariant&)> validator;
        QString errorMessage;
    };

    explicit ConfigValidator(QObject *parent = nullptr);

    // Validate all settings and return list of invalid keys
    QStringList validateSettings();

    // Reset invalid settings to defaults
    void resetInvalidSettings();

    // Get validated value with fallback
    template<typename T>
    T getValidatedValue(const QString &key, const T &defaultValue) {
        QSettings settings;
        QVariant value = settings.value(key, defaultValue);
        
        if (m_rules.contains(key)) {
            const ValidationRule &rule = m_rules[key];
            if (!rule.validator(value)) {
                qWarning() << "Invalid setting" << key << ":" << rule.errorMessage;
                return defaultValue;
            }
        }
        return value.value<T>();
    }

signals:
    void settingsValidated(const QStringList &invalidKeys);
    void settingsReset(const QStringList &resetKeys);

private:
    QMap<QString, ValidationRule> m_rules;
    void initializeRules();
};

#endif // CONFIGVALIDATOR_H