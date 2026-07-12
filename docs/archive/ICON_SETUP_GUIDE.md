# Custom Icon Setup Guide

## 📁 Directory Structure

Create this folder structure:
```
/Users/Lukovic/Apps/DrawingStudio/
└── resources/
    └── icons/
        ├── select.png
        ├── move.png
        ├── line.png
        ├── rectangle.png
        ├── ellipse.png
        ├── circle.png
        ├── arc.png
        ├── curve.png
        ├── bezier.png
        ├── spline.png
        ├── polygon.png
        ├── text.png
        ├── brush.png
        ├── eraser.png
        ├── fill.png
        ├── blur.png
        ├── measure.png
        ├── image.png
        └── hand.png
```

## 🎨 Icon Specifications

### Recommended Format
- **Format**: PNG with transparency
- **Size**: 32x32 pixels (will be auto-scaled if different)
- **Color**: Any - but consider the dark toolbar background
- **Style**: Modern, consistent across all icons

### Icon Names (must match exactly)
| Tool | Filename | Description |
|------|----------|-------------|
| Select | `select.png` | Selection cursor/arrow |
| Move | `move.png` | Move/pan hand |
| Line | `line.png` | Straight line |
| Rectangle | `rectangle.png` | Rectangle shape |
| Ellipse | `ellipse.png` | Ellipse/oval shape |
| Circle | `circle.png` | Perfect circle |
| Arc | `arc.png` | Curved arc |
| Curve | `curve.png` | Free-form curve |
| Bezier | `bezier.png` | Bezier curve with control points |
| Spline | `spline.png` | Smooth spline curve |
| Polygon | `polygon.png` | Multi-sided polygon |
| Text | `text.png` | Text/typography |
| Brush | `brush.png` | Paint brush |
| Eraser | `eraser.png` | Eraser tool |
| Fill | `fill.png` | Paint bucket/fill |
| Blur | `blur.png` | Blur effect |
| Measure | `measure.png` | Measurement/dimension |
| Image | `image.png` | Image/photo |
| Hand | `hand.png` | Pan/move canvas |

## 🚀 How It Works

### Automatic Loading
The app will automatically:
1. Check for custom icons in `/Users/Lukovic/Apps/DrawingStudio/resources/icons/`
2. Load your custom PNG files if they exist
3. Auto-scale them to 32x32 if needed
4. Fall back to programmatic icons if files don't exist

### Testing Your Icons
1. Create the `resources/icons/` folder
2. Add your PNG files with exact names from the table above
3. Restart the application
4. Check the console for messages like:
   - `"Loaded custom icon: /path/to/icon.png"` ✅ Success
   - `"Using fallback icon for: iconname"` ⚠️ File not found

## 💡 Tips for Creating Icons

### Using AI (Stable Diffusion, DALL-E, etc.)
**Prompt suggestions:**
```
"Modern minimalist [tool name] icon, 32x32 pixels, flat design, 
blue and white color scheme, transparent background, vector style"
```

**Examples:**
- Select: "cursor arrow icon, modern UI, blue gradient"
- Line: "straight line icon with endpoints, geometric, minimal"
- Rectangle: "rectangle shape icon, rounded corners, gradient fill"
- Brush: "paint brush icon, artistic, modern design"

### Design Consistency
- Use similar color palette across all icons
- Keep visual weight balanced
- Ensure icons are recognizable at small size
- Test on dark background (toolbar is dark)

### Tools You Can Use
- **Stable Diffusion** (integrated in your app)
- **DALL-E / Midjourney** (online)
- **Figma / Adobe Illustrator** (manual design)
- **Icon generators** (online tools)

## 🔄 Updating Icons
To update icons:
1. Replace the PNG file in `resources/icons/`
2. Restart the application
3. New icon will load automatically

## ❌ Troubleshooting

### Icon not loading?
- Check filename matches exactly (case-sensitive)
- Verify file is in correct folder
- Ensure file is valid PNG format
- Check console output for error messages

### Icon looks blurry?
- Create icon at exactly 32x32 pixels
- Use PNG format with transparency
- Avoid JPEG (no transparency support)

### Icon too dark/light?
- Remember toolbar background is dark (#2c3e50)
- Use lighter colors or add glow effects
- Test visibility against dark background

## 📝 Example Workflow

1. **Generate icons with AI:**
   ```
   Use Stable Diffusion with prompt:
   "minimalist line tool icon, 32x32, blue gradient, transparent background"
   ```

2. **Save files:**
   ```
   Save as: line.png
   Location: /Users/Lukovic/Apps/DrawingStudio/resources/icons/
   ```

3. **Restart app:**
   ```
   Console shows: "Loaded custom icon: .../line.png"
   ```

4. **Verify:**
   ```
   Check toolbar - your custom icon appears!
   ```

## 🎯 Current Status

The app is configured to load custom icons. Currently:
- ✅ Icon loading system: **Active**
- ✅ Fallback icons: **Working** (programmatic blue gradients)
- ⏳ Custom icons: **Waiting for your PNG files**

Once you add PNG files to `resources/icons/`, they will automatically replace the fallback icons!
