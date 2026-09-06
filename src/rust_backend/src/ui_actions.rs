//! ## UI Action Handler
//!
//! Every user interaction (button press, search submit, ...) is routed through
//! this module. Qt/C++ only *draws* the widgets; every *decision* — validating
//! the action and its payload, deciding what should happen, auditing the
//! outcome — happens here in safe Rust.
//!
//! `handle()` returns a JSON document:
//!
//! ```json
//! {
//!   "commands": [ { "cmd": "window.minimize" }, ... ],
//!   "error": null
//! }
//! ```
//!
//! Each command is the smallest Qt-only operation needed to reflect the
//! decision (e.g. `window.minimize`). C++ executes commands; Rust decides them.
//! Action and command names are mirrored in `include/scriptura/ui_actions.h` —
//! keep the two sides in sync when adding or renaming an action.

use serde::de::DeserializeOwned;
use serde::Deserialize;
use serde_json::{json, Value};
use std::collections::VecDeque;

// ── Action names ────────────────────────────────────────────────────────
// Mirrored in include/scriptura/ui_actions.h — keep in sync.

pub const TITLEBAR_MINIMIZE: &str = "ui.titlebar.minimize";
pub const TITLEBAR_MAXIMIZE: &str = "ui.titlebar.maximize";
pub const TITLEBAR_CLOSE: &str = "ui.titlebar.close";
pub const TITLEBAR_SIDEBAR_TOGGLE: &str = "ui.titlebar.sidebarToggle";
pub const TITLEBAR_INSPECTOR_TOGGLE: &str = "ui.titlebar.inspectorToggle";
pub const TITLEBAR_SETTINGS: &str = "ui.titlebar.settings";
pub const TITLEBAR_SEARCH: &str = "ui.titlebar.search";

pub const WELCOME_MINIMIZE: &str = "ui.welcome.minimize";
pub const WELCOME_MAXIMIZE: &str = "ui.welcome.maximize";
pub const WELCOME_CLOSE: &str = "ui.welcome.close";
pub const WELCOME_OPEN_PROJECT: &str = "ui.welcome.openProject";
pub const WELCOME_RECENT_PROJECT: &str = "ui.welcome.recentProject";
pub const WELCOME_CLONE_REPO: &str = "ui.welcome.cloneRepo";
pub const WELCOME_NEW_FILE: &str = "ui.welcome.newFile";

/// Published by C++ after the native folder picker returns a path.
pub const PROJECT_CHOSEN: &str = "ui.project.chosen";

// ── Command names (returned to C++ for execution) ──────────────────────

pub const CMD_WINDOW_MINIMIZE: &str = "window.minimize";
pub const CMD_WINDOW_TOGGLE_MAXIMIZED: &str = "window.toggleMaximized";
pub const CMD_WINDOW_CLOSE: &str = "window.close";
pub const CMD_SIDEBAR_TOGGLE: &str = "sidebar.toggle";
pub const CMD_INSPECTOR_TOGGLE: &str = "inspector.toggle";
pub const CMD_SETTINGS_OPEN: &str = "settings.open";
pub const CMD_SEARCH_OPEN: &str = "search.open";
pub const CMD_PROJECT_PROMPT_OPEN: &str = "project.promptOpen";
pub const CMD_PROJECT_OPEN: &str = "project.open";
pub const CMD_GIT_CLONE: &str = "git.clone";
pub const CMD_FILE_NEW: &str = "file.new";

// ── Typed payloads (validated with serde before any decision is made) ──

#[derive(Deserialize)]
struct SearchPayload {
    query: String,
}

#[derive(Deserialize)]
struct PathPayload {
    path: String,
}

#[derive(Deserialize)]
struct UrlPayload {
    url: String,
}

// ── Handler ─────────────────────────────────────────────────────────────

pub struct UiActionHandler {
    /// Ring buffer of the most recent handled actions (audit trail).
    audit_log: VecDeque<String>,
    handled_count: u64,
    rejected_count: u64,
    max_log_entries: usize,
}

impl UiActionHandler {
    pub fn new() -> Self {
        Self {
            audit_log: VecDeque::new(),
            handled_count: 0,
            rejected_count: 0,
            max_log_entries: 100,
        }
    }

    /// Route a user action through Rust: validate the payload, decide what
    /// should happen, audit the outcome, and return the JSON commands for the
    /// C++ side to execute. A rejected action yields an empty command list
    /// plus a non-null `error` — nothing is executed on bad input.
    pub fn handle(&mut self, action: &str, payload: &str) -> String {
        match self.decide(action, payload) {
            Ok(commands) => {
                self.audit(action, true);
                json!({ "commands": commands, "error": null }).to_string()
            }
            Err(err) => {
                self.audit(action, false);
                json!({ "commands": [], "error": err }).to_string()
            }
        }
    }

    fn decide(&self, action: &str, payload: &str) -> Result<Vec<Value>, String> {
        match action {
            TITLEBAR_MINIMIZE | WELCOME_MINIMIZE => Ok(vec![cmd(CMD_WINDOW_MINIMIZE)]),
            TITLEBAR_MAXIMIZE | WELCOME_MAXIMIZE => Ok(vec![cmd(CMD_WINDOW_TOGGLE_MAXIMIZED)]),
            TITLEBAR_CLOSE | WELCOME_CLOSE => Ok(vec![cmd(CMD_WINDOW_CLOSE)]),
            TITLEBAR_SIDEBAR_TOGGLE => Ok(vec![cmd(CMD_SIDEBAR_TOGGLE)]),
            TITLEBAR_INSPECTOR_TOGGLE => Ok(vec![cmd(CMD_INSPECTOR_TOGGLE)]),
            TITLEBAR_SETTINGS => Ok(vec![cmd(CMD_SETTINGS_OPEN)]),
            TITLEBAR_SEARCH => {
                let p: SearchPayload = parse(payload)?;
                if p.query.trim().is_empty() {
                    return Err("search query must not be empty".into());
                }
                Ok(vec![json!({ "cmd": CMD_SEARCH_OPEN, "query": p.query })])
            }
            WELCOME_OPEN_PROJECT => Ok(vec![cmd(CMD_PROJECT_PROMPT_OPEN)]),
            WELCOME_RECENT_PROJECT => {
                let p: PathPayload = parse(payload)?;
                if !is_directory(&p.path) {
                    return Err(format!("not a directory: {}", p.path));
                }
                Ok(vec![json!({ "cmd": CMD_PROJECT_OPEN, "path": p.path })])
            }
            WELCOME_CLONE_REPO => {
                let p: UrlPayload = parse(payload)?;
                if !is_valid_git_url(&p.url) {
                    return Err("invalid git URL".into());
                }
                Ok(vec![json!({ "cmd": CMD_GIT_CLONE, "url": p.url })])
            }
            WELCOME_NEW_FILE => Ok(vec![cmd(CMD_FILE_NEW)]),
            PROJECT_CHOSEN => {
                let p: PathPayload = parse(payload)?;
                if !is_directory(&p.path) {
                    return Err(format!("not a directory: {}", p.path));
                }
                Ok(vec![json!({ "cmd": CMD_PROJECT_OPEN, "path": p.path })])
            }
            _ => Err(format!("unknown action: {}", action)),
        }
    }

    fn audit(&mut self, action: &str, accepted: bool) {
        let result = if accepted { "accepted" } else { "rejected" };
        let entry = format!(
            "[{}] {} result={}",
            chrono::Utc::now().to_rfc3339_opts(chrono::SecondsFormat::Secs, true),
            action,
            result
        );
        self.audit_log.push_back(entry);
        while self.audit_log.len() > self.max_log_entries {
            self.audit_log.pop_front();
        }
        if accepted {
            self.handled_count += 1;
        } else {
            self.rejected_count += 1;
        }
    }

    /// Audit trail of the most recent handled actions (oldest first).
    pub fn audit_log(&self) -> Vec<String> {
        self.audit_log.iter().cloned().collect()
    }

    pub fn handled_count(&self) -> u64 {
        self.handled_count
    }

    pub fn rejected_count(&self) -> u64 {
        self.rejected_count
    }
}

impl Default for UiActionHandler {
    fn default() -> Self {
        Self::new()
    }
}

fn cmd(name: &str) -> Value {
    json!({ "cmd": name })
}

fn parse<T: DeserializeOwned>(payload: &str) -> Result<T, String> {
    serde_json::from_str(payload).map_err(|e| format!("invalid payload: {}", e))
}

fn is_directory(path: &str) -> bool {
    std::fs::metadata(path).map(|m| m.is_dir()).unwrap_or(false)
}

/// Accepts https/http/git/ssh URLs and the SCP-style `git@host:path` form.
fn is_valid_git_url(url: &str) -> bool {
    url.starts_with("http://")
        || url.starts_with("https://")
        || url.starts_with("git://")
        || url.starts_with("ssh://")
        || url.starts_with("git@")
        || (url.contains('@') && url.contains(':'))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn commands_for(result: &str) -> Vec<Value> {
        let v: Value = serde_json::from_str(result).unwrap();
        v["commands"].as_array().unwrap().clone()
    }

    fn error_for(result: &str) -> String {
        let v: Value = serde_json::from_str(result).unwrap();
        v["error"].as_str().unwrap_or("").to_string()
    }

    fn temp_dir(tag: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!(
            "scriptura_ui_action_{}_{}",
            tag,
            std::process::id()
        ));
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn minimize_returns_window_command() {
        let mut h = UiActionHandler::new();
        let res = h.handle(TITLEBAR_MINIMIZE, "{}");
        assert_eq!(commands_for(&res)[0]["cmd"], "window.minimize");
        assert!(error_for(&res).is_empty());
    }

    #[test]
    fn toggles_route_commands() {
        let mut h = UiActionHandler::new();
        assert_eq!(
            commands_for(&h.handle(TITLEBAR_SIDEBAR_TOGGLE, "{}"))[0]["cmd"],
            "sidebar.toggle"
        );
        assert_eq!(
            commands_for(&h.handle(TITLEBAR_INSPECTOR_TOGGLE, "{}"))[0]["cmd"],
            "inspector.toggle"
        );
        assert_eq!(
            commands_for(&h.handle(TITLEBAR_MAXIMIZE, "{}"))[0]["cmd"],
            "window.toggleMaximized"
        );
        assert_eq!(
            commands_for(&h.handle(WELCOME_OPEN_PROJECT, "{}"))[0]["cmd"],
            "project.promptOpen"
        );
    }

    #[test]
    fn search_query_passed_through() {
        let mut h = UiActionHandler::new();
        let res = h.handle(TITLEBAR_SEARCH, r#"{"query": "open project"}"#);
        let cmds = commands_for(&res);
        assert_eq!(cmds[0]["cmd"], "search.open");
        assert_eq!(cmds[0]["query"], "open project");
    }

    #[test]
    fn empty_search_query_rejected() {
        let mut h = UiActionHandler::new();
        let res = h.handle(TITLEBAR_SEARCH, r#"{"query": "   "}"#);
        assert!(commands_for(&res).is_empty());
        assert!(!error_for(&res).is_empty());
    }

    #[test]
    fn malformed_payload_rejected() {
        let mut h = UiActionHandler::new();
        let res = h.handle(TITLEBAR_SEARCH, "not json");
        assert!(commands_for(&res).is_empty());
        assert!(!error_for(&res).is_empty());
    }

    #[test]
    fn recent_project_accepts_existing_dir() {
        let dir = temp_dir("recent");
        let mut h = UiActionHandler::new();
        let path_str = dir.display().to_string();
        let payload = serde_json::json!({"path": path_str}).to_string();
        let res = h.handle(WELCOME_RECENT_PROJECT, &payload);
        let cmds = commands_for(&res);
        assert_eq!(cmds[0]["cmd"], "project.open");
        assert_eq!(cmds[0]["path"], path_str);
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn recent_project_rejects_missing_dir() {
        let mut h = UiActionHandler::new();
        let res = h.handle(WELCOME_RECENT_PROJECT, r#"{"path": "/no/such/dir/xyz"}"#);
        assert!(commands_for(&res).is_empty());
        assert!(!error_for(&res).is_empty());
    }

    #[test]
    fn clone_repo_url_validation() {
        let mut h = UiActionHandler::new();
        let ok = h.handle(WELCOME_CLONE_REPO, r#"{"url": "https://github.com/u/r.git"}"#);
        assert_eq!(commands_for(&ok)[0]["cmd"], "git.clone");
        let scp = h.handle(WELCOME_CLONE_REPO, r#"{"url": "git@github.com:u/r.git"}"#);
        assert_eq!(commands_for(&scp)[0]["cmd"], "git.clone");
        let bad = h.handle(WELCOME_CLONE_REPO, r#"{"url": "not a url"}"#);
        assert!(commands_for(&bad).is_empty());
        assert!(!error_for(&bad).is_empty());
    }

    #[test]
    fn new_file_returns_command() {
        let mut h = UiActionHandler::new();
        let res = h.handle(WELCOME_NEW_FILE, "{}");
        assert_eq!(commands_for(&res)[0]["cmd"], "file.new");
    }

    #[test]
    fn project_chosen_validates_dir() {
        let dir = temp_dir("chosen");
        let mut h = UiActionHandler::new();
        let path_str = dir.display().to_string();
        let payload = serde_json::json!({"path": path_str}).to_string();
        let res = h.handle(PROJECT_CHOSEN, &payload);
        assert_eq!(commands_for(&res)[0]["cmd"], "project.open");
        std::fs::remove_dir_all(&dir).ok();

        let res2 = h.handle(PROJECT_CHOSEN, r#"{"path": "/no/such/dir/xyz"}"#);
        assert!(commands_for(&res2).is_empty());
        assert!(!error_for(&res2).is_empty());
    }

    #[test]
    fn unknown_action_rejected() {
        let mut h = UiActionHandler::new();
        let res = h.handle("ui.mystery.button", "{}");
        assert!(commands_for(&res).is_empty());
        assert!(error_for(&res).contains("unknown action"));
        assert_eq!(h.rejected_count(), 1);
    }

    #[test]
    fn audit_log_records_actions() {
        let mut h = UiActionHandler::new();
        h.handle(TITLEBAR_CLOSE, "{}");
        h.handle(TITLEBAR_SETTINGS, "{}");
        h.handle("ui.bogus", "{}");
        let log = h.audit_log();
        assert_eq!(log.len(), 3);
        assert!(log[0].contains(TITLEBAR_CLOSE));
        assert!(log[0].contains("accepted"));
        assert!(log[2].contains("rejected"));
        assert_eq!(h.handled_count(), 2);
        assert_eq!(h.rejected_count(), 1);
    }
}