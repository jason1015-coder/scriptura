#!/bin/bash
set -e

# Linux deployment script for Scriptura
# This script builds the project and packages the executable
# Linux does not bundle Qt libraries - users should install Qt via their package manager

BUILD_DIR="build"
DEPLOY_DIR="deploy"

echo "=== Scriptura Linux Deployment ==="
echo

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release
fi

# Build the project
echo
echo "Building project..."
cmake --build "$BUILD_DIR" --config Release

if [ $? -ne 0 ]; then
    echo
    echo "ERROR: Build failed"
    exit 1
fi

# Create deployment directory
echo
echo "Creating deployment directory..."
if [ -d "$DEPLOY_DIR" ]; then
    rm -rf "$DEPLOY_DIR"
fi
mkdir -p "$DEPLOY_DIR"

# Copy the executable
echo "Copying executable..."
cp "$BUILD_DIR/scriptura" "$DEPLOY_DIR/"

# Copy additional resources
echo
echo "Copying additional resources..."
cp -f icon.png "$DEPLOY_DIR/" 2>/dev/null || true
cp -f scriptura.desktop "$DEPLOY_DIR/" 2>/dev/null || true
cp -f README.md "$DEPLOY_DIR/" 2>/dev/null || true

# Create tarball
echo
echo "Creating tarball..."
tar -czf scriptura-linux.tar.gz -C "$DEPLOY_DIR" .

echo
echo "=== Deployment Complete ==="
echo "Executable is in: $DEPLOY_DIR/"
echo "Distribution archive: scriptura-linux.tar.gz"
echo
echo "Note: Linux does not bundle Qt libraries."
echo "Ensure Qt6 (Core, Widgets, Network, Gui) is installed on the target system."
