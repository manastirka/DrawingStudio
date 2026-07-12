#!/usr/bin/env python3
"""
Image Content Analyzer
This script analyzes the actual content of the image to find the best extraction method
"""

import os
from PIL import Image
import sys

def analyze_image_content(image_path):
    """
    Analyze the image to find the best extraction method
    """
    
    try:
        with Image.open(image_path) as img:
            print(f"🔍 Analyzing {image_path}")
            print(f"Image size: {img.size[0]}x{img.size[1]} pixels")
            
            # Convert to RGB for analysis
            rgb_img = img.convert('RGB')
            width, height = img.size
            
            print(f"\n📊 Image Analysis:")
            print(f"Width: {width} pixels")
            print(f"Height: {height} pixels")
            print(f"Aspect ratio: {width/height:.2f}")
            
            # Check if it's square
            if width == height:
                print("✅ Square image - likely a grid layout")
            else:
                print("❌ Not square - might be a single row or column")
            
            # Try to detect grid patterns by sampling
            print(f"\n🔍 Grid Pattern Detection:")
            
            # Sample different grid sizes
            for cols in [2, 3, 4, 5, 10]:
                for rows in [1, 2, 3, 4, 5]:
                    if cols * rows >= 10:  # We need at least 10 icons
                        icon_width = width // cols
                        icon_height = height // rows
                        
                        if icon_width >= 32 and icon_height >= 32:  # Minimum usable size
                            print(f"  {cols}x{rows}: {icon_width}x{icon_height} icons - ✅ Possible")
                        else:
                            print(f"  {cols}x{rows}: {icon_width}x{icon_height} icons - ❌ Too small")
            
            print(f"\n💡 Recommendations:")
            print("1. If the image is square, try 3x4 or 4x3 layouts")
            print("2. If icons are in a row, try 10x1 layout")
            print("3. If icons are in 2 rows, try 5x2 layout")
            print("4. Look for the most logical arrangement for 10 icons")
            
            return True
            
    except Exception as e:
        print(f"❌ Error analyzing image: {e}")
        return False

def create_preview_extractions(image_path, output_dir):
    """
    Create preview extractions with different layouts so you can see which one looks right
    """
    
    layouts = [
        ("10x1_64", 10, 1, 64, 8),
        ("5x2_128", 5, 2, 128, 8),
        ("3x4_128", 3, 4, 128, 8),
        ("2x5_128", 2, 5, 128, 8),
    ]
    
    for layout_name, cols, rows, icon_size, spacing in layouts:
        preview_dir = f"{output_dir}_preview_{layout_name}"
        os.makedirs(preview_dir, exist_ok=True)
        
        try:
            with Image.open(image_path) as img:
                # Check if layout fits
                total_width = cols * icon_size + (cols - 1) * spacing
                total_height = rows * icon_size + (rows - 1) * spacing
                
                if total_width > img.size[0] or total_height > img.size[1]:
                    print(f"❌ {layout_name}: Layout doesn't fit")
                    continue
                
                # Extract first few icons as preview
                for i in range(min(3, cols * rows)):  # Extract first 3 icons
                    row = i // cols
                    col = i % cols
                    
                    x = col * (icon_size + spacing)
                    y = row * (icon_size + spacing)
                    
                    icon_box = (x, y, x + icon_size, y + icon_size)
                    icon = img.crop(icon_box)
                    
                    preview_path = os.path.join(preview_dir, f"preview_{i+1}.png")
                    icon.save(preview_path)
                
                print(f"✅ {layout_name}: Created preview in {preview_dir}")
                
        except Exception as e:
            print(f"❌ {layout_name}: Error - {e}")

def main():
    image_path = "drawinstudioicons.png"
    output_dir = "resources/icons"
    
    if not os.path.exists(image_path):
        print(f"❌ Error: Source image '{image_path}' not found!")
        return
    
    print("🔍 Image Content Analyzer")
    print("=" * 30)
    
    # Analyze the image
    if analyze_image_content(image_path):
        print(f"\n🎨 Creating preview extractions...")
        create_preview_extractions(image_path, output_dir)
        
        print(f"\n📁 Check the preview directories:")
        print("1. resources/icons_preview_10x1_64/")
        print("2. resources/icons_preview_5x2_128/")
        print("3. resources/icons_preview_3x4_128/")
        print("4. resources/icons_preview_2x5_128/")
        print("\nLook at the preview images to see which layout looks correct!")
        print("Then we can use the correct layout for the final extraction.")

if __name__ == "__main__":
    main()


