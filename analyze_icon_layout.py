#!/usr/bin/env python3
"""
Icon Layout Analysis Script
This script helps analyze the layout of icons in the drawinstudioicons.png image
"""

import os
from PIL import Image
import sys

def analyze_icon_layout(image_path):
    """
    Analyze the icon layout in the source image
    """
    
    try:
        with Image.open(image_path) as img:
            print(f"Source image size: {img.size}")
            width, height = img.size
            
            # Common grid layouts to try
            layouts = [
                (10, 1, "10x1 (horizontal row)"),
                (5, 2, "5x2 (2 rows of 5)"),
                (2, 5, "2x5 (5 rows of 2)"),
                (4, 3, "4x3 (3 rows of 4)"),
                (3, 4, "3x4 (4 rows of 3)"),
                (6, 2, "6x2 (2 rows of 6)"),
                (2, 6, "2x6 (6 rows of 2)"),
            ]
            
            print("\nAnalyzing possible grid layouts:")
            print("=" * 50)
            
            for cols, rows, description in layouts:
                # Calculate icon size based on available space
                # Account for spacing between icons
                spacing = 8  # 8px spacing between icons
                
                available_width = width - (cols - 1) * spacing
                available_height = height - (rows - 1) * spacing
                
                icon_width = available_width // cols
                icon_height = available_height // rows
                
                total_width = cols * icon_width + (cols - 1) * spacing
                total_height = rows * icon_height + (rows - 1) * spacing
                
                fits = total_width <= width and total_height <= height
                status = "✅ FITS" if fits else "❌ Too big"
                
                print(f"{description:20} | {cols}x{rows:2} | {icon_width:3}x{icon_height:3} | {status}")
                
                if fits and icon_width >= 32 and icon_height >= 32:
                    print(f"  → Good candidate: {icon_width}x{icon_height} icons")
            
            print("\n" + "=" * 50)
            print("Recommendation: Choose the layout that makes the most sense")
            print("for your icon arrangement. Look for layouts that:")
            print("- Have ✅ FITS status")
            print("- Have reasonable icon sizes (64x64 or larger)")
            print("- Match how your icons are actually arranged")
            
    except Exception as e:
        print(f"Error: {e}")
        return False
    
    return True

def main():
    image_path = "drawinstudioicons.png"
    
    if not os.path.exists(image_path):
        print(f"Error: Source image '{image_path}' not found!")
        return
    
    print("🔍 DrawingStudio Icon Layout Analyzer")
    print("=" * 40)
    print(f"Analyzing: {image_path}")
    print()
    
    analyze_icon_layout(image_path)

if __name__ == "__main__":
    main()


