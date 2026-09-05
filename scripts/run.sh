#!/bin/bash

# Allow invocation from anywhere inside the repo
cd "$(dirname "$0")/.."

BUILD_DIR="cmake-build-Debug"

if [ ! -f "$BUILD_DIR/scriptura" ]; then
    echo "Building first..."
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$BUILD_DIR" -j$(nproc)
fi

"$BUILD_DIR/scriptura"
