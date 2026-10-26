#include "customtitlebar.h"
#include "themeicons.h"
#include "thememanager.h"
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
    , titleLabel(nullptr)
    , sidebarToggleButton(nullptr)
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
    auto tm = qobject_cast<ThemeManager*>(qApp->property("themeManager").value<QObject*>());
    const int spacingSm = tm ? tm->spacingSm() : 8;
    const int spacingMd = tm ? tm->spacingMd() : 12;
    const int spacingXs = tm ? tm->spacingXs() : 4;
    const int fontSizeSm = tm ? tm->fontSizeSm() : 12;

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(spacingMd, 0, spacingMd, 0);
    layout->setSpacing(spacingXs);
    // Remembered so setMirrored() can rebuild the row without re-deriving them.
    m_layout = layout;
    m_spacingXs = spacingXs;
    m_spacingSm = spacingSm;

    // -- Leading: file tree toggle --
    sidebarToggleButton = new QPushButton(this);
    sidebarToggleButton->setObjectName("TitleBarFileTreeToggle");
    sidebarToggleButton->setFixedSize(36, 36);
    sidebarToggleButton->setToolTip(tr("File Tree"));
    sidebarToggleButton->setCheckable(true);
    sidebarToggleButton->setChecked(false);
    sidebarToggleButton->setIconSize(QSize(18, 18));
    ThemeIcons::instance()->setIcon(sidebarToggleButton, ":/icons/folder.svg",
                                    ThemeIcons::Role::Normal, 18);
    connect(sidebarToggleButton, &QPushButton::clicked, this, &CustomTitleBar::sidebarToggleClicked);
    layout->addWidget(sidebarToggleButton);

    // -- Spacer --
    layout->addSpacing(spacingXs);

    // -- Center: title --
    titleLabel = new QLabel(tr("Scriptura"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(fontSizeSm);
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

    layout->addSpacing(spacingXs);

    // -- Trailing: inspector toggle only — the window already has its own
    // frame, so the − / □ / ✕ controls are not needed here.
    inspectorToggleButton = new QPushButton(this);
    inspectorToggleButton->setObjectName("TitleBarInspectorToggle");
    inspectorToggleButton->setFixedSize(36, 36);
    inspectorToggleButton->setToolTip(tr("Toggle Inspector"));
    inspectorToggleButton->setCheckable(true);
    inspectorToggleButton->setIconSize(QSize(18, 18));
    ThemeIcons::instance()->setIcon(inspectorToggleButton, ":/icons/inspector.svg",
                                    ThemeIcons::Role::Normal, 18);
    connect(inspectorToggleButton, &QPushButton::clicked, this, &CustomTitleBar::inspectorToggleClicked);
    layout->addWidget(inspectorToggleButton);
}

void CustomTitleBar::setMirrored(bool mirrored)
{
    if (m_mirrored == mirrored)
        return;
    m_mirrored = mirrored;

    QHBoxLayout *layout = m_layout;
    if (!layout)
        return;

    // Drop every item (widgets survive — only their layout membership goes),
    // then re-add in the order this mode calls for.
    while (layout->count() > 0)
        delete layout->takeAt(0);

    if (!mirrored) {
        // Leading: file tree toggle, title, stretch, then the trailing cluster.
        layout->addWidget(sidebarToggleButton);
        layout->addSpacing(m_spacingXs);
        layout->addWidget(titleLabel, 0, Qt::AlignVCenter);
        layout->addStretch(1);
        layout->addWidget(searchField, 0, Qt::AlignVCenter);
        layout->addSpacing(m_spacingXs);
        layout->addWidget(inspectorToggleButton);
    } else {
        // Exact reverse of the order above, so each control sits opposite the
        // pane it toggles and close ends up on the leading edge.
        layout->addWidget(inspectorToggleButton);
        layout->addWidget(searchField, 0, Qt::AlignVCenter);
        layout->addStretch(1);
        layout->addWidget(titleLabel, 0, Qt::AlignVCenter);
        layout->addSpacing(m_spacingXs);
        layout->addWidget(sidebarToggleButton);
    }

    // Text inside the search field has to flip too, otherwise it reads left
    // aligned in a right-anchored field.
    if (searchField) {
        searchField->setLayoutDirection(mirrored ? Qt::RightToLeft
                                                 : Qt::LeftToRight);
    }

    updateGeometry();
}

void CustomTitleBar::styleButtons()
{
    auto tm = qobject_cast<ThemeManager*>(qApp->property("themeManager").value<QObject*>());
    const int radius = tm ? tm->radiusMd() : 8;
    const int spacingSm = tm ? tm->spacingSm() : 8;
    const int spacingXs = tm ? tm->spacingXs() : 4;
    const int spacingMd = tm ? tm->spacingMd() : 12;

    const QString checkableButtonStyle = QString(R"(
        QPushButton {
            border: none;
            background-color: transparent;
            color: palette(text);
            border-radius: %1px;
            padding: 0px;
        }
        QPushButton:hover {
            background-color: rgba(128, 128, 128, 0.10);
        }
        QPushButton:checked {
            background-color: rgba(128, 128, 128, 0.15);
        }
        QPushButton:pressed {
            background-color: rgba(128, 128, 128, 0.18);
        }
    )").arg(radius);

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


    
    // File tree toggle button - non-checkable, simple button style
    const QString simpleButtonStyle = QString(R"(
        QPushButton {
            border: none;
            background-color: transparent;
            color: palette(text);
            border-radius: %1px;
            padding: 0px;
        }
        QPushButton:hover {
            background-color: rgba(128, 128, 128, 0.10);
        }
        QPushButton:pressed {
            background-color: rgba(128, 128, 128, 0.18);
        }
    )").arg(radius);
    sidebarToggleButton->setStyleSheet(checkableButtonStyle);
    inspectorToggleButton->setStyleSheet(checkableButtonStyle);

    // Search field styling - use theme-aware colors
    const QString searchStyle = QString(R"(
        QLineEdit#unifiedSearchField {
            background-color: rgba(128, 128, 128, 0.06);
            border: 1px solid rgba(128, 128, 128, 0.10);
            border-radius: %1px;
            padding: %2px %3px;
            color: palette(text);
            font-size: %4px;
            font-family: "Inter", "SF Pro Text", sans-serif;
        }
        QLineEdit#unifiedSearchField:focus {
            background-color: rgba(128, 128, 128, 0.12);
            border: 1px solid rgba(128, 128, 128, 0.22);
        }
        QLineEdit#unifiedSearchField:hover {
            background-color: rgba(128, 128, 128, 0.08);
        }
    )").arg(radius).arg(spacingXs).arg(spacingMd).arg(tm ? tm->fontSizeSm() : 12);

    searchField->setStyleSheet(searchStyle);
}

void CustomTitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    paintTitleBarBackground(p);


}

void CustomTitleBar::paintTitleBarBackground(QPainter &p)
{
    // The editor container this bar lives in is rendered with rounded corners
    // (border-radius: 14px). Paint the bar's own background with the same top
    // corners so its square edges don't poke out over the rounded frame, and
    // no mid-coloured sliver from the window's margin ring shows through the
    // previously transparent corner pixels.
    constexpr int kContainerCornerRadius = 14;

    const QColor bg = palette().color(QPalette::Window);

    const qreal w = rect().width();
    const qreal h = rect().height();

    QPainterPath path;
    path.moveTo(0, h);
    path.lineTo(0, kContainerCornerRadius);
    path.quadTo(QPointF(0, 0), QPointF(kContainerCornerRadius, 0));
    path.lineTo(w - kContainerCornerRadius, 0);
    path.quadTo(QPointF(w, 0), QPointF(w, kContainerCornerRadius));
    path.lineTo(w, h);
    path.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawPath(path);
}


bool CustomTitleBar::eventFilter(QObject *watched, QEvent *event)
{

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
