# DrawingStudio Toolbar Icons - Complete List & AI Prompt

## Left Toolbar Icons (15 Tools)

### Drawing Tools
1. **Select** - Selection tool for moving and editing objects
2. **Line** - Draw straight lines (Shortcut: L)
3. **Curve** - Draw curved lines (Shortcut: C)
4. **Bezier** - Draw Bezier curves with control points (Shortcut: B)
5. **Spline** - Draw smooth spline curves (Shortcut: P)
6. **Rectangle** - Draw rectangles and squares (Shortcut: R)
7. **Ellipse** - Draw ellipses and circles (Shortcut: E)

### Editing Tools
8. **Eraser** - Erase objects (Shortcut: X)
9. **Fill** - Fill areas with color
10. **Brush** - Paint with brush strokes (Shortcut: D)
11. **Blur** - Blur areas of the canvas (Shortcut: U)

### Utility Tools
12. **Measure** - Measure distances and dimensions (Shortcut: M)
13. **Image** - Import and place images (Shortcut: I)
14. **Hand** - Pan/move the canvas view (Shortcut: H)
15. **Text** - Add text labels (Shortcut: T)

---

## Gemini Image Generation Prompt (Imagen 3)

Use this prompt with Gemini's Imagen 3 API to generate all 15 icons in a single image:

```
Create a single image containing 15 professional toolbar icons arranged in a 5x3 grid (5 columns, 3 rows) on a white background.

ICON SPECIFICATIONS:
- Each icon: 128x128 pixels
- Spacing: 32 pixels between icons
- Total image size: 832x544 pixels
- Style: Pure black (#000000) icons on white background
- Design: Minimalist, flat, vector-style, bold lines (5-6px thick)
- No gradients, no shadows, no 3D effects, no text labels

ICONS TO CREATE (in order, left to right, top to bottom):

ROW 1 (Drawing Tools):
1. Select - Arrow cursor pointing at a square selection box
2. Line - Diagonal straight line with endpoints
3. Curve - Smooth curved line (S-shape)
4. Bezier - Curved line with two control point handles
5. Spline - Smooth wavy line through multiple points

ROW 2 (Shapes & Editing):
6. Rectangle - Simple rectangle outline
7. Ellipse - Simple circle/ellipse outline
8. Eraser - Classic eraser shape (rectangular block)
9. Fill - Paint bucket pouring into a container
10. Brush - Artist paintbrush at angle

ROW 3 (Utilities):
11. Blur - Concentric circles or wavy lines (blur effect)
12. Measure - Ruler with measurement marks
13. Image - Picture frame with mountain/sun icon inside
14. Hand - Open hand (palm) for panning
15. Text - Letter "T" or "Aa" representing text

CRITICAL REQUIREMENTS:
- Pure black icons only (#000000)
- White background (#FFFFFF)
- Consistent icon size and spacing
- Bold, thick lines for visibility
- Simple, instantly recognizable symbols
- Professional software UI style (like Adobe, Figma, or VS Code icons)
- Each icon clearly separated and centered in its grid cell
```

---

## Alternative: Row Layout Prompt

If you prefer a single row layout:

```
Create a single image containing 15 professional toolbar icons arranged in a horizontal row on a white background.

ICON SPECIFICATIONS:
- Each icon: 128x128 pixels
- Spacing: 32 pixels between icons
- Total image size: 2400x128 pixels
- Style: Pure black (#000000) icons on white background
- Design: Minimalist, flat, vector-style, bold lines (5-6px thick)

ICONS (left to right):
1. Select (arrow cursor)
2. Line (diagonal line)
3. Curve (S-curve)
4. Bezier (curve with handles)
5. Spline (wavy line)
6. Rectangle (square outline)
7. Ellipse (circle outline)
8. Eraser (eraser block)
9. Fill (paint bucket)
10. Brush (paintbrush)
11. Blur (concentric circles)
12. Measure (ruler)
13. Image (picture frame)
14. Hand (open palm)
15. Text (letter T)

Same requirements: Pure black, white background, bold lines, professional style.
```

---

## Using with Gemini API (Python Example)

```python
from google import genai
from PIL import Image
from io import BytesIO

client = genai.Client(api_key='YOUR_API_KEY')

prompt = """
Create a single image containing 15 professional toolbar icons arranged in a 5x3 grid...
[full prompt from above]
"""

response = client.models.generate_images(
    model='imagen-3.0-generate-002',
    prompt=prompt,
    config={
        'number_of_images': 1,
        'aspect_ratio': '3:2',  # Approximately 832x544
    }
)

# Save the generated image
for generated_image in response.generated_images:
    img = Image.open(BytesIO(generated_image.image.image_bytes))
    img.save('toolbar_icons.png')
    print("Icons saved to toolbar_icons.png")
```

---

## Using with Gemini API (REST/cURL)

```bash
curl -X POST \
  "https://generativelanguage.googleapis.com/v1beta/models/imagen-3.0-generate-002:predict" \
  -H "x-goog-api-key: YOUR_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "instances": [{
      "prompt": "Create a single image containing 15 professional toolbar icons arranged in a 5x3 grid on a white background. Each icon 128x128 pixels, 32px spacing. Pure black icons. Icons: 1.Select arrow 2.Line 3.Curve 4.Bezier 5.Spline 6.Rectangle 7.Ellipse 8.Eraser 9.Fill bucket 10.Brush 11.Blur circles 12.Measure ruler 13.Image frame 14.Hand palm 15.Text T. Bold lines, flat design, professional software UI style."
    }],
    "parameters": {
      "sampleCount": 1,
      "aspectRatio": "3:2"
    }
  }'
```

---

## After Generation

Once you have the generated image:

1. **Save the image** as `toolbar_icons.png`

2. **Extract icons** using the app:
   ```cpp
   mainWindow->extractIconsFromImage();
   ```
   - Select your generated image
   - AI will automatically detect and extract all 15 icons
   - Icons will be processed and applied to toolbar

3. **Or manually extract** in image editor:
   - Open in Photoshop/GIMP/Figma
   - Crop each icon to 128x128
   - Save individually
   - Import to app

---

## Tips for Best Results

### Prompt Optimization
- Be very specific about layout (grid dimensions)
- Specify exact pixel sizes
- Emphasize "pure black" and "white background"
- Request "bold lines" for visibility
- Reference professional software (Adobe, Figma, VS Code)

### If Icons Don't Generate Well
Try these variations:
1. **Simpler prompt**: "15 black toolbar icons in a row: arrow, line, curve..."
2. **Reference style**: "in the style of Material Design icons"
3. **One at a time**: Generate each icon separately (slower but more control)
4. **Different model**: Try Imagen 4 or DALL-E 3 instead

### Post-Processing
If generated icons need cleanup:
1. Open in image editor
2. Increase contrast (make blacks pure black)
3. Remove any gray artifacts
4. Ensure white background is pure white
5. Crop/align if needed

---

## Icon Design Guidelines

For each icon type:

### Select
- Arrow cursor (45° angle)
- Small selection box outline
- Classic pointer shape

### Line
- Diagonal line (top-left to bottom-right)
- Circular endpoints
- Clean, straight

### Curve
- Smooth S-curve
- No control points visible
- Flowing, organic

### Bezier
- Curved line
- Two control point handles (small circles on lines)
- Shows the control mechanism

### Spline
- Smooth wavy line
- Multiple gentle curves
- Passes through several points

### Rectangle
- Simple square or rectangle outline
- Rounded corners optional
- Centered

### Ellipse
- Perfect circle or ellipse outline
- Centered
- Clean stroke

### Eraser
- Rectangular block shape
- Slight 3D perspective optional
- Recognizable as eraser

### Fill
- Paint bucket tilted
- Liquid pouring out
- Classic fill tool icon

### Brush
- Artist paintbrush
- Angled (45°)
- Bristles visible
- Handle clear

### Blur
- Concentric circles (ripple effect)
- Or wavy/fuzzy lines
- Suggests motion/blur

### Measure
- Ruler with tick marks
- Numbers optional (1,2,3)
- Diagonal or horizontal

### Image
- Picture frame
- Mountain and sun inside
- Classic image placeholder

### Hand
- Open palm facing viewer
- Five fingers clearly visible
- Suggests grabbing/panning

### Text
- Large letter "T"
- Or "Aa" (showing case)
- Typography-focused

---

## Quick Reference: Icon Names

For use in code:
```cpp
QStringList iconNames = {
    "Select", "Line", "Curve", "Bezier", "Spline",
    "Rectangle", "Ellipse", "Eraser", "Fill", "Brush",
    "Blur", "Measure", "Image", "Hand", "Text"
};
```

---

## Troubleshooting

**Icons too complex?**
- Simplify prompt: "simple geometric shapes only"
- Add: "maximum 2-3 elements per icon"

**Icons have backgrounds?**
- Emphasize: "PURE WHITE background, no gradients"
- Post-process to remove backgrounds

**Icons not aligned?**
- Specify: "perfectly aligned grid, equal spacing"
- May need manual adjustment

**Icons too light?**
- Request: "PURE BLACK #000000, maximum contrast"
- Post-process to darken

**Wrong style?**
- Add reference: "like Apple SF Symbols" or "like Material Design"
- Show example images if possible
