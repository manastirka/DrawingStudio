#!/usr/bin/env python3
"""
Automatic Icon Extraction Script for DrawingStudio
This script tries to automatically extract icons from a 1024x1024 image
"""

import os
from PIL import Image
import sys

def extract_icons_1024x1024(image_path, output_dir):
    """
    Extract icons from a 1024x1024 image with various grid layouts
    """
    
    # Icon names in order
    icon_names = [
        'select_icon.png',    # Arrow/pointer icon
        'line_icon.png',      # Straight line icon
        'curve_icon.png',     # Curved line icon
        'bezier_icon.png',    # Bezier curve icon
        'spline_icon.png',    # Spline curve icon
        'rectangle_icon.png', # Rectangle/square icon
        'ellipse_icon.png',   # Ellipse/circle icon
        'eraser_icon.png',    # Eraser icon
        'measure_icon.png',   # Ruler/measure icon
        'image_icon.png'      # Image/frame icon
    ]
    
    try:
        # Open the source image
        with Image.open(image_path) as img:
            print(f"Source image size: {img.size}")
            
            # Create output directory if it doesn't exist
            os.makedirs(output_dir, exist_ok=True)
            
            # Try different grid layouts for 1024x1024 image
            layouts = [
                (5, 2, 128),  # 5x2 grid, 128x128 icons
                (4, 3, 128),  # 4x3 grid, 128x128 icons  
                (3, 4, 128),  # 3x4 grid, 128x128 icons
                (10, 1, 64),  # 10x1 grid, 64x64 icons
                (5, 2, 160),  # 5x2 grid, 160x160 icons
            ]
            
            for grid_cols, grid_rows, icon_size in layouts:
                print(f"\nTrying layout: {grid_cols}x{grid_rows}, icon size: {icon_size}x{icon_size}")
                
                # Check if this layout fits
                total_width = grid_cols * icon_size + (grid_cols - 1) * 8  # 8px spacing
                total_height = grid_rows * icon_size + (grid_rows - 1) * 8
                
                if total_width <= 1024 and total_height <= 1024:
                    print(f"Layout fits! Total size: {total_width}x{total_height}")
                    
                    # Extract icons
                    success_count = 0
                    for i, icon_name in enumerate(icon_names):
                        if i >= grid_cols * grid_rows:
                            break
                            
                        # Calculate position in grid
                        row = i // grid_cols
                        col = i % grid_cols
                        
                        # Calculate pixel coordinates with spacing
                        x = col * (icon_size + 8)
                        y = row * (icon_size + 8)
                        
                        print(f"Extracting {icon_name} from position ({x}, {y})")
                        
                        # Crop the icon
                        icon_box = (x, y, x + icon_size, y + icon_size)
                        icon = img.crop(icon_box)
                        
                        # Save the icon
                        output_path = os.path.join(output_dir, icon_name)
                        icon.save(output_path)
                        success_count += 1
                        print(f"Saved: {output_path}")
                    
                    if success_count == len(icon_names):
                        print(f"\n✅ Successfully extracted all {success_count} icons!")
                        return True
                    else:
                        print(f"Only extracted {success_count} icons, trying next layout...")
                else:
                    print(f"Layout doesn't fit: {total_width}x{total_height} > 1024x1024")
            
            print("\n❌ Could not find a suitable grid layout automatically.")
            print("Please check the image layout manually and use the manual extraction method.")
            return False
            
    except Exception as e:
        print(f"Error: {e}")
        return False

def main():
    # Paths
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    # Check if source image exists
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        return
    
    print("🎨 DrawingStudio Automatic Icon Extraction")
    print("==========================================")
    print(f"Source image: {image_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Extract icons
    success = extract_icons_1024x1024(image_path, output_dir)
    
    if success:
        print("\n🚀 Next steps:")
        print("1. Build the application: cd build && make -j4")
        print("2. Run the application: ./DrawingStudio")
        print("3. Check if the new icons appear in the toolbar!")
    else:
        print("\n📖 Please use the manual extraction method:")
        print("1. Open drawinstudioicons.png in Preview")
        print("2. Use Tools > Rectangular Selection")
        print("3. Select each icon and save with the correct filename")
        print("4. See MANUAL_ICON_EXTRACTION.md for detailed instructions")

if __name__ == "__main__":
    main()


