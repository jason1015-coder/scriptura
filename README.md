# Scriptura

A simple Qt-based text editor with project file browsing.


** note: this project is at early development stage which expect bugs and occasional broken features **

## Features

- **Project-based workflow**: Open a project directory to browse files
- **File tree sidebar**: Navigate project structure with clickable folders
- **Directory navigation**: "Go Up" button to navigate to parent directories
- **Tabbed editing**: Multiple files open in tabs
- **Status bar**: Shows cursor position (line/column)
- **Edit operations**: Cut, Copy, Paste, Undo, Redo
- **File management**: Create files and directories within projects
- **Delete files/directories**: Remove files or directories from the project
- **Save As**: Save files with a different name
- **Theme support**: Multiple light/dark themes available
- **Plugin Manager**: Install, remove, and manage plugins via the GUI (**Plugins → Manage Plugins...**).
- **Plugin system**: Extensible architecture with event bus, service locator, and settings support
- **Built-in Git plugin**: Version control integration for commits and pushes

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
# Debug build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Running

```bash
./run.sh
```

Or directly:
```bash
./cmake-build-Debug/scriptura
```

## Requirements

- Qt 5 or Qt 6 (with Widgets, Network, and LinguistTools modules)
- CMake 3.16+
- C++17 compiler

## Documentation

- [How to Develop a Plugin](docs/how-to-develop-a-plugin.md)
- [How to Install a Plugin](docs/how-to-install-a-plugin.md)
- [Windows Deployment Guide](WINDOWS_DEPLOYMENT.md)

## CI/CD

Automated builds are available for Linux, macOS, and Windows via GitHub Actions. See `.github/workflows/build.yml` for the workflow configuration.

## Windows Installer

For Windows users, pre-built installers are available that bundle all Qt dependencies, so no manual Qt installation is required. The installer:

- Includes all necessary Qt DLLs and plugins
- Installs to Program Files by default
- Creates Start Menu shortcuts
- Supports automatic updates and uninstallation

### Building the Windows Installer

See [WINDOWS_DEPLOYMENT.md](WINDOWS_DEPLOYMENT.md) for detailed instructions on building the Windows installer locally.


## License

MIT License - see LICENSE file
