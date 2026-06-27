#include "configvalidator.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFont>

ConfigValidator::ConfigValidator(QObject *parent)
    : QObject(parent)
{
    initializeRules();
}

void ConfigValidator::initializeRules()
{
    // Theme settings validation
    m_rules["theme/selected"] = {
        "theme/selected", 0,
        [](const QVariant &v) { return v.canConvert<int>() && v.toInt() >= 0 && v.toInt() <= 100; },
        "Theme value must be an integer between 0 and 100"
    };

    // Editor settings validation
    m_rules["editor/font"] = {
        "editor/font", QVariant(),
        [](const QVariant &v) { return v.isNull() || v.canConvert<QFont>(); },
        "Font setting must be a valid QFont"
    };

    m_rules["editor/tabWidth"] = {
        "editor/tabWidth", 4,
        [](const QVariant &v) { return v.canConvert<int>() && v.toInt() >= 1 && v.toInt() <= 16; },
        "Tab width must be between 1 and 16"
    };

    // UI settings validation
    m_rules["ui/sidebarCollapsed"] = {
        "ui/sidebarCollapsed", true,
        [](const QVariant &v) { return v.canConvert<bool>(); },
        "Sidebar collapsed must be a boolean"
    };

    // Recent projects validation
    m_rules["recentProjects"] = {
        "recentProjects", QStringList(),
        [](const QVariant &v) { return v.canConvert<QStringList>(); },
        "Recent projects must be a string list"
    };

    // Updater settings validation
    m_rules["updater/checkEnabled"] = {
        "updater/checkEnabled", true,
        [](const QVariant &v) { return v.canConvert<bool>(); },
        "Update check enabled must be a boolean"
    };

    m_rules["updater/checkInterval"] = {
        "updater/checkInterval", 7,
        [](const QVariant &v) { return v.canConvert<int>() && v.toInt() >= 1 && v.toInt() <= 365; },
        "Update check interval must be between 1 and 365 days"
    };

    m_rules["updater/currentVersion"] = {
        "updater/currentVersion", "0.0.0",
        [](const QVariant &v) { return v.canConvert<QString>(); },
        "Current version must be a string"
    };
}

QStringList ConfigValidator::validateSettings()
{
    QStringList invalidKeys;
    QSettings settings;

    for (auto it = m_rules.constBegin(); it != m_rules.constEnd(); ++it) {
        const QString &key = it.key();
        const ValidationRule &rule = it.value();

        QVariant value = settings.value(key, rule.defaultValue);
        if (!rule.validator(value)) {
            invalidKeys.append(key);
            qWarning() << "Invalid setting found:" << key << "-" << rule.errorMessage;
        }
    }

    emit settingsValidated(invalidKeys);
    return invalidKeys;
}

void ConfigValidator::resetInvalidSettings()
{
    QStringList invalidKeys = validateSettings();
    QSettings settings;

    for (const QString &key : invalidKeys) {
        if (m_rules.contains(key)) {
            settings.setValue(key, m_rules[key].defaultValue);
        }
    }

    emit settingsReset(invalidKeys);
}