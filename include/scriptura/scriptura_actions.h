#ifndef UI_ACTIONS_H
#define UI_ACTIONS_H

// ── Scriptura UI Action Catalog ─────────────────────────────────────────
// Mirrors src/rust_backend/src/ui_actions.rs — keep the two sides in sync
// when adding or renaming an action or command.
//
// Routing model: every user interaction is routed through the Rust
// UiActionHandler (RustBackend::instance()->uiActions()). Rust validates the
// action + payload and decides; C++/Qt only draws the widgets and executes
// the returned commands.

namespace UiActions {

// Title bar
inline constexpr const char* TitlebarMinimize      = "ui.titlebar.minimize";
inline constexpr const char* TitlebarMaximize      = "ui.titlebar.maximize";
inline constexpr const char* TitlebarClose         = "ui.titlebar.close";
inline constexpr const char* TitlebarSidebarToggle = "ui.titlebar.sidebarToggle";
inline constexpr const char* TitlebarInspectorToggle = "ui.titlebar.inspectorToggle";
inline constexpr const char* TitlebarSettings      = "ui.titlebar.settings";
inline constexpr const char* TitlebarSearch        = "ui.titlebar.search";

// Welcome screen
inline constexpr const char* WelcomeMinimize       = "ui.welcome.minimize";
inline constexpr const char* WelcomeMaximize       = "ui.welcome.maximize";
inline constexpr const char* WelcomeClose          = "ui.welcome.close";
inline constexpr const char* WelcomeOpenProject    = "ui.welcome.openProject";
inline constexpr const char* WelcomeRecentProject  = "ui.welcome.recentProject";
inline constexpr const char* WelcomeCloneRepo      = "ui.welcome.cloneRepo";
inline constexpr const char* WelcomeNewFile        = "ui.welcome.newFile";

// Published by C++ after the native folder picker returns a path.
inline constexpr const char* ProjectChosen         = "ui.project.chosen";

} // namespace UiActions

// Commands Rust returns for C++ to execute (the "drawer" operations).
namespace UiCommands {

inline constexpr const char* WindowMinimize        = "window.minimize";
inline constexpr const char* WindowToggleMaximized = "window.toggleMaximized";
inline constexpr const char* WindowClose           = "window.close";
inline constexpr const char* SidebarToggle         = "sidebar.toggle";
inline constexpr const char* InspectorToggle       = "inspector.toggle";
inline constexpr const char* SettingsOpen          = "settings.open";
inline constexpr const char* SearchOpen            = "search.open";
inline constexpr const char* ProjectPromptOpen     = "project.promptOpen";
inline constexpr const char* ProjectOpen           = "project.open";
inline constexpr const char* GitClone              = "git.clone";
inline constexpr const char* FileNew               = "file.new";

} // namespace UiCommands

#endif // UI_ACTIONS_H