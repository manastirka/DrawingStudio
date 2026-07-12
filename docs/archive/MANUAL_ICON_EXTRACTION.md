# Manual Icon Extraction Guide

Since automated tools aren't available, here's how to manually extract the icons from your `drawinstudioicons.png` image.

## Method 1: Using Preview (macOS) - Recommended

### Step 1: Open the Image
1. Open `drawinstudioicons.png` in Preview
2. Make sure you can see the entire image

### Step 2: Determine Grid Layout
Look at your image and count:
- How many icons are in each row?
- How many rows are there?
- What's the approximate size of each icon?

### Step 3: Extract Each Icon
For each icon, follow these steps:

1. **Select the icon area:**
   - Use Tools > Rectangular Selection
   - Click and drag to select the entire icon area
   - Make sure the selection includes the complete icon

2. **Copy the selection:**
   - Press `Cmd+C` to copy

3. **Create new document:**
   - Press `Cmd+N` to create a new document
   - The copied icon should appear

4. **Save the icon:**
   - Press `Cmd+S` to save
   - Choose location: `resources/icons/`
   - Use the exact filename from the list below
   - Format: PNG
   - Click Save

### Required Icon Files (in order):
```
resources/icons/
├── select_icon.png      # Arrow/pointer icon
├── line_icon.png        # Straight line icon
├── curve_icon.png       # Curved line icon
├── bezier_icon.png      # Bezier curve icon
├── spline_icon.png      # Spline curve icon
├── rectangle_icon.png   # Rectangle/square icon
├── ellipse_icon.png     # Ellipse/circle icon
├── eraser_icon.png      # Eraser icon
├── measure_icon.png     # Ruler/measure icon
└── image_icon.png       # Image/frame icon
```

## Method 2: Using Photoshop/GIMP

### Step 1: Open Image
1. Open `drawinstudioicons.png` in your image editor

### Step 2: Extract Icons
1. Use the rectangular selection tool
2. Select each icon area
3. Copy (Ctrl+C / Cmd+C)
4. Create new document (Ctrl+N / Cmd+N)
5. Paste (Ctrl+V / Cmd+V)
6. Save as PNG with the correct filename

## Method 3: Using Online Tools

### Option A: Photopea (Free Online Photoshop)
1. Go to https://www.photopea.com/
2. Open your `drawinstudioicons.png`
3. Use rectangular selection tool
4. Copy each icon to new document
5. Export as PNG with correct filename

### Option B: GIMP (Free)
1. Download and install GIMP
2. Open your image
3. Use rectangle select tool
4. Copy each icon
5. Paste as new image
6. Export as PNG

## Icon Specifications

- **Format**: PNG with transparency
- **Size**: 32x32 pixels (recommended) or match your source
- **Background**: Transparent or white
- **Quality**: Clear and recognizable at small sizes

## Testing

After extracting all icons:

1. **Build the application:**
   ```bash
   cd build
   make -j4
   ```

2. **Run the application:**
   ```bash
   ./DrawingStudio
   ```

3. **Check the toolbar:** The new icons should appear in the left toolbar

## Troubleshooting

- **Icons don't appear**: Check that filenames match exactly (case-sensitive)
- **Wrong icons**: Verify you extracted the correct icon for each tool
- **Application crashes**: Ensure all icon files are valid PNG images
- **Fallback icons show**: The app will show default icons if custom ones aren't found

## Quick Reference

| Tool | Icon File | Description |
|------|-----------|-------------|
| Select | select_icon.png | Arrow cursor |
| Line | line_icon.png | Straight line |
| Curve | curve_icon.png | Curved line |
| Bezier | bezier_icon.png | Bezier curve |
| Spline | spline_icon.png | Spline curve |
| Rectangle | rectangle_icon.png | Rectangle |
| Ellipse | ellipse_icon.png | Ellipse/circle |
| Eraser | eraser_icon.png | Eraser |
| Measure | measure_icon.png | Ruler |
| Image | image_icon.png | Image frame |

The application will automatically use your custom icons once they're properly placed in the `resources/icons/` folder!


