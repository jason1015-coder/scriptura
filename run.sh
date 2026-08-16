#!/bin/bash

BUILD_DIR="cmake-build-Debug"

if [ ! -f "$BUILD_DIR/scriptura" ] && [ ! -d "$BUILD_DIR/scriptura.app" ]; then
    echo "Building first..."
    ./build.sh
fi

if [ -d "$BUILD_DIR/scriptura.app" ]; then
    open "$BUILD_DIR/scriptura.app"
else
    "$BUILD_DIR/scriptura"
fi