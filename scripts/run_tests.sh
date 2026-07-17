#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/cmake-build-tests"

echo "==> Configuring tests..."
cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON

echo "==> Building tests..."
cmake --build "$BUILD_DIR" --target scriptura_tests -j"$(nproc)"

echo "==> Running unit tests..."
cd "$BUILD_DIR"
ctest --output-on-failure

echo "==> Done."
