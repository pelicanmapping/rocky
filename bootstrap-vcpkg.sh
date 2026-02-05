#!/bin/bash

# Exit on error
set -e

ERROR_MSG=""

# Try to find VCPKG_DIR if it's not set
if [ -z "$VCPKG_DIR" ]; then
    VCPKG_PATH=$(which vcpkg 2>/dev/null || true)
    if [ -n "$VCPKG_PATH" ]; then
        VCPKG_DIR=$(dirname "$VCPKG_PATH")
    fi
fi

# Verify vcpkg is available
VCPKG_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"

if [ ! -f "$VCPKG_TOOLCHAIN_FILE" ]; then
    ERROR_MSG="Please set the VCPKG_DIR environment variable to your vcpkg install location"
    echo "Error: $ERROR_MSG"
    echo "Usage: $0 -S source_folder -B build_folder [-G generator]"
    echo "Example:"
    echo "   $0 -S . -B ../build -I ../install -G \"Unix Makefiles\""
    exit 1
fi

# Default values
SOURCE_DIR=.
KERNEL=`uname -s`
BUILD_DIR="../rocky-build-$KERNEL"
GENERATOR="Unix Makefiles"

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -S)
            SOURCE_DIR="$2"
            shift 2
            ;;
        -B)
            BUILD_DIR="$2"
            shift 2
            ;;
        -G)
            GENERATOR="$2"
            shift 2
            ;;
        *)
            echo "Error: Invalid option $1"
            echo "Usage: $0 -S source_folder -B build_folder [-G generator]"
            exit 1
            ;;
    esac
done

# Convert to absolute paths
SOURCE_DIR=$(realpath "$SOURCE_DIR")
BUILD_DIR=$(realpath "$BUILD_DIR")

# Display configuration
echo "Source location  = $SOURCE_DIR"
echo "Build location   = $BUILD_DIR"
echo "Generator        = $GENERATOR"

# Ask for confirmation
read -p "Continue? [Y/n] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]] && [[ -n $REPLY ]]; then
    echo "Aborted."
    exit 0
fi

# Verify vcpkg.json exists
if [ ! -f "vcpkg.json" ]; then
    echo "Error: No vcpkg.json manifest found. Run this script from the root folder of the git repository."
    exit 1
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Remove old CMake cache if it exists
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    rm "$BUILD_DIR/CMakeCache.txt"
fi

echo \
cmake \
    -S "$SOURCE_DIR" \
    -B "$BUILD_DIR" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE" \
    -DVCPKG_MANIFEST_DIR="$SOURCE_DIR"
echo

# Run CMake
cmake \
    -S "$SOURCE_DIR" \
    -B "$BUILD_DIR" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE"

echo "CMake configuration complete!"
