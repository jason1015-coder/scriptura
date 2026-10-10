# Plugin System — Plan (themes-only phase, tabs deferred)

> Status: **PLAN / NOT IMPLEMENTED YET.** This document describes the design for a
> plugin system for Scriptura. Phase 1 = theme plugins (pure config data, validated
> by Rust). Phase 2 (tabs) = config entry + compiled Rust plugin (deferred).

Scriptura is a Qt 6 C++ editor with a Rust backend (148 `extern "C"` FFI functions,
an `EventBus`, an encrypted settings store, and a `UiActionHandler` that already
implements the "Rust decides, C++ draws" pattern). This plan reuses that pattern and
the existing adapter classes so nothing gets ripped out later.

---

## 1. Guiding decisions

| Concern | Decision | Rationale |
|---|---|---|
| Plugin language | **Rust** (compiled to `cdylib` `.so`/`.dll`/`.dylib`) | User said "all code in Rust". |
| Themes | **Pure config/data files** (JSON) | Matches "theme: config". Safe; no compiler needed by users for themes. |
| Tabs | **Config entry + Rust plugin**, `renderer`-mapped to existing C++ widgets | "tab: config entry + rust". Tabs stay deferred in phase 1. |
| Install UX | **Drop file/folder → no click → reboot → auto-load** | You said: no marketplace, no picker; a single Plugins page list of all kinds is the only UI. |
| Extension model | **Themes are a strict subset of the plugin manifest** | Adding tab plugins later never changes the theme path. |

---

## 2. Shared manifest schema

Both themes and (later) tab plugins use one shape so the loader branches on `kind`:

```jsonc
{
  "id": "my-theme",                 // stable, unique
  "name": "My Theme",
  "version": "1.0.0",
  "kind": "theme",                  // "theme" | "tab"
  // --- theme-only (pure data) ---
  "theme": {
    "family": "blue",               // fallback family if not fully specified
    "mode": "dark",                 // "light" | "dark"
    "features": { "highContrast": false },
    "colors": {                     // any subset overrides built-ins
      "bgColor": "#1a1b1e",
      "highlightColor": "#3b82f6",
      "synKeyword": "#93c5fd",
      "synString": "#86efac"
      // ... full ThemeDefinition color set is the schema
    }
  }
}
```

A tab plugin manifest (phase 2) just adds:

```jsonc
{
  "id": "bookmarks-plus",
  "name": "Bookmarks+",
  "version": "1.0.0",
  "kind": "tab",
  "entrypoint": "bookmarks_plus",   // symbol: scriptura_plugin_entry
  "tab": {
    "tabs": [
      { "tabId": "bookmarks", "title": "Bookmarks", "icon": "★",
        "renderer": "builtin:BookmarkPanel" }   // maps to existing C++ widget
    ]
  }
}
```

A theme file = manifest with `kind:"theme"` and no `entrypoint`. A reader can treat every manifest uniformly; unknown keys are ignored (forward compatible).

---

## 3. Phase 1 — Themes (config data)

### 3.1 Where files live

```
~/.config/scriptura/themes/           (Linux)
~/Library/Application Support/scriptura/themes/   (macOS)
%APPDATA%\scriptura\themes\           (Windows)
```

Themes are plain `.json` files matching the schema above. They share one Settings → Plugins page with tab plugins (see §4.6); the Appearance →
Themes picker is just a themed view over the enabled theme subset.

### 3.2 Install (zero-click)

- Drop `my-theme.json` into the themes dir. That is the only step.
- Or, from inside the editor: **Settings → Appearance → Themes → Import…**, which just
  copies a picked file into the themes dir. (Optional convenience; the primary path is
  the filesystem drop.)

### 3.3 Discover (on startup)

New Rust module `src/rust_backend/src/theme_engine.rs`:

```text
ThemeEngine::scan(themes_dir)
  for each *.json:
    1. serde_json::from_str → serde_json::Value
    2. JSON-schema-validate   (fills the config_validator gap; rejects malformed)
    3. store  { id → (name, version, resolved ThemeDefinition JSON) }
    if invalid: skip + log  →  one-time toast on first run only
```

This plugs into `lib.rs` and gets a handful of new C-ABI exports (see §5).

### 3.4 Apply

Settings → Appearance → Themes → click a theme:

```
C++ ThemeManager::applyCustomTheme(id)
  → rust_theme_apply(id)        // FFI: Rust returns resolved ThemeDefinition JSON
  → buildPalette(def) / stylesheet / syntax colors
  → emit themeChanged()
```

`ThemeManager` already builds `QPalette` + a global stylesheet from `ThemeDefinition`
(`src/thememanager.cpp:buildPalette`). Phase 1 reuses that; Rust owns *validate + merge*,
C++ owns *draw*. Built-in families (8 × light/dark) remain the defaults when a field is
omitted, so a theme can specify only `colors.bgColor`.

### 3.5 Uninstall / disable

- **Uninstall**: delete the file. Auto-discovered on next launch.
- **Disable temporarily**: a per-theme flag in Settings (the Plugins-style toggle is
  tab plugins only in phase 2; for themes the Appearance page hides disabled themes and
  the flag lives in the settings store).

---

## 4. Phase 2 — Tabs (config entry + Rust plugin)  — DEFERRED, designed-for

> Out of scope for implementation now; documented so phase 1 does not preclude it.

### 4.1 Where plugins live

```
~/.config/scriptura/plugins/<id>/     # folder per plugin
   plugin.toml   OR  plugin.json      # manifest (TOML, because plugins are crates)
   lib<name>.so   (or .dll / .dylib)  # the compiled cdylib
```

On Linux, a `.toml` manifest is natural (the plugin is itself a Cargo cdylib). On the
other platforms the same Cargo project emits `.dll`/`.dylib`.

### 4.2 Install (zero-click)

Drop the folder → done. No marketplace, no picker, no Enable button at install time.
Reboot (or "Reload plugins" on the Plugins page — a dev convenience only).

### 4.3 Load (on startup)

New Rust module `src/rust_backend/src/plugin_manager.rs`:

```text
PluginManager::scan(plugins_dir)
  for each plugin/<id>/:
    1. parse manifest → check scriptura_version / abi_version compatibility
    2. libloading::library_load the .so
    3. dlsym("scriptura_plugin_entry") → PluginVTable*
       struct PluginVTable { void(*init)(const HostApi*); void(*activate)(); ... }
    4. pass a HostApi* into vtable->init()
    if any step fails: log + disable + one-time toast "Plugin failed to load: <id>"
```

`libloading` is added to `Cargo.toml` (works on Linux/macOS/Windows). A plugin `panic`
is caught by a `catch_unwind` FFI shim so a crashing plugin degrades gracefully instead
of killing the process (do **not** set `panic=abort` for plugins; keep unwinding so the
shim can recover).

### 4.4 Register (activate inactive C++ tab logic)

Plugin `init()`/`activate()`:

```rust
host.tabs.register(TabSpec {
    tab_id:  "bookmarks",
    title:   "Bookmarks",
    icon:    "★",
    renderer: "builtin:BookmarkPanel",   // existing C++ class — currently unwired
});
```

New C++ `TabHost` (owns the existing `ui->tabWidget`):

| `renderer` value | Materializes | Notes |
|---|---|---|
| `builtin:BookmarkPanel` | existing `BookmarkPanel` | currently present-but-unwired; now active |
| `builtin:Terminal` | existing `TerminalPanel` | |
| `builtin:Minimap` | existing `Minimap` widget | legacy / README "partially implemented" |
| `builtin:Breadcrumb` | existing `Breadcrumb` widget | legacy |
| `data` | generic host `QTextBrowser` | plugin pushes content via `setData` |
| `code` *(future)* | plugin emits `QWidget*` via FFI | the only way to build a fully custom widget |

The plugin **did not write the widget** — it declared and activated it. That is how the
inactive C++ tab logic gets turned on.

### 4.5 Behavior — "how a click in area A causes effect X" (reactive bridge)

The plugin never touches C++ widget internals. Behavior is a four-hop pipeline that
already exists in the codebase:

```
1. User clicks area A
     C++ widget emits a scoped Qt signal
     → TabHost bridge → EventBus::publish("tab.<tabId>.<area>.<action>", JSON)
2. Plugin subscribed in activate():
     host.events.subscribe("tab.bookmarks.itemSelected", on_selected)
3. Rust callback runs pure logic on the typed payload:
     on_selected({path}):
       if !is_safe_path(path) { host.tabs.show_toast("blocked"); return }
       host.tabs.invoke("bookmarks","setFilter", {path})
       host.ui.open_file(path)
4. TabHost::invoke dispatches to the widget's existing method:
     BookmarkPanel::setFilter(path)
```

Scoped via: **event names** `tab.<tabId>.<area>.<action>`, **typed JSON** payloads
(serde-validated, bad input rejected by Rust), and **idempotent invoke verbs**
(`setFilter`, `setData`, `setSelection`, `showToast`). Debounce, async, and errors are
handled in Rust.

### 4.6 Plugins page (the only plugin UI)

Settings → Plugins: a single flat list of **all** plugin kinds (themes + tab plugins).
It is the trivial merge of `ThemeEngine::scan` and `PluginManager::scan`, because both
share the manifest shape (`id`/`name`/`version`/`kind`).

```
id              version   kind   [x] Enabled
-------------------------------------------------
My Theme        1.0.0     theme  [x] Enabled
Bookmarks+      1.0.0     tab    [x] Enabled
minimap-plus    0.2.1     tab    [ ] Disabled
```

Columns: `id`, `version`, `kind` (`theme` | `tab`), and an Enable toggle. No
install/uninstall buttons, no marketplace, no file pickers (install = drop + reboot).
The toggle writes a `pluginDisabled/<id>` flag to the SettingsStore; disabled plugins
are skipped on the next scan. A disabled theme is simply absent from the theme list and
the Appearance → Themes picker; a disabled tab plugin is not loaded. Toggling a theme
here is equivalent to disabling it through the Themes page.

---

## 5. FFI surface (new `extern "C"` exports, Rust → C++)

Phase 1 (themes):

```c
// include/scriptura/rust_backend.h additions
RustThemeEngine* rust_theme_engine_new(void);
void             rust_theme_engine_free(RustThemeEngine* te);
void             rust_theme_engine_scan(RustThemeEngine* te, const char* dir);
char*            rust_theme_engine_list(RustThemeEngine* te);          // JSON array
char*            rust_theme_engine_get(RustThemeEngine* te, const char* id); // ThemeDefinition JSON
// returns "" on success, error msg otherwise
char*            rust_theme_engine_validate(RustThemeEngine* te,
                                            const char* json_manifest);
void             rust_theme_engine_set_disabled(RustThemeEngine* te,
                                                const char* id, int disabled);
int              rust_theme_engine_is_disabled(RustThemeEngine* te, const char* id);
```

Phase 2 (tabs, deferred) adds parallel `RustPluginManager*` + `PluginVTable`/`HostApi`
symbols, plus a `TabHost` C-ABI bridge (register + invoke + event subscribe). These are
sketched in §4 and do **not** need to ship with phase 1.

---

## 6. Open items / scope cut for phase 1

- **Tabs are out of scope** for implementation (phase 2). The design holds so they can
  be added without touching themes.
- **No marketplace, no signatures** (you asked for drop-and-reboot). A future "signed
  plugins" feature can slot onto `PluginManager` without changing `ThemeEngine`.
- **`panic=abort` is NOT used for plugins** — `catch_unwind` keeps a bad plugin from
  killing the app. (Phase 1 themes cannot panic in-process, so this only matters later.)
- **Plugins page vs Themes picker** — one Settings → Plugins page lists all kinds
(enabled-toggle drives both). The Appearance → Themes picker is a *view* over the
enabled theme subset, not a separate store.

---

## 7. Implementation order (suggested)

1. `src/rust_backend/src/theme_engine.rs` — scan + schema-validate + merge + FFI.
   (Reuses `ThemeDefinition` fields from `src/thememanager.h`.)
2. FFI exports in `ffi.rs` + header entries in `include/scriptura/rust_backend.h` + a
   small `RustThemeEngineAdapter` C++ class mirroring `RustLspClientAdapter`.
3. Wire `ThemeManager::applyCustomTheme(id)` to the adapter; reuse `buildPalette`.
4. Add the theme list to the existing Appearance → Themes page (no new UI window).
5. Tests: `theme_engine` cargo tests (schema, merge, fallback, invalid-skip) + one C++
   `RustThemeEngineAdapter` test that loads a fixture theme and asserts a color.

Phase 2 (tabs) begins only when explicitly queued.
