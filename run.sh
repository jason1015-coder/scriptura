#!/bin/bash
set -e

BUILD_DIR="cmake-build-Debug"

echo "=== Building Scriptura (Debug) ==="

# Build the Rust backend first with the same --target-dir that CMake expects.
# Note: the target dir must be resolved from the project root BEFORE cd-ing
# into src/rust_backend, otherwise cargo would emit the library into
# src/rust_backend/cmake-build-Debug and CMake would link a stale copy.
if command -v cargo &>/dev/null && [ -d src/rust_backend ]; then
    echo "Building Rust backend library..."
    mkdir -p "$BUILD_DIR/rust_backend_target"
    RUST_TARGET_DIR="$(pwd)/$BUILD_DIR/rust_backend_target"
    (cd src/rust_backend && cargo build --release --target-dir "$RUST_TARGET_DIR")
else
    echo "WARNING: Rust/Cargo not found. The Rust backend library cannot be built."
    echo "Install Rust via: curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    echo "Continuing build without Rust backend (Qt UI only)..."
fi

# Always (re)configure CMake every run, so glob changes (CONFIGURE_DEPENDS),
# CMakeLists edits, and the generated build files are always up to date.
echo "Configuring CMake..."
mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Debug

echo "Building C++ project..."
# Remove the previous executable up front so a fresh binary is ALWAYS linked
# from the latest objects — even if CMake would otherwise consider it
# up to date. Object compilation stays incremental, so this is fast.
rm -f "$BUILD_DIR/scriptura" "$BUILD_DIR/scriptura.exe"
rm -rf "$BUILD_DIR/scriptura.app"

if command -v nproc &>/dev/null; then
    cmake --build "$BUILD_DIR" -j$(nproc)
else
    cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu)
fi

echo
echo "Build complete. Launching Scriptura..."

if [ -d "$BUILD_DIR/scriptura.app" ]; then
    open "$BUILD_DIR/scriptura.app"
else
    "$BUILD_DIR/scriptura"
fi
