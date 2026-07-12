# Icon Setup Instructions

## Overview
The DrawingStudio application has been updated to support loading icons from image files. The system will first try to load icons from the `resources/icons/` folder, and if not found, will fall back to the programmatically created icons.

## Required Icon Files
You need to extract the following icons from your `drawinstudioicons.png` image and save them as individual PNG files in the `resources/icons/` folder:

### Icon Files Needed:
1. `select_icon.png` - Select/arrow cursor tool
2. `line_icon.png` - Line drawing tool
3. `curve_icon.png` - Curve drawing tool
4. `bezier_icon.png` - Bézier curve tool
5. `spline_icon.png` - Spline curve tool
6. `rectangle_icon.png` - Rectangle drawing tool
7. `ellipse_icon.png` - Ellipse drawing tool
8. `eraser_icon.png` - Eraser tool
9. `measure_icon.png` - Measurement/ruler tool
10. `image_icon.png` - Image import tool

## How to Extract Icons

### Option 1: Manual Extraction (Recommended)
1. Open `drawinstudioicons.png` in an image editor (Photoshop, GIMP, Preview, etc.)
2. For each icon, crop the appropriate section and save as a separate PNG file
3. Save each icon with the exact filename listed above in the `resources/icons/` folder
4. Recommended icon size: 32x32 pixels or 64x64 pixels (the system will scale as needed)

### Option 2: Using ImageMagick (Command Line)
If you know the grid layout of your icons, you can use ImageMagick to extract them automatically:

```bash
# Example: If icons are in a 5x2 grid, each 32x32 pixels
# First row (0,0) to (4,0), second row (0,1) to (4,1)

# Extract select icon (position 0,0)
convert drawinstudioicons.png -crop 32x32+0+0 select_icon.png

# Extract line icon (position 1,0)
convert drawinstudioicons.png -crop 32x32+32+0 line_icon.png

# Continue for all icons...
```

### Option 3: Using Preview (macOS)
1. Open `drawinstudioicons.png` in Preview
2. Use Tools > Rectangular Selection to select each icon
3. Copy (Cmd+C) the selection
4. Create a new document (Cmd+N)
5. Paste (Cmd+V) the icon
6. Save as PNG with the appropriate filename

## Icon Specifications
- **Format**: PNG with transparency support
- **Size**: 32x32 pixels (recommended) or 64x64 pixels
- **Background**: Transparent or white
- **Style**: Should be clear and recognizable at small sizes

## Testing
After placing the icon files in `resources/icons/`, build and run the application:

```bash
cd build
make -j4
./DrawingStudio
```

The application will automatically use the new icons if they are found in the correct location.

## Troubleshooting
- If icons don't appear, check that filenames match exactly (case-sensitive)
- Ensure icons are in PNG format
- Verify the `resources/icons/` folder exists and contains the files
- Check the console output for any loading errors

## Fallback System
If any icon file is missing, the application will automatically fall back to the programmatically created icon, so the application will always work even if some icons are missing.


