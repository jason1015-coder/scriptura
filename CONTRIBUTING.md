# Contributing Guidelines

First off, thank you for considering contributing to this project! It's people like you that make the open-source community such an amazing place to learn, inspire, and create.

Please take a moment to review this document to make the contribution process easy and effective for everyone involved.

## Code of Conduct

This project and everyone participating in it is governed by our Code of Conduct. By participating, you are expected to uphold this code. Please report unacceptable behavior to the project maintainers.

---

## How Can I Contribute?

### Reporting Bugs

Before creating a bug report, please check the existing issues to see if the problem has already been reported.

When creating a bug report, please include as many details as possible:

- **Use a clear and descriptive title.**
- **Describe the exact steps to reproduce the problem.**
- **Provide specific examples** (e.g., code snippets or screenshots) to demonstrate the steps.
- **Describe the behavior you observed** after following the steps and point out what exactly is the problem with that behavior.
- **Explain which behavior you expected to see instead and why.**
- **Include environment details:** Mention your OS, Qt version, Rust version (`rustc --version`), and compiler.

### Suggesting Enhancements

If you have an idea to improve the project, feel free to submit an issue.

When submitting an enhancement suggestion, please:

- **Use a clear and descriptive title.**
- **Provide a step-by-step description** of the suggested feature or enhancement.
- **Provide specific examples** of how the feature would work or how it would benefit the project.
- **Explain why this enhancement would be useful** to most users.

---

## Development Setup

### Prerequisites

1. **Qt 6** (or Qt 5) — with Widgets, Network, Sql, and LinguistTools modules
2. **CMake 3.16+**
3. **C++17 compiler** — GCC, Clang, or MSVC
4. **Rust toolchain** — Install via [rustup](https://rustup.rs/):
   ```bash
   curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

### Building

```bash
# Quick build & run (Debug — Rust backend + C++ UI)
./run.sh

# Or build a Release build manually (CMake also builds the Rust backend):
cmake -B cmake-build-Release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-Release -j$(nproc)
```

### Code Style

#### C++ (Qt UI Layer)
- Follow the project's existing code conventions and formatting rules
- Use Qt coding style (PascalCase for classes, camelCase for methods)
- Keep UI code focused on presentation — call Rust adapters for business logic
- All `Q_OBJECT` classes should be minimal wrappers that bridge to Rust via FFI

#### Rust (Backend Layer)
- Follow standard Rust conventions (`rustfmt`, `clippy`)
- Run `cargo fmt` and `cargo clippy` before committing Rust changes
- All `extern "C"` FFI functions must be `unsafe` internally (safe wrapper on Rust side)
- Use `serde_json` for cross-language data serialization (JSON strings over FFI)
- New backend features should be added as separate modules under `src/rust_backend/src/`
- Each module should expose its type via `pub struct` and FFI via `#[no_mangle]` in `ffi.rs`
- Avoid `unwrap()` in production paths — prefer `?` or `.unwrap_or_default()`
- Use `CString::as_ptr()` (scoped) rather than `CString::into_raw()` (leaked) for FFI strings

### Pull Requests

To ensure a smooth review process, please follow these guidelines when submitting code:

1. **Fork the repository** and create your branch from `main` (or `master`).
2. **Install dependencies** and ensure the project builds locally:
   ```bash
   cd src/rust_backend && cargo check && cd ../..
   cmake -B build -S . && cmake --build build -j$(nproc)
   ```
3. **Make your changes:**
   - Keep your commits small, logical, and well-described.
   - Update or add documentation where relevant.
   - Add tests for new features or bug fixes, if applicable.
   - For Rust changes, add unit tests in the module.
   - For C++ adapter changes, add tests in `tests/`.

4. **Run tests:**
   ```bash
   # Rust tests
   cd src/rust_backend && cargo test && cd ../..
   
   # C++ tests (requires full CMake build)
   cd build && ctest --output-on-failure && cd ..
   ```

5. **Open a Pull Request (PR):**
   - Reference any related issue(s) in the PR description (e.g., `Fixes #123`).
   - Provide a clear description of the changes made.
   - Wait for a maintainer to review your code. Be open to feedback.

### Commit Messages

- Use the imperative mood (e.g., `"Add feature"` instead of `"Added feature"` or `"Adds feature"`).
- Limit the first line to 50 characters or less.
- Reference issues and pull requests liberally after the first line.
- For cross-language changes, prefix with `[rust]`, `[cpp]`, or `[build]`.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│  C++ Qt UI Layer (codeeditor, panels, widgets)  │
│         │ emit signals               │ call      │
│         ▼                           ▼           │
│  ┌─────────────────────────────────────────┐     │
│  │  C++ Adapter Layer (rust_adapter.h/cpp) │     │
│  │  QObject wrappers ↔ C callbacks ↔ FFI   │     │
│  └─────────────┬───────────────────────────┘     │
│                │ extern "C"                       │
│                ▼                                  │
│  ┌─────────────────────────────────────────┐     │
│  │  Rust Backend Library (src/rust_backend/)│     │
│  │  ffi.rs → eventbus, lsp, dap, plugin,   │     │
│  │  taskrunner, workspace, updater, ...     │     │
│  └─────────────────────────────────────────┘     │
└─────────────────────────────────────────────────┘
```

The Rust backend is compiled into a static library (`libscriptura_backend.a`) via Cargo.
All complex data crosses the FFI boundary as JSON strings via `const char*`.
Callbacks use C function pointers, dispatched to Qt's event loop via `QMetaObject::invokeMethod`.

---

Thank you for contributing! 🎉
