# Plugin System — Design & Plan

> **Status: PARTIALLY IMPLEMENTED (theme kind only).** A Rust `theme_engine`
> skeleton exists and passes unit tests. The generic registry, the FFI layer, the
> C++ adapter, the merge step, and all UI are **not** implemented yet, and a few
> broken stub files must be removed. This document is the single source of truth
> for the whole system: a small, open kind-registry core plus the first two kinds
> (`theme`, `tab`).

Scriptura is a Qt 6 C++ editor with a Rust backend (`scriptura-backend`, built as
a `staticlib`, consumed through ~150 `extern "C"` FFI functions). It has an
`EventBus`, an encrypted `SettingsStorage`, and a `UiActionBridge` that already
implement the **"Rust decides, C++ draws"** contract. The plugin system reuses
that contract exactly: Rust owns *discover → validate → resolve → decide*, C++
owns *build widgets/palettes and draw*.

---

## 0. Design decisions (the contract)

These were chosen up front; everything else in this doc follows from them.

| # | Concern | Decision | Why |
|---|---|---|---|
| 1 | Extension model | **Open & extensible** — a kind registry; `theme` and `tab` are the first two of many kinds (`command`, `panel`, `language`, …) | Future kinds add without reworking the core. |
| 2 | Manifest format | **JSON everywhere** — one `plugin.json`-style file for every kind | Matches existing theme `.json` + serde; least code. |
| 3 | Disk layout | **Flat `plugins/` dir** — one `<id>.json` per plugin; a code kind references its cdylib by relative path | Simplest single-pass scan. |
| 4 | Safety model | **Trust-all, in-process** — cdylibs are `dlopen`'d into the app process; `catch_unwind` + a scoped `HostApi` are the only safety nets | Fastest; matches the current Rust-backend pattern. |
| 5 | Kind registry | **Add-only kinds.** Plugins may register *new* kind handlers at runtime via the HostApi, but can **never** disable, remove, or override an official built-in kind; **id conflicts are rejected** | Extensible yet stable. |
| 6 | Split of labor | **Rust** = discover, validate, resolve, merge, dispatch, persist-enabled. **C++** = materialize widgets, build palette/stylesheet, draw | The established Rust↔C++ contract. |
| 7 | Data dir | Themes & plugins live under the **settings data dir** (`settings_get_dir()` → `plugins/`) | Honors `SCRIPTURA_SETTINGS_DIR` + `directories`; no hardcoded paths. |

**Non-negotiables that follow from #5:**
- Built-in kinds (`theme`, `tab`) are compiled into the app and always available.
- A plugin that tries to register a kind whose id already exists → **rejected** (logged, that plugin disabled).
- A plugin that tries to unregister/override `theme` or `tab` → **rejected** (no-op + warning).
- Enabling/disabling applies per **plugin id**, never per kind.

---

## 1. Architecture at a glance

```
                ┌──────────────────────────────────────────────┐
   plugins/ ───▶│  PluginRegistry  (Rust)                        │
   <id>.json    │   scan → parse JSON → validate → resolve kind  │
                │   enabled? (settings) → activate               │
                └───────────────┬──────────────────────────────┘
                                │  kind dispatch
        ┌───────────────────────┼───────────────────────────┐
        ▼                       ▼                           ▼
  ┌───────────┐          ┌───────────┐               ┌──────────────┐
  │ theme     │          │ tab       │   … future …  │ plugin-regd  │
  │ handler   │          │ handler   │               │ kind handler │
  │ (data)    │          │ (code)    │               │ (code)       │
  └─────┬─────┘          └─────┬─────┘               └──────┬───────┘
        │ resolved JSON        │ TabSpec list                │ HostApi calls
        ▼                      ▼                             ▼
  ThemeManager            TabHost (C++)               C++ kind hosts
  buildPalette/stylesheet owns ui->tabWidget          (panel, command, …)
```

- **One registry, many handlers.** The registry knows nothing about colors or
  tabs; it parses the manifest, picks a handler by `kind`, and hands off.
- **Two handler archetypes:** *data handlers* (theme) receive resolved config and
  return resolved JSON for C++ to draw; *code handlers* (tab, and any future
  code kind) `dlopen` a cdylib, call its entry point with a `HostApi`, and let it
  register concrete capabilities (tabs, commands, panels) with C++ hosts.
- **Add-only extensibility:** a code plugin can call `host.kinds.register(id, …)`
  to introduce a brand-new kind; the registry enforces the #5 rules.

---

## 2. Unified manifest schema (JSON)

Every plugin is one `<id>.json` file in `plugins/`. A shared envelope is common
to all kinds; a per-`kind` block carries the specifics. Unknown keys are ignored
(forward compatible). The loader validates the envelope, then dispatches on `kind`.

```jsonc
{
  // ── common envelope (all kinds) ──────────────────────────────
  "id": "my-theme",              // stable, unique; MUST equal the filename stem
  "name": "My Theme",
  "version": "1.0.0",
  "kind": "theme",               // dispatches to a handler: "theme" | "tab" | "command" | …
  "apiVersion": 1,               // host API version this plugin targets (checked at load)
  "description": "Optional one-liner shown in the Plugins page.",

  // ── code kinds only: points at the cdylib, relative to plugins/ ──
  "entrypoint": "bookmarks_plus",      // resolves to plugins/bookmarks_plus.{so,dll,dylib}
                                       //   (or a relative subpath: "libs/bookmarks_plus")

  // ── theme kind ───────────────────────────────────────────────
  "theme": {
    "family": "blue",             // fallback base: default|blue|green|red|yellow|brown|cyan|violet
    "mode":   "dark",             // "light" | "dark"
    "features": { "highContrast": false },
    "colors": {                   // ANY subset — overlaid onto the family/mode base
      "bgColor": "#1a1b1e",
      "highlightColor": "#3b82f6",
      "synKeyword": "#93c5fd"
      // key set == the 24 fields of struct ThemeDefinition (src/thememanager.h)
    }
  },

  // ── tab kind (code) ──────────────────────────────────────────
  "tab": { "tabs": [
    { "tabId": "bookmarks", "title": "Bookmarks", "icon": "★",
      "renderer": "builtin:BookmarkPanel" }   // maps to an existing C++ widget
  ] }
}
```

**Color keys** (`theme.colors`) are exactly the public fields of `ThemeDefinition`:
`bgColor, textColor, svgColor, buttonColor, highlightColor, baseColor,
borderColor, lightColor, midColor, darkColor, buttonTextColor, highlightedText,
synKeyword, synString, synComment, synNumber, synPreprocessor, synTag,
synAttribute, synCssProperty, synVariable, synFunction, synEscape`.

**Validation rules (enforced by the Rust registry):**
- `id` is non-empty, unique, and equals the filename stem → else reject + log.
- `version` is valid semver → else reject.
- `kind` resolves to a known (built-in or plugin-registered) handler → else reject.
- `apiVersion` ≤ host's supported API → else reject (skip, one-time toast).
- `entrypoint` present for code kinds; the referenced library exists → else reject.
- Per-kind block matches its handler's schema (unknown keys ignored).


---

## 3. The registry & the kind system (the core)

New Rust module `src/rust_backend/src/plugin_registry.rs` (the existing
`theme_engine.rs` becomes the **theme kind handler** used by the registry).

### 3.1 Responsibilities

```text
PluginRegistry::scan(plugins_dir)
  for each <id>.json in dir:
    1. parse envelope (serde)                         → reject malformed (skip + log)
    2. validate id/version/kind/apiVersion            → reject on failure
    3. enabled? (read plugin/disabled/<id> setting)   → skip if disabled
    4. dispatch by kind:
         theme  → ThemeHandler::resolve()             → resolved ThemeDefinition JSON
         tab    → CodeHandler::load() + activate()    → registers capabilities
         <new>  → the handler registered for that kind
    5. record { id → PluginRecord{ kind, version, name, state } }
  return summary (for the Plugins page)
```

### 3.2 Kind handlers

A **kind handler** is a trait with a small, fixed surface:

```rust
trait KindHandler {
    fn kind(&self) -> &str;                              // e.g. "theme", "tab"
    fn validate(&self, manifest: &Value) -> Result<(), String>;
    fn activate(&self, ctx: &ActivateCtx) -> Result<(), String>;
    fn deactivate(&self, id: &str);
    fn builtin(&self) -> bool { true }                   // official vs plugin-registered
}
```

- **Built-in handlers** (`builtin() == true`): compiled into the app — `theme`
  and `tab`. Always present; cannot be removed, disabled, or overridden.
- **Plugin-registered handlers** (`builtin() == false`): created at runtime by a
  code plugin calling `host.kinds.register(id, handler)` (see §4.4).

### 3.3 Add-only rules (from decision #5) — enforced here

`PluginRegistry::register_kind(id, handler)`:

| Attempt | Result |
|---|---|
| Register a brand-new id | ✅ accepted; stored in the runtime kind table |
| Register an id that already exists (built-in **or** another plugin) | ❌ **rejected** — logged; the offending plugin is marked failed |
| Unregister / override `theme` or `tab` (or any `builtin()` handler) | ❌ **rejected** — no-op + warning; built-ins are immutable |
| Disable a *kind* | ❌ not a concept — enabling/disabling is per **plugin id** only |

This keeps the core stable (the editor's own kinds can't be hijacked) while still
letting the ecosystem grow new kinds additively.

### 3.4 Enable / disable (persisted)

- The Plugins page toggle writes `plugin/disabled/<id> = "true"` via the existing
  `settings_set`. Disabled ids are skipped on the next `scan`.
- Data-only keys are never secrets → plain settings store, not the keychain.
- Uninstall = delete the `<id>.json` (+ its cdylib); rediscovered next launch.


---

## 4. Code plugins: loading, HostApi, and extensibility

Applies to `tab` today and every future code kind. (Data kinds like `theme` never
load a library and are covered in §5.)

### 4.1 Loading (trust-all, in-process)

**Add `libloading` to `Cargo.toml`.** The `scriptura-backend` crate stays a
`staticlib`; plugins are separate `cdylib`s `dlopen`'d at runtime, so the crate
type is unchanged.

```text
CodeHandler::load(manifest)
  1. resolve entrypoint → plugins/<entrypoint>.{so,dll,dylib}   (or relative subpath)
  2. libloading load the library
  3. dlsym("scriptura_plugin_entry") → *const PluginVTable
  4. build a HostApi (scoped, see 4.2) and call vtable.init(&host)
  5. vtable.activate()
  any failure → log + mark plugin failed + one-time toast "Plugin failed to load: <id>"
```

```rust
#[repr(C)]
pub struct PluginVTable {
    pub api_version: u32,
    pub init:      extern "C" fn(*const HostApi),
    pub activate:  extern "C" fn(),
    pub deactivate: extern "C" fn(),
}
```

Every call into plugin code is wrapped in `std::panic::catch_unwind` so a panicking
plugin degrades gracefully instead of killing the app. Do **not** set `panic=abort`
for the plugin target (keep unwinding so the shim can recover). Trust-all means we
do **not** sandbox or sign — the scoped `HostApi` is the only guardrail.

### 4.2 The HostApi (scoped, C-ABI)

The only door between a plugin and the app. A function-pointer table; each plugin
gets one scoped to its own `id` (so events, toasts, and registrations are
namespaced). Versioned by `api_version`; a mismatch is rejected at load.

```rust
#[repr(C)]
pub struct HostApi {
    pub api_version: u32,

    // events (reuses the existing EventBus; topics are namespaced per plugin)
    pub events_subscribe: extern "C" fn(topic: *const c_char, cb: OnEvent, ctx: *mut c_void);
    pub events_publish:  extern "C" fn(topic: *const c_char, json: *const c_char),

    // tabs (the tab kind uses this; idempotent verbs)
    pub tabs_register:  extern "C" fn(spec_json: *const c_char),   // TabSpec
    pub tabs_invoke:    extern "C" fn(tab_id, verb, json: *const c_char),

    // ui
    pub ui_open_file:   extern "C" fn(path: *const c_char),
    pub ui_toast:       extern "C" fn(message: *const c_char),

    // ── extensibility (decision #5) ────────────────────────────
    // Register a NEW kind. Returns 0 on success; nonzero if the id
    // already exists or is a built-in kind (rejected).
    pub kinds_register: extern "C" fn(kind_id: *const c_char, handler: *const KindHandlerFfi) -> i32,
}
```

### 4.3 Registering capabilities (tab example)

The plugin never touches C++ widget internals. In `activate()` it declares what it
wants; a C++ host materializes it. New C++ `TabHost` owns `ui->tabWidget` and maps
`renderer` → widget:

| `renderer` value | Materializes | Notes |
|---|---|---|
| `builtin:BookmarkPanel` | `src/panels/bookmarkpanel.{h,cpp}` | present; now activated |
| `builtin:Terminal` | `src/panels/terminal/terminalpanel.*` | |
| `builtin:Minimap` | `src/widgets/minimap.*` | legacy / partially wired |
| `builtin:Breadcrumb` | `src/widgets/breadcrumb.*`, `breadcrumbbar.*` | legacy |
| `data` | generic host `QTextBrowser` | plugin pushes content via `setData` |
| `code` *(future)* | plugin returns `QWidget*` via FFI | the only way to build a fully custom widget |

The plugin **declares and activates** a widget; it does not write it.

### 4.4 Behavior — reactive bridge (reuses the existing EventBus)

```
1. Widget emits a scoped Qt signal
     → TabHost bridge → EventBus::publish("tab.<tabId>.<area>.<action>", JSON)
2. Plugin subscribed in activate():
     host.events.subscribe("tab.bookmarks.itemSelected", on_selected)
3. Rust callback runs pure logic on the typed payload:
     on_selected({path}):
       if !is_safe_path(path) { host.ui.toast("blocked"); return }
       host.tabs.invoke("bookmarks","setFilter", {path}); host.ui.open_file(path)
4. TabHost::invoke dispatches to the widget's existing method (e.g. BookmarkPanel::setFilter)
```

Scoped via **event topics** `tab.<tabId>.<area>.<action>`, **typed JSON** payloads
(serde-validated, bad input rejected), and **idempotent invoke verbs**
(`setFilter`, `setData`, `setSelection`). Debounce/async/errors live in Rust.

### 4.5 Adding a brand-new kind (extensibility in practice)

A code plugin calls `host.kinds.register("panel", handler)`. On success it may then
register concrete panels with the C++ panel host. This is how `command`, `panel`,
`language`, … arrive **without** an editor update to the core:

```rust
// inside a plugin's activate()
let ok = host.kinds_register(c"panel", &my_panel_kind_handler);
assert_eq!(ok, 0); // nonzero ⇒ id already taken or built-in ⇒ rejected
```

Because kinds are add-only, two plugins shipping a `panel` kind collide on the
*first* registration; the second is rejected and disabled. Distinct new kinds
(`panel`, `command`) coexist fine.


---

## 5. Built-in kind: `theme` (data)

### 5.1 Files & discovery

Themes are `<id>.json` files in `<data dir>/plugins/` with `kind:"theme"` (the dir
comes from `settings_get_dir()`, so `SCRIPTURA_SETTINGS_DIR` and the `directories`
crate's per-OS rules apply automatically). Install = drop the file; reboot
auto-loads. Optional **Appearance → Themes → Import…** copies a picked file in.

### 5.2 Resolve (validate + merge, in Rust)

`ThemeHandler::resolve` extends the existing `theme_engine.rs`:

```text
for each theme <id>.json:
  1. serde_json::from_str::<ThemeManifest>     (reject malformed → skip + log)
  2. require kind == "theme" (or empty)        (else it's another kind)
  3. base   = builtin(family, mode)            // 8 families × 2 modes fallback
     result = merge(base, theme.colors)        // overlay any subset of the 24 colors
  4. store { id → resolved ThemeDefinition JSON }   // COMPLETE object, C++ fills nothing
```

Invalid files are skipped + logged; a one-time toast fires only on the first run
that finds any. (The current code only echoes the manifest as `resolved` — the
merge in step 3 is the gap being closed.)

### 5.3 Apply (C++ draws)

```
RustThemeEngineAdapter::get(id) → resolved ThemeDefinition JSON
  → decode JSON → struct ThemeDefinition          // new decoder
  → ThemeManager::applyDefinition(def)            // NEW entry point (see 5.3a)
  → buildPalette(def) + stylesheet + syntax colors // reuse existing
  → push palette/syntax to all widgets & editors   // existing MainWindow::applyTheme body
```

`ThemeManager` already builds a `QPalette` + global stylesheet from a
`ThemeDefinition` (`src/thememanager.cpp:buildPalette`). Built-in families remain
the defaults when a field is omitted, so a theme may specify only `colors.bgColor`.

**5.3a — the real C++ gap.** `ThemeManager::applyTheme(const Theme&)` only accepts
a `ColorFamily`/`Mode`/`Features` triple. A custom theme is an *arbitrary*
`ThemeDefinition`, so add:

```cpp
// src/thememanager.h
void applyDefinition(const ThemeDefinition &def);  // sets m_currentDefinition, emits themeChanged()
```

`applyTheme(theme)` stays for built-ins and internally becomes
`applyDefinition(buildDefinition(theme.family, theme.mode))` (+ the existing
high-contrast adjustment). Refactor `MainWindow::applyTheme`'s "push palette +
syntax colors to every widget" body into a helper both paths call, so custom and
built-in themes get identical treatment.

### 5.4 Disable / uninstall

- **Uninstall**: delete `<id>.json`; rediscovered next launch.
- **Disable**: toggle writes `plugin/disabled/<id>`; the theme is absent from both
  the Plugins list's enabled view and Appearance → Themes.


---

## 6. FFI surface (Rust → C++), matching existing conventions

Follow `src/rust_backend/src/ffi.rs`: opaque handle enum, `make_new!`/`make_free!`
macros, returned strings freed with `rust_free_string`, errors via `rust_last_error`.
Declarations go in `include/scriptura/rust_backend.h` (replacing the current
invalid theme block).

```c
// ── registry (new) ──────────────────────────────────────────────
RustPluginRegistry* rust_plugin_registry_new(void);
void                rust_plugin_registry_free(RustPluginRegistry* r);
void                rust_plugin_registry_scan(RustPluginRegistry* r, const char* plugins_dir);
char*               rust_plugin_registry_list(RustPluginRegistry* r);        // JSON: [{id,name,version,kind,enabled,state}]
void                rust_plugin_registry_set_disabled(RustPluginRegistry* r, const char* id, int disabled);

// ── theme kind (existing engine, re-homed under the registry) ────
char*               rust_theme_engine_get(RustThemeEngine* te, const char* id);      // resolved ThemeDefinition JSON
char*               rust_theme_engine_validate(RustThemeEngine* te, const char* json);
```

Phase 2+ adds `RustPluginManager*`, the `PluginVTable`/`HostApi` symbols, the
`KindHandlerFfi` registration path, and a `TabHost` C-ABI bridge (register +
invoke + event subscribe). These are sketched in §4 and do **not** ship with the
theme-only increment.

### 6.1 C++ adapter

Add `RustPluginRegistryAdapter` (+ reuse a `RustThemeEngineAdapter`) in
`src/rust_adapter.h` as members of the `RustBackend` singleton, mirroring
`RustLspClientAdapter`/`RustConfigValidatorAdapter`. **Delete** the broken
`src/rust_backend/src/theme_engine.{h,cpp}` stubs and the bogus `extern "C"`
block at the top of `include/scriptura/rust_backend.h` — CMake's
`file(GLOB_RECURSE "src/*.cpp")` would otherwise compile those undefined symbols
into `scriptura_core`.

---

## 7. The Plugins page (the only plugin UI)

A single flat list of **all** kinds — the trivial merge of the registry's `list`,
since every plugin shares the envelope. Rendered on the existing unified Settings
page (no new window).

```
id              version   kind    state        [x] Enabled
------------------------------------------------------------
My Theme        1.0.0     theme   loaded       [x] Enabled
Bookmarks+      1.0.0     tab     loaded       [x] Enabled
minimap-plus    0.2.1     tab     failed-load  [ ] Disabled
```

- Columns: `id`, `version`, `kind`, `state` (`loaded`/`failed-load`/`disabled`),
  and an Enable toggle. **No** install/uninstall/marketplace controls (install =
  drop + reboot).
- The toggle writes `plugin/disabled/<id>`; disabled plugins are skipped next scan.
- Kinds are never shown as disable-able — only plugins are.
- A disabled theme also disappears from Appearance → Themes.

---

## 8. Current state vs. this plan

| Area | Exists today | Gap this plan closes |
|---|---|---|
| Rust scan/validate | ✅ `theme_engine.rs` — `ThemeEngine{scan,list,get,validate,set_disabled,is_disabled}` + tests, in `lib.rs` | Wrap in a `PluginRegistry`; add the `resolve()` merge; generalize dispatch by `kind` |
| FFI exports | ❌ none (`rust_theme_engine_*` / registry absent from `ffi.rs`) | Add registry + theme exports (§6) |
| C++ adapter | ⚠️ broken `theme_engine.{h,cpp}` stubs + bogus `rust_backend.h` block | Delete; add `RustPluginRegistryAdapter` to `rust_adapter.h` |
| Custom-theme apply | ❌ `applyTheme` only takes family/mode/features | Add `applyDefinition` + JSON decoder (§5.3a) |
| Code-plugin loading | ❌ none | `libloading` + `PluginVTable`/`HostApi` + `catch_unwind` (§4) |
| Kind registry | ❌ none | `KindHandler` trait + add-only `register_kind` (§3) |
| Plugins / Appearance UI | ❌ none | Plugins list + theme entries on the unified Settings page (§7) |
| Disabled persistence | ⚠️ in-memory only | Persist `plugin/disabled/<id>` |

---

## 9. Scope cut & non-goals

- **Sandboxing / signatures are out of scope** (trust-all, per decision #4).
- **Built-in kinds are immutable** and always available (decision #5).
- **No marketplace, no in-app install** — install is filesystem drop + reboot.
- **Only `theme` ships in the first increment.** `tab` and every future kind reuse
  the same registry/handler/HostApi machinery, so adding them never reworks the core.

---

## 10. Implementation order (suggested)

1. **Delete the broken stubs**: `src/rust_backend/src/theme_engine.{h,cpp}`; clean
   the theme block from `include/scriptura/rust_backend.h`.
2. **`plugin_registry.rs`**: envelope parse + validate + enable/disable + `scan`;
   register the built-in `theme` handler (delegating to `theme_engine`).
3. **`theme_engine.rs`**: add `resolve()` merge so `get()` returns a complete
   `ThemeDefinition` JSON; keep existing tests.
4. **FFI**: registry + theme exports in `ffi.rs`; header decls.
5. **Adapter**: `RustPluginRegistryAdapter` in `src/rust_adapter.h` + `RustBackend`.
6. **Apply path**: `ThemeManager::applyDefinition` + JSON decoder; factor the
   push-body in `MainWindow::applyTheme` into a shared helper; wire the adapter.
7. **UI**: Plugins list (persisting `plugin/disabled/<id>`) + custom themes in
   Appearance → Themes, on the unified Settings page.
8. **Tests**: cargo tests (registry dispatch, add-only rules incl. conflict +
   built-in-override rejection, merge, fallback, invalid-skip, disabled) + a Qt
   Test that loads a fixture theme and asserts a merged color (mirroring
   `tests/test_thememanager.cpp`).

The `tab` kind (and any future code kind) begins only when explicitly queued,
reusing §4's loading/HostApi/registry machinery unchanged.

