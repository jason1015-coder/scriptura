# Contributing to Scriptura

This file is the project-specific half of contributing to Scriptura. Collective-level terms — the Code of Conduct, how contributor compensation works — are linked below and deliberately not restated here.

If you are new to the Nano Collective, read [Contributing](https://docs.nanocollective.org/collective/projects/contributing) first. It covers finding work, the two contribution modes, and what review looks like. Where that page and this one differ, this one wins for Scriptura.

The most useful work right now is the Rust ↔ Nanocoder seam (an ACP client in the Rust backend against Nanocoder's existing ACP server) and the testing pass that has to accompany it. Both are specified in the [Scriptura whitepaper](https://docs.nanocollective.org/collective/whitepapers/scriptura) under Must-do(s).

## Before you start

Scriptura is pre-1.0 and its AI layer is an integration into the Qt shell rather than a plugin. That boundary is a settled design decision, not an oversight, and changes that cross it need an issue before the code.

- **Small, self-contained fixes**: open the PR. Bug fixes, typo corrections, dependency bumps, and behaviour-preserving refactors do not need to be discussed first.
- **Anything that changes architecture, the plugin surface, the FFI boundary, or the AI layer's position in the shell**: open an issue describing what you want to change and why before you write it. This includes new third-party dependencies, new panels, and new FFI exports.
- **Claim the issue** by commenting on it, so two people do not write the same patch.

Every NC project tags `good first issue` and `help wanted`. The dormant panels listed under "Partially implemented" in the [README](README.md) are a reasonable place to start: they are complete, tested classes that were never wired in, which makes them small, self-contained, and genuinely useful.

## Prerequisites

- **Qt 6** — Widgets, Network, Sql, and LinguistTools modules
- **CMake 3.16+**
- **C++17 compiler** (GCC, Clang, or MSVC)
- **Rust toolchain** — install via [rustup](https://rustup.rs):

```bash
curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

CI builds on Linux, macOS, and Windows. A change that only builds on one of them will be caught, so check the others before pushing if you touch anything platform-adjacent — `QSettings` paths, process invocation, or the custom title bar.

## Build and run

```bash
./run.sh
```

`run.sh` builds the Rust backend with the `--target-dir` CMake expects, re-runs CMake configure, and launches the app. It always re-configures and always relinks the executable, so a stale binary is not something you have to think about.

Release build:

```bash
cmake -B cmake-build-Release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-Release -j$(nproc)
```

## Running the tests

This is the gate CI runs. Run it locally before opening a PR.

```bash
# Rust backend
cd src/rust_backend && cargo test && cd ../..

# C++/Qt layer
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --config Debug -j$(nproc) --target scriptura_tests
QT_QPA_PLATFORM=offscreen ./build/tests/scriptura_tests
cd build && ctest --output-on-failure --timeout 300
```

`QT_QPA_PLATFORM=offscreen` is what lets the suite run headless on CI. Use it locally too unless you specifically need to watch a widget.

**On Windows, configure the test build as Release, not Debug.** A Debug build selects Qt's pre-built debug libraries, which are compiled against the debug CRT, while this project's C++ code and the Rust release staticlib use the release CRT. Mixing the two puts two heap managers in one process and the test binary dies with `STATUS_HEAP_CORRUPTION` shortly after startup. Release keeps everything on one CRT.

```powershell
cmake -B build -S . -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --target scriptura_tests
cd build; ctest --output-on-failure -C Release --timeout 300
```

## Writing tests

- **Rust**: `#[test]` functions live inline in the module they cover, under `src/rust_backend/src/`. There is no separate `tests/` directory.
- **C++**: add `tests/test_<name>.cpp` and `tests/test_<name>.h`. `tests/CMakeLists.txt` globs `test_*.cpp` recursively, so no CMake edit is needed — but CMake only re-globs on configure, so make sure a configure has run (`./run.sh` always does one).
- **When you change behaviour, add or update a test.** `ctest` registers a single target, `scriptura_unit_tests`, which runs the whole `scriptura_tests` binary.
- **Say what your test does not cover.** Several components in this repo are complete but not wired into `MainWindow`, so a passing test on one of them says nothing about the user-facing path. The README's "Partially implemented" section is the current list; if your change moves something off it, update that section in the same PR.

## Coding standards

No formatter is enforced in CI and the tree ships no `.clang-format` or `rustfmt.toml`, so match the surrounding code rather than reformatting what you did not touch. `cargo fmt` and `cargo clippy` are available if you want them.

The conventions that actually matter:

- **UI lives in C++/Qt.** Visual components, panels, dialogs, and menus belong in the Qt layer. Do not move UI into the Rust backend.
- **The Rust backend does not link Qt.** It is pure Rust reached through `extern "C"` in `src/rust_backend/src/ffi.rs`, with JSON strings for structured payloads. Keep the FFI boundary narrow — a new export needs a matching declaration in `include/scriptura/rust_backend.h` and, where the UI needs it, a wrapper in `src/rust_adapter.cpp`.
- **Network access is declared, not assumed.** A plugin that reaches the network declares `network.access` in its manifest. Do not add a code path that opens a socket without one.
- **New panels need wiring.** A panel class that is never `new`'d in `MainWindow` and never handed to `MainWindow::registerPanel()` is dormant code, however complete it is. `registerPanel()` is what gives the panel its tab in the tab bar (and its entry in the tab bar's "+" menu) and makes it a page of the content area. Wire yours, or say in the PR that it is a staged extraction.
- **Third-party dependencies need an issue first.** Scriptura is a desktop application; every dependency is a build requirement imposed on every user and every CI runner.

## Commit messages

Short subject lines, matching the existing history: lowercase or capitalised, imperative or noun phrase.

```
tab fixes
joining NC filetree resizing
fix test failed
```

Scope any larger change into a series of commits that each build and each make sense on their own.

## Pull requests

- One change per PR. If you have unrelated improvements, split them.
- Explain what you did **and what you did not do** — skipped tests, `TODO`s left in place, workarounds you did not want to leave in. The repository's status sections are meant to be honest, and a PR that overstates its coverage makes those sections wrong.
- If you used an AI tool to write part of the change, say so in the description. It is not disqualifying and nobody is asked to justify it; it is information the reviewer needs.
- If your change affects user-visible behaviour, update the [README](README.md) in the same PR. Its opening, the project description, and the status sections follow the [Nano Collective brand guidelines](https://docs.nanocollective.org/collective/organisation/brand) — project name as the H1, the canonical tagline, and shipped work in the present tense with planned work marked as planned.

## Releases

Contributors do not bump versions, tag, or cut a release. That is a maintainer's job, and your change is picked up by the next release once it is merged.

## Code of conduct

Everyone participating in this project is expected to follow the Nano Collective [Code of Conduct](https://docs.nanocollective.org/collective/organisation/community). To report a problem, reach a maintainer on [Discord](https://discord.gg/ktPDV6rekE) or email hello@nanocollective.org.

## Contributor compensation

Most contribution here is volunteer, and the collective does not have a bounty posted for this project today. If you are taking on a substantial piece of work and want to ask whether support is available, see [Contributor Resources](https://docs.nanocollective.org/collective/organisation/contributor-resources).

Any question about scope, rates, or payment terms belongs to the [Economics Charter](https://docs.nanocollective.org/collective/organisation/economics-charter), which is the only place those terms are defined. Read it before agreeing to any paid work.

## Questions

[Discord](https://discord.gg/ktPDV6rekE) is the fastest path. For anything more formal or non-public, email hello@nanocollective.org.

- **The collective**: [nanocollective.org](https://nanocollective.org) · [docs](https://docs.nanocollective.org) · [GitHub](https://github.com/Nano-Collective) · [Discord](https://discord.gg/ktPDV6rekE)
