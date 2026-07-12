#!/usr/bin/env python3
"""
Icon Extraction Script for DrawingStudio
This script helps extract individual icons from the drawinstudioicons.png image
"""

import os
from PIL import Image
import sys

def extract_icons_from_image(image_path, output_dir, icon_size=32, grid_cols=5, grid_rows=2):
    """
    Extract icons from a grid layout in the source image
    
    Args:
        image_path: Path to the source image file
        output_dir: Directory to save extracted icons
        icon_size: Size of each icon in pixels
        grid_cols: Number of columns in the grid
        grid_rows: Number of rows in the grid
    """
    
    # Icon names in order (left to right, top to bottom)
    icon_names = [
        'select_icon.png',    # Row 1, Col 1
        'line_icon.png',      # Row 1, Col 2
        'curve_icon.png',     # Row 1, Col 3
        'bezier_icon.png',    # Row 1, Col 4
        'spline_icon.png',    # Row 1, Col 5
        'rectangle_icon.png', # Row 2, Col 1
        'ellipse_icon.png',   # Row 2, Col 2
        'eraser_icon.png',    # Row 2, Col 3
        'measure_icon.png',   # Row 2, Col 4
        'image_icon.png'      # Row 2, Col 5
    ]
    
    try:
        # Open the source image
        with Image.open(image_path) as img:
            print(f"Source image size: {img.size}")
            print(f"Expected icon size: {icon_size}x{icon_size}")
            
            # Create output directory if it doesn't exist
            os.makedirs(output_dir, exist_ok=True)
            
            # Extract each icon
            for i, icon_name in enumerate(icon_names):
                # Calculate position in grid
                row = i // grid_cols
                col = i % grid_cols
                
                # Calculate pixel coordinates
                x = col * icon_size
                y = row * icon_size
                
                print(f"Extracting {icon_name} from position ({x}, {y})")
                
                # Crop the icon
                icon_box = (x, y, x + icon_size, y + icon_size)
                icon = img.crop(icon_box)
                
                # Save the icon
                output_path = os.path.join(output_dir, icon_name)
                icon.save(output_path)
                print(f"Saved: {output_path}")
            
            print(f"\nSuccessfully extracted {len(icon_names)} icons to {output_dir}")
            
    except Exception as e:
        print(f"Error: {e}")
        return False
    
    return True

def main():
    # Paths
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    # Check if source image exists
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        print("Please make sure drawinstudioicons.png is in the current directory.")
        return
    
    print("DrawingStudio Icon Extraction Tool")
    print("==================================")
    print(f"Source image: {image_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Ask user for grid configuration
    print("Please specify the grid layout of your icons:")
    try:
        grid_cols = int(input("Number of columns (default 5): ") or "5")
        grid_rows = int(input("Number of rows (default 2): ") or "2")
        icon_size = int(input("Icon size in pixels (default 32): ") or "32")
    except ValueError:
        print("Invalid input, using defaults...")
        grid_cols = 5
        grid_rows = 2
        icon_size = 32
    
    print(f"\nUsing grid: {grid_cols}x{grid_rows}, icon size: {icon_size}x{icon_size}")
    
    # Extract icons
    success = extract_icons_from_image(image_path, output_dir, icon_size, grid_cols, grid_rows)
    
    if success:
        print("\n✅ Icon extraction completed successfully!")
        print("\nNext steps:")
        print("1. Build the application: cd build && make -j4")
        print("2. Run the application: ./DrawingStudio")
        print("3. The new icons should now appear in the toolbar!")
    else:
        print("\n❌ Icon extraction failed!")

if __name__ == "__main__":
    main()


