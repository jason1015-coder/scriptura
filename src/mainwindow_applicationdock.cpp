#include "mainwindow.h"
#include "applicationdock.h"
#include "applicationmanager.h"
#include "applicationinterface.h"
#include "applicationcontext.h"
#include "codeeditor.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QResizeEvent>
#include <QDebug>

/**
 * @file mainwindow_applicationdock.cpp
 * @brief Application Dock integration for MainWindow
 *
 * This file implements:
 * - setupApplicationDock(): Creates the floating dock and registers built-in apps
 * - showApplicationTab(): Shows/hides application content in the bottom panel
 * - repositionDock(): Repositions the dock at bottom center on resize
 * - resizeEvent(): Overrides MainWindow resize to reposition the dock
 */

void MainWindow::setupApplicationDock()
{
    // ── Create Application Manager ──────────────────────────────
    m_appManager = new ApplicationManager(this);

    // Register built-in applications (Git, HTTP, Database, etc.)
    m_appManager->registerBuiltins();

    // ── Create Application Dock (floating bottom bar) ───────────
    m_appDock = new ApplicationDock(this);
    m_appDock->setObjectName("applicationDock");

    // Position the dock at the bottom center of the main window
    QTimer::singleShot(0, this, [this]() {
        if (!m_appDock) return;

        // Float the dock at the bottom center
        m_appDock->setParent(this);
        m_appDock->raise();
        m_appDock->show();
        repositionDock();
    });

    // ── Populate dock with built-in application icons ───────────
    auto apps = m_appManager->allApplications();
    for (const auto &app : apps) {
        if (!app.iconPath.isEmpty()) {
            m_appDock->addEntry(app.id, app.iconPath, app.tooltip);
        }
    }

    // ── Connect dock clicks to show/hide application tabs ───────
    connect(m_appDock, &ApplicationDock::appClicked, this, [this](const QString &appId) {
        showApplicationTab(appId);
    });

    // ── Connect application loaded/unloaded signals ─────────────
    connect(m_appManager, &ApplicationManager::applicationLoaded,
            this, [this](const QString &id, const QString &name, const QString &iconPath) {
        if (m_appDock && !m_appDock->hasEntry(id)) {
            auto info = m_appManager->appInfo(id);
            m_appDock->addEntry(id, iconPath, info.tooltip);
        }
    });

    connect(m_appManager, &ApplicationManager::applicationUnloaded,
            this, [this](const QString &id) {
        if (m_appDock) {
            m_appDock->removeEntry(id);
        }
    });

    qDebug() << "ApplicationDock: initialized with" << m_appDock->entryCount() << "entries";
}

void MainWindow::repositionDock()
{
    if (!m_appDock) return;
    int dockW = m_appDock->width();
    int dockH = m_appDock->height();
    int x = (width() - dockW) / 2;
    int y = height() - dockH - 12; // 12px margin from bottom
    m_appDock->move(x, y);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionDock();
}

void MainWindow::showApplicationTab(const QString &appId)
{
    if (!m_appManager) return;

    // Get or create the application's widget
    QWidget *appWidget = m_appManager->widget(appId);
    if (!appWidget) {
        qDebug() << "ApplicationDock: no widget for" << appId;
        return;
    }

    // Find the tab index for this application in the bottom panel
    QString tabTitle = m_appManager->tabTitle(appId);
    int tabIndex = -1;
    for (int i = 0; i < bottomPanelTabs->count(); ++i) {
        if (bottomPanelTabs->tabText(i) == tabTitle) {
            tabIndex = i;
            break;
        }
    }

    // If tab doesn't exist yet, create it
    if (tabIndex < 0) {
        tabIndex = bottomPanelTabs->addTab(tabTitle);
        bottomPanelStack->addWidget(appWidget);
    }

    // Check if this tab is already active
    bool isCurrentlyActive = (bottomPanelTabs->currentIndex() == tabIndex &&
                               ui->bottomPanelContainer->isVisible());

    if (isCurrentlyActive) {
        // Toggle off — hide the panel
        ui->bottomPanelContainer->hide();
        bottomPanelStack->widget(tabIndex)->hide();
        m_appDock->setActiveApp(QString());
    } else {
        // Toggle on — show the panel with this app's tab
        ui->bottomPanelContainer->show();
        bottomPanelTabs->setCurrentIndex(tabIndex);
        bottomPanelStack->setCurrentIndex(tabIndex);
        bottomPanelStack->widget(tabIndex)->show();
        m_appDock->setActiveApp(appId);

        // Animate panel appearance
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, 200);
        }
    }
}
