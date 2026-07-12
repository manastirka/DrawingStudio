#!/usr/bin/env python3
"""
Manual Icon Extractor
This script helps you manually specify the exact positions of each icon
"""

import os
from PIL import Image
import sys

def extract_icon_at_position(image_path, output_dir, icon_name, x, y, width, height):
    """
    Extract a single icon at specific coordinates
    """
    try:
        with Image.open(image_path) as img:
            print(f"Extracting {icon_name} from ({x}, {y}) size {width}x{height}")
            
            # Crop the icon
            icon_box = (x, y, x + width, y + height)
            icon = img.crop(icon_box)
            
            # Save the icon
            output_path = os.path.join(output_dir, icon_name)
            icon.save(output_path)
            print(f"Saved: {output_path}")
            return True
            
    except Exception as e:
        print(f"Error extracting {icon_name}: {e}")
        return False

def main():
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        return
    
    print("🎨 Manual Icon Extractor")
    print("=" * 30)
    print(f"Source image: {image_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Get image dimensions
    with Image.open(image_path) as img:
        print(f"Image size: {img.size[0]}x{img.size[1]} pixels")
        print()
    
    print("To extract icons manually, you need to specify the position and size")
    print("of each icon in your image. Here are some common layouts:")
    print()
    print("Common icon sizes:")
    print("- 64x64 pixels (small)")
    print("- 128x128 pixels (medium)")
    print("- 256x256 pixels (large)")
    print()
    print("To find the correct positions:")
    print("1. Open your image in Preview or any image editor")
    print("2. Use the selection tool to measure each icon")
    print("3. Note the X, Y coordinates and width, height")
    print()
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Example extraction - you can modify these coordinates
    print("Example extraction with common coordinates:")
    print("(You can modify these in the script)")
    print()
    
    # Default coordinates for 10x1 layout with 64x64 icons
    icon_specs = [
        ("select_icon.png", 0, 0, 64, 64),
        ("line_icon.png", 72, 0, 64, 64),
        ("curve_icon.png", 144, 0, 64, 64),
        ("bezier_icon.png", 216, 0, 64, 64),
        ("spline_icon.png", 288, 0, 64, 64),
        ("rectangle_icon.png", 360, 0, 64, 64),
        ("ellipse_icon.png", 432, 0, 64, 64),
        ("eraser_icon.png", 504, 0, 64, 64),
        ("measure_icon.png", 576, 0, 64, 64),
        ("image_icon.png", 648, 0, 64, 64),
    ]
    
    print("Trying default 10x1 layout with 64x64 icons...")
    success_count = 0
    
    for icon_name, x, y, width, height in icon_specs:
        if extract_icon_at_position(image_path, output_dir, icon_name, x, y, width, height):
            success_count += 1
    
    print(f"\n✅ Extracted {success_count}/10 icons")
    
    if success_count == 10:
        print("\n🚀 Next steps:")
        print("1. Build the application: cd build && make -j4")
        print("2. Run the application: ./DrawingStudio")
        print("3. Check if the icons look correct!")
        print()
        print("If the icons are still wrong, you can:")
        print("1. Open the image in Preview")
        print("2. Measure the exact positions of each icon")
        print("3. Edit this script with the correct coordinates")
        print("4. Run it again")
    else:
        print("\n❌ Some icons failed to extract")
        print("Please check the coordinates and try again")

if __name__ == "__main__":
    main()


