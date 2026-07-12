#!/usr/bin/env python3
"""
Smart Icon Extractor
This script tries multiple common layouts and lets you choose the best one
"""

import os
from PIL import Image
import sys

def extract_with_layout(image_path, output_dir, layout_name, cols, rows, icon_size, spacing=8):
    """
    Extract icons with a specific layout
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
            print(f"\n🔍 Trying {layout_name}: {cols}x{rows}, {icon_size}x{icon_size} icons")
            
            # Check if layout fits
            total_width = cols * icon_size + (cols - 1) * spacing
            total_height = rows * icon_size + (rows - 1) * spacing
            
            if total_width > img.size[0] or total_height > img.size[1]:
                print(f"❌ Layout doesn't fit: {total_width}x{total_height} > {img.size[0]}x{img.size[1]}")
                return False
            
            # Create output directory
            os.makedirs(output_dir, exist_ok=True)
            
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
                
                # Crop the icon
                icon_box = (x, y, x + icon_size, y + icon_size)
                icon = img.crop(icon_box)
                
                # Save the icon
                output_path = os.path.join(output_dir, icon_name)
                icon.save(output_path)
                success_count += 1
            
            print(f"✅ Extracted {success_count} icons with {layout_name}")
            return True
            
    except Exception as e:
        print(f"❌ Error with {layout_name}: {e}")
        return False

def main():
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        return
    
    print("🎨 Smart Icon Extractor")
    print("=" * 30)
    print(f"Source image: {image_path}")
    print(f"Output directory: {output_dir}")
    
    # Get image dimensions
    with Image.open(image_path) as img:
        print(f"Image size: {img.size[0]}x{img.size[1]} pixels")
    
    # Try different layouts
    layouts = [
        ("10x1 (horizontal row)", 10, 1, 64, 8),
        ("10x1 (horizontal row)", 10, 1, 128, 8),
        ("5x2 (2 rows of 5)", 5, 2, 128, 8),
        ("5x2 (2 rows of 5)", 5, 2, 256, 8),
        ("2x5 (5 rows of 2)", 2, 5, 128, 8),
        ("2x5 (5 rows of 2)", 2, 5, 256, 8),
        ("4x3 (3 rows of 4)", 4, 3, 128, 8),
        ("3x4 (4 rows of 3)", 3, 4, 128, 8),
    ]
    
    successful_layouts = []
    
    for layout_name, cols, rows, icon_size, spacing in layouts:
        if extract_with_layout(image_path, output_dir, layout_name, cols, rows, icon_size, spacing):
            successful_layouts.append((layout_name, cols, rows, icon_size, spacing))
    
    print(f"\n📊 Results:")
    print(f"✅ {len(successful_layouts)} layouts worked")
    print(f"❌ {len(layouts) - len(successful_layouts)} layouts failed")
    
    if successful_layouts:
        print(f"\n🎯 Successful layouts:")
        for i, (name, cols, rows, size, spacing) in enumerate(successful_layouts, 1):
            print(f"{i}. {name} - {cols}x{rows}, {size}x{size} icons")
        
        print(f"\n🚀 Icons extracted! Now test the application:")
        print("1. cd build && make -j4")
        print("2. ./DrawingStudio")
        print("3. Check if the icons look correct!")
        
        if len(successful_layouts) > 1:
            print(f"\n💡 If the icons are still wrong, try a different layout:")
            print("Edit this script and comment out the layouts that don't work.")
    else:
        print(f"\n❌ No layouts worked. The image might have a different arrangement.")
        print("Please check the image manually and provide the correct layout details.")

if __name__ == "__main__":
    main()


