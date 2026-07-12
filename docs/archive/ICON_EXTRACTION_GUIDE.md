# Icon Extraction from Image - User Guide

## Overview
You can now create all your toolbar icons manually in a single image file, and the AI will automatically extract and apply them to your application!

## How It Works

### 1. **Create Your Icons Image**
Create a single image file (PNG, JPG, JPEG, or BMP) containing all your toolbar icons:
- Arrange icons in a grid or row
- Each icon should be clearly separated
- Recommended: Use a white or transparent background
- Icons can be any size (they'll be resized to 64x64)

**Example Layout:**
```
[Pen] [Line] [Rectangle] [Circle] [Ellipse]
[Text] [Eraser] [Fill] [Select] [Move]
[Zoom] [Pan] [Undo] [Redo] [Clear]
```

### 2. **Call the Extraction Function**
In your code, call:
```cpp
mainWindow->extractIconsFromImage();
```

Or add a menu item to trigger it (see below).

### 3. **Select Your Image**
- A file dialog will open
- Select your image file containing all the icons
- Click "Open"

### 4. **Confirm Extraction**
You'll see a dialog showing:
- Number of icons to extract
- List of icon names (from your toolbar)
- Confirmation to proceed

### 5. **AI Processing**
The system will:
1. **Analyze** the image using Gemini Vision API
2. **Identify** the position of each icon
3. **Extract** each icon region
4. **Process** each icon:
   - Remove background
   - Convert to pure black
   - Resize to 64x64 pixels
   - Apply sharpening
5. **Apply** to your toolbar

### 6. **Results**
You'll see a progress dialog showing:
- "📸 Analyzing image..."
- "✨ Extracted 'IconName' icon (X/15)"
- Final summary of successful/failed extractions

## Requirements

### API Key
- Requires **Gemini API key** (for vision analysis)
- Set in **AI Settings** dialog
- Get your key from: https://makersuite.google.com/app/apikey

### Icon Names
The system uses your existing toolbar icon names:
- Pen, Line, Rectangle, Circle, Ellipse
- Text, Eraser, Fill, Select, Move
- Zoom, Pan, Undo, Redo, Clear

Make sure your icons match these names (or update the names in your toolbar).

## Adding to Menu

To add this feature to your menu, add this code in `MainWindow::createMenus()`:

```cpp
// In the Tools or AI menu
QAction *extractIconsAction = new QAction("📸 Extract Icons from Image", this);
extractIconsAction->setStatusTip("Extract toolbar icons from a single image using AI");
connect(extractIconsAction, &QAction::triggered, this, &MainWindow::extractIconsFromImage);
toolsMenu->addAction(extractIconsAction); // or aiMenu->addAction(extractIconsAction);
```

## Tips for Best Results

### Image Preparation
1. **Clear separation** - Leave space between icons
2. **Consistent size** - Make all icons roughly the same size
3. **High contrast** - Dark icons on light background work best
4. **Clean design** - Simple, clear icon designs
5. **No text** - Icons should be symbols only

### Icon Design
- **Simple shapes** - 2-3 basic geometric elements
- **Thick lines** - 4-6px for visibility
- **Centered** - Icon centered in its space
- **Monochrome** - Single color (preferably black)
- **No gradients** - Flat design works best

### Troubleshooting

**If extraction fails:**
1. Check your Gemini API key is valid
2. Ensure image file is not corrupted
3. Try a simpler layout (single row)
4. Make icons larger and more separated
5. Check console output for specific errors

**If icons don't match:**
- The AI tries to match icon positions to names
- If it can't identify an icon, it will skip it
- Check the console for "Failed icons" list
- You may need to manually adjust icon positions

**If quality is poor:**
- Start with higher resolution icons in your source image
- Ensure good contrast in original icons
- Try regenerating with different source images

## Technical Details

### Processing Pipeline
1. **Load** source image
2. **Send** to Gemini Vision API with icon names
3. **Receive** JSON with icon positions: `[{name, x, y, width, height}, ...]`
4. **Extract** each icon region using coordinates
5. **Process** each icon:
   - Convert to RGBA
   - Remove background (aggressive)
   - Remove isolated pixels
   - Convert to pure black
   - Scale to 64x64
   - Apply sharpening
6. **Emit** `iconRegenerated` signal for each icon
7. **Update** toolbar icons

### API Request Format
```json
{
  "contents": [{
    "parts": [
      {"text": "Analyze this image with 15 icons..."},
      {"inline_data": {"mime_type": "image/png", "data": "base64..."}}
    ]
  }]
}
```

### Expected Response
```json
[
  {"name": "Pen", "x": 10, "y": 10, "width": 64, "height": 64},
  {"name": "Line", "x": 84, "y": 10, "width": 64, "height": 64},
  ...
]
```

## Advantages Over AI Generation

### Why Use Image Extraction?
1. **Full control** - You design exactly what you want
2. **Consistency** - All icons have your exact style
3. **Speed** - One API call instead of 15
4. **Cost** - Cheaper than generating 15 images
5. **Reliability** - No variation in AI output
6. **Customization** - Use your own design tools

### When to Use AI Generation Instead?
- You don't have design skills
- You want quick prototypes
- You're experimenting with styles
- You need variations

## Example Workflow

1. **Design in Figma/Illustrator:**
   - Create 15 icons at 128x128 each
   - Arrange in 3 rows of 5
   - Export as PNG (1920x384 pixels)

2. **Extract in App:**
   - Click "Extract Icons from Image"
   - Select your exported PNG
   - Confirm extraction
   - Wait ~5 seconds

3. **Result:**
   - All 15 toolbar icons updated
   - Consistent style
   - Perfect quality

## Future Enhancements

Potential improvements:
- Manual position adjustment UI
- Grid detection (auto-detect icon positions)
- Batch processing multiple images
- Icon preview before applying
- Save/load icon sets
- Export current icons to image
