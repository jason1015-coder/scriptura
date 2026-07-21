# Windows Deployment Guide

This guide explains how to build and package Scriptura for Windows with all Qt and Rust dependencies bundled, so users don't need to manually install Qt or the Rust runtime.

## Prerequisites

### For Building on Windows

1. **Visual Studio 2022** (with C++ desktop development workload) or **MSVC Build Tools**
2. **CMake 3.16+** — [Download](https://cmake.org/download/)
3. **Qt 6** (with Widgets, Network, Sql, LinguistTools modules) — [Download](https://www.qt.io/download)
4. **Rust toolchain** — Install via [rustup](https://rustup.rs/):
   ```batch
   curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```
5. **Git** — [Download](https://git-scm.com/download/win)
6. **NSIS** (for creating installers) — [Download](https://nsis.sourceforge.io/Download)

### For Building on Linux/macOS (cross-compilation)

Cross-compilation for Windows is complex. It's recommended to build on Windows using GitHub Actions or a Windows machine.

## Quick Start

### Option 1: Using the Deployment Script (Recommended)

1. Open **x64 Native Tools Command Prompt for VS 2022** (or similar MSVC command prompt)
2. Navigate to the Scriptura directory
3. Build the Rust backend first:
   ```batch
   cd src\rust_backend
   cargo build --release
   cd ..\..
   ```
4. Run the deployment script:
   ```batch
   deploy-windows.bat Release
   ```

This will:
- Build the C++ application in Release mode
- Use `windeployqt` to copy all Qt dependencies
- Create a `deploy\Release` folder with all necessary files

### Option 2: Manual Build and Deploy

```batch
REM Build Rust backend
cd src\rust_backend
cargo build --release
cd ..\..

REM Configure C++ build
cmake -B build -S . -A x64 -DCMAKE_BUILD_TYPE=Release

REM Build C++ project
cmake --build build --config Release -j

REM Deploy Qt dependencies
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw build\Release\scriptura.exe

REM The executable and all dependencies are now in build\Release\
```

## Creating an Installer

### Using the Packaging Script

After running `deploy-windows.bat`, create the installer:

```batch
package-windows.bat Release
```

This will:
- Copy all deployment files to a package directory
- Generate the NSIS installer script with the correct version
- Build `Scriptura-Setup.exe`

### Manual NSIS Build

```batch
REM Copy deployment files
mkdir deploy\Release
copy build\Release\scriptura.exe deploy\Release\
xcopy /E /I /Y build\Release\*.dll deploy\Release\
xcopy /E /I /Y build\Release\platforms deploy\Release\
xcopy /E /I /Y build\Release\styles deploy\Release\
xcopy /E /I /Y build\Release\imageformats deploy\Release\
if exist build\Release\plugins xcopy /E /I /Y build\Release\plugins deploy\Release\plugins
if exist resources xcopy /E /I /Y resources deploy\Release\resources
if exist icon.png copy icon.png deploy\Release\

REM Build installer
cd deploy\Release
makensis ..\..\scriptura.nsi
```

## What Gets Bundled

The `windeployqt` tool automatically copies:

- **Qt Core DLLs**: Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, etc.
- **Platform plugins**: platforms/qwindows.dll
- **Styles**: Generic, Fusion, etc.
- **Image formats**: JPEG, PNG, GIF, etc.
- **SQL drivers**: SQLite, etc.
- **Network SSL libraries**: OpenSSL DLLs (if used)
- **Microsoft Visual C++ Redistributable**: Required runtime libraries

The Rust backend is compiled **statically** into `scriptura.exe` via `libscriptura_backend.lib`,
so no separate Rust runtime DLLs are needed. The Rust standard library is linked statically
by default when building with `cargo build --release`.

## Installer Features

The NSIS installer (`Scriptura-Setup.exe`) provides:

- **Installation to Program Files** (default: `C:\Program Files\Scriptura`)
- **Start Menu shortcuts** for Scriptura and Uninstaller
- **Automatic uninstallation** via Windows Add/Remove Programs
- **Version detection** from git tags
- **Upgrade support** — detects existing installations

## Portable Version

For a portable version (no installation required):

1. Build the Rust backend: `cd src\rust_backend && cargo build --release`
2. Build the C++ project: `cmake --build build --config Release -j`
3. Run `windeployqt build\Release\scriptura.exe`
4. Zip the contents of `build\Release\`
5. Users can extract and run `scriptura.exe` directly

## Troubleshooting

### "Qt5Core.dll not found" or similar errors

- Ensure `windeployqt` was run successfully
- Check that all Qt DLLs are in the same directory as `scriptura.exe`
- Verify that the `platforms` folder exists with `qwindows.dll`

### "MSVCP140.dll not found"

- Install the [Microsoft Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- Or include the redistributable in your installer

### Application crashes on startup

- Run from command prompt to see error messages
- Check that all plugins are in the correct `plugins` subdirectory
- Verify Qt plugin paths are correct
- If the Rust backend fails, run `rust_last_error()` from a debugger to get the error message

## GitHub Actions CI/CD

The project includes a GitHub Actions workflow (`.github/workflows/build.yml`) that automatically:

1. Installs the Rust toolchain via `dtolnay/rust-toolchain`
2. Builds the Rust backend library (`cargo build --release`)
3. Builds Scriptura for Windows on every push
4. Deploys Qt dependencies using `windeployqt`
5. Creates an NSIS installer
6. Uploads both the installer and portable version as artifacts

To enable automated builds:
1. Push your code to GitHub
2. Go to the "Actions" tab in your repository
3. Download the artifacts from the latest Windows build

## Customization

### Changing the Application Icon

1. Convert your icon to `.ico` format (multiple sizes: 16x16, 32x32, 48x48, 256x256)
2. Save as `icon.ico` in the project root
3. The NSIS script will automatically use it

### Changing Installer Settings

Edit `scriptura.nsi` to customize:
- Application name and version
- Installation directory
- Registry keys
- Shortcuts
- License text

### Adding More Qt Plugins

If your application uses additional Qt plugins (e.g., database drivers), copy them to the deployment directory:

```batch
xcopy /E /I /Y build\Release\qsqlite deploy\Release\
```

## Dependencies Summary

| Component | Purpose | Bundled? |
|-----------|---------|----------|
| Qt6Core.dll | Core Qt functionality | Yes |
| Qt6Gui.dll | GUI functionality | Yes |
| Qt6Widgets.dll | Widgets | Yes |
| Qt6Network.dll | Networking | Yes |
| Qt6Sql.dll | SQL database support | Yes |
| platforms/qwindows.dll | Windows platform plugin | Yes |
| scriptura_backend.lib | Rust backend (statically linked) | Compiled into executable |
| MSVC Runtime | C++ runtime library | Yes (via windeployqt) |
| OpenSSL | HTTPS support | If used |

## License

The installer is created using NSIS, which is licensed under the zlib/libpng license.
