#ifndef PLUGINFEATURE_H
#define PLUGINFEATURE_H

#include <Qt>
#include <QMetaType>

/**
 * @file pluginfeature.h
 * @brief Defines plugin feature types for the two-tier plugin system
 *
 * Scriptura now has two distinct extension types:
 *
 * 1. **Plugins** (ScripturaPlugin) — Full plugins that interact with the core UI,
 *    modify menus, toolbars, editors, themes, etc. These implement the full
 *    ScripturaPlugin interface and have access to the complete PluginUIApi.
 *
 * 2. **Applications** (ScripturaApplication) — Self-contained UI panels that get
 *    an icon in the floating dock and a bottom-panel tab. They are simpler,
 *    only need an SVG icon, a name, and a content widget.
 *
 * The 'Application' feature type below is used for the new Application system.
 */

enum class PluginFeature {
    // ── Application Feature ──────────────────────────────────────
    Application,            ///< Self-contained app with dock icon + tab (ScripturaApplication)

    // ── Editor Features ──────────────────────────────────────────
    EditorExtension,        ///< Editor extension
    SyntaxHighlighting,     ///< Syntax highlighting
    CodeCompletion,         ///< Code completion

    // ── UI Control Features ──────────────────────────────────────
    MenuAction,             ///< Menu action
    ToolbarButton,          ///< Toolbar button
    StatusBarWidget,        ///< Status bar widget
    SidePanel,              ///< Side panel
    BottomPanel,            ///< Bottom panel
    SidebarButton,          ///< Sidebar button

    // ── Tool Features ────────────────────────────────────────────
    ToolPanel,              ///< Tool panel

    // ── Project Features ─────────────────────────────────────────
    ProjectWizard,          ///< Project wizard
    BuildSystem,            ///< Build system
    FileExplorer,           ///< File explorer

    // ── Editor Extension Features ────────────────────────────────
    EditorDecoration,       ///< Editor decoration
    EditorAnnotation,       ///< Editor annotation
    InlineCompletion,       ///< Inline completion

    // ── Notification Features ────────────────────────────────────
    Notification,           ///< Notification display
    ProgressIndicator,      ///< Progress indicator

    // ── Analysis Features ────────────────────────────────────────
    LSPProvider,            ///< LSP provider
    DiagnosticsProvider,    ///< Diagnostics provider
    Formatter,              ///< Formatter tool

    // ── Integration Features ─────────────────────────────────────
    VCSIntegration,         ///< Version control
    TerminalEmulator,       ///< Terminal emulator
    ExternalTool,           ///< External tool

    // ── Theme Features ───────────────────────────────────────────
    ThemeProvider           ///< Theme provider (custom color themes)
};

Q_DECLARE_METATYPE(PluginFeature)

#endif // PLUGINFEATURE_H
