#!/bin/bash

echo "🔍 Icon Position Finder"
echo "======================"
echo ""

# Open the image in Preview
echo "Opening drawinstudioicons.png in Preview..."
open -a Preview drawinstudioicons.png

echo ""
echo "📏 How to find the correct icon positions:"
echo "1. In Preview, use Tools > Rectangular Selection"
echo "2. Select the first icon (select tool - arrow cursor)"
echo "3. Note the coordinates shown in the bottom status bar"
echo "4. Note the width and height of the selection"
echo "5. Repeat for all 10 icons"
echo ""
echo "📝 Icon order (left to right, top to bottom):"
echo "1. select_icon.png    - Arrow cursor"
echo "2. line_icon.png      - Straight line"
echo "3. curve_icon.png     - Curved line"
echo "4. bezier_icon.png    - Bezier curve"
echo "5. spline_icon.png    - Spline curve"
echo "6. rectangle_icon.png - Rectangle"
echo "7. ellipse_icon.png   - Ellipse/circle"
echo "8. eraser_icon.png    - Eraser"
echo "9. measure_icon.png   - Ruler"
echo "10. image_icon.png    - Picture frame"
echo ""
echo "💡 Common layouts to check:"
echo "- 10x1: All icons in a single horizontal row"
echo "- 5x2: 5 icons per row, 2 rows"
echo "- 2x5: 2 icons per row, 5 rows"
echo ""
echo "📐 Common icon sizes:"
echo "- 64x64 pixels"
echo "- 128x128 pixels"
echo "- 256x256 pixels"
echo ""
echo "Once you have the coordinates, edit manual_icon_extractor.py"
echo "with the correct positions and run it again."


