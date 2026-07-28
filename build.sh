#!/bin/bash
set -e

# Build type: Debug or Release (default: Debug)
BUILD_TYPE="${1:-Debug}"

BUILD_DIR="cmake-build-$BUILD_TYPE"

echo "=== Building Scriptura ($BUILD_TYPE) ==="

# Check for Rust toolchain (required for backend)
if ! command -v cargo &>/dev/null; then
    echo "WARNING: Rust/Cargo not found. The Rust backend library cannot be built."
    echo "Install Rust via: curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    echo "Continuing build without Rust backend (Qt UI only)..."
fi

# Build Rust backend with the same --target-dir that CMake expects
if command -v cargo &>/dev/null && [ -d src/rust_backend ]; then
    echo "Building Rust backend library..."
    mkdir -p "$BUILD_DIR/rust_backend_target"
    (cd src/rust_backend && cargo build --release --target-dir "$(pwd)/$BUILD_DIR/rust_backend_target") || echo "Rust build failed, continuing with C++ only"
fi

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

echo "Building C++ project..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo
echo "Build complete. Run with: ./$BUILD_DIR/scriptura"
