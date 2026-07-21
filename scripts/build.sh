#!/bin/bash
set -e

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="cmake-build-$BUILD_TYPE"

echo "=== Building Scriptura ($BUILD_TYPE) ==="

# Check for Rust toolchain (required for backend)
if ! command -v cargo &>/dev/null; then
    echo "WARNING: Rust/Cargo not found. The Rust backend library cannot be built."
    echo "Install Rust via: curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    echo "Continuing build without Rust backend (Qt UI only)..."
fi

# Build Rust backend first if cargo is available
if command -v cargo &>/dev/null && [ -d "$(dirname "$0")/../src/rust_backend" ]; then
    echo "Building Rust backend library..."
    (cd "$(dirname "$0")/../src/rust_backend" && cargo build --release) || echo "Rust build failed, continuing with C++ only"
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTING=OFF
fi

echo "Building C++ project..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo
echo "Build complete. Run with: ./$BUILD_DIR/scriptura"
