#!/bin/bash
set -e

BUILD_DIR="cmake-build-Debug"

# Always rebuild before running. A binary left over from a previous run is
# stale the moment any source changes, and launching it makes fixes appear to
# "not work". build.sh is incremental, so this stays fast.
./build.sh Debug

if [ -d "$BUILD_DIR/scriptura.app" ]; then
    open "$BUILD_DIR/scriptura.app"
else
    "$BUILD_DIR/scriptura"
fi