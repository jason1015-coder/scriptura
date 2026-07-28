#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "version.h"
#include "thememanager.h"
#include "themeicons.h"
#include "rust_adapter.h"
#include "rust_adapter.h"
#include "pluginmanagerdialog.h"
#include "rust_adapter.h"

#include <QScrollArea>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSettings>
#include <QToolTip>
#include <QFileInfo>
#include <QStandardPaths>

QWidget* MainWindow::createUnifiedSettingsWidget()
{
    // Unified scrollable settings page combining Theme, Editor, Keyboard Shortcuts, and Updates
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *content = new QWidget();
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ── Theme Section ────────────────────────────────────────────────────
    QGroupBox *themeGroup = new QGroupBox(tr("Theme"), content);
    QGridLayout *themeGrid = new QGridLayout(themeGroup);
    themeGrid->setSpacing(6);
    QButtonGroup *themeBtnGroup = new QButtonGroup(themeGroup);

    struct FamilyEntry {
        ThemeColorFamily family;
        QString name;
    };
    FamilyEntry families[] = {
        {ThemeColorFamily::Default, "Default"}, {ThemeColorFamily::Blue, "Blue"},
        {ThemeColorFamily::Green, "Green"},     {ThemeColorFamily::Red, "Red"},
        {ThemeColorFamily::Yellow, "Yellow"},   {ThemeColorFamily::Brown, "Brown"},
        {ThemeColorFamily::Cyan, "Cyan"},       {ThemeColorFamily::Violet, "Violet"}
    };

    struct ThemeButtonEntry { QPushButton *btn; Theme theme; };
    QList<ThemeButtonEntry> themeButtons;

    auto refreshButtonStyle = [](QPushButton *btn, const Theme &t) {
        ThemeManager::ColorFamily tmF = static_cast<ThemeManager::ColorFamily>(static_cast<int>(t.family));
        ThemeManager::Mode tmM = static_cast<ThemeManager::Mode>(static_cast<int>(t.mode));
        ThemeDefinition td = ThemeManager::buildDefinition(tmF, tmM);
        QPalette p = ThemeManager::buildPalette(td);
        QColor bg = p.color(QPalette::Window);
        QColor tc = p.color(QPalette::WindowText);
        btn->setStyleSheet(QStringLiteral(
            "background-color: %1; color: %2; padding: 8px;"
            " border: 1px solid %3; border-radius: 8px; text-align: left;")
            .arg(bg.name(), tc.name(), tc.name()));
    };

    int btnIdx = 0;
    for (int fi = 0; fi < 8; ++fi) {
        for (int mi = 0; mi < 2; ++mi) {
            for (int hci = 0; hci < 2; ++hci) {
                ThemeMode mode = static_cast<ThemeMode>(mi);
                ThemeFeatures features = hci ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures();
                Theme t(static_cast<ThemeColorFamily>(fi), mode, features);
                QString label = families[fi].name + QStringLiteral(" \u2013 ")
                    + (mode == ThemeMode::Light ? tr("Light") : tr("Dark"));
                if (hci) label += QStringLiteral(" (") + tr("High Contrast") + QStringLiteral(")");

                QPushButton *btn = new QPushButton(label, themeGroup);
                btn->setCheckable(true);
                btn->setCursor(Qt::PointingHandCursor);
                refreshButtonStyle(btn, t);
                themeGrid->addWidget(btn, btnIdx / 4, btnIdx % 4);
                themeBtnGroup->addButton(btn, btnIdx);
                themeButtons.append({btn, t});
                if (t == selectedTheme) btn->setChecked(true);
                ++btnIdx;
            }
        }
    }

    connect(m_themeManager, &ThemeManager::themeChanged, content, [themeButtons, refreshButtonStyle]() {
        for (const auto &e : themeButtons) refreshButtonStyle(e.btn, e.theme);
    });

    mainLayout->addWidget(themeGroup);

    connect(themeBtnGroup, static_cast<void (QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, [this, themeButtons](QAbstractButton *button) {
        QPushButton *btn = qobject_cast<QPushButton*>(button);
        if (!btn) return;
        for (const auto &e : themeButtons) {
            if (e.btn == btn) {
                if (e.theme != selectedTheme) {
                    selectedTheme = e.theme;
                    applyTheme(selectedTheme);
                    QSettings s;
                    s.setValue("theme/selected", themeToLegacyInt(selectedTheme));
                }
                break;
            }
        }
    });

    // ── Editor Section ───────────────────────────────────────────────────
    CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->currentWidget());

    // Font
    QGroupBox *fontGroup = new QGroupBox(tr("Font"), content);
    QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);

    QHBoxLayout *fontNameLayout = new QHBoxLayout();
    QLabel *fontLabel = new QLabel(tr("Font Family:"), content);
    QFontComboBox *fontCombo = new QFontComboBox(content);
    fontCombo->setFontFilters(QFontComboBox::ScalableFonts);
    if (editor) fontCombo->setCurrentFont(editor->font());
    fontNameLayout->addWidget(fontLabel);
    fontNameLayout->addWidget(fontCombo, 1);
    fontLayout->addLayout(fontNameLayout);

    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel(tr("Size:"), content);
    QSpinBox *sizeSpin = new QSpinBox(content);
    sizeSpin->setRange(8, 72);
    if (editor) sizeSpin->setValue(editor->font().pointSize());
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeSpin);
    sizeLayout->addStretch();
    fontLayout->addLayout(sizeLayout);
    mainLayout->addWidget(fontGroup);

    // Behavior
    QGroupBox *behaviorGroup = new QGroupBox(tr("Behavior"), content);
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);

    QHBoxLayout *tabLayout = new QHBoxLayout();
    QLabel *tabLabel = new QLabel(tr("Tab Width:"), content);
    QSpinBox *tabSpin = new QSpinBox(content);
    tabSpin->setRange(1, 16);
    if (editor) tabSpin->setValue(editor->tabWidth());
    tabLayout->addWidget(tabLabel);
    tabLayout->addWidget(tabSpin);
    tabLayout->addStretch();
    behaviorLayout->addLayout(tabLayout);

    QCheckBox *wordWrapCheckbox = new QCheckBox(tr("Word Wrap"), content);
    if (editor) wordWrapCheckbox->setChecked(editor->lineWrapMode() != QPlainTextEdit::NoWrap);
    behaviorLayout->addWidget(wordWrapCheckbox);

    QCheckBox *indentGuidesCheckbox = new QCheckBox(tr("Show Indent Guides"), content);
    if (editor) indentGuidesCheckbox->setChecked(editor->property("showIndentGuides").toBool());
    behaviorLayout->addWidget(indentGuidesCheckbox);
    mainLayout->addWidget(behaviorGroup);

    // Display
    QGroupBox *displayGroup = new QGroupBox(tr("Display"), content);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);

    QCheckBox *lineNumbersCheckbox = new QCheckBox(tr("Show Line Numbers"), content);
    if (editor) lineNumbersCheckbox->setChecked(editor->property("showLineNumbers").toBool());
    displayLayout->addWidget(lineNumbersCheckbox);

    QHBoxLayout *widthLayout = new QHBoxLayout();
    QLabel *widthLabel = new QLabel(tr("Editor Width:"), content);
    QSpinBox *widthSpin = new QSpinBox(content);
    widthSpin->setRange(200, 2000);
    widthSpin->setSuffix(" px");
    if (editor) widthSpin->setValue(editor->minimumWidth());
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(widthSpin);
    widthLayout->addStretch();
    displayLayout->addLayout(widthLayout);
    mainLayout->addWidget(displayGroup);

    // Apply editor settings
    auto applySettings = [=]() {
        QSettings s;
        QFont f = fontCombo->currentFont();
        f.setPointSize(sizeSpin->value());
        s.setValue("editor/font", f);
        s.setValue("editor/tabWidth", tabSpin->value());
        s.setValue("editor/width", widthSpin->value());
        s.setValue("editor/wordWrap", wordWrapCheckbox->isChecked());
        s.setValue("editor/showIndentGuides", indentGuidesCheckbox->isChecked());
        s.setValue("editor/showLineNumbers", lineNumbersCheckbox->isChecked());
        for (int i = 0; i < ui->tabWidget->count(); i++) {
            if (CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                QFont ef = fontCombo->currentFont();
                ef.setPointSize(sizeSpin->value());
                ed->setFont(ef);
                ed->setTabWidth(tabSpin->value());
                ed->setLineWrapMode(wordWrapCheckbox->isChecked() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
                ed->setProperty("showIndentGuides", indentGuidesCheckbox->isChecked());
                ed->setProperty("showLineNumbers", lineNumbersCheckbox->isChecked());
                ed->setMinimumWidth(widthSpin->value());
            }
        }
    };
    connect(fontCombo, &QFontComboBox::currentFontChanged, content, applySettings);
    connect(sizeSpin, &QSpinBox::valueChanged, content, applySettings);
    connect(tabSpin, &QSpinBox::valueChanged, content, applySettings);
    connect(wordWrapCheckbox, &QCheckBox::toggled, content, applySettings);
    connect(indentGuidesCheckbox, &QCheckBox::toggled, content, applySettings);
    connect(lineNumbersCheckbox, &QCheckBox::toggled, content, applySettings);
    connect(widthSpin, &QSpinBox::valueChanged, content, applySettings);

    // ── Keyboard Shortcuts Section ───────────────────────────────────────
    QGroupBox *shortcutsGroup = new QGroupBox(tr("Keyboard Shortcuts"), content);
    QVBoxLayout *shortcutsLayout = new QVBoxLayout(shortcutsGroup);
    ShortcutEditorWidget *shortcutEditor = new ShortcutEditorWidget(shortcutsGroup);
    shortcutEditor->loadShortcuts();
    shortcutsLayout->addWidget(shortcutEditor);
    connect(shortcutEditor, &ShortcutEditorWidget::shortcutChanged, this,
            [](const QString &action, const QKeySequence &shortcut) {
        QSettings s;
        s.setValue("shortcuts/" + action, shortcut.toString());
    });
    mainLayout->addWidget(shortcutsGroup);

    // ── Snippets Section ──────────────────────────────────────────────────
    QGroupBox *snippetsGroup = new QGroupBox(tr("Code Snippets"), content);
    QVBoxLayout *snippetsLayout = new QVBoxLayout(snippetsGroup);
    QPushButton *openSnippetEditorBtn = new QPushButton(tr("Open Snippet Manager..."), snippetsGroup);
    snippetsLayout->addWidget(openSnippetEditorBtn);
    mainLayout->addWidget(snippetsGroup);
    connect(openSnippetEditorBtn, &QPushButton::clicked, this, [this]() {
        SnippetEditorDialog dlg(this);
        // Load current snippets from active editor's snippet manager
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && editor->snippetManager()) {
            // For now load empty; real data comes from settings
        }
        QSettings s;
        QString data = s.value("snippets/data").toString();
        if (!data.isEmpty()) {
            dlg.loadSnippets(QJsonDocument::fromJson(data.toUtf8()).object());
        }
        if (dlg.exec() == QDialog::Accepted) {
            // Save in the format SnippetManager expects (flat JSON array)
            QJsonArray arr;
            QJsonObject dialogSnippets = dlg.snippets();
            for (auto it = dialogSnippets.begin(); it != dialogSnippets.end(); ++it) {
                QJsonObject langSnippets = it.value().toObject();
                for (auto sit = langSnippets.begin(); sit != langSnippets.end(); ++sit) {
                    QJsonObject snippet = sit.value().toObject();
                    QJsonObject sObj;
                    sObj["id"] = it.key() + "/" + sit.key();
                    sObj["name"] = sit.key();
                    sObj["prefix"] = snippet["prefix"].toString();
                    QJsonArray bodyArr = snippet["body"].toArray();
                    QStringList bodyLines;
                    for (const QJsonValue &bv : bodyArr) bodyLines.append(bv.toString());
                    sObj["body"] = bodyLines.join("\n");
                    sObj["description"] = snippet["description"].toString();
                    sObj["language"] = it.key();
                    sObj["tabStops"] = 0;
                    arr.append(sObj);
                }
            }
            s.setValue("snippets", QJsonDocument(arr).toJson());
            // Sync to SnippetManager in all open editors
            for (int i = 0; i < ui->tabWidget->count(); ++i) {
                if (CodeEditor *ed = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                    if (ed->snippetManager()) {
                        ed->snippetManager()->loadFromSettings();
                    }
                }
            }
        }
    });

    // ── Updates Section ───────────────────────────────────────────────────
    QGroupBox *updateGroup = new QGroupBox(tr("Updates"), content);
    QVBoxLayout *updateLayout = new QVBoxLayout(updateGroup);

    QPushButton *checkStableButton = new QPushButton(tr("Check for Latest Release (Recommended)"), updateGroup);
    QPushButton *checkPreReleaseButton = new QPushButton(tr("Check for Latest Pre-release"), updateGroup);
    checkPreReleaseButton->setToolTip(tr("Pre-releases may be unstable. Update at your own risk."));
    updateLayout->addWidget(checkStableButton);
    updateLayout->addWidget(checkPreReleaseButton);

    QLabel *versionLabel = new QLabel(tr("Current Version: %1").arg(SCRIPTURA_VERSION), updateGroup);
    updateLayout->addWidget(versionLabel);

    mainLayout->addWidget(updateGroup);

    // Plugin Registry
    QGroupBox *registryGroup = new QGroupBox(tr("Plugin Registry"), content);
    QVBoxLayout *registryLayout = new QVBoxLayout(registryGroup);
    QLabel *registryUrlLabel = new QLabel(tr("Registry URL:"), registryGroup);
    QLineEdit *registryUrlEdit = new QLineEdit(registryGroup);
    registryUrlEdit->setPlaceholderText(tr("https://example.com/plugin-registry.json"));
    registryUrlEdit->setText(registryUrl);
    connect(registryUrlEdit, &QLineEdit::textChanged, this, [this](const QString &url) {
        m_pluginRegistry->setRegistryUrl(url);
        QSettings().setValue("plugin/registryUrl", url);
    });
    registryLayout->addWidget(registryUrlLabel);
    registryLayout->addWidget(registryUrlEdit);
    mainLayout->addWidget(registryGroup);

    // Connect update buttons
    connect(checkStableButton, &QPushButton::clicked, this, [this]() {
        updater->checkForUpdates(QCoreApplication::applicationVersion(), QSettings().value("updates/url", "https://api.github.com/repos/jason1015-coder/scriptura/releases/latest").toString());
    });
    connect(checkPreReleaseButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this, tr("Pre-release Warning"),
            tr("Pre-releases may be unstable and contain bugs.\n"
               "They are intended for testing new features before the stable release.\n\n"
               "Do you want to continue?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
            updater->checkForUpdates(QCoreApplication::applicationVersion(), QSettings().value("updates/url", "https://api.github.com/repos/jason1015-coder/scriptura/releases").toString());
    });

    mainLayout->addStretch();

    scrollArea->setWidget(content);
    return scrollArea;
}


void MainWindow::on_action_editor_settings_triggered()
{
    qDebug() << "on_action_editor_settings_triggered: switching to unifiedSettingsWidget";
    // Find existing settings tab or add a new one
    int settingsTabIndex = -1;
    for (int i = 0; i < tabBar->count(); ++i) {
        if (tabBar->tabData(i).typeId() == QMetaType::Int &&
            static_cast<TabType>(tabBar->tabData(i).toInt()) == TabType::Settings) {
            settingsTabIndex = i;
            break;
        }
    }
    if (settingsTabIndex == -1) {
        settingsTabIndex = tabBar->addTab(tr("Settings"));
        tabBar->setTabData(settingsTabIndex, static_cast<int>(TabType::Settings));
        tabBar->setTabButton(settingsTabIndex, QTabBar::RightSide, createSettingsTabCloseButton(settingsTabIndex));
    }
    tabBar->setCurrentIndex(settingsTabIndex);
    editorStack->setCurrentWidget(unifiedSettingsWidget);
}

void MainWindow::applyTheme(const Theme &theme)
{
    // Delegate palette, stylesheet, and theme definition to ThemeManager
    ThemeManager::Theme newTheme(
        static_cast<ThemeManager::ColorFamily>(static_cast<int>(theme.family)),
        static_cast<ThemeManager::Mode>(static_cast<int>(theme.mode)),
        ThemeManager::Features(static_cast<ThemeManager::Feature>(static_cast<int>(theme.features)))
    );
    if (m_themeManager) {
        m_themeManager->setCurrentTheme(newTheme);
    }

    // Use the resolved theme definition for all colours
    ThemeDefinition def = m_themeManager ? m_themeManager->currentDefinition()
                                         : ThemeManager::buildDefinition(
                                             static_cast<ThemeManager::ColorFamily>(static_cast<int>(theme.family)),
                                             static_cast<ThemeManager::Mode>(static_cast<int>(theme.mode)));

    // Wire ThemeIcons to the ThemeManager so SVG icons use the dedicated svgColor
    ThemeIcons::instance()->setThemeManager(m_themeManager);
    ThemeIcons::instance()->recolorAll();

    // Apply syntax colours from the theme definition to all open editors
    QList<QColor> synColors = def.syntaxColorList();
    if (synColors.size() >= 11) {
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            if (auto *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i))) {
                editor->setThemeColors(
                    synColors[0],  synColors[1],  synColors[2],  synColors[3],
                    synColors[4],  synColors[5],  synColors[6],  synColors[7],
                    synColors[8],  synColors[9],  synColors[10]);
            }
        }
    }

    QPalette palette = ThemeManager::buildPalette(def);
    bool isDark = theme.isDark();

    QColor windowColor = def.bgColor;
    QColor baseColor = def.baseColor;
    QColor buttonColor = def.buttonColor;
    QColor textColor = def.textColor;
    QColor accentColor = def.highlightColor;
    QColor midColor = def.midColor;
    QColor lightColor = def.lightColor;

    // Compute glassmorphism translucents
    QColor glassSidebar = isDark ? QColor(42, 44, 47, 217) : QColor(242, 242, 247, 217);
    QColor glassPanel = isDark ? QColor(38, 40, 43, 217) : QColor(232, 232, 232, 217);
    QColor glassToolbar = isDark ? QColor(56, 58, 61, 230) : QColor(246, 246, 246, 230);
    QColor glassStatus = isDark ? QColor(28, 31, 33, 242) : QColor(240, 240, 240, 242);
    QColor glassBorder = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 25);
    QColor neumorphicLight = isDark ? QColor(255, 255, 255, 12) : QColor(255, 255, 255, 180);
    QColor neumorphicDark = isDark ? QColor(0, 0, 0, 40) : QColor(0, 0, 0, 25);

    QString modernSheet = QString(R"(
        /* Xcode-Style Layout — Glassmorphism & Neumorphism */

        /* Base widget styling */
        QMainWindow, QDialog {
            background-color: %1;
        }

        /* Group boxes and frames — neumorphic */
        QGroupBox, QFrame#recentProjectsFrame {
            background-color: rgba(%8, %9, %10, 0.6);
            border: 1px solid %3;
            border-radius: 12px;
            margin-top: 16px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top center;
            padding: 0 8px;
            color: %4;
            font-weight: 600;
        }

        /* Tool buttons — neumorphic soft press */
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
            min-width: 24px;
            min-height: 24px;
            margin: 0px;
            color: %4;
        }
        QToolButton:hover {
            background-color: rgba(%11, %12, %13, 0.10);
        }
        QToolButton:checked {
            background-color: rgba(%14, %15, %16, 0.15);
            color: %4;
        }
        QToolButton:pressed {
            background-color: rgba(%11, %12, %13, 0.18);
        }
        QToolButton:disabled {
            color: %3;
        }

        /* Push buttons — neumorphic raised */
        QPushButton {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.08);
            border-radius: 8px;
            padding: 6px 14px;
            color: %4;
            font-weight: 500;
            min-height: 28px;
            min-width: 64px;
        }
        QPushButton:hover {
            background-color: rgba(%8, %9, %10, 0.7);
            border-color: rgba(%14, %15, %16, 0.3);
        }
        QPushButton:pressed {
            background-color: rgba(%8, %9, %10, 0.4);
        }
        QPushButton:disabled {
            background-color: rgba(%8, %9, %10, 0.3);
            color: %3;
            border-color: transparent;
        }

        /* Input fields — neumorphic inset */
        QLineEdit, QSpinBox, QFontComboBox {
            background-color: rgba(%20, %21, %22, 0.4);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 6px;
            padding: 6px 10px;
            color: %4;
            min-height: 28px;
        }
        QLineEdit:focus, QSpinBox:focus, QFontComboBox:focus {
            border: 1px solid rgba(%14, %15, %16, 0.4);
            background-color: rgba(%20, %21, %22, 0.5);
        }
        QLineEdit:disabled, QSpinBox:disabled, QFontComboBox:disabled {
            border-color: transparent;
            color: %3;
            background-color: rgba(%8, %9, %10, 0.3);
        }

        /* Text edit areas */
        QTextEdit, QPlainTextEdit {
            background-color: %7;
            border: 1px solid rgba(%17, %18, %19, 0.08);
            border-radius: 6px;
            padding: 8px;
            color: %4;
            selection-background-color: %6;
            selection-color: %1;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid rgba(%14, %15, %16, 0.3);
        }

        /* Tree view — glassmorphism */
        QTreeView {
            background-color: transparent;
            border: none;
            selection-background-color: rgba(%14, %15, %16, 0.20);
            selection-color: %4;
            outline: none;
        }
        QTreeView::item {
            padding: 4px 8px;
            border-radius: 6px;
            margin: 1px 4px;
            color: %4;
        }
        QTreeView::item:hover {
            background-color: rgba(%11, %12, %13, 0.08);
        }
        QTreeView::item:selected {
            background-color: rgba(%14, %15, %16, 0.20);
            color: %4;
        }

        /* Xcode-style tabs — active merges with editor */
        QTabBar::tab {
            background-color: transparent;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 8px 16px;
            margin-right: 1px;
            color: %3;
            min-width: 80px;
            font-size: 12px;
        }
        QTabBar::tab:hover:!selected {
            background-color: rgba(%11, %12, %13, 0.08);
            color: %4;
        }
        QTabBar::tab:selected {
            background-color: %7;
            border-bottom: 2px solid %6;
            color: %4;
        }
        QTabBar::tab:disabled {
            color: %3;
        }
        QTabWidget::pane {
            border: none;
            top: -1px;
        }

        /* Status bar — darkest glassmorphism layer */
        QStatusBar {
            background-color: rgba(%23, %24, %25, 0.95);
            border-top: 1px solid %3;
            color: %3;
            padding: 2px 10px;
            font-size: 12px;
        }

        /* Menu bar — hidden but styled for dropdown */
        QMenuBar {
            background-color: transparent;
            border: none;
            padding: 2px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 12px;
            border-radius: 6px;
            color: %4;
        }
        QMenuBar::item:selected {
            background-color: rgba(%11, %12, %13, 0.10);
        }

        /* Menu dropdown — glassmorphism */
        QMenu {
            background-color: rgba(%8, %9, %10, 0.92);
            border: 1px solid rgba(%17, %18, %19, 0.12);
            border-radius: 10px;
            padding: 6px;
            margin: 2px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 6px 24px 6px 10px;
            border-radius: 6px;
            color: %4;
        }
        QMenu::item:selected {
            background-color: rgba(%14, %15, %16, 0.20);
            color: %4;
        }
        QMenu::item:disabled {
            color: %3;
        }
        QMenu::separator {
            height: 1px;
            background-color: rgba(%17, %18, %19, 0.10);
            margin: 4px 8px;
        }

        /* Scrollbars — thin overlay */
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0px;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background-color: rgba(%11, %12, %13, 0.25);
            border-radius: 6px;
            min-height: 30px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background-color: rgba(%11, %12, %13, 0.25);
            border-radius: 6px;
            min-width: 30px;
            margin: 2px;
        }
        QScrollBar::handle:hover {
            background-color: rgba(%11, %12, %13, 0.45);
        }
        QScrollBar::add-line, QScrollBar::sub-line,
        QScrollBar::add-page, QScrollBar::sub-page {
            height: 0px;
            width: 0px;
            border: none;
            background: transparent;
        }

        /* Checkbox and radio — neumorphic */
        QCheckBox, QRadioButton {
            spacing: 8px;
            color: %4;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 6px;
            border: 1px solid rgba(%17, %18, %19, 0.15);
            background-color: rgba(%20, %21, %22, 0.3);
        }
        QRadioButton::indicator {
            border-radius: 8px;
        }
        QCheckBox::indicator:hover, QRadioButton::indicator:hover {
            border-color: rgba(%14, %15, %16, 0.4);
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background-color: %6;
            border: 1px solid %6;
        }
        QCheckBox:disabled, QRadioButton:disabled {
            color: %3;
        }
        QCheckBox::indicator:disabled, QRadioButton::indicator:disabled {
            border-color: transparent;
            background-color: rgba(%8, %9, %10, 0.3);
        }

        /* Tooltip — glassmorphism */
        QToolTip {
            background-color: rgba(%8, %9, %10, 0.92);
            color: %4;
            border: 1px solid rgba(%17, %18, %19, 0.12);
            border-radius: 8px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* Toolbar */
        QToolBar {
            background-color: transparent;
            border: none;
            spacing: 4px;
            padding: 2px;
        }
        QTabBar {
            background-color: transparent;
            border: none;
        }

        /* Container widgets — glassmorphism layers */
        QWidget#bottomPanelContainer {
            background-color: rgba(%26, %27, %28, 0.85);
            border-top: 1px solid rgba(%17, %18, %19, 0.10);
        }
        QWidget#sidebarDrawer {
            background-color: rgba(%29, %30, %31, 0.85);
            border-right: 1px solid rgba(%17, %18, %19, 0.10);
        }
        QWidget#editorContainer {
            background-color: %7;
        }
        QWidget#unifiedTitleBar {
            background-color: rgba(%32, %33, %34, 0.90);
            border-bottom: 1px solid rgba(%17, %18, %19, 0.10);
        }

        QFrame#recentProjectsFrame {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 12px;
            padding: 16px;
        }

        /* Special buttons */
        QPushButton#projectButton {
            background-color: rgba(%8, %9, %10, 0.5);
            border: 1px solid rgba(%17, %18, %19, 0.10);
            border-radius: 8px;
            padding: 8px;
            text-align: left;
        }
        QPushButton#projectButton:hover {
            background-color: rgba(%8, %9, %10, 0.7);
        }

        /* Primary buttons — neumorphic accent */
        QPushButton#primaryButton {
            background-color: %6;
            border: none;
            border-radius: 10px;
            padding: 12px 24px;
            color: %1;
            font-weight: 600;
            font-size: 14px;
            min-width: 140px;
        }
        QPushButton#primaryButton:hover {
            background-color: %6;
        }
        QPushButton#primaryButton:pressed {
            background-color: %6;
        }

        /* Welcome screen title */
        QLabel#welcomeTitle {
            color: %4;
        }

        /* Side bar icon bar — glassmorphism */
        QWidget#sidebarIconBar {
            background-color: transparent;
            border-top: 1px solid rgba(%17, %18, %19, 0.10);
            padding: 8px;
        }

        /* Focus styling */
        :focus {
            outline: none;
        }
        ::selection {
            background-color: %6;
            color: %1;
        }
    )")
        .arg(windowColor.name())          // %1
        .arg(buttonColor.name())          // %2
        .arg(midColor.name())             // %3
        .arg(textColor.name())            // %4
        .arg(lightColor.name())           // %5
        .arg(accentColor.name())          // %6
        .arg(baseColor.name())            // %7
        .arg(QString::number(glassSidebar.red()))    // %8
        .arg(QString::number(glassSidebar.green()))  // %9
        .arg(QString::number(glassSidebar.blue()))   // %10
        .arg(QString::number(windowColor.red()))     // %11
        .arg(QString::number(windowColor.green()))   // %12
        .arg(QString::number(windowColor.blue()))    // %13
        .arg(QString::number(accentColor.red()))     // %14
        .arg(QString::number(accentColor.green()))   // %15
        .arg(QString::number(accentColor.blue()))    // %16
        .arg(QString::number(midColor.red()))        // %17
        .arg(QString::number(midColor.green()))      // %18
        .arg(QString::number(midColor.blue()))       // %19
        .arg(QString::number(baseColor.red()))       // %20
        .arg(QString::number(baseColor.green()))     // %21
        .arg(QString::number(baseColor.blue()))      // %22
        .arg(QString::number(glassStatus.red()))     // %23
        .arg(QString::number(glassStatus.green()))   // %24
        .arg(QString::number(glassStatus.blue()))    // %25
        .arg(QString::number(glassPanel.red()))      // %26
        .arg(QString::number(glassPanel.green()))    // %27
        .arg(QString::number(glassPanel.blue()))     // %28
        .arg(QString::number(glassSidebar.red()))    // %29
        .arg(QString::number(glassSidebar.green()))  // %30
        .arg(QString::number(glassSidebar.blue()))   // %31
        .arg(QString::number(glassToolbar.red()))    // %32
        .arg(QString::number(glassToolbar.green()))  // %33
        .arg(QString::number(glassToolbar.blue()));  // %34

    for (QWidget *widget : QApplication::allWidgets()) {
        widget->setPalette(palette);
        widget->setAutoFillBackground(true);
        if (CodeEditor *editor = qobject_cast<CodeEditor*>(widget)) {
            editor->viewport()->setPalette(palette);
            editor->viewport()->setAutoFillBackground(true);
            editor->setDarkMode(isDark);
            QColor trailingBg = isDark ? QColor("#7f1d1d") : QColor("#fecaca");
            editor->setThemeColors(def.synKeyword, def.synString, def.synComment, def.synNumber,
                                   def.synPreprocessor, def.synTag, def.synAttribute,
                                   def.synCssProperty, def.synVariable, def.synFunction,
                                   def.synEscape, trailingBg);
        }
    }

    QSettings settings;
    settings.setValue("theme/selected", themeToLegacyInt(theme));
}

void MainWindow::on_action_theme_triggered()
{
    // Theme settings are now part of the unified settings page
    on_action_editor_settings_triggered();
}


void MainWindow::on_action_license_triggered()
{
    QMessageBox::about(this, tr("License"), tr("MIT License\n\n"
        "Copyright (c) 2026 Scriptura\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE."));
  }

  void MainWindow::on_action_manage_plugins_triggered()
  {
      if (pluginManagerDialog) {
          pluginManagerDialog->refresh();
          pluginManagerDialog->show();
      }
  }

  void MainWindow::setSidebarCollapsed(bool collapsed)
{
    QSettings settings;
    settings.setValue("ui/sidebarCollapsed", collapsed);

    if (collapsed) {
        if (sidebarToggleButton)
            sidebarToggleButton->setChecked(false);
        QPropertyAnimation *animation = new QPropertyAnimation(ui->sidebarDrawer, "maximumWidth");
        animation->setDuration(200);
        animation->setStartValue(ui->sidebarDrawer->width());
        animation->setEndValue(0);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this]() {
            ui->sidebarDrawer->setMinimumWidth(0);
            ui->sidebarDrawer->setMaximumWidth(0);
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        if (sidebarToggleButton)
            sidebarToggleButton->setChecked(true);
        QPropertyAnimation *animation = new QPropertyAnimation(ui->sidebarDrawer, "maximumWidth");
        animation->setDuration(200);
        animation->setStartValue(ui->sidebarDrawer->width());
        animation->setEndValue(240);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this]() {
            ui->sidebarDrawer->setMinimumWidth(48);
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);

        QPropertyAnimation *minAnim = new QPropertyAnimation(ui->sidebarDrawer, "minimumWidth");
        minAnim->setDuration(200);
        minAnim->setStartValue(0);
        minAnim->setEndValue(48);
        minAnim->setEasingCurve(QEasingCurve::InOutCubic);
        minAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}


QString MainWindow::findTerminal()
{
#ifdef Q_OS_WIN
    return "cmd.exe";
#elif defined(Q_OS_MAC)
    return "open";
#else
    QStringList candidates = {"konsole", "gnome-terminal", "xfce4-terminal",
                              "mate-terminal", "alacritty", "kitty", "xterm"};
    for (const QString &term : candidates) {
        if (!QStandardPaths::findExecutable(term).isEmpty())
            return term;
    }
    return "xterm";
#endif
}


void MainWindow::updateFamilyButtonPreview(QPushButton *btn, ThemeColorFamily family, ThemeMode mode, ThemeFeatures features)
{
    ThemeManager::ColorFamily tmFamily = static_cast<ThemeManager::ColorFamily>(static_cast<int>(family));
    ThemeManager::Mode tmMode = static_cast<ThemeManager::Mode>(static_cast<int>(mode));
    ThemeDefinition td = ThemeManager::buildDefinition(tmFamily, tmMode);
    QPalette p = ThemeManager::buildPalette(td);

    QColor bg = p.color(QPalette::Window);
    QColor accent = td.highlightColor;
    QColor textColor = p.color(QPalette::WindowText);

    btn->setStyleSheet(QString("background-color: %1; color: %2; padding: 10px; border: 1px solid %2; border-radius: 4px;")
                       .arg(bg.name()).arg(textColor.name()));
}

QPalette MainWindow::buildBasePalette(ThemeColorFamily family, ThemeMode mode)
{
    QPalette p;
    bool isDark = (mode == ThemeMode::Dark);

    auto s = [&](QPalette::ColorRole role, const QColor &c) { p.setColor(role, c); };

    switch (family) {
    case ThemeColorFamily::Default:
        if (isDark) {
            s(QPalette::Window, QColor(45, 45, 48));
            s(QPalette::WindowText, Qt::white);
            s(QPalette::Base, QColor(30, 30, 32));
            s(QPalette::AlternateBase, QColor(55, 55, 60));
            s(QPalette::ToolTipBase, Qt::white);
            s(QPalette::ToolTipText, Qt::white);
            s(QPalette::Text, Qt::white);
            s(QPalette::Button, QColor(60, 60, 65));
            s(QPalette::ButtonText, Qt::white);
            s(QPalette::BrightText, Qt::red);
            s(QPalette::Link, QColor(100, 150, 255));
            s(QPalette::Highlight, QColor(100, 150, 255));
            s(QPalette::HighlightedText, Qt::white);
            s(QPalette::Light, QColor(80, 80, 85));
            s(QPalette::Mid, QColor(50, 50, 55));
            s(QPalette::Dark, QColor(35, 35, 38));
            s(QPalette::Midlight, QColor(70, 70, 75));
        } else {
            s(QPalette::Window, QColor(245, 245, 247));
            s(QPalette::WindowText, QColor(30, 30, 32));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(235, 235, 238));
            s(QPalette::ToolTipBase, Qt::white);
            s(QPalette::ToolTipText, QColor(30, 30, 32));
            s(QPalette::Text, QColor(30, 30, 32));
            s(QPalette::Button, QColor(240, 240, 242));
            s(QPalette::ButtonText, QColor(30, 30, 32));
            s(QPalette::BrightText, Qt::red);
            s(QPalette::Link, QColor(0, 122, 255));
            s(QPalette::Highlight, QColor(0, 122, 255));
            s(QPalette::HighlightedText, Qt::white);
            s(QPalette::Light, QColor(250, 250, 252));
            s(QPalette::Mid, QColor(200, 200, 205));
            s(QPalette::Dark, QColor(180, 180, 185));
            s(QPalette::Midlight, QColor(245, 245, 247));
        }
        return p;

    case ThemeColorFamily::Blue:
        if (isDark) {
            s(QPalette::Window, QColor(35, 42, 55));
            s(QPalette::Base, QColor(25, 30, 42));
            s(QPalette::AlternateBase, QColor(45, 52, 65));
            s(QPalette::Button, QColor(50, 58, 72));
            s(QPalette::Highlight, QColor(80, 140, 255));
        } else {
            s(QPalette::Window, QColor(235, 243, 255));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(220, 230, 245));
            s(QPalette::Button, QColor(220, 230, 245));
            s(QPalette::Highlight, QColor(0, 122, 255));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 30, 42));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 72, 85) : QColor(240, 245, 255));
        s(QPalette::Mid, isDark ? QColor(45, 52, 60) : QColor(175, 190, 210));
        s(QPalette::Dark, isDark ? QColor(30, 35, 45) : QColor(150, 165, 185));
        s(QPalette::Midlight, isDark ? QColor(55, 62, 75) : QColor(230, 240, 250));
        break;

    case ThemeColorFamily::Green:
        if (isDark) {
            s(QPalette::Window, QColor(35, 48, 35));
            s(QPalette::Base, QColor(25, 38, 25));
            s(QPalette::AlternateBase, QColor(45, 58, 45));
            s(QPalette::Button, QColor(50, 65, 50));
            s(QPalette::Highlight, QColor(80, 200, 120));
        } else {
            s(QPalette::Window, QColor(235, 248, 235));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(220, 240, 220));
            s(QPalette::Button, QColor(220, 240, 220));
            s(QPalette::Highlight, QColor(0, 180, 80));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 40, 25));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 78, 65) : QColor(240, 250, 240));
        s(QPalette::Mid, isDark ? QColor(45, 58, 48) : QColor(170, 200, 170));
        s(QPalette::Dark, isDark ? QColor(30, 42, 30) : QColor(140, 170, 140));
        s(QPalette::Midlight, isDark ? QColor(55, 68, 55) : QColor(230, 245, 230));
        break;

    case ThemeColorFamily::Red:
        if (isDark) {
            s(QPalette::Window, QColor(48, 35, 35));
            s(QPalette::Base, QColor(38, 25, 25));
            s(QPalette::AlternateBase, QColor(58, 45, 45));
            s(QPalette::Button, QColor(65, 50, 50));
            s(QPalette::Highlight, QColor(255, 100, 100));
        } else {
            s(QPalette::Window, QColor(248, 238, 238));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(245, 225, 225));
            s(QPalette::Button, QColor(245, 225, 225));
            s(QPalette::Highlight, QColor(255, 55, 55));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::Text, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(40, 25, 25));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(78, 65, 65) : QColor(252, 240, 240));
        s(QPalette::Mid, isDark ? QColor(58, 45, 45) : QColor(210, 185, 185));
        s(QPalette::Dark, isDark ? QColor(42, 30, 30) : QColor(190, 160, 160));
        s(QPalette::Midlight, isDark ? QColor(68, 55, 55) : QColor(248, 235, 235));
        break;

    case ThemeColorFamily::Yellow:
        if (isDark) {
            s(QPalette::Window, QColor(48, 45, 32));
            s(QPalette::Base, QColor(38, 35, 22));
            s(QPalette::AlternateBase, QColor(58, 55, 42));
            s(QPalette::Button, QColor(65, 62, 48));
            s(QPalette::Highlight, QColor(220, 200, 80));
        } else {
            s(QPalette::Window, QColor(248, 248, 235));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(245, 242, 215));
            s(QPalette::Button, QColor(245, 242, 215));
            s(QPalette::Highlight, QColor(210, 180, 0));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::Text, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(42, 40, 22));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(78, 75, 62) : QColor(252, 250, 240));
        s(QPalette::Mid, isDark ? QColor(58, 55, 42) : QColor(210, 200, 150));
        s(QPalette::Dark, isDark ? QColor(42, 40, 25) : QColor(190, 180, 130));
        s(QPalette::Midlight, isDark ? QColor(68, 65, 52) : QColor(248, 245, 225));
        break;

    case ThemeColorFamily::Brown:
        if (isDark) {
            s(QPalette::Window, QColor(42, 35, 28));
            s(QPalette::Base, QColor(32, 25, 18));
            s(QPalette::AlternateBase, QColor(52, 45, 38));
            s(QPalette::Button, QColor(58, 50, 42));
            s(QPalette::Highlight, QColor(190, 140, 90));
        } else {
            s(QPalette::Window, QColor(248, 245, 238));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(240, 232, 218));
            s(QPalette::Button, QColor(240, 232, 218));
            s(QPalette::Highlight, QColor(180, 130, 70));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::Text, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(35, 28, 20));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(72, 65, 58) : QColor(250, 247, 240));
        s(QPalette::Mid, isDark ? QColor(52, 45, 38) : QColor(200, 185, 160));
        s(QPalette::Dark, isDark ? QColor(35, 30, 25) : QColor(180, 165, 140));
        s(QPalette::Midlight, isDark ? QColor(62, 55, 48) : QColor(245, 240, 228));
        break;

    case ThemeColorFamily::Cyan:
        if (isDark) {
            s(QPalette::Window, QColor(35, 48, 50));
            s(QPalette::Base, QColor(25, 38, 40));
            s(QPalette::AlternateBase, QColor(45, 58, 60));
            s(QPalette::Button, QColor(50, 65, 68));
            s(QPalette::Highlight, QColor(60, 190, 210));
        } else {
            s(QPalette::Window, QColor(235, 248, 250));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(215, 238, 242));
            s(QPalette::Button, QColor(215, 238, 242));
            s(QPalette::Highlight, QColor(0, 180, 200));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::Text, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(25, 40, 42));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(65, 78, 82) : QColor(238, 248, 250));
        s(QPalette::Mid, isDark ? QColor(45, 58, 62) : QColor(165, 200, 210));
        s(QPalette::Dark, isDark ? QColor(30, 42, 45) : QColor(140, 180, 190));
        s(QPalette::Midlight, isDark ? QColor(55, 68, 72) : QColor(228, 242, 245));
        break;

    case ThemeColorFamily::Violet:
        if (isDark) {
            s(QPalette::Window, QColor(40, 35, 55));
            s(QPalette::Base, QColor(30, 25, 42));
            s(QPalette::AlternateBase, QColor(50, 45, 65));
            s(QPalette::Button, QColor(55, 48, 70));
            s(QPalette::Highlight, QColor(140, 100, 220));
        } else {
            s(QPalette::Window, QColor(240, 238, 252));
            s(QPalette::Base, QColor(255, 255, 255));
            s(QPalette::AlternateBase, QColor(228, 222, 242));
            s(QPalette::Button, QColor(228, 222, 242));
            s(QPalette::Highlight, QColor(140, 80, 230));
        }
        s(QPalette::WindowText, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::Text, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::ButtonText, isDark ? Qt::white : QColor(32, 28, 45));
        s(QPalette::HighlightedText, Qt::white);
        s(QPalette::Light, isDark ? QColor(72, 65, 85) : QColor(245, 242, 252));
        s(QPalette::Mid, isDark ? QColor(52, 45, 62) : QColor(190, 178, 215));
        s(QPalette::Dark, isDark ? QColor(35, 30, 48) : QColor(165, 150, 195));
        s(QPalette::Midlight, isDark ? QColor(62, 55, 75) : QColor(238, 232, 248));
        break;
    }

    return p;
}

