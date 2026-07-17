#!/bin/bash
set -e

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="cmake-build-$BUILD_TYPE"

echo "=== Building Scriptura ($BUILD_TYPE) ==="

if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTING=OFF
fi

echo "Building project..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo
echo "Build complete. Run with: ./$BUILD_DIR/scriptura"
