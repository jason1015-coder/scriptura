#include "firstruninstalldialog.h"
#include "plugins/api/applicationmanager.h"

#include <QScrollArea>
#include <QGroupBox>
#include <QFont>
#include <QApplication>
#include <QScreen>
#include <QDebug>

FirstRunInstallDialog::FirstRunInstallDialog(ApplicationManager *appManager, QWidget *parent)
    : QDialog(parent)
    , m_appManager(appManager)
{
    setWindowTitle(tr("Install Applications"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(520, 480);
    setMaximumSize(640, 700);

    setupUI();
}

void FirstRunInstallDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(16);

    // ── Title ────────────────────────────────────────────────────────
    QLabel *titleLabel = new QLabel(tr("Welcome to Scriptura!"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(
        tr("You're all set to start coding. Before you begin, "
           "choose which applications to install from the Scriptura ecosystem.\n\n"
           "These apps provide additional functionality like Git integration, "
           "HTTP client, database viewer, and more.\n"
           "You can always install more later from the dock."),
        this);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = subtitleLabel->font();
    subFont.setPointSize(11);
    subtitleLabel->setFont(subFont);
    mainLayout->addWidget(subtitleLabel);

    // ── Separator ────────────────────────────────────────────────────
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("max-height: 1px;");
    mainLayout->addWidget(separator);

    // ── Available Apps List ──────────────────────────────────────────
    QLabel *listTitle = new QLabel(tr("Available Applications:"), this);
    QFont listTitleFont = listTitle->font();
    listTitleFont.setPointSize(12);
    listTitleFont.setBold(true);
    listTitle->setFont(listTitleFont);
    mainLayout->addWidget(listTitle);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(6);

    auto apps = m_appManager->allApplications();
    for (const auto &app : apps) {
        QFrame *appFrame = new QFrame(scrollContent);
        appFrame->setFrameShape(QFrame::StyledPanel);
        appFrame->setStyleSheet(
            "QFrame {"
            "  border: 1px solid palette(mid);"
            "  border-radius: 10px;"
            "  background-color: palette(base);"
            "  padding: 12px;"
            "}"
            "QFrame:hover {"
            "  border-color: palette(highlight);"
            "  background-color: palette(light);"
            "}"
        );

        QHBoxLayout *appLayout = new QHBoxLayout(appFrame);
        appLayout->setContentsMargins(12, 8, 12, 8);
        appLayout->setSpacing(12);

        // Checkbox
        QCheckBox *checkBox = new QCheckBox(appFrame);
        checkBox->setChecked(true); // Pre-selected by default
        checkBox->setText(QString());
        checkBox->setFixedSize(20, 20);
        appLayout->addWidget(checkBox);

        // App info
        QVBoxLayout *infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(2);

        QLabel *nameLabel = new QLabel(app.name, appFrame);
        QFont nameFont = nameLabel->font();
        nameFont.setPointSize(11);
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        infoLayout->addWidget(nameLabel);

        QLabel *descLabel = new QLabel(app.description, appFrame);
        QFont descFont = descLabel->font();
        descFont.setPointSize(9);
        descLabel->setFont(descFont);
        descLabel->setStyleSheet("color: palette(midlight);");
        descLabel->setWordWrap(true);
        infoLayout->addWidget(descLabel);

        // GitHub repo info
        if (!app.githubRepo.isEmpty()) {
            QLabel *repoLabel = new QLabel(app.githubRepo, appFrame);
            QFont repoFont = repoLabel->font();
            repoFont.setPointSize(8);
            repoLabel->setFont(repoFont);
            repoLabel->setStyleSheet("color: palette(mid); font-family: monospace;");
            repoLabel->setWordWrap(true);
            infoLayout->addWidget(repoLabel);
        }

        appLayout->addLayout(infoLayout, 1);

        scrollLayout->addWidget(appFrame);

        m_appCheckBoxes.append(checkBox);
        m_appIds.append(app.id);
    }

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // ── Button row ───────────────────────────────────────────────────
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    QPushButton *skipBtn = new QPushButton(tr("Skip"), this);
    skipBtn->setObjectName("secondaryButton");
    skipBtn->setCursor(Qt::PointingHandCursor);
    skipBtn->setMinimumHeight(36);
    skipBtn->setStyleSheet(
        "QPushButton#secondaryButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 8px 24px;"
        "  background-color: transparent;"
        "  color: palette(text);"
        "}"
        "QPushButton#secondaryButton:hover {"
        "  background-color: palette(light);"
        "}"
    );
    connect(skipBtn, &QPushButton::clicked, this, &FirstRunInstallDialog::onSkipClicked);

    QPushButton *installBtn = new QPushButton(tr("Install Selected"), this);
    installBtn->setObjectName("primaryButton");
    installBtn->setCursor(Qt::PointingHandCursor);
    installBtn->setMinimumHeight(36);
    installBtn->setDefault(true);
    installBtn->setStyleSheet(
        "QPushButton#primaryButton {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 1px solid palette(highlight);"
        "  border-radius: 10px;"
        "  padding: 8px 24px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#primaryButton:hover {"
        "  background: palette(highlight);"
        "}"
    );
    connect(installBtn, &QPushButton::clicked, this, &FirstRunInstallDialog::onInstallClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(skipBtn);
    buttonLayout->addWidget(installBtn);
    mainLayout->addLayout(buttonLayout);

    // ── Additional info ──────────────────────────────────────────────
    QLabel *footerLabel = new QLabel(
        tr("Note: GitHub repository URLs are placeholders. "
           "Replace them with actual repository URLs to enable installation."),
        this);
    QFont footerFont = footerLabel->font();
    footerFont.setPointSize(8);
    footerLabel->setFont(footerFont);
    footerLabel->setStyleSheet("color: palette(mid);");
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setWordWrap(true);
    mainLayout->addWidget(footerLabel);
}

void FirstRunInstallDialog::onInstallClicked()
{
    m_skipped = false;
    m_selectedApps.clear();

    for (int i = 0; i < m_appCheckBoxes.size(); ++i) {
        if (m_appCheckBoxes[i]->isChecked()) {
            m_selectedApps.append(m_appIds[i]);
        }
    }

    accept();
}

void FirstRunInstallDialog::onSkipClicked()
{
    m_skipped = true;
    reject();
}
