#!/bin/bash
set -e

# macOS deployment script for Scriptura
# This script builds the project and creates a macOS app bundle with macdeployqt

BUILD_DIR="build"
DEPLOY_DIR="deploy"
APP_NAME="Scriptura.app"

echo "=== Scriptura macOS Deployment ==="
echo

# Check if macdeployqt is installed
if ! command -v macdeployqt &> /dev/null; then
    echo "ERROR: macdeployqt not found in PATH"
    echo "Please ensure Qt is installed and macdeployqt is in your PATH"
    exit 1
fi

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

# The executable should already be in an app bundle from CMake
APP_BUNDLE="$BUILD_DIR/$APP_NAME"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: App bundle not found at $APP_BUNDLE"
    echo "CMake should have created it with MACOSX_BUNDLE property"
    exit 1
fi

# Copy the app bundle to deployment directory
echo "Copying app bundle..."
cp -R "$APP_BUNDLE" "$DEPLOY_DIR/"

# Run macdeployqt to bundle Qt frameworks and libraries
echo
echo "Deploying Qt dependencies with macdeployqt..."
macdeployqt "$DEPLOY_DIR/$APP_NAME" -always-overwrite

if [ $? -ne 0 ]; then
    echo
    echo "ERROR: macdeployqt failed"
    exit 1
fi

# Copy additional resources
echo
echo "Copying additional resources..."
cp -f icon.png "$DEPLOY_DIR/$APP_NAME/Contents/Resources/" 2>/dev/null || true
cp -f README.md "$DEPLOY_DIR/" 2>/dev/null || true

# Create DMG
echo
echo "Creating DMG..."
DMG_NAME="scriptura-macos.dmg"
hdiutil create -srcfolder "$DEPLOY_DIR/$APP_NAME" -volname "Scriptura" -fs HFS+ "$DEPLOY_DIR/$DMG_NAME" 2>/dev/null || \
hdiutil create -srcfolder "$DEPLOY_DIR/$APP_NAME" -volname "Scriptura" -fs HFS+ "$DEPLOY_DIR/$DMG_NAME"

# Create ZIP as fallback
if [ ! -f "$DEPLOY_DIR/$DMG_NAME" ]; then
    echo "DMG creation failed, creating ZIP instead..."
    cd "$DEPLOY_DIR"
    zip -r "scriptura-macos.zip" "$APP_NAME"
    cd ..
fi

echo
echo "=== Deployment Complete ==="
echo "App bundle is in: $DEPLOY_DIR/$APP_NAME"
if [ -f "$DEPLOY_DIR/$DMG_NAME" ]; then
    echo "DMG installer: $DEPLOY_DIR/$DMG_NAME"
else
    echo "ZIP archive: $DEPLOY_DIR/scriptura-macos.zip"
fi
echo