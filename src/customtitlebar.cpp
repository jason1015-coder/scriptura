#include "customtitlebar.h"
#include "themeicons.h"
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDebug>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
    , minimizeButton(nullptr)
    , maximizeButton(nullptr)
    , closeButton(nullptr)
    , titleLabel(nullptr)
    , sidebarToggleButton(nullptr)
    , settingsButton(nullptr)
    , inspectorToggleButton(nullptr)
    , searchField(nullptr)
    , m_isDragging(false)
    , m_dragPosition(QPoint())
{
    setFixedHeight(52);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setObjectName("unifiedTitleBar");
    setupLayout();
    styleButtons();
}

void CustomTitleBar::setupLayout()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(2);

    // -- Leading: sidebar toggle --
    sidebarToggleButton = new QPushButton(this);
    sidebarToggleButton->setObjectName("TitleBarSidebarToggle");
    sidebarToggleButton->setFixedSize(36, 36);
    sidebarToggleButton->setToolTip(tr("Toggle Sidebar"));
    sidebarToggleButton->setCheckable(true);
    sidebarToggleButton->setChecked(true);
    ThemeIcons::instance()->setIcon(sidebarToggleButton, ":/icons/sidebar-toggle.svg");
    sidebarToggleButton->setIconSize(QSize(18, 18));
    connect(sidebarToggleButton, &QPushButton::clicked, this, &CustomTitleBar::sidebarToggleClicked);
    layout->addWidget(sidebarToggleButton);

    // -- Spacer --
    layout->addSpacing(4);

    // -- Center: title --
    titleLabel = new QLabel(tr("Scriptura"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(11);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: palette(text); background: transparent;");
    layout->addWidget(titleLabel, 0, Qt::AlignVCenter);

    // -- Spacer --
    layout->addStretch(1);

    // -- Search field --
    searchField = new QLineEdit(this);
    searchField->setObjectName("unifiedSearchField");
    searchField->setPlaceholderText(tr("Search..."));
    searchField->setFixedSize(220, 30);
    searchField->setClearButtonEnabled(true);
    connect(searchField, &QLineEdit::returnPressed, this, &CustomTitleBar::searchRequested);
    layout->addWidget(searchField, 0, Qt::AlignVCenter);

    layout->addSpacing(4);

    // -- Trailing: inspector toggle + window controls --
    inspectorToggleButton = new QPushButton(this);
    inspectorToggleButton->setObjectName("TitleBarInspectorToggle");
    inspectorToggleButton->setFixedSize(36, 36);
    inspectorToggleButton->setToolTip(tr("Toggle Inspector"));
    inspectorToggleButton->setCheckable(true);
    ThemeIcons::instance()->setIcon(inspectorToggleButton, ":/icons/inspector.svg");
    inspectorToggleButton->setIconSize(QSize(18, 18));
    connect(inspectorToggleButton, &QPushButton::clicked, this, &CustomTitleBar::inspectorToggleClicked);
    layout->addWidget(inspectorToggleButton);

    // -- Settings toggle --
    settingsButton = new QPushButton(this);
    settingsButton->setObjectName("TitleBarSettings");
    settingsButton->setFixedSize(36, 36);
    settingsButton->setToolTip(tr("Editor Settings"));
    ThemeIcons::instance()->setIcon(settingsButton, ":/icons/settings.svg");
    settingsButton->setIconSize(QSize(18, 18));
    connect(settingsButton, &QPushButton::clicked, this, &CustomTitleBar::settingsClicked);
    layout->addWidget(settingsButton);

    layout->addSpacing(8);

    // -- Window controls (traffic lights) --
    minimizeButton = new QPushButton(this);
    maximizeButton = new QPushButton(this);
    closeButton = new QPushButton(this);

    minimizeButton->setObjectName("TitleBarMinimize");
    maximizeButton->setObjectName("TitleBarMaximize");
    closeButton->setObjectName("TitleBarClose");

    minimizeButton->setFixedSize(28, 28);
    maximizeButton->setFixedSize(28, 28);
    closeButton->setFixedSize(28, 28);

    layout->addWidget(minimizeButton);
    layout->addWidget(maximizeButton);
    layout->addWidget(closeButton);

    connect(minimizeButton, &QPushButton::clicked, this, [this]() { emit minimizeRequest(); });
    connect(maximizeButton, &QPushButton::clicked, this, [this]() { emit maximizeRequest(); });
    connect(closeButton, &QPushButton::clicked, this, [this]() { emit closeRequest(); });
}

void CustomTitleBar::styleButtons()
{
    const QString buttonStyle = R"(
        QPushButton {
            border: none;
            background-color: transparent;
            color: palette(text);
            border-radius: 14px;
            padding: 0px;
        }
        QPushButton:hover {
            background-color: rgba(128, 128, 128, 0.15);
        }
        QPushButton:pressed {
            background-color: rgba(128, 128, 128, 0.25);
        }
    )";

    const QString checkableButtonStyle = R"(
        QPushButton {
            border: none;
            background-color: transparent;
            color: palette(text);
            border-radius: 6px;
            padding: 0px;
        }
        QPushButton:hover {
            background-color: rgba(128, 128, 128, 0.15);
        }
        QPushButton:checked {
            background-color: rgba(128, 128, 128, 0.20);
        }
        QPushButton:pressed {
            background-color: rgba(128, 128, 128, 0.25);
        }
    )";

    minimizeButton->setStyleSheet(buttonStyle);
    maximizeButton->setStyleSheet(buttonStyle);
    closeButton->setStyleSheet(buttonStyle);
    sidebarToggleButton->setStyleSheet(checkableButtonStyle);
    settingsButton->setStyleSheet(buttonStyle);
    inspectorToggleButton->setStyleSheet(checkableButtonStyle);

    // Search field styling
    searchField->setStyleSheet(R"(
        QLineEdit#unifiedSearchField {
            background-color: rgba(128, 128, 128, 0.08);
            border: 1px solid rgba(128, 128, 128, 0.12);
            border-radius: 6px;
            padding: 4px 10px;
            color: palette(text);
            font-size: 12px;
            font-family: "Inter", "SF Pro Text", sans-serif;
        }
        QLineEdit#unifiedSearchField:focus {
            background-color: rgba(128, 128, 128, 0.14);
            border: 1px solid rgba(128, 128, 128, 0.25);
        }
        QLineEdit#unifiedSearchField:hover {
            background-color: rgba(128, 128, 128, 0.10);
        }
    )");
}

void CustomTitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Window control glyphs — color is read from palette() inside paintWindowControls
    paintWindowControls(p, minimizeButton->geometry(), minimizeButton->underMouse(), minimizeButton->isDown(), QStringLiteral("\u2014"));
    paintWindowControls(p, maximizeButton->geometry(), maximizeButton->underMouse(), maximizeButton->isDown(), QStringLiteral("\u25a1"));
    paintWindowControls(p, closeButton->geometry(), closeButton->underMouse(), closeButton->isDown(), QStringLiteral("\u2715"));
}

void CustomTitleBar::paintWindowControls(QPainter &p, const QRect &buttonRect, bool hovered, bool pressed, const QString &glyph)
{
    if (glyph.isEmpty())
        return;

    QColor color = palette().color(foregroundRole());
    if (closeButton && buttonRect == closeButton->geometry()) {
        if (pressed) {
            color = Qt::white;
        } else if (hovered) {
            color = Qt::white;
        }
    }

    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);
    p.setPen(color);

    QRect textRect = buttonRect.adjusted(0, 2, 0, -2);
    p.drawText(textRect, Qt::AlignCenter, glyph);
}

void CustomTitleBar::handleMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint();
        m_isDragging = true;
    }
}

void CustomTitleBar::handleMouseMove(QMouseEvent *event)
{
    if (m_isDragging) {
        QWidget *mainWindow = window();
        if (mainWindow) {
            QPoint delta = event->globalPosition().toPoint() - m_dragPosition;
            mainWindow->move(mainWindow->pos() + delta);
            m_dragPosition = event->globalPosition().toPoint();
        }
    }
}

void CustomTitleBar::stopDrag()
{
    m_isDragging = false;
}
