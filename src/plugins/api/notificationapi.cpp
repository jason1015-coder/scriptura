#include "notificationapi.h"
#include "mainwindow.h"
#include <QStatusBar>
#include <QHBoxLayout>
#include <QTimer>
#include <QDebug>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>

PluginNotificationApi::PluginNotificationApi(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

PluginNotificationApi::~PluginNotificationApi() = default;

// ── Toast Notifications ─────────────────────────────────────────

void PluginNotificationApi::showToast(const QString &title, const QString &message,
                                       const QString &styleClass, int durationMs)
{
    if (!m_mainWindow) return;

    // Create a toast widget in the main window
    QFrame *toast = new QFrame(m_mainWindow);
    toast->setWindowFlags(Qt::ToolTip);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setObjectName("pluginToast_" + styleClass);

    QVBoxLayout *lay = new QVBoxLayout(toast);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(4);

    if (!title.isEmpty()) {
        QLabel *titleLbl = new QLabel(title, toast);
        titleLbl->setStyleSheet("font-weight: bold; font-size: 13px;");
        lay->addWidget(titleLbl);
    }

    if (!message.isEmpty()) {
        QLabel *msgLbl = new QLabel(message, toast);
        msgLbl->setWordWrap(true);
        msgLbl->setStyleSheet("font-size: 12px;");
        lay->addWidget(msgLbl);
    }

    // Style based on type
    QString bg;
    if (styleClass == "error")       bg = "#d32f2f";
    else if (styleClass == "warning") bg = "#f57c00";
    else                              bg = "#323232";

    toast->setStyleSheet(
        QString("QFrame#pluginToast_%1 { background: %2; color: white; "
                "border-radius: 6px; padding: 4px; }")
            .arg(styleClass, bg)
    );
    toast->adjustSize();

    // Position at bottom-right of main window
    QRect parentRect = m_mainWindow->rect();
    int x = parentRect.width() - toast->width() - 20;
    int y = parentRect.height() - toast->height() - 40;
    toast->move(m_mainWindow->mapToGlobal(QPoint(x, y)));
    toast->show();

    // Auto-dismiss after duration
    if (durationMs > 0) {
        QTimer::singleShot(durationMs, toast, &QFrame::close);
    }
}

void PluginNotificationApi::showInfo(const QString &title, const QString &message, int durationMs)
{
    showToast(title, message, "info", durationMs);
}

void PluginNotificationApi::showWarning(const QString &title, const QString &message, int durationMs)
{
    showToast(title, message, "warning", durationMs);
}

void PluginNotificationApi::showError(const QString &title, const QString &message, int durationMs)
{
    showToast(title, message, "error", durationMs);
}

// ── Status Bar Messages ─────────────────────────────────────────

void PluginNotificationApi::showStatusMessage(const QString &message, int timeoutMs)
{
    if (m_mainWindow && m_mainWindow->statusBar())
        m_mainWindow->statusBar()->showMessage(message, timeoutMs);
}

// ── Progress Indicator ──────────────────────────────────────────

void PluginNotificationApi::showProgress(const QString &taskId, const QString &label,
                                          int minimum, int maximum)
{
    if (!m_mainWindow || !m_mainWindow->statusBar())
        return;

    // If already showing, just update label
    if (m_progressBars.contains(taskId)) {
        ProgressEntry &entry = m_progressBars[taskId];
        if (entry.label)
            entry.label->setText(label);
        if (entry.bar) {
            entry.bar->setRange(minimum, maximum);
            entry.bar->setValue(minimum);
        }
        return;
    }

    // Create a container widget for the progress bar
    QWidget *container = new QWidget(m_mainWindow->statusBar());
    QHBoxLayout *lay = new QHBoxLayout(container);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(4);

    QLabel *lbl = new QLabel(label, container);
    lbl->setStyleSheet("font-size: 11px;");
    lay->addWidget(lbl);

    QProgressBar *bar = new QProgressBar(container);
    bar->setRange(minimum, maximum);
    bar->setValue(minimum);
    bar->setFixedWidth(120);
    bar->setFixedHeight(16);
    bar->setTextVisible(false);
    lay->addWidget(bar);

    m_mainWindow->statusBar()->addPermanentWidget(container);
    container->show();

    ProgressEntry entry;
    entry.container = container;
    entry.label = lbl;
    entry.bar = bar;
    m_progressBars[taskId] = entry;
}

void PluginNotificationApi::updateProgress(const QString &taskId, int value)
{
    if (!m_progressBars.contains(taskId))
        return;

    ProgressEntry &entry = m_progressBars[taskId];
    if (entry.bar)
        entry.bar->setValue(value);
}

void PluginNotificationApi::hideProgress(const QString &taskId)
{
    if (!m_progressBars.contains(taskId))
        return;

    ProgressEntry &entry = m_progressBars[taskId];
    if (entry.container) {
        entry.container->hide();
        entry.container->deleteLater();
    }
    m_progressBars.remove(taskId);
}
