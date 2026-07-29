// mainwindow_ui.cpp - Setup helper methods extracted from the constructor
// These methods are called from the MainWindow constructor to keep it clean

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "themeicons.h"
#include "customtitlebar.h"
#include "windowanimator.h"
#include "thememanager.h"
#include "universalsearch.h"
#include "findreplace.h"
#include "projectsearch.h"
#include "commandpalette.h"
#include "minimap.h"
#include "breadcrumb.h"
#include "codeactionui.h"
#include "aiinlinecompletion.h"
#include "plugincontext.h"
#include "plugins/api/uiapi.h"
#include "plugins/api/editorapi.h"
#include "plugins/api/notificationapi.h"
#include "rust_adapter.h"
#include "foldmanager.h"
#include "bookmarkmanager.h"
#include "snippetmanager.h"
#include "sessionmanager.h"
#include "refactoringmanager.h"
#include "codelensmanager.h"
#include "gitblame.h"
#include "statusbarwidget.h"
#include "encodingmanager.h"
#include "notificationcenter.h"
#include "gitrebase.h"
#include "taskrunnerui.h"
#include "bookmarkpanel.h"
#include "zenmode.h"
#include "debugconfiguration.h"

#include <QShortcut>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>

// Placeholder - setup methods will be extracted from constructor

