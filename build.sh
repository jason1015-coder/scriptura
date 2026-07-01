#!/bin/bash
set -e

# Build type: Debug or Release (default: Debug)
BUILD_TYPE="${1:-Debug}"
DEPLOY=false

# Check for --deploy flag
if [[ "$@" == *"--deploy"* ]]; then
    DEPLOY=true
fi

BUILD_DIR="cmake-build-$BUILD_TYPE"

echo "=== Building Scriptura ($BUILD_TYPE) ==="

if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

echo "Building project..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo
echo "Build complete. Run with: ./$BUILD_DIR/scriptura"

# Deploy if requested
if [ "$DEPLOY" = true ]; then
    echo
    if [ "$(uname)" == "Darwin" ]; then
        echo "=== Deploying with Qt dependencies (macOS) ==="
        echo "Running macOS deployment..."
        ./deploy-macos.sh
    elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
        echo "=== Deploying (Linux - no Qt bundling) ==="
        echo "Running Linux deployment..."
        ./deploy-linux.sh
    else
        echo "=== Deploying with Qt dependencies ==="
        echo "WARNING: Automated deployment not supported on this platform"
        echo "Please use the appropriate deploy script manually:"
        echo "  Linux:   ./deploy-linux.sh"
        echo "  macOS:   ./deploy-macos.sh"
        echo "  Windows: deploy-windows.bat"
    fi
else
    echo
    echo "To deploy, run:"
    echo "  ./build.sh $BUILD_TYPE --deploy"
    echo
    echo "Or use platform-specific deploy scripts:"
    echo "  Linux:   ./deploy-linux.sh"
    echo "  macOS:   ./deploy-macos.sh"
    echo "  Windows: deploy-windows.bat"
fi
