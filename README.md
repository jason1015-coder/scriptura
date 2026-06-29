# Scriptura

![Scriptura Icon](icon.png)

[![Storage](https://img.shields.io/github/repo-size/jason1015-coder/scriptura)](https://github.com/jason1015-coder/scriptura)
[![Stars](https://img.shields.io/github/stars/jason1015-coder/scriptura)](https://github.com/jason1015-coder/scriptura/stargazers)
[![Forks](https://img.shields.io/github/forks/jason1015-coder/scriptura)](https://github.com/jason1015-coder/scriptura/network/members)


A simple Qt-based text editor with project file browsing.

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

## Building

### Quick Build (Debug)
```bash
./build.sh
```

### Build with Deployment (Release)
```bash
# Linux/macOS - builds and deploys with Qt dependencies
./build.sh Release --deploy

# Windows - builds and deploys with Qt dependencies
build.bat Release --deploy
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
./build/Desktop-Debug/scriptura
```

## Deployment

The build scripts now support automatic deployment with the `--deploy` flag. This will bundle all Qt dependencies (DLLs, libraries, plugins) automatically.

### Quick Deployment

**Linux:**
```bash
./build.sh Release --deploy
```

**macOS:**
```bash
./build.sh Release --deploy
```

**Windows:**
```batch
build.bat Release --deploy
```

### Manual Deployment

See [DEPLOYMENT.md](DEPLOYMENT.md) for detailed deployment instructions including:
- Building release versions
- Bundling Qt DLLs/libraries with windeployqt, linuxdeployqt, and macdeployqt
- Creating distributable packages (ZIP, AppImage, DMG)
- Troubleshooting common deployment issues

## Requirements

- Qt 5 or Qt 6 (with Widgets, Network, and LinguistTools modules)
- CMake 3.16+
- C++17 compiler

## CI/CD

Automated builds are available for Linux, macOS, and Windows via GitHub Actions. See `.github/workflows/build.yml` for the workflow configuration.

## Star History

<a href="https://www.star-history.com/?repos=jason1015-coder%2Fscriptura&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=jason1015-coder/scriptura&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=jason1015-coder/scriptura&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=jason1015-coder/scriptura&type=date&legend=top-left" />
 </picture>
</a>

## License

MIT License - see LICENSE file
