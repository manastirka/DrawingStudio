#!/bin/bash

echo "🎸 Guitar Builder App Setup Script"
echo "=================================="

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "❌ This script is designed for macOS. Please adapt for your platform."
    exit 1
fi

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew is required but not installed. Please install Homebrew first:"
    echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

echo "📦 Installing dependencies..."

# Install Qt6
if ! brew list qt6 &> /dev/null; then
    echo "   Installing Qt6..."
    brew install qt6
else
    echo "   ✅ Qt6 already installed"
fi

# Install CMake
if ! brew list cmake &> /dev/null; then
    echo "   Installing CMake..."
    brew install cmake
else
    echo "   ✅ CMake already installed"
fi

# Install vcpkg (optional, for additional dependencies)
if ! brew list vcpkg &> /dev/null; then
    echo "   Installing vcpkg..."
    brew install vcpkg
else
    echo "   ✅ vcpkg already installed"
fi

# Set up Qt6 environment
echo "🔧 Setting up Qt6 environment..."
QT6_PATH=$(brew --prefix qt6)
export CMAKE_PREFIX_PATH="$QT6_PATH:$CMAKE_PREFIX_PATH"
export PATH="$QT6_PATH/bin:$PATH"

echo "   Qt6 installed at: $QT6_PATH"
echo "   Add this to your ~/.zshrc or ~/.bashrc:"
echo "   export CMAKE_PREFIX_PATH=\"$QT6_PATH:\$CMAKE_PREFIX_PATH\""
echo "   export PATH=\"$QT6_PATH/bin:\$PATH\""

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build

# Generate build files
echo "🔨 Generating build files..."
cd build
cmake -DCMAKE_PREFIX_PATH="$QT6_PATH" .. || {
    echo "❌ CMake configuration failed!"
    echo "   Make sure Qt6 is properly installed and try again."
    exit 1
}

echo ""
echo "✅ Setup complete! To build the project:"
echo "   cd build"
echo "   make -j\$(nproc)"
echo ""
echo "🚀 To get started with development:"
echo "   1. Implement the basic MainWindow class"
echo "   2. Set up the drawing canvas"
echo "   3. Add menu system"
echo ""
echo "📚 See DEVELOPMENT_PLAN.md for detailed implementation guide"