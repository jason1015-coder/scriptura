# Scriptura Feature Roadmap

> Comprehensive catalog of features needed to make Scriptura a competitive modern IDE.
> Organized by priority: 🔴 Critical → 🟡 High-Value → 🟢 Medium → 🔵 Nice-to-Have

---

## 🔴 P0 — Critical Features (Core IDE Expectations)

### 1. Persistent Sessions

**Impact:** 🔴 Critical | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Save and restore the entire editor state across restarts:
- Open files and their tab order
- Cursor positions per file (line, column)
- Split view layouts and sizes
- Unsaved / modified buffer content (hot exit)
- Bottom panel visibility and selected tab
- Sidebar collapsed/expanded state
- Recently opened projects list

**Implementation:**
- `SessionManager` class with `saveSession()`, `restoreSession()` methods
- Rust backend `session_engine.rs` with 6 FFI functions (save/load/clear/hot exit)
- Hot exit preserves unsaved buffer content in filesystem storage
- Connected on startup before `showEditorInterface()`
- Welcome screen with "Restore Previous Session" option

**Dependencies:** None

---

### 2. Rename Symbol Across Files (Refactoring)

**Impact:** 🔴 Critical | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Rename a symbol (variable, function, class, method) across all files in the project using LSP `textDocument/rename`:
- Right-click context menu → "Rename Symbol"
- Keyboard shortcut: `F2` (rename under cursor)
- Preview all changes before applying
- Handle symbol name conflicts across files
- Support undo of batch rename

**Implementation:**
- `RefactoringManager::renameSymbol()` using LSP `textDocument/rename`
- Returns `WorkspaceEdit` with `changes` map (URI → TextEdit[])
- Shows preview dialog listing all files and changes
- Applies edits across all files
- Wired to `F2` shortcut and right-click context menu

**Dependencies:** LSP client (already exists)

---

### 3. Extract Method / Extract Variable (Refactoring)

**Impact:** 🔴 Critical | **Effort:** ⬛⬛⬛⬛ High | **Status: ✅ Implemented**

Select a block of code and extract it into a new function or variable:
- **Extract Method:** Select statements → create a new function with those statements
- **Extract Variable:** Select an expression → assign to a new variable and replace
- **Inline:** Replace a function call with the function body
- **Change Signature:** Rename parameters, add/remove parameters, reorder

**Implementation:**
- `RefactoringManager::extractMethod()` and `extractVariable()`
- Shortcuts: `Ctrl+Alt+M` (Extract Method), `Ctrl+Alt+V` (Extract Variable)
- Uses LSP `textDocument/codeAction` with `kind` = `refactor.extract`

**Dependencies:** LSP code actions (already exists)

---

### 4. Code Lens

**Impact:** 🔴 Critical | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Show inline annotations above functions/classes displaying contextual information:
- **Reference count:** "3 references" above function definitions
- **Test count:** "2 tests" above test functions
- **Git blame:** "Last modified by John, 2 days ago" (when integrated)
- **Debug status:** "All tests passing" / "2 failing"

**Implementation:**
- `CodeLensManager` with `requestCodeLens()`, `parseCodeLens()`, `itemsForDocument()`
- Requests code lens items from LSP on document open/change
- Renders as overlay text above lines in `paintEvent`
- Click handling via `mousePressEvent` hit-testing
- Connected in `mainwindow_lsp.cpp` on document change

**Dependencies:** LSP client (already exists)

---

### 5. Emmet / Abbreviation Expansion

**Impact:** 🔴 Critical | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Expand shorthand abbreviations into full HTML/CSS markup:
- `div.container>ul>li*3` → full HTML structure
- `ul>li.item*5` → 5 list items with class
- `h1+p+bq` → heading + paragraph + blockquote
- `#id.class` → `<div id="id" class="class"></div>`
- CSS: `m10` → `margin: 10px;`

**Implementation:**
- `EmmetParser` class in C++ (abbreviation parsing)
- Rust backend `emmet_engine.rs` with `rust_emmet_expand()`, `rust_emmet_is_css_shorthand()`
- Tab-triggered expansion in `codeeditor.cpp` when cursor follows an abbreviation
- CSS shorthand property expansion (m → margin, p → padding, fs → font-size, etc.)
- Active in HTML, CSS, and template files via language detection

**Dependencies:** None (standalone parser)

---

## 🟡 P1 — High-Value Features (Productivity Multipliers)

### 6. Git Blame Inline

**Impact:** 🟡 High | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Show commit author, date, and message for each line in the gutter or as hover:
- Display "Author, Date" in gray text after line numbers
- Show full commit hash, message, and stats on hover
- Click to see full commit in Git panel
- Toggle on/off via settings or shortcut

**Implementation:**
- `GitBlame` class with `blameForLine()`
- Rust backend `blame_engine.rs` with `rust_blame_parse()`
- Rendered in `lineNumberAreaPaintEvent` after line numbers
- Toggle via `Ctrl+Shift+B` shortcut

**Dependencies:** Git binary

---

### 7. Stash Management

**Impact:** 🟡 High | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Complete Git stash workflow from the Git panel:
- View list of all stash entries with messages and dates
- Apply stash (keep in stash list)
- Pop stash (apply and remove from stash list)
- Drop stash (delete without applying)
- Create stash with custom message
- View stash diff before applying

**Implementation:**
- `GitStashWidget` with full stash workflow
- Connected as bottom panel tab in `mainwindow.cpp`

**Dependencies:** Git binary, GitPanel

---

### 8. Interactive Rebase UI

**Impact:** 🟡 High | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Visual interface for rewriting Git history:
- Show commit list for the branch being rebased
- Actions per commit: Pick, Squash, Edit, Drop
- Continue/abort rebase
- Automatic detection of rebase in progress

**Implementation:**
- `GitRebaseWidget` with continue/abort buttons
- Detects interactive rebase in progress via `git status`
- Handles conflicts during rebase gracefully

**Dependencies:** Git binary, GitPanel

---

### 9. Keyboard Shortcut Customization

**Impact:** 🟡 High | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Settings UI to view, search, and remap all keyboard shortcuts:
- Visual shortcut editor with key capture
- Search/filter shortcuts by name or key combination
- Show conflicting shortcuts
- Reset individual or all shortcuts to defaults
- Import/export shortcut profiles

**Implementation:**
- `ShortcutEditorWidget` with `QKeySequenceEdit` key capture
- `ShortcutManager` singleton centralizing shortcut definitions
- Custom shortcuts stored in `QSettings`
- Warning when a shortcut conflicts with an existing one

**Dependencies:** None

---

### 10. Integrated Test Runner

**Impact:** 🟡 High | **Effort:** ⬛⬛⬛⬛ High | **Status: ✅ Implemented**

Run unit tests and display results inline:
- Detect test framework from project files (pytest, jest, cargo test, go test)
- Run all tests or individual test suites
- Show pass/fail/pending indicators next to test functions
- Display test output in a dedicated Test panel
- Jump to failed test on click
- Re-run failed tests only

**Implementation:**
- `TestRunner` class with framework detection
- `TestPanel` widget showing test tree with pass/fail icons
- Rust backend `test_engine.rs` with framework detection, command building, output parsing
- `Ctrl+Shift+T` shortcut to open test panel

**Dependencies:** TaskRunner (Rust backend)

---

### 11. Task Runner UI

**Impact:** 🟡 High | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Visual interface for detecting and running project build tasks:
- Auto-detect tasks from project files (`package.json`, `Makefile`, `Cargo.toml`)
- Show task selector UI
- Run tasks in the terminal panel
- Show task output in real-time

**Implementation:**
- `TaskRunnerUI` widget: a searchable list of detected tasks
- Executes selected task via `TerminalPanel::runCommand()`
- `Ctrl+Shift+B` shortcut to open task selector

**Dependencies:** TerminalPanel, TaskRunner (Rust backend)

---

### 12. Status Bar Enhancements

**Impact:** 🟡 High | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Clickable status bar indicators for quick info and settings:
- **Language:** Show current language, click to switch
- **Encoding:** Show file encoding, click to change
- **Line Endings:** Show CRLF/LF, click to switch
- **Indentation:** Show spaces/tabs + width, click to configure
- **Line/Column:** Current cursor position
- **Git Branch:** Current branch name, click for branch menu
- **Errors/Warnings:** Count badges, click to open Problems panel

**Implementation:**
- Custom `StatusBarWidget` using `QHBoxLayout` with `QPushButton` indicators
- Each indicator is styled as a flat label with click handlers
- Updates on file change, cursor move, and content change
- Connected in `mainwindow_actions.cpp`

**Dependencies:** None

---

### 13. File Encoding & Line Ending Support

**Impact:** 🟡 High | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Detect, display, and allow switching of file encodings and line endings:
- Auto-detect encoding on file open
- Display current encoding in status bar
- Allow re-encoding (UTF-8 ↔ Latin-1 ↔ UTF-16)
- Detect CRLF/LF/CR line endings
- Allow switching line endings
- Handle BOM (Byte Order Mark) detection

**Implementation:**
- `EncodingManager` class with encoding detection, conversion, BOM detection
- Rust backend `encoding_engine.rs` with 7 FFI functions
- Encoding/line ending shown in `StatusBarWidget`
- Connected on file open in `mainwindow_tabs.cpp`

**Dependencies:** None

---

## 🟢 P2 — Medium Priority Features (Polish & Completeness)

### 14. File Compare (Diff Viewer)

**Impact:** 🟢 Medium | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Side-by-side or unified diff comparison between any two files:
- Select two files from file tree to compare
- Side-by-side view with synchronized scrolling
- Highlight additions (green) and deletions (red)
- Navigate between changes with next/prev buttons

**Implementation:**
- `DiffViewerWidget` with two `QTextEdit` panels and `QSplitter`
- Synchronized scrolling via scrollbar signal connections
- Change navigation (next/previous)
- Rust backend `diff_engine.rs` for diff computation

**Dependencies:** None

---

### 15. Global Find & Replace with Preview

**Impact:** 🟢 Medium | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Show replacement preview across the entire project before applying:
- List all matches grouped by file
- Show before/after for each match
- Allow deselecting individual matches
- Apply selected replacements atomically

**Implementation:**
- `GlobalReplacePreview` widget with search, replace, case-sensitive, regex, whole-word options
- Tree view with checkboxes for selective replacement
- Connected as bottom panel tab in `mainwindow.cpp`

**Dependencies:** ProjectSearchPanel

---

### 16. Bookmark Navigation Panel

**Impact:** 🟢 Medium | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Dedicated panel showing all bookmarks across files:
- List all bookmarks grouped by file
- Click to jump to bookmark location
- Show line text preview for each bookmark
- Remove individual bookmarks
- Persist bookmarks across sessions

**Implementation:**
- `BookmarkPanelWidget` with `QTreeWidget` grouped by file path
- Connect to `BookmarkManager::bookmarksChanged` signal
- Connected as bottom panel tab

**Dependencies:** BookmarkManager

---

### 17. Notification Center

**Impact:** 🟢 Medium | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Panel collecting all toast notifications with history and search:
- Store all notifications with timestamp, type, message
- Show notification count badge on a toolbar button
- Filter by type (info, warning, error, success)
- Search through notification history
- Clear individual or all notifications

**Implementation:**
- `NotificationCenter` class with persistence, filtering, search, unread count
- Store notifications in a `QList<Notification>` with metadata
- Persist last N notifications to QSettings via `saveToSettings()`, `loadFromSettings()`
- Connected to status bar via `unreadCountChanged` signal

**Dependencies:** None

---

### 18. CSS/HTML Breadcrumb Enhancement

**Impact:** 🟢 Medium | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Show full DOM hierarchy in the breadcrumb bar for HTML/CSS files:
- Show nesting: `html > body > div.container > ul > li.active`
- Click any level to select that element
- Show class/id in breadcrumb chips
- Switch between DOM view and file path view

**Implementation:**
- `CssBreadcrumbParser` class for DOM hierarchy parsing
- `BreadcrumbBar::updateForEditor()` with parser integration

**Dependencies:** Breadcrumb

---

## 🔵 P3 — Nice-to-Have Features (Advanced & Niche)

### 19. Remote Development (SSH/WSL)

**Impact:** 🔵 Low-Medium | **Effort:** ⬛⬛⬛⬛⬛ Very High | **Status: ⬜ Not implemented**

Edit files on remote servers via SSH or WSL:
- Connect to remote hosts via SSH
- Browse remote file system
- Edit remote files with full IDE features
- Run remote terminal commands
- Forward LSP/DAP to remote

**Dependencies:** SSH client, Rust backend extensions

---

### 20. Container/DevContainer Support

**Impact:** 🔵 Low-Medium | **Effort:** ⬛⬛⬛⬛⬛ Very High | **Status: ⬜ Not implemented**

Develop inside Docker containers for consistent environments:
- Detect `.devcontainer/devcontainer.json`
- Build and start dev containers
- Mount project files into container
- Forward ports
- Run terminal inside container

**Dependencies:** Docker CLI

---

### 21. DevTools Integration

**Impact:** 🔵 Low-Medium | **Effort:** ⬛⬛⬛⬛ High | **Status: ⬜ Not implemented**

Embedded web inspector for debugging web content:
- Inspect HTML/CSS of rendered pages
- JavaScript console
- Network request viewer
- Performance profiler
- Mobile device emulation

**Dependencies:** QtWebEngine

---

### 22. Emmet Snippet Customization

**Impact:** 🔵 Low | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Allow users to define custom Emmet snippets and abbreviations:
- JSON-based snippet definitions
- Per-language snippet files via `SnippetEditorDialog`
- Variable support ($TM_FILENAME, $CLIPBOARD, etc.)
- Tab stops for cursor placement

**Implementation:**
- `SnippetEditorDialog` for editing/importing/exporting snippets
- `SnippetManager` with file-based loading
- Connected in `mainwindow.cpp`

**Dependencies:** SnippetManager

---

### 23. Live Share / Collaborative Editing

**Impact:** 🔵 Low | **Effort:** ⬛⬛⬛⬛⬛ Very High | **Status: ⬜ Not implemented**

Real-time collaborative editing between developers:
- Share a session via link
- Multiple cursors with author labels
- Shared terminal access
- Voice chat integration
- Session permissions (read-only, full access)

**Dependencies:** WebSocket server, CRDT library

---

### 24. Plugin Marketplace UI

**Impact:** 🔵 Low | **Effort:** ⬛⬛⬛ Medium | **Status: ✅ Implemented**

Visual interface for browsing and installing plugins:
- Search plugins by name, category, or keyword
- Show plugin details (description, author, version, screenshots)
- Install/uninstall/update plugins
- Show installed plugins with update notifications

**Implementation:**
- `PluginMarketplaceWidget` with card-based layout
- Uses `PluginRegistry` to fetch registry data
- `QNetworkAccessManager` to download plugin archives
- Connected in `mainwindow.cpp`

**Dependencies:** PluginRegistry, PluginManager

---

### 25. Theme Marketplace

**Impact:** 🔵 Low | **Effort:** ⬛⬛ Low | **Status: ✅ Implemented**

Browse and install community themes:
- Fetch theme list from a registry
- Preview theme before applying
- One-click install
- Import/export theme packages

**Implementation:**
- `ThemeMarketplaceWidget` with preview and install
- Extends `ThemeManager` with remote theme fetching
- Shows color preview swatches

**Dependencies:** ThemeManager

---

## ✅ Already Implemented in Scriptura

These features are already functional and do not need to be built:

| Category | Feature | Status |
|----------|---------|--------|
| **Editing** | Tabbed editing | ✅ Done |
| **Editing** | Multi-cursor (Ctrl+D, Alt+Click, column selection) | ✅ Done |
| **Editing** | Find & Replace in file | ✅ Done |
| **Editing** | Project-wide search | ✅ Done |
| **Editing** | Command palette (Ctrl+Shift+P) | ✅ Done |
| **Editing** | Smart indentation | ✅ Done |
| **Editing** | Bracket auto-close | ✅ Done |
| **Editing** | Bracket pair colorization | ✅ Done |
| **Editing** | Code folding | ✅ Done |
| **Editing** | Bookmarks | ✅ Done |
| **Editing** | Snippet manager | ✅ Done |
| **Navigation** | File tree sidebar | ✅ Done |
| **Navigation** | Breadcrumb | ✅ Done |
| **Navigation** | Universal search (Ctrl+P) | ✅ Done |
| **Navigation** | Go to definition/declaration/implementation | ✅ Done |
| **Navigation** | Outline/symbol tree | ✅ Done |
| **View** | Split views | ✅ Done |
| **View** | Minimap | ✅ Done |
| **View** | Custom title bar | ✅ Done |
| **View** | Zen mode | ✅ Done |
| **Git** | Commit, push, pull, fetch | ✅ Done |
| **Git** | Branch management | ✅ Done |
| **Git** | Diff view | ✅ Done |
| **Git** | Merge conflict resolution | ✅ Done |
| **Git** | Stash management | ✅ Done |
| **Git** | Interactive rebase | ✅ Done |
| **Git** | Inline blame annotations | ✅ Done |
| **Debug** | DAP-based debugging | ✅ Done |
| **Debug** | Breakpoints | ✅ Done |
| **Debug** | Step over/into/out | ✅ Done |
| **Debug** | Variable inspection | ✅ Done |
| **Refactoring** | Rename symbol (F2) | ✅ Done |
| **Refactoring** | Extract method (Ctrl+Alt+M) | ✅ Done |
| **Refactoring** | Extract variable (Ctrl+Alt+V) | ✅ Done |
| **Panels** | Terminal | ✅ Done |
| **Panels** | Problems panel | ✅ Done |
| **Panels** | TODO comments | ✅ Done |
| **Panels** | Git panel | ✅ Done |
| **Panels** | SQLite viewer | ✅ Done |
| **Panels** | HTTP client | ✅ Done |
| **Panels** | Regex tester | ✅ Done |
| **Panels** | Data formatter (JSON/YAML) | ✅ Done |
| **Panels** | Markdown preview | ✅ Done |
| **Panels** | Global replace preview | ✅ Done |
| **Panels** | Diff viewer (file compare) | ✅ Done |
| **Panels** | Bookmark panel | ✅ Done |
| **Panels** | Test panel | ✅ Done |
| **Panels** | Task runner UI | ✅ Done |
| **Panels** | Plugin marketplace | ✅ Done |
| **Panels** | Theme marketplace | ✅ Done |
| **LSP** | Completion | ✅ Done |
| **LSP** | Diagnostics | ✅ Done |
| **LSP** | Inlay hints | ✅ Done |
| **LSP** | Ghost text (inline completion) | ✅ Done |
| **LSP** | Code actions | ✅ Done |
| **LSP** | Code lens | ✅ Done |
| **Plugins** | Plugin manager | ✅ Done |
| **Plugins** | Plugin registry | ✅ Done |
| **Plugins** | Event bus | ✅ Done |
| **Plugins** | UI/editor/notification/theme APIs | ✅ Done |
| **Plugins** | Dependency resolution | ✅ Done |
| **Plugins** | Crash handler | ✅ Done |
| **Themes** | 8 color families × light/dark | ✅ Done |
| **Themes** | High contrast mode | ✅ Done |
| **Shortcuts** | Shortcut editor | ✅ Done |
| **Status Bar** | Language, encoding, line endings, indentation, git branch | ✅ Done |
| **Encoding** | File encoding detection & conversion | ✅ Done |
| **Encoding** | BOM detection | ✅ Done |
| **Encoding** | Line ending detection & conversion | ✅ Done |
| **Notifications** | Notification center | ✅ Done |
| **Backend** | Persistent sessions (save/restore/hot exit) | ✅ Done |
| **Backend** | Rust-based LSP/DAP clients | ✅ Done |
| **Backend** | Workspace management | ✅ Done |
| **Backend** | Task runner (Rust) | ✅ Done |
| **Backend** | Auto-updater | ✅ Done |
| **Backend** | Plugin updater | ✅ Done |
| **Backend** | Archive extraction | ✅ Done |
| **Backend** | Diff engine (Rust) | ✅ Done |
| **Backend** | Emmet engine (Rust) | ✅ Done |
| **Backend** | Encoding engine (Rust) | ✅ Done |
| **Backend** | Blame engine (Rust) | ✅ Done |
| **Backend** | Test engine (Rust) | ✅ Done |

---

## 📊 Implementation Priority Matrix

| Priority | Feature | Impact | Effort | Status |
|----------|---------|--------|--------|--------|
| **P0** | Persistent Sessions | 🔴 Critical | ⬛⬛⬛ | ✅ Done |
| **P0** | Rename Across Files | 🔴 Critical | ⬛⬛⬛ | ✅ Done |
| **P0** | Extract Method/Variable | 🔴 Critical | ⬛⬛⬛⬛ | ✅ Done |
| **P0** | Code Lens | 🔴 Critical | ⬛⬛⬛ | ✅ Done |
| **P0** | Emmet Expansion | 🔴 Critical | ⬛⬛⬛ | ✅ Done |
| **P1** | Git Blame Inline | 🟡 High | ⬛⬛ | ✅ Done |
| **P1** | Stash Management | 🟡 High | ⬛⬛ | ✅ Done |
| **P1** | Interactive Rebase | 🟡 High | ⬛⬛⬛ | ✅ Done |
| **P1** | Shortcut Customization | 🟡 High | ⬛⬛⬛ | ✅ Done |
| **P1** | Test Runner | 🟡 High | ⬛⬛⬛⬛ | ✅ Done |
| **P1** | Task Runner UI | 🟡 High | ⬛⬛⬛ | ✅ Done |
| **P1** | Status Bar Enhancements | 🟡 High | ⬛⬛ | ✅ Done |
| **P1** | File Encoding Support | 🟡 High | ⬛⬛ | ✅ Done |
| **P2** | File Compare (Diff) | 🟢 Medium | ⬛⬛⬛ | ✅ Done |
| **P2** | Global Replace Preview | 🟢 Medium | ⬛⬛⬛ | ✅ Done |
| **P2** | Bookmark Panel | 🟢 Medium | ⬛⬛ | ✅ Done |
| **P2** | Notification Center | 🟢 Medium | ⬛⬛ | ✅ Done |
| **P2** | CSS/HTML Breadcrumb | 🟢 Medium | ⬛⬛ | ✅ Done |
| **P3** | Remote Development | 🔵 Low-Med | ⬛⬛⬛⬛⬛ | ⬜ Not done |
| **P3** | DevContainer Support | 🔵 Low-Med | ⬛⬛⬛⬛⬛ | ⬜ Not done |
| **P3** | DevTools Integration | 🔵 Low-Med | ⬛⬛⬛⬛ | ⬜ Not done |
| **P3** | Custom Snippets UI | 🔵 Low | ⬛⬛ | ✅ Done |
| **P3** | Live Share | 🔵 Low | ⬛⬛⬛⬛⬛ | ⬜ Not done |
| **P3** | Plugin Marketplace | 🔵 Low | ⬛⬛⬛ | ✅ Done |
| **P3** | Theme Marketplace | 🔵 Low | ⬛⬛ | ✅ Done |

---

## 🗓️ Implementation Status

All P0-P2 features are fully implemented. The remaining unimplemented features are all P3 (Nice-to-Have):

### Still Remaining (4 features)
1. **Remote Development (SSH/WSL)** — Very High effort, advanced infrastructure
2. **DevContainer Support** — Very High effort, Docker CLI integration
3. **DevTools Integration** — High effort, requires QtWebEngine + CDP
4. **Live Share / Collaborative Editing** — Very High effort, WebSocket + CRDT

---

*Last updated: July 28, 2026*
*Scriptura IDE — Feature Roadmap v2.0*
