#!/bin/bash
set -e

# Linux deployment script for Scriptura
# This script builds the project and bundles all Qt libraries using linuxdeployqt

BUILD_DIR="build"
DEPLOY_DIR="deploy"
APPIMAGE_NAME="Scriptura.AppImage"

echo "=== Scriptura Linux Deployment ==="
echo

# Check if linuxdeployqt is installed
if ! command -v linuxdeployqt &> /dev/null; then
    echo "WARNING: linuxdeployqt not found"
    echo "Install it from: https://github.com/probonopd/linuxdeployqt"
    echo
    echo "Attempting to use standard deployment instead..."
    USE_LINUXDEPLOYQT=false
else
    USE_LINUXDEPLOYQT=true
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

# Copy the executable
echo "Copying executable..."
cp "$BUILD_DIR/scriptura" "$DEPLOY_DIR/"

if [ "$USE_LINUXDEPLOYQT" = true ]; then
    # Use linuxdeployqt for AppImage creation
    echo
    echo "Deploying Qt dependencies with linuxdeployqt..."
    cd "$DEPLOY_DIR"
    linuxdeployqt scriptura -appimage
    cd ..
    
    if [ -f "$DEPLOY_DIR/$APPIMAGE_NAME" ]; then
        echo
        echo "AppImage created: $DEPLOY_DIR/$APPIMAGE_NAME"
    fi
else
    # Manual deployment - copy Qt libraries
    echo
    echo "Deploying Qt dependencies manually..."
    
    # Find Qt libraries
    QT_LIB_DIR=$(qmake -query QT_INSTALL_LIBS)
    QT_PLUGIN_DIR=$(qmake -query QT_INSTALL_PLUGINS)
    
    # Copy required Qt libraries
    echo "Copying Qt libraries..."
    cp -P "$QT_LIB_DIR"/libQt6Core.so* "$DEPLOY_DIR/" 2>/dev/null || cp -P "$QT_LIB_DIR"/libQt5Core.so* "$DEPLOY_DIR/" 2>/dev/null || true
    cp -P "$QT_LIB_DIR"/libQt6Widgets.so* "$DEPLOY_DIR/" 2>/dev/null || cp -P "$QT_LIB_DIR"/libQt5Widgets.so* "$DEPLOY_DIR/" 2>/dev/null || true
    cp -P "$QT_LIB_DIR"/libQt6Network.so* "$DEPLOY_DIR/" 2>/dev/null || cp -P "$QT_LIB_DIR"/libQt5Network.so* "$DEPLOY_DIR/" 2>/dev/null || true
    cp -P "$QT_LIB_DIR"/libQt6Gui.so* "$DEPLOY_DIR/" 2>/dev/null || cp -P "$QT_LIB_DIR"/libQt5Gui.so* "$DEPLOY_DIR/" 2>/dev/null || true
    
    # Copy platform plugins
    echo "Copying Qt plugins..."
    mkdir -p "$DEPLOY_DIR/platforms"
    cp -r "$QT_PLUGIN_DIR/platforms/libqxcb.so" "$DEPLOY_DIR/platforms/" 2>/dev/null || true
    cp -r "$QT_PLUGIN_DIR/platforms/libqwayland-egl.so" "$DEPLOY_DIR/platforms/" 2>/dev/null || true
    
    # Create AppRun script
    echo "Creating AppRun script..."
    cat > "$DEPLOY_DIR/AppRun" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
exec "$SCRIPT_DIR/scriptura" "$@"
EOF
    chmod +x "$DEPLOY_DIR/AppRun"
fi

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
echo "Executable and dependencies are in: $DEPLOY_DIR/"
echo "Distribution archive: scriptura-linux.tar.gz"
echo