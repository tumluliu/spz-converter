#!/bin/bash

# SPZ Installation Script
# Builds the SPZ library and command-line tools

set -e

echo "Building SPZ library and tools..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build completed successfully!"
echo ""
echo "Command-line tool location: $(pwd)/spz-convert"
echo ""
echo "Usage examples:"
echo "  $(pwd)/spz-convert input.ply output.spz"
echo "  $(pwd)/spz-convert input.splat output.spz"
echo "  $(pwd)/spz-convert --help"
echo ""

# Ask if user wants to install system-wide
read -p "Install spz-convert to system PATH? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Installing to system..."
    sudo make install
    echo "Installation completed! You can now use 'spz-convert' from anywhere."
else
    echo "Skipping system installation."
    echo "To use the tool, run: $(pwd)/spz-convert"
fi

echo ""
echo "Done!" 