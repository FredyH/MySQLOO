#!/bin/bash
set -e

if [ -n "$1" ]; then
    PRESET="$1"
else
    ARCH=$(uname -m)
    if [[ "$ARCH" == "x86_64" ]]; then
        PRESET="linux-x64-release"
    else
        PRESET="linux-x86-release"
    fi
fi

echo "Using preset: $PRESET"

if ! grep -q "\"name\": \"$PRESET\"" CMakePresets.json; then
    echo "ERROR: Preset '$PRESET' not found in CMakePresets.json"
    echo "Available Linux presets: linux-x86-release, linux-x64-release"
    exit 1
fi

# Install vcpkg if not present
if [ ! -d "$HOME/vcpkg" ]; then
    echo "vcpkg not found. Installing..."
    git clone https://github.com/Microsoft/vcpkg.git "$HOME/vcpkg"
    "$HOME/vcpkg/bootstrap-vcpkg.sh"
fi

# Determine build directory from preset
BUILD_DIR=$(grep -A 10 "\"name\": \"$PRESET\"" CMakePresets.json | grep "binaryDir" | sed 's/.*"binaryDir": "\${sourceDir}\///' | sed 's/".*//')

# Clean any partial build artifacts
if [ -n "$BUILD_DIR" ] && [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build artifacts in $BUILD_DIR..."
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
fi

export VCPKG_ROOT="$HOME/vcpkg"

echo ""
echo "=== Building MySQLOO with preset: $PRESET ==="
echo "This may take several minutes on first run as vcpkg builds dependencies from source..."
echo ""

cmake --preset "$PRESET"
cmake --build --preset "$PRESET"

echo ""
echo "Build complete!"
