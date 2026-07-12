#!/usr/bin/env python3
"""
Extract Icons from drawingstudioicons.png
This script analyzes and extracts icons from the correct image file
"""

import os
from PIL import Image
import sys

def analyze_correct_image(image_path):
    """
    Analyze the correct drawingstudioicons.png image
    """
    
    try:
        with Image.open(image_path) as img:
            print(f"🔍 Analyzing {image_path}")
            print(f"Image size: {img.size[0]}x{img.size[1]} pixels")
            
            width, height = img.size
            print(f"Width: {width} pixels")
            print(f"Height: {height} pixels")
            print(f"Aspect ratio: {width/height:.2f}")
            
            # Check if it's square
            if width == height:
                print("✅ Square image - likely a grid layout")
            else:
                print("❌ Not square - might be a single row or column")
            
            return True
            
    except Exception as e:
        print(f"❌ Error analyzing image: {e}")
        return False

def extract_icons_manual(image_path, output_dir):
    """
    Extract icons with manual coordinate specification
    """
    
    # Icon names in order
    icon_names = [
        'select_icon.png',    # Arrow cursor
        'line_icon.png',      # Straight line
        'curve_icon.png',     # Curved line
        'bezier_icon.png',    # Bezier curve
        'spline_icon.png',    # Spline curve
        'rectangle_icon.png', # Rectangle
        'ellipse_icon.png',   # Ellipse/circle
        'eraser_icon.png',    # Eraser
        'measure_icon.png',   # Ruler
        'image_icon.png'      # Picture frame
    ]
    
    try:
        with Image.open(image_path) as img:
            print(f"\n🎨 Extracting icons from {image_path}")
            
            # Create output directory
            os.makedirs(output_dir, exist_ok=True)
            
            # Try different common layouts for 1024x1024 image
            layouts = [
                # (name, cols, rows, icon_size, spacing)
                ("5x2_128", 5, 2, 128, 8),
                ("3x4_128", 3, 4, 128, 8),
                ("2x5_128", 2, 5, 128, 8),
                ("10x1_64", 10, 1, 64, 8),
                ("4x3_128", 4, 3, 128, 8),
            ]
            
            for layout_name, cols, rows, icon_size, spacing in layouts:
                print(f"\n🔍 Trying {layout_name}: {cols}x{rows}, {icon_size}x{icon_size} icons")
                
                # Check if layout fits
                total_width = cols * icon_size + (cols - 1) * spacing
                total_height = rows * icon_size + (rows - 1) * spacing
                
                if total_width > img.size[0] or total_height > img.size[1]:
                    print(f"❌ Layout doesn't fit: {total_width}x{total_height} > {img.size[0]}x{img.size[1]}")
                    continue
                
                print(f"✅ Layout fits! Extracting icons...")
                
                # Extract icons
                success_count = 0
                for i, icon_name in enumerate(icon_names):
                    if i >= cols * rows:
                        break
                        
                    # Calculate position in grid
                    row = i // cols
                    col = i % cols
                    
                    # Calculate pixel coordinates
                    x = col * (icon_size + spacing)
                    y = row * (icon_size + spacing)
                    
                    print(f"  Extracting {icon_name} from ({x}, {y})")
                    
                    # Crop the icon
                    icon_box = (x, y, x + icon_size, y + icon_size)
                    icon = img.crop(icon_box)
                    
                    # Save the icon
                    output_path = os.path.join(output_dir, icon_name)
                    icon.save(output_path)
                    success_count += 1
                
                if success_count == len(icon_names):
                    print(f"✅ Successfully extracted all {success_count} icons with {layout_name}!")
                    return True
                else:
                    print(f"❌ Only extracted {success_count} icons with {layout_name}")
            
            print(f"\n❌ No suitable layout found")
            return False
            
    except Exception as e:
        print(f"❌ Error extracting icons: {e}")
        return False

def main():
    image_path = "drawingstudioicons.png"
    output_dir = "resources/icons"
    
    if not os.path.exists(image_path):
        print(f"❌ Error: Source image '{image_path}' not found!")
        return
    
    print("🎨 DrawingStudio Icon Extractor")
    print("=" * 40)
    print(f"Using correct image: {image_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Analyze the image
    if analyze_correct_image(image_path):
        print(f"\n🚀 Extracting icons...")
        if extract_icons_manual(image_path, output_dir):
            print(f"\n✅ Icon extraction completed!")
            print(f"📁 Icons saved to: {output_dir}")
            print(f"\n🔧 Next steps:")
            print("1. cd build && make -j4")
            print("2. ./DrawingStudio")
            print("3. Check if the icons look correct!")
        else:
            print(f"\n❌ Icon extraction failed!")
            print("The image might have a different layout than expected.")
    else:
        print(f"\n❌ Could not analyze the image!")

if __name__ == "__main__":
    main()


