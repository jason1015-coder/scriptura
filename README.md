# Scriptura

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

```bash
./build.sh
```

Or manually:
```bash
cmake -B build/Desktop-Debug -S .
cmake --build build/Desktop-Debug
```

## Running

```bash
./run.sh
```

Or directly:
```bash
./build/Desktop-Debug/scriptura
```

## Requirements

- Qt 5 or Qt 6 (with Widgets module)
- CMake 3.16+
- C++17 compiler

## CI/CD

Automated builds are available for Linux, macOS, and Windows via GitHub Actions. See `.github/workflows/build.yml` for the workflow configuration.

## License

MIT License - see LICENSE file

## Alpha Stage Checklist

- [x] Application packaging (AppImage / DMG / NSIS installer / Snap)
- [x] Project auto-discovery / recent projects on welcome screen
- [x] Open/Save file dialog native file format filters
- [x] Auto-save / crash recovery
- [x] Syntax highlighting usage/configuration
- [x] UX refinements (animations, keyboard shortcuts cheatsheet)
- [x] Runtime crash / minidump reporting
- [x] Confirm exit prompt
- [x] OS-level file association/desktop integration

## Packaging

### Linux (AppImage)
```bash
# Build and create AppImage
./build.sh
linuxdeployqt build/Desktop-Release/scriptura -appimage
```

### Linux (Snap)
```bash
# Build snap package
cd snap
snapcraft
```

### macOS (DMG)
```bash
# Build and create DMG
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
macdeployqt build/scriptura.app -dmg
```

### Windows (Installer)
```cmd
# Build and deploy
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -A x64
cmake --build build --config Release
windeployqt --no-translations --compiler-runtime build/Release/scriptura.exe
```

Automated releases are created via GitHub Actions when pushing tags (e.g., `git tag v0.1.0 && git push --tags`).