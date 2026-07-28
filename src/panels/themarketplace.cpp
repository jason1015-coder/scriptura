#include "themarketplace.h"
#include "thememanager.h"
#include "themedefs.h"
#include <QMessageBox>
#include <QColorDialog>

ThemeMarketplaceWidget::ThemeMarketplaceWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ThemeMarketplaceWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Title
    QLabel *title = new QLabel(tr("Theme Marketplace"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    layout->addWidget(title);

    // Theme list
    m_themeList = new QListWidget(this);
    m_themeList->setAlternatingRowColors(true);
    connect(m_themeList, &QListWidget::currentRowChanged, this, &ThemeMarketplaceWidget::onThemeSelected);
    layout->addWidget(m_themeList, 1);

    // Preview area
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumHeight(80);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("QLabel { border: 1px solid palette(mid); border-radius: 8px; padding: 8px; }");
    m_previewLabel->setText(tr("Select a theme to preview"));
    layout->addWidget(m_previewLabel);

    // Detail label
    m_detailLabel = new QLabel(tr("Select a theme to see details"), this);
    m_detailLabel->setWordWrap(true);
    layout->addWidget(m_detailLabel);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_installBtn = new QPushButton(tr("Install & Apply"), this);
    m_previewBtn = new QPushButton(tr("Preview"), this);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_installBtn->setEnabled(false);
    m_previewBtn->setEnabled(false);
    connect(m_installBtn, &QPushButton::clicked, this, &ThemeMarketplaceWidget::onInstallClicked);
    connect(m_previewBtn, &QPushButton::clicked, this, &ThemeMarketplaceWidget::onPreviewClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ThemeMarketplaceWidget::onRefreshClicked);
    btnLayout->addWidget(m_installBtn);
    btnLayout->addWidget(m_previewBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_refreshBtn);
    layout->addLayout(btnLayout);

    // Status
    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel);
}

void ThemeMarketplaceWidget::loadThemes(const QJsonArray &themes)
{
    m_themes = themes;
    m_themeList->clear();
    for (const QJsonValue &v : themes) {
        QJsonObject theme = v.toObject();
        QListWidgetItem *item = new QListWidgetItem(theme["name"].toString(), m_themeList);
        item->setData(Qt::UserRole, QJsonDocument(theme).toJson());
        QString author = theme["author"].toString();
        if (!author.isEmpty()) {
            item->setToolTip(tr("by %1").arg(author));
        }
    }
    m_statusLabel->setText(tr("%1 themes available").arg(themes.size()));
}

void ThemeMarketplaceWidget::loadBuiltinThemes()
{
    QJsonArray themes;
    struct ThemeDef { const char *name; int family; int mode; };
    ThemeDef defs[] = {
        {"Default Light", static_cast<int>(ThemeColorFamily::Default), 0},
        {"Default Dark", static_cast<int>(ThemeColorFamily::Default), 1},
        {"Blue Light", static_cast<int>(ThemeColorFamily::Blue), 0},
        {"Blue Dark", static_cast<int>(ThemeColorFamily::Blue), 1},
        {"Green Light", static_cast<int>(ThemeColorFamily::Green), 0},
        {"Green Dark", static_cast<int>(ThemeColorFamily::Green), 1},
        {"Red Light", static_cast<int>(ThemeColorFamily::Red), 0},
        {"Red Dark", static_cast<int>(ThemeColorFamily::Red), 1},
        {"Yellow Light", static_cast<int>(ThemeColorFamily::Yellow), 0},
        {"Yellow Dark", static_cast<int>(ThemeColorFamily::Yellow), 1},
        {"Brown Light", static_cast<int>(ThemeColorFamily::Brown), 0},
        {"Brown Dark", static_cast<int>(ThemeColorFamily::Brown), 1},
        {"Cyan Light", static_cast<int>(ThemeColorFamily::Cyan), 0},
        {"Cyan Dark", static_cast<int>(ThemeColorFamily::Cyan), 1},
        {"Violet Light", static_cast<int>(ThemeColorFamily::Violet), 0},
        {"Violet Dark", static_cast<int>(ThemeColorFamily::Violet), 1},
    };
    for (const auto &d : defs) {
        QJsonObject t;
        t["name"] = d.name;
        t["description"] = QString();
        t["author"] = "Scriptura";
        t["family"] = d.family;
        t["mode"] = d.mode;
        // Derive real colors from ThemeManager::buildDefinition
        ThemeManager::ColorFamily fam = static_cast<ThemeManager::ColorFamily>(d.family);
        ThemeManager::Mode mod = static_cast<ThemeManager::Mode>(d.mode);
        ThemeDefinition def = ThemeManager::buildDefinition(fam, mod);
        QJsonObject colors;
        colors["editor.background"] = def.baseColor.name();
        colors["editor.foreground"] = def.textColor.name();
        colors["accent"] = def.highlightColor.name();
        colors["sidebar.background"] = def.bgColor.name();
        t["colors"] = colors;
        themes.append(t);
    }
    loadThemes(themes);
}

void ThemeMarketplaceWidget::refreshThemes()
{
    m_statusLabel->setText(tr("Refreshing..."));
    // Theme loading would be triggered by the parent/manager
}

void ThemeMarketplaceWidget::onThemeSelected(int index)
{
    if (index < 0 || index >= m_themeList->count()) {
        m_detailLabel->setText(tr("Select a theme to see details"));
        m_previewLabel->setText(tr("Select a theme to preview"));
        m_installBtn->setEnabled(false);
        m_previewBtn->setEnabled(false);
        return;
    }

    QListWidgetItem *item = m_themeList->item(index);
    QJsonObject theme = QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();

    QString detail = QString("<b>%1</b><br>%2<br><i>by %3</i>")
        .arg(theme["name"].toString(),
             theme["description"].toString(),
             theme["author"].toString());
    m_detailLabel->setText(detail);

    updatePreview(theme);
    m_installBtn->setEnabled(true);
    m_previewBtn->setEnabled(true);
}

void ThemeMarketplaceWidget::updatePreview(const QJsonObject &theme)
{
    QJsonObject colors = theme["colors"].toObject();
    QString bgColor = colors.value("editor.background").toString("#ffffff");
    QString fgColor = colors.value("editor.foreground").toString("#000000");
    QString accentColor = colors.value("accent").toString("#007acc");

    QString preview = QString(
        "<div style='background:%1; color:%2; padding:12px; border-radius:8px; font-family:monospace;'>"
        "<span style='color:%3; font-weight:bold;'>def </span>"
        "<span style='color:#d97706;'>hello_world</span>():<br>"
        "&nbsp;&nbsp;<span style='color:%3;'>print</span>(<span style='color:#15803d;'>'Hello!'</span>)<br>"
        "<span style='color:#64748b;'># This is a comment</span><br>"
        "<span style='color:#9333ea;'>42</span> + <span style='color:#9333ea;'>3.14</span>"
        "</div>"
    ).arg(bgColor, fgColor, accentColor);

    m_previewLabel->setText(preview);
}

void ThemeMarketplaceWidget::onInstallClicked()
{
    QListWidgetItem *item = m_themeList->currentItem();
    if (!item) return;

    QJsonObject theme = QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();
    QString themeName = theme["name"].toString();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Install Theme"),
        tr("Install and apply theme \"%1\"?").arg(themeName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        emit themeInstalled(themeName);
        m_statusLabel->setText(tr("Theme \"%1\" installed").arg(themeName));
    }
}

void ThemeMarketplaceWidget::onPreviewClicked()
{
    QListWidgetItem *item = m_themeList->currentItem();
    if (!item) return;

    QJsonObject theme = QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object();
    emit themePreviewRequested(theme);
}

void ThemeMarketplaceWidget::onRefreshClicked()
{
    refreshThemes();
}
