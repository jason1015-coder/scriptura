# How to Develop a Plugin

## 1. Prerequisites

- Qt 6.5+ with Widgets and Core modules
- CMake 3.16+
- C++17 compatible compiler
- Rust toolchain (for the backend — plugins can optionally use Rust)
- Scriptura source tree (or installed SDK)

## 2. Directory Structure

Each plugin lives in its own subdirectory. You can place it anywhere while developing, or inside the Scriptura source tree.

```
plugins/
└── myplugin/
    ├── plugin.json          # Plugin metadata (required)
    ├── myplugin.h           # Plugin class header (C++ Qt)
    ├── myplugin.cpp         # Plugin class implementation
    ├── CMakeLists.txt       # Build configuration
    └── resources/           # Optional icons, styles, translations
```

### Rust Plugins (Experimental)

Plugins can also be written in Rust using the C FFI bridge:

```
plugins/
└── myrustplugin/
    ├── plugin.json          # Plugin metadata (required)
    ├── Cargo.toml           # Rust project
    └── src/
        ├── lib.rs           # Implements the ScripturaPlugin interface via C FFI
        └── ffi.rs           # C-compatible exports
```

## 3. Plugin Metadata (`plugin.json`)

```json
{
    "id": "com.example.myplugin",
    "name": "My Plugin",
    "version": "1.0.0",
    "author": "Author Name",
    "description": "What this plugin does",
    "mainClass": "MyPlugin",
    "library": "libmyplugin.so",
    "category": "tool",
    "tags": ["example", "editor"],
    "dependencies": [],
    "optionalDependencies": [],
    "settings": {},
    "permissions": []
}
```

> **Note:** The `library` value must match the exact compiled output filename for the current platform:
> - Linux: `libmyplugin.so`
> - macOS: `libmyplugin.dylib`
> - Windows: `myplugin.dll`

## 4. Plugin Interface

All C++ plugins must inherit from both `QObject` and `ScripturaPlugin`, and declare the Qt plugin macros.

```cpp
#include <QObject>
#include "../include/scriptura/plugininterface.h"
#include "../include/scriptura/plugincontext.h"

class MyPlugin : public QObject, public ScripturaPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.scriptura.plugin/1.0" FILE "plugin.json")
    Q_INTERFACES(ScripturaPlugin)

public:
    explicit MyPlugin(QObject* parent = nullptr);
    ~MyPlugin() override;

    bool initialize(PluginContext* context) override;
    void shutdown() override;

    QString id() const override { return "com.example.myplugin"; }
    QString name() const override { return "My Plugin"; }
    QString version() const override { return "1.0.0"; }
    QString author() const override { return "Author Name"; }
    QString description() const override { return "Plugin description"; }
    QStringList dependencies() const override { return {}; }

    bool hasFeature(PluginFeature feature) const override;

private:
    PluginContext* m_context;
};
```

## 5. Plugin Lifecycle

### `initialize(PluginContext* context)`

Called after the plugin is loaded. Use this to:

- Store the context pointer
- Create UI panels or widgets
- Register menu actions or status bar widgets
- Subscribe to events via the Rust-powered `EventBus`
- Register services via the Rust-powered `ServiceLocator`
- Check permissions

Return `true` on success, `false` on failure.

### `shutdown()`

Called before the plugin is unloaded. Use this to:

- Delete allocated UI widgets
- Disconnect signals and slots
- Save plugin state to settings
- Unsubscribe from events
- Stop background threads or processes

## 6. Accessing Host Services (`PluginContext`)

```cpp
// Core
MainWindow* mainWindow = m_context->mainWindow();
QSettings* settings = m_context->settings();

// Editor
CodeEditor* editor = m_context->currentEditor();

// Panels
LspClient* lsp = m_context->lspClient();
ProblemPanel* problems = m_context->problemPanel();
TerminalPanel* terminal = m_context->terminalPanel();
GitPanel* git = m_context->gitPanel();

// Project
QString projectPath = m_context->currentProjectPath();
```

> **Note:** `LspClient` and `DapClient` now use Rust backends via C FFI.
> The `QObject`-based API remains identical — the Rust integration is transparent to plugins.

## 7. Settings

Use `PluginSettings` to store plugin-specific settings. Settings are automatically grouped by plugin ID.

```cpp
#include "pluginsettings.h"

// In initialize():
PluginSettings settings("com.example.myplugin", m_context->settings());

// Set defaults
QJsonObject defaults;
defaults["enabled"] = true;
defaults["maxItems"] = 100;
settings.setDefaults(defaults);

// Read values
bool enabled = settings.value("enabled").toBool();
int maxItems = settings.value("maxItems", 50).toInt();

// Write values
settings.setValue("lastTimestamp", QDateTime::currentDateTime().toString());

// Reset to defaults
settings.resetToDefaults();
```

## 8. Event System

Publish and subscribe to events for inter-plugin communication. The EventBus now runs on the Rust backend.

```cpp
// Publish an event
m_context->notify("myplugin.event", "some data");

// Subscribe to events
m_context->subscribe("editor.saved", [](const QVariant& data) {
    qDebug() << "Editor saved:" << data.toString();
});

// Or access the Rust-backed EventBus directly
RustBackend::instance()->eventBus()->publish("myplugin.event", {{"key", "value"}});
```

## 9. Service Locator

Register and discover services across plugins. The ServiceLocator now runs on the Rust backend.

```cpp
// Register a service
RustBackend::instance()->eventBus()->publish("service.register", {
    {"id", "myplugin.formatter"},
    {"instance", QVariant::fromValue(this)}
});

// Check existence via the Rust-powered ServiceLocator
// (C++ adapter wraps the FFI calls)
```

## 10. Permissions

Declare required permissions in `plugin.json`:

```json
{
    "permissions": ["process.execution", "file.read"]
}
```

The host will prompt the user before granting these. In code, use the Rust-backed permission manager:

Permissions are managed by the Rust `PermissionManager` backend (accessible via low-level FFI).
The host framework supplies the permission manager instance at runtime.

Available permissions:
- `file.read`
- `file.write`
- `network.access`
- `process.execution`
- `system.settings`
- `clipboard.access`
- `notification`

## 11. Features

Declare supported features to allow the host and other plugins to discover capabilities.

```cpp
bool hasFeature(PluginFeature feature) const override
{
    switch (feature) {
        case PluginFeature::MenuAction:
        case PluginFeature::ToolPanel:
        case PluginFeature::StatusBarWidget:
            return true;
        default:
            return false;
    }
}
```

Available `PluginFeature` values:
- `EditorExtension`
- `SyntaxHighlighting`
- `CodeCompletion`
- `ToolPanel`
- `StatusBarWidget`
- `MenuAction`
- `ProjectWizard`
- `BuildSystem`
- `FileExplorer`
- `LSPProvider`
- `DiagnosticsProvider`
- `Formatter`
- `VCSIntegration`
- `TerminalEmulator`
- `ExternalTool`

## 12. CMake Build Configuration

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(myplugin SHARED
    myplugin.cpp
    myplugin.h
)

target_link_libraries(myplugin PRIVATE
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(myplugin PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/plugins/myplugin
)

set_target_properties(myplugin PROPERTIES
    OUTPUT_NAME myplugin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/myplugin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/myplugin
)

if(WIN32)
    set_target_properties(myplugin PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(myplugin PROPERTIES SUFFIX ".dylib")
elseif(UNIX)
    set_target_properties(myplugin PROPERTIES SUFFIX ".so")
endif()

install(FILES plugin.json
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/myplugin
)

install(TARGETS myplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/myplugin
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/myplugin
)
```

## 13. Building

From the Scriptura root, build the entire project including Rust backend and plugins:

```bash
# Build Rust backend
cd src/rust_backend && cargo build --release && cd ../..

# Build C++ project (includes plugins)
cmake -B build -S .
cmake --build build -j$(nproc)
```

Or build the plugin standalone if using the SDK:

```bash
cd /path/to/myplugin
cmake -B build -S .
cmake --build build
```

The output library will be placed in `build/plugins/myplugin/`.

## 14. Testing

Scriptura enforces GUI-only plugin installation. To test a built plugin during development:

1. Open Scriptura
2. Go to **Plugins → Manage Plugins...**
3. Click **Install Plugin...**
4. Select your plugin directory, typically `build/plugins/myplugin/`
5. Confirm the installation
6. Restart Scriptura to load it

## 15. Complete Example

```cpp
// myplugin.h
#include <QObject>
#include <QAction>
#include <QWidget>
#include <QLabel>
#include "../include/scriptura/plugininterface.h"
#include "../include/scriptura/plugincontext.h"

class MyPlugin : public QObject, public ScripturaPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.scriptura.plugin/1.0" FILE "plugin.json")
    Q_INTERFACES(ScripturaPlugin)

public:
    explicit MyPlugin(QObject* parent = nullptr);
    ~MyPlugin() override;

    bool initialize(PluginContext* context) override;
    void shutdown() override;

    QString id() const override { return "com.example.myplugin"; }
    QString name() const override { return "My Plugin"; }
    QString version() const override { return "1.0.0"; }
    QString author() const override { return "Author"; }
    QString description() const override { return "A sample plugin"; }
    QStringList dependencies() const override { return {}; }

    bool hasFeature(PluginFeature feature) const override;

private slots:
    void doSomething();

private:
    PluginContext* m_context;
    QAction* m_action;
    QWidget* m_widget;
};

// myplugin.cpp
#include "myplugin.h"
#include <QMessageBox>

MyPlugin::MyPlugin(QObject* parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_action(nullptr)
    , m_widget(nullptr)
{
}

MyPlugin::~MyPlugin()
{
    shutdown();
}

bool MyPlugin::initialize(PluginContext* context)
{
    m_context = context;
    if (!m_context) return false;

    if (m_context->mainWindow()) {
        m_action = new QAction("My Plugin Action", m_context->mainWindow());
        connect(m_action, &QAction::triggered, this, &MyPlugin::doSomething);
        m_context->mainWindow()->menuBar()->addAction(m_action);
    }

    m_widget = new QWidget();
    new QLabel("Hello from MyPlugin", m_widget);

    qDebug() << "MyPlugin initialized";
    return true;
}

void MyPlugin::shutdown()
{
    if (m_action) {
        delete m_action;
        m_action = nullptr;
    }
    if (m_widget) {
        delete m_widget;
        m_widget = nullptr;
    }
    m_context = nullptr;
    qDebug() << "MyPlugin shutdown";
}

bool MyPlugin::hasFeature(PluginFeature feature) const
{
    switch (feature) {
        case PluginFeature::MenuAction:
        case PluginFeature::ToolPanel:
            return true;
        default:
            return false;
    }
}

void MyPlugin::doSomething()
{
    QMessageBox::information(nullptr, "My Plugin", "Hello from MyPlugin!");
}
```
