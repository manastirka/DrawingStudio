#!/bin/bash

# Installation script for Tesseract OCR on macOS

echo "======================================"
echo "Installing Tesseract OCR for DrawingStudio"
echo "======================================"
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew not found!"
    echo "Please install Homebrew first: https://brew.sh"
    exit 1
fi

echo "✓ Homebrew found"
echo ""

# Install Tesseract
echo "📦 Installing Tesseract OCR..."
brew install tesseract

# Install language packs
echo ""
echo "📦 Installing language packs..."
echo "Installing: English, Spanish, French, German, Italian, Portuguese"
brew install tesseract-lang

# Verify installation
echo ""
echo "🔍 Verifying installation..."
if command -v tesseract &> /dev/null; then
    echo "✓ Tesseract installed successfully!"
    echo "Version: $(tesseract --version | head -n 1)"
    echo ""
    echo "📍 Tessdata location:"
    TESSDATA_PREFIX=$(brew --prefix)/share/tessdata
    echo "   $TESSDATA_PREFIX"
    echo ""
    echo "📋 Available languages:"
    tesseract --list-langs 2>&1 | grep -v "List of available languages"
else
    echo "❌ Installation failed"
    exit 1
fi

echo ""
echo "======================================"
echo "✅ Installation complete!"
echo "======================================"
echo ""
echo "Next steps:"
echo "1. Rebuild DrawingStudio:"
echo "   cd /Users/Lukovic/Apps/DrawingStudio/build"
echo "   cmake .."
echo "   make"
echo ""
echo "2. Run DrawingStudio and try OCR features!"
echo ""
