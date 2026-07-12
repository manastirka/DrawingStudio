#!/bin/bash

# Icon Extraction Helper Script for macOS
# This script opens Preview with your icon image and provides guidance

echo "🎨 DrawingStudio Icon Extraction Helper"
echo "======================================="
echo ""

# Check if the source image exists
if [ ! -f "drawinstudioicons.png" ]; then
    echo "❌ Error: drawinstudioicons.png not found!"
    echo "Please make sure the image file is in the current directory."
    exit 1
fi

# Create the icons directory if it doesn't exist
mkdir -p resources/icons

echo "✅ Found drawinstudioicons.png"
echo "📁 Created resources/icons/ directory"
echo ""

echo "🖼️  Opening image in Preview..."
echo ""
echo "📋 Manual Extraction Steps:"
echo "1. In Preview, use Tools > Rectangular Selection"
echo "2. Select each icon area (32x32 pixels recommended)"
echo "3. Copy (Cmd+C) the selection"
echo "4. Create new document (Cmd+N)"
echo "5. Save as PNG with these exact filenames:"
echo ""

# List the required icon files
icon_files=(
    "select_icon.png"
    "line_icon.png" 
    "curve_icon.png"
    "bezier_icon.png"
    "spline_icon.png"
    "rectangle_icon.png"
    "ellipse_icon.png"
    "eraser_icon.png"
    "measure_icon.png"
    "image_icon.png"
)

echo "📝 Required icon files:"
for i in "${!icon_files[@]}"; do
    echo "   $((i+1)). ${icon_files[$i]}"
done

echo ""
echo "💡 Tips:"
echo "   - Save all files in: resources/icons/"
echo "   - Use PNG format"
echo "   - Make icons 32x32 pixels for best results"
echo "   - Ensure icons are clear and recognizable"
echo ""

echo "🚀 After extracting all icons, run:"
echo "   cd build && make -j4 && ./DrawingStudio"
echo ""

# Open the image in Preview
open -a Preview drawinstudioicons.png

echo "🎯 Preview opened with your icon image!"
echo "   Follow the steps above to extract each icon."
echo ""
echo "📖 For detailed instructions, see: MANUAL_ICON_EXTRACTION.md"


