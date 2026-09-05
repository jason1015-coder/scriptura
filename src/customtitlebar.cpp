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

namespace {
// Perceived luminance in 0..1 (ITU-R BT.601 weights). Used to guarantee the
// window control glyphs stay visible against any theme's title bar colour.
qreal colorLuminance(const QColor &c)
{
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
}

qreal colorContrastRatio(const QColor &a, const QColor &b)
{
    const qreal hi = qMax(colorLuminance(a), colorLuminance(b));
    const qreal lo = qMin(colorLuminance(a), colorLuminance(b));
    return (hi + 0.05) / (lo + 0.05);
}
} // namespace

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
    setStyleSheet(R"(
        CustomTitleBar#unifiedTitleBar {
            background-color: palette(window);
            border-radius: 14px;
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
        }
    )");
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
    connect(searchField, &QLineEdit::returnPressed, this, [this]() {
        emit searchRequested(searchField->text());
    });
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

    // These buttons are fully transparent — this title bar paints their glyphs
    // and hover/pressed backgrounds (see paintWindowControls). They must not
    // take focus, and must notify this bar so it can repaint on state changes.
    minimizeButton->setFocusPolicy(Qt::NoFocus);
    maximizeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setFocusPolicy(Qt::NoFocus);
    minimizeButton->installEventFilter(this);
    maximizeButton->installEventFilter(this);
    closeButton->installEventFilter(this);
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
            border-radius: 8px;
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

    // Window control buttons must NOT paint their own hover/pressed background:
    // this title bar paints both the glyph and its hover state in paintEvent(),
    // with theme-aware colours that stay visible in every theme.
    const QString windowButtonStyle = R"(
        QPushButton {
            border: none;
            background-color: transparent;
            padding: 0px;
        }
    )";

    minimizeButton->setStyleSheet(windowButtonStyle);
    maximizeButton->setStyleSheet(windowButtonStyle);
    closeButton->setStyleSheet(windowButtonStyle);
    sidebarToggleButton->setStyleSheet(checkableButtonStyle);
    settingsButton->setStyleSheet(buttonStyle);
    inspectorToggleButton->setStyleSheet(checkableButtonStyle);

    // Search field styling
    searchField->setStyleSheet(R"(
        QLineEdit#unifiedSearchField {
            background-color: rgba(128, 128, 128, 0.08);
            border: 1px solid rgba(128, 128, 128, 0.12);
            border-radius: 8px;
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

    // Window control glyphs and their hover/pressed backgrounds are drawn here
    // with theme-aware colours so the − / □ / ✕ stay visible in every theme.
    paintWindowControls(p, minimizeButton, QStringLiteral("\u2014"));
    paintWindowControls(p, maximizeButton, QStringLiteral("\u25a1"));
    paintWindowControls(p, closeButton, QStringLiteral("\u2715"));
}

void CustomTitleBar::paintWindowControls(QPainter &p, QPushButton *button, const QString &glyph)
{
    if (!button || glyph.isEmpty())
        return;

    const QRect buttonRect = button->geometry();
    const bool hovered = button->underMouse();
    const bool pressed = button->isDown();
    const bool hoverActive = hovered || pressed;

    QColor glyphColor = windowControlForeground();

    if (button == closeButton && hoverActive) {
        // Close: red hover background + white glyph — readable on any theme.
        QColor red(0xE8, 0x11, 0x23);
        if (pressed)
            red = red.darker(115);
        p.setPen(Qt::NoPen);
        p.setBrush(red);
        p.drawRoundedRect(buttonRect.adjusted(2, 2, -2, -2), 12, 12);
        glyphColor = Qt::white;
    } else if (hoverActive) {
        // Minimize/Maximize: subtle overlay derived from the theme foreground,
        // so it reads on both light and dark themes.
        QColor overlay = glyphColor;
        overlay.setAlphaF(pressed ? 0.25 : 0.14);
        p.setPen(Qt::NoPen);
        p.setBrush(overlay);
        p.drawRoundedRect(buttonRect.adjusted(2, 2, -2, -2), 12, 12);
    }

    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);
    p.setPen(glyphColor);
    p.drawText(buttonRect.adjusted(0, 2, 0, -2), Qt::AlignCenter, glyph);
}

QColor CustomTitleBar::windowControlForeground() const
{
    const QColor background = palette().color(QPalette::Window);
    const QColor themeForeground = palette().color(QPalette::WindowText);

    // Prefer the theme's own text colour, but only when it clearly contrasts
    // with the title bar background — otherwise fall back to black/white so the
    // glyph never disappears regardless of the active theme.
    if (colorContrastRatio(themeForeground, background) >= 3.0)
        return themeForeground;

    return colorLuminance(background) > 0.5 ? QColor(25, 25, 28) : QColor(235, 235, 238);
}

bool CustomTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    // The transparent window control buttons are painted by this bar, so
    // repaint whenever their hover/pressed state may have changed.
    if (watched == minimizeButton || watched == maximizeButton || watched == closeButton) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
            update();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
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
