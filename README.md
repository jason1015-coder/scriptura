# Scriptura

A hybrid Qt/Rust text editor with project file browsing — **Qt for the UI, Rust for safe backend services**.

> **Note:** This project is at an early development stage which expects bugs and occasional broken features.
> The Rust backend migration aims to eliminate C++ segmentation faults by moving all protocol, infrastructure,
> and service code into safe Rust.

## Architecture

Scriptura uses a **dual-language architecture**:

| Layer | Language | Technology | Responsibility |
|-------|----------|-----------|----------------|
| **UI Layer** | C++ | Qt 6 Widgets | All visual components (editor, panels, menus, dialogs) |
| **Adapter Layer** | C++ | Qt + C FFI | Thin wrappers bridging Qt signals/slots to Rust callbacks |
| **Backend Layer** | Rust | Pure Rust (no Qt) | LSP/DAP protocol, event bus, plugin manager, task runner, updater, workspace, config, permissions |

The backend services are compiled into a static library (`libscriptura_backend.a`) via **Cargo** and linked into the C++ executable. Cross-language communication uses **C FFI** (`extern "C"`) with JSON strings for complex data. All state management and protocol handling runs in safe Rust.

## Features

- **Project-based workflow**: Open a project directory to browse files
- **File tree sidebar**: Navigate project structure with clickable folders
- **Directory navigation**: "Go Up" button to navigate to parent directories
- **Tabbed editing**: Multiple files open in tabs
- **Status bar**: Shows cursor position (line/column)
- **Edit operations**: Cut, Copy, Paste, Undo, Redo
- **File management**: Create and delete files/directories within projects
- **Save As**: Save files with a different name
- **Theme support**: Multiple light/dark themes with Xcode-inspired design system
- **Plugin Manager**: Install, remove, and manage plugins via the GUI (**Plugins → Manage Plugins...**)
- **Plugin system**: Extensible architecture with event bus, service locator, and settings support
- **Built-in Git plugin**: Version control integration for commits and pushes
- **LSP support**: Code completion, diagnostics, go-to-definition (via Rust client)
- **Debug support**: DAP-based debugging (via Rust client)
- **Safe Rust backend**: Thread-safe event bus, memory-safe protocol handlers, no segmentation faults

## Requirements

- **Qt 6** (with Widgets, Network, Sql, and LinguistTools modules) — Qt 5 also supported
- **CMake 3.16+**
- **C++17 compiler** (GCC, Clang, MSVC)
- **Rust toolchain** (for building the backend) — Install via [rustup](https://rustup.rs/):
  ```bash
  curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh
  ```

## Building

### Quick Build (Debug)
```bash
./build.sh
```

### Build (Release)
```bash
./build.sh Release
```

### Manual Build
```bash
# Build Rust backend first
cd src/rust_backend && cargo build --release && cd ../..

# Build C++ project
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

## Running

```bash
./run.sh
```

Or directly:
```bash
./cmake-build-Debug/scriptura
```

## Project Structure

```
scriptura/
├── src/
│   ├── rust_backend/         # Rust backend library (Cargo project)
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs         # Module tree & helpers
│   │       ├── ffi.rs         # All C FFI exports (~180 functions)
│   │       ├── eventbus.rs    # Pub/sub event system
│   │       ├── lsp/           # LSP protocol client
│   │       ├── dap/           # DAP protocol client
│   │       ├── plugin/        # Plugin manager & crash handler
│   │       ├── service_locator.rs, task_runner.rs, updater.rs, ...
│   │       └── workspace.rs, permission.rs, language_registry.rs, ...
│   ├── rust_adapter.cpp/h     # C++ wrappers bridging Qt ↔ Rust FFI
│   ├── *.cpp, *.h, *.ui       # Qt UI layer (code editor, panels, etc.)
│   ├── main.cpp               # Application entry point
│   └── ...
├── include/
│   └── scriptura/
│       ├── rust_backend.h     # C FFI header for all backend services
│       └── plugininterface.h  # Plugin SDK interface
├── docs/                      # Documentation
├── .github/workflows/         # CI/CD workflows
└── CMakeLists.txt             # CMake build system
```

## Documentation

- [How to Develop a Plugin](docs/how-to-develop-a-plugin.md)
- [How to Install a Plugin](docs/how-to-install-a-plugin.md)
- [Windows Deployment Guide](docs/windows-deployment.md)
- [Contributing Guidelines](CONTRIBUTING.md)

## CI/CD

Automated builds are available for Linux, macOS, and Windows via GitHub Actions.
The CI pipeline:
1. Installs Qt 6 and the Rust toolchain
2. Builds the Rust backend library (`cargo build --release`)
3. Builds the C++ project with CMake
4. Runs unit tests
5. Packages installers (AppImage, macOS DMG, Windows NSIS)

## Windows Installer

For Windows users, pre-built installers are available that bundle all Qt and Rust runtime dependencies,
so no manual Qt or Rust installation is required. The installer:

- Includes all necessary Qt DLLs and plugins
- Installs to Program Files by default
- Creates Start Menu shortcuts
- Supports automatic updates and uninstallation

## License

MIT License — see [LICENSE](LICENSE) file
