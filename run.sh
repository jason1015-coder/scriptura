#!/bin/bash
set -e

BUILD_DIR="cmake-build-Debug"

echo "=== Building Scriptura (Debug) ==="

echo "Configuring CMake..."
mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Debug

echo "Building project..."
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
