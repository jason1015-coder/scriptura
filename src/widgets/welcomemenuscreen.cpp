#include "welcomemenuscreen.h"
#include "rust_adapter.h"
#include "scriptura_actions.h"

#include <QIcon>
#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QPalette>
#include <QFont>
#include <QScrollArea>
#include <QStandardPaths>
#include <QPainter>
#include <QPixmap>
#include <QImageReader>
#include <QDebug>

// ── Helper: render & tint a monochrome SVG icon ───────────────────────
// Uses QImageReader (handles SVG via Qt's built-in plugin) so we don't
// need to link against the QtSvg module directly.
static QPixmap renderSvgPixmap(const QString &path, int size)
{
    QImageReader reader(path);
    if (reader.canRead()) {
        reader.setScaledSize(QSize(size, size));
        QImage img = reader.read();
        if (!img.isNull())
            return QPixmap::fromImage(img);
    }
    QPixmap fallback(path);
    if (!fallback.isNull())
        return fallback.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QPixmap();
}

static QIcon tintedIcon(const QString &svgPath, const QColor &color, int size = 20)
{
    QPixmap shape = renderSvgPixmap(svgPath, size);
    if (shape.isNull())
        return QIcon(svgPath);
    QPixmap out(size, size);
    out.fill(color);
    {
        QPainter p(&out);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(0, 0, shape);
    }
    QIcon icon;
    icon.addPixmap(out, QIcon::Normal);
    return icon;
}

// ── Static helpers ────────────────────────────────────────────────────

QString WelcomeMenuScreen::storagePath()
{
    // OS-specific application data directory:
    //   Linux:   ~/.local/share/Scriptura
    //   macOS:   ~/Library/Application Support/Scriptura
    //   Windows: %APPDATA%/Scriptura
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool WelcomeMenuScreen::isFirstLaunch()
{
    // If the storage directory does not exist yet, this is the first launch
    return !QDir(storagePath()).exists();
}

void WelcomeMenuScreen::markLaunched()
{
    // Create the application data directory so future launches detect it
    QDir().mkpath(storagePath());
}

// ── Constructor ───────────────────────────────────────────────────────

WelcomeMenuScreen::WelcomeMenuScreen(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Start with a fixed size — user can maximize but not resize freely
    resize(620, 560);
    setMinimumSize(480, 420);

    setupUI();
    loadRecentProjects();
}

// ── Window dragging ───────────────────────────────────────────────────

void WelcomeMenuScreen::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Allow dragging on the window itself and passive children (labels, frames)
        // but NOT on interactive widgets (buttons, list widgets, etc.)
        QWidget *child = childAt(event->position().toPoint());
        bool onInteractive = qobject_cast<QPushButton*>(child)
                          || qobject_cast<QListWidget*>(child)
                          || qobject_cast<QLineEdit*>(child);
        if (!onInteractive) {
            m_dragging = true;
            m_dragStartPos = event->globalPosition().toPoint();
        }
    }
    QWidget::mousePressEvent(event);
}

void WelcomeMenuScreen::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        move(pos() + delta);
        m_dragStartPos = event->globalPosition().toPoint();
    }
}

void WelcomeMenuScreen::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void WelcomeMenuScreen::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool isMax = windowState().testFlag(Qt::WindowMaximized);
        m_maximized = isMax;
        if (m_maxBtn) {
            m_maxBtn->setText(isMax ? "\u25a3" : "\u25a1");
            m_maxBtn->setToolTip(isMax ? tr("Restore") : tr("Maximize"));
        }
    }
    QWidget::changeEvent(event);
}

void WelcomeMenuScreen::setupUI()
{
    // ── Main vertical layout ──────────────────────────────────────────
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 28, 32, 28);
    mainLayout->setSpacing(0);

    // ── Top section: big logo + title + window controls ───────────────
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    // Large logo
    QLabel *logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/icons/app-icon.svg");
    if (!logoPixmap.isNull()) {
        logoLabel->setPixmap(logoPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // Fallback text logo
        logoLabel->setText("SC");
        QFont logoFont = logoLabel->font();
        logoFont.setPointSize(28);
        logoFont.setBold(true);
        logoLabel->setFont(logoFont);
    }
    logoLabel->setFixedSize(64, 64);
    logoLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(logoLabel);
    headerLayout->addStretch();

    // ── Window control buttons (minimize, maximize, close) ─────────────
    QHBoxLayout *winCtrlLayout = new QHBoxLayout();
    winCtrlLayout->setSpacing(6);
    winCtrlLayout->setContentsMargins(0, 0, 0, 0);

    auto makeWinBtn = [this](const QString &glyph, const QString &tip) -> QPushButton* {
        QPushButton *btn = new QPushButton(glyph, this);
        btn->setFixedSize(32, 32);
        btn->setToolTip(tip);
        btn->setCursor(Qt::ArrowCursor);
        btn->setObjectName("windowControlButton");
        btn->setStyleSheet(
            "QPushButton#windowControlButton {"
            "  border: none;"
            "  border-radius: 8px;"
            "  background: transparent;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "}"
            // Neutral hover overlay — visible on both light and dark themes.
            "QPushButton#windowControlButton:hover {"
            "  background-color: rgba(128, 128, 128, 0.18);"
            "}"
        );
        return btn;
    };

    m_minBtn = makeWinBtn("\u2014", tr("Minimize"));
    m_maxBtn = makeWinBtn("\u25a1", tr("Maximize"));
    m_closeBtn = makeWinBtn("\u2715", tr("Close"));

    // Close button hover turns solid red — the white glyph stays readable on
    // both light and dark themes (a translucent red washes out on light ones).
    m_closeBtn->setStyleSheet(
        "QPushButton#windowControlButton {"
        "  border: none;"
        "  border-radius: 8px;"
        "  background: transparent;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#windowControlButton:hover {"
        "  background-color: #E81123;"
        "  color: white;"
        "}"
        "QPushButton#windowControlButton:pressed {"
        "  background-color: #B00E1E;"
        "  color: white;"
        "}"
    );

    // Window controls route through the Rust UiActionHandler — Qt only draws
    // the buttons and executes the commands Rust decides (see main.cpp). The
    // maximize glyph/tooltip stay in sync via changeEvent() below.
    connect(m_minBtn, &QPushButton::clicked, this, [this]() {
        RustBackend::instance()->uiActions()->handle(UiActions::WelcomeMinimize);
    });
    connect(m_maxBtn, &QPushButton::clicked, this, [this]() {
        RustBackend::instance()->uiActions()->handle(UiActions::WelcomeMaximize);
    });
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        RustBackend::instance()->uiActions()->handle(UiActions::WelcomeClose);
    });

    winCtrlLayout->addWidget(m_minBtn);
    winCtrlLayout->addWidget(m_maxBtn);
    winCtrlLayout->addWidget(m_closeBtn);

    headerLayout->addLayout(winCtrlLayout);

    mainLayout->addLayout(headerLayout);

    // Separator line
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName("menuSeparator");
    separator->setObjectName("menuSeparator");
    separator->setStyleSheet("max-height: 1px;");
    mainLayout->addSpacing(16);
    mainLayout->addWidget(separator);
    mainLayout->addSpacing(20);

    // ── Content area: Left (actions) + Right (recent projects) ────────
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(24);

    // ─── Left panel: Action buttons ─────────────────────────────────
    QVBoxLayout *leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(10);

    // ── Action buttons ──
    auto makeActionBtn = [this](const QIcon &icon, const QString &text, const QString &objName) -> QPushButton* {
        QPushButton *btn = new QPushButton(icon, text, this);
        btn->setObjectName(objName);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(34);
        btn->setMinimumWidth(200);
        QFont f = btn->font();
        f.setPointSize(11);
        btn->setFont(f);
        btn->setStyleSheet(
            QStringLiteral(
                "QPushButton#%1 {"
                "  background-color: transparent;"
                "  border-radius: 8px;"
                "  padding: 6px 16px;"
                "  text-align: left;"
                "}"
                "QPushButton#%1:hover {"
                "  background-color: palette(light);"
                "}"
            ).arg(objName)
        );
        return btn;
    };

    QPushButton *openProjectBtn = makeActionBtn(
        QIcon(":/icons/folder.svg"), tr("  Open New Project"), QStringLiteral("openProjectBtn"));
    openProjectBtn->setStyleSheet(
        "QPushButton#openProjectBtn {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 6px 16px;"
        "  text-align: left;"
        "}"
        "QPushButton#openProjectBtn:hover {"
        "  background-color: palette(highlight);"
        "  opacity: 0.9;"
        "}"
    );
    leftPanel->addWidget(openProjectBtn);

    m_cloneBtn = makeActionBtn(
        QIcon(":/icons/git.svg"), tr("  Clone Repository"), QStringLiteral("cloneBtn"));
    leftPanel->addWidget(m_cloneBtn);

    m_newFileBtn = makeActionBtn(
        QIcon(":/icons/file.svg"), tr("  New File"), QStringLiteral("newFileBtn"));
    leftPanel->addWidget(m_newFileBtn);

    leftPanel->addStretch();

    // ─── Right panel: Recent projects list (directly on background, no title) ─
    QVBoxLayout *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(6);

    m_recentProjectsList = new QListWidget(this);
    m_recentProjectsList->setObjectName("recentProjectsList");
    m_recentProjectsList->setFrameShape(QFrame::NoFrame);
    m_recentProjectsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentProjectsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_recentProjectsList->setMinimumWidth(200);
    m_recentProjectsList->setMaximumWidth(240);
    m_recentProjectsList->setStyleSheet(
        "QListWidget#recentProjectsList {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget#recentProjectsList::item {"
        "  padding: 8px 10px;"
        "  border-radius: 8px;"
        "  margin: 2px 0px;"
        "}"
        "QListWidget#recentProjectsList::item:hover {"
        "  background-color: palette(light);"
        "}"
        "QListWidget#recentProjectsList::item:selected {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
    );
    m_recentProjectsList->setSpacing(2);

    rightPanel->addWidget(m_recentProjectsList);

    contentLayout->addLayout(leftPanel, 1);
    contentLayout->addLayout(rightPanel);

    mainLayout->addLayout(contentLayout);

    // ── Connect signals ─────────────────────────────────────────────
    connect(openProjectBtn, &QPushButton::clicked, this, [this]() {
        emit openProjectRequested();
    });

    connect(m_cloneBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString gitUrl = QInputDialog::getText(
            this,
            tr("Clone Repository"),
            tr("Enter Git repository URL:"),
            QLineEdit::Normal,
            QString(),
            &ok);
        if (ok && !gitUrl.isEmpty()) {
            emit cloneRequested(gitUrl);
        }
    });

    connect(m_newFileBtn, &QPushButton::clicked, this, [this]() {
        emit newFileRequested();
    });

    connect(m_recentProjectsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            QString path = item->data(Qt::UserRole).toString();
            QDir dir(path);
            if (dir.exists()) {
                emit recentProjectSelected(path);
            }
        }
    });
}

void WelcomeMenuScreen::setThemeBackground(const QColor &color)
{
    // Determine if the background is dark by luminance
    double lum = 0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue();
    bool isDark = lum < 160.0;

    QColor textColor       = isDark ? QColor(220, 220, 220) : QColor(30, 30, 32);
    QColor midColor        = isDark ? QColor(160, 160, 165) : QColor(100, 100, 105);
    QColor buttonTextColor = isDark ? QColor(220, 220, 220) : QColor(30, 30, 32);

    setStyleSheet(QString(R"(
        WelcomeMenuScreen {
            background-color: %1;
            border: none;
            border-radius: 14px;
        }
        QLabel {
            color: %2;
        }

        /* Window control buttons (minimize, maximize, close) */
        QPushButton#windowControlButton {
            color: %2;
        }

        /* Secondary buttons (Clone Repository, New File) */
        QPushButton#cloneBtn, QPushButton#newFileBtn {
            color: %3;
            border: 1px solid %4;
            background-color: transparent;
        }
        QPushButton#cloneBtn:hover, QPushButton#newFileBtn:hover {
            background-color: rgba(255, 255, 255, 0.10);
            border-color: %5;
        }

        /* Separator line */
        QFrame#menuSeparator {
            color: %4;
        }

        /* Recent projects list items */
        QListWidget#recentProjectsList::item {
            color: %2;
        }
        QListWidget#recentProjectsList::item:hover {
            background-color: rgba(128, 128, 128, 0.15);
        }
    )").arg(color.name())
        .arg(textColor.name())
        .arg(buttonTextColor.name())
        .arg(midColor.name())
        .arg(isDark ? QColor(180, 180, 255).name() : QColor(0, 100, 200).name()));

    // Tint SVG icons to match the theme
    if (m_cloneBtn)
        m_cloneBtn->setIcon(tintedIcon(":/icons/git.svg", buttonTextColor));
    if (m_newFileBtn)
        m_newFileBtn->setIcon(tintedIcon(":/icons/file.svg", buttonTextColor));
}

void WelcomeMenuScreen::loadRecentProjects()
{
    QSettings settings;
    m_recentProjects = settings.value("recentProjects").toStringList();
    updateRecentProjectsList();
}

void WelcomeMenuScreen::updateRecentProjectsList()
{
    m_recentProjectsList->clear();

    if (m_recentProjects.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem(tr("  No recent projects"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        m_recentProjectsList->addItem(emptyItem);
        return;
    }

    int count = 0;
    for (const QString &project : m_recentProjects) {
        if (count >= MAX_RECENT_PROJECTS)
            break;

        QDir dir(project);
        if (!dir.exists())
            continue;

        QFileInfo info(project);
        QString displayName = info.fileName();
        QString tooltip = project;

        QListWidgetItem *item = new QListWidgetItem(displayName);
        item->setToolTip(tooltip);
        item->setData(Qt::UserRole, project);
        m_recentProjectsList->addItem(item);
        ++count;
    }
}
