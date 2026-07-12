# AI Image Effects Guide

## Complete! Image Manipulation via Natural Language

The AI Assistant can now apply effects and transformations to images using natural language commands!

---

## Blur Effects

### Edge Blur
Blurs only the edges of images - perfect for vignette effects!

**Commands:**
- "Blur the edges"
- "Blur edges of image"
- "Apply edge blur 15" (custom radius)

**How it works:**
- Detects edges automatically
- Applies graduated blur from edge to center
- Preserves center sharpness

**Example:**
```
You: "Select all"
You: "Blur the edges"
Assistant: Applied edge blur (radius: 10px) to 3 image(s).
```

### Full Blur
Blurs entire image

**Commands:**
- "Blur image"
- "Blur selected"
- "Apply blur 8" (custom radius)

**Example:**
```
You: "Blur image 5"
Assistant: Applied blur (radius: 5px) to 2 image(s).
```

---

## Color Effects

### Grayscale
Converts images to black and white

**Commands:**
- "Make grayscale"
- "Convert to black and white"
- "Grayscale"

**Example:**
```
You: "Make grayscale"
Assistant: Converted 3 image(s) to grayscale.
```

### Sepia
Applies vintage sepia tone

**Commands:**
- "Apply sepia"
- "Sepia tone"
- "Make sepia"

**Example:**
```
You: "Apply sepia"
Assistant: Applied sepia effect to 2 image(s).
```

### Invert Colors
Inverts all colors (negative effect)

**Commands:**
- "Invert colors"
- "Invert image colors"

**Example:**
```
You: "Invert colors"
Assistant: Inverted colors of 1 image(s).
```

---

## Transformations

### Flip Horizontal
Mirrors image left-to-right

**Commands:**
- "Flip horizontal"
- "Flip horizontally"
- "Mirror horizontal"

**Example:**
```
You: "Flip horizontal"
Assistant: Flipped 2 image(s) horizontally.
```

### Flip Vertical
Mirrors image top-to-bottom

**Commands:**
- "Flip vertical"
- "Flip vertically"
- "Mirror vertical"

**Example:**
```
You: "Flip vertical"
Assistant: Flipped 1 image(s) vertically.
```

---

## How to Use

### Step 1: Load Images
```
"Find all .jpg on desktop"
```

### Step 2: Select Images
```
"Select all"
```
Or click to select specific images

### Step 3: Apply Effect
```
"Blur the edges"
```

### Result
All selected images get the effect applied!

---

## Custom Parameters

### Blur Radius
Specify custom blur amount:
```
"Blur edges 5"    (light blur)
"Blur edges 15"   (heavy blur)
"Blur image 3"    (subtle blur)
```

Default values:
- Edge blur: 10px
- Full blur: 5px

---

## Complete Workflow Example

```
You: "Find vacation images"
Assistant: Opened 5 image(s) successfully!

You: "Select all"
Assistant: Selected all objects on canvas.

You: "Blur the edges 12"
Assistant: Applied edge blur (radius: 12px) to 5 image(s).

You: "Apply sepia"
Assistant: Applied sepia effect to 5 image(s).
```

Result: All vacation photos now have blurred edges and vintage sepia tone!

---

## All Supported Effects

| Effect | Command | Parameters |
|--------|---------|------------|
| Edge Blur | "blur edges" | radius (default: 10) |
| Full Blur | "blur image" | radius (default: 5) |
| Grayscale | "grayscale" | none |
| Sepia | "sepia" | none |
| Invert | "invert colors" | none |
| Flip H | "flip horizontal" | none |
| Flip V | "flip vertical" | none |

---

## Tips

1. **Select First**: Always select images before applying effects
2. **Undo Available**: Use Ctrl+Z to undo effects
3. **Batch Processing**: Select multiple images to apply effect to all
4. **Custom Blur**: Add numbers for custom blur radius
5. **Combine Effects**: Apply multiple effects in sequence

---

## Example Workflows

### Vintage Photo Effect
```
1. "Find photos in pictures"
2. "Select all"
3. "Apply sepia"
4. "Blur edges 8"
```

### Artistic Black & White
```
1. "Open *.jpg"
2. "Select all"
3. "Make grayscale"
4. "Blur edges 15"
```

### Mirror Effect
```
1. "Find portrait image"
2. "Flip horizontal"
```

---

## Status

Build: Successful
Effects: 7 implemented
Selection: Required
Parameters: Customizable
Ready: Yes!

---

Try it now:
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

Press Ctrl+H and try:
```
"Find all .jpg on desktop"
"Select all"
"Blur the edges"
```

Watch your images transform! 🎨✨
