#!/usr/bin/env python3
"""
Extract Icons with 10x1 Layout (Horizontal Row)
This script extracts icons assuming they're arranged in a single horizontal row
"""

import os
from PIL import Image
import sys

def extract_icons_10x1(image_path, output_dir):
    """
    Extract icons from a 1024x1024 image assuming 10x1 layout
    """
    
    # Icon names in order (left to right)
    icon_names = [
        'select_icon.png',    # Position 1
        'line_icon.png',      # Position 2
        'curve_icon.png',     # Position 3
        'bezier_icon.png',    # Position 4
        'spline_icon.png',    # Position 5
        'rectangle_icon.png', # Position 6
        'ellipse_icon.png',   # Position 7
        'eraser_icon.png',    # Position 8
        'measure_icon.png',   # Position 9
        'image_icon.png'      # Position 10
    ]
    
    try:
        with Image.open(image_path) as img:
            print(f"Source image size: {img.size}")
            width, height = img.size
            
            # 10x1 layout: 10 icons in a single row
            num_icons = 10
            spacing = 8  # 8px spacing between icons
            
            # Calculate icon size
            available_width = width - (num_icons - 1) * spacing
            icon_width = available_width // num_icons
            icon_height = height  # Use full height for 10x1 layout
            
            print(f"10x1 Layout: {icon_width}x{icon_height} icons with {spacing}px spacing")
            
            # Create output directory if it doesn't exist
            os.makedirs(output_dir, exist_ok=True)
            
            # Extract each icon
            for i, icon_name in enumerate(icon_names):
                # Calculate position (left to right)
                x = i * (icon_width + spacing)
                y = 0
                
                print(f"Extracting {icon_name} from position ({x}, {y})")
                
                # Crop the icon
                icon_box = (x, y, x + icon_width, y + icon_height)
                icon = img.crop(icon_box)
                
                # Save the icon
                output_path = os.path.join(output_dir, icon_name)
                icon.save(output_path)
                print(f"Saved: {output_path}")
            
            print(f"\n✅ Successfully extracted all {len(icon_names)} icons with 10x1 layout!")
            return True
            
    except Exception as e:
        print(f"Error: {e}")
        return False

def main():
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        return
    
    print("🎨 DrawingStudio Icon Extraction - 10x1 Layout")
    print("=" * 50)
    print(f"Source image: {image_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Extract icons
    success = extract_icons_10x1(image_path, output_dir)
    
    if success:
        print("\n🚀 Next steps:")
        print("1. Build the application: cd build && make -j4")
        print("2. Run the application: ./DrawingStudio")
        print("3. Check if the icons look correct now!")
    else:
        print("\n❌ Extraction failed!")

if __name__ == "__main__":
    main()


