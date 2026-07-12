# Text Box & Word Wrap Feature

## Overview

The text box feature allows you to constrain text within a defined rectangular area, automatically wrapping text to fit within the specified width. This is essential for creating formatted text layouts, paragraphs, and text-heavy designs.

## Features

### ✅ Automatic Word Wrapping
- Text automatically wraps at the specified box width
- Respects word boundaries (won't break words mid-way)
- Handles multiple paragraphs (preserves line breaks)
- **Strictly clips text that exceeds box boundaries** (text cannot render outside box)

### ✅ Visual Feedback
- **Dashed blue outline** shows text box boundaries when selected
- **8 resize handles** (4 corners + 4 edges) for interactive resizing
- **Green rotation handle** at top center for rotating text
- All handles are visible at constant size regardless of zoom level

### ✅ Interactive Resizing
- Drag any corner handle to resize proportionally
- Drag edge handles to resize width or height independently
- Text automatically reflows as you resize
- Real-time preview of text wrapping

## Usage

### Method 1: Text Box Dialog (Recommended)

1. **Create or select text** on the canvas
2. **Open dialog**: `Format → Text Box...` (or select text and use menu)
3. **Configure settings**:
   - ☑️ Enable Text Box (Word Wrap) - Toggle on/off
   - Set Width (50-10000 px) - Controls where text wraps
   - Set Height (50-10000 px) - Maximum text area height
4. **Use quick presets**:
   - Narrow (200px) - For narrow columns
   - Medium (400px) - Standard paragraph width
   - Wide (600px) - Wide text blocks
5. **Click OK** to apply

### Method 2: Quick Toggle (Fastest)

**Keyboard Shortcut:** `Ctrl+Shift+W`

- **If word wrap is OFF**: Enables with default size (400 x 300 px)
- **If word wrap is ON**: Disables word wrap (text flows freely)

Status bar shows current state after toggle.

### Method 3: Interactive Handles

1. **Select text** with word wrap enabled
2. **Drag resize handles**:
   - **Corner handles**: Resize proportionally
   - **Edge handles**: 
     - Left/Right: Adjust width (changes wrap point)
     - Top/Bottom: Adjust height (changes clipping)
3. **Text reflows automatically** as you drag

## Text Alignment with Word Wrap

Word wrap works with all text alignment modes:

- **Left**: Text wraps and aligns to left edge
- **Center**: Each wrapped line is centered
- **Right**: Each wrapped line aligns to right edge
- **Justify**: Text stretches to fill width (except last line)

Change alignment: `Format → Text Alignment`

## Technical Details

### How Word Wrapping Works

1. **Text is split** into paragraphs (by `\n` characters)
2. **Each paragraph** is split into words (by spaces)
3. **Words are accumulated** into lines until width is exceeded
4. **Letter spacing** is respected in width calculations
5. **Lines are rendered** with proper alignment
6. **Text is clipped** if it exceeds box height

### Text Box Dimensions

- **Width = 0, Height = 0**: Word wrap disabled (default)
- **Width > 0, Height > 0**: Word wrap enabled
- Dimensions are in **world space pixels** (scale with zoom)
- Minimum size: 50 x 50 px
- Maximum size: 10000 x 10000 px

### Performance

- Word wrapping is **highly optimized** for real-time editing
- Uses efficient character-by-character width calculation
- Handles thousands of characters smoothly
- No lag during interactive resizing

## Use Cases

### 📝 Paragraphs & Articles
```
Width: 400-600px
Height: Auto (large value)
Alignment: Left or Justify
```

### 📊 Labels & Captions
```
Width: 200-300px
Height: 100-150px
Alignment: Center
```

### 📰 Narrow Columns
```
Width: 150-250px
Height: Auto
Alignment: Left
```

### 🎨 Text Blocks in Designs
```
Width: Custom (fit design)
Height: Custom (fit design)
Alignment: Any
```

## Tips & Tricks

### 💡 Quick Workflow

1. **Create text** without word wrap first
2. **Type all content** freely
3. **Enable word wrap** when ready to format
4. **Adjust width** by dragging right edge handle
5. **Fine-tune** with Text Box dialog if needed

### 💡 Disable Word Wrap Temporarily

- Press `Ctrl+Shift+W` to toggle off
- Edit text freely
- Press `Ctrl+Shift+W` again to re-enable

### 💡 Copy Text Box Settings

1. Select text with desired box size
2. Note dimensions in status bar
3. Apply same dimensions to other text objects

### 💡 Text Overflow Handling

If text exceeds box boundaries:
- Text is **strictly clipped** at box edges (cannot render outside)
- Overflow text is **not visible** (clean, professional appearance)
- **Solution**: Increase box height or reduce font size
- **No visual overflow indicator** (by design - maintains clean look)

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+W` | Toggle word wrap on/off |
| `Format → Text Box...` | Open text box dialog |
| Drag handles | Resize text box interactively |

## Troubleshooting

### Text Not Wrapping?

✅ **Check**: Is text box enabled? (Width/Height > 0)
✅ **Try**: `Ctrl+Shift+W` to toggle on
✅ **Verify**: Select text and check for blue dashed outline

### Text Disappears?

✅ **Cause**: Text exceeds box height (clipped)
✅ **Fix**: Increase height or reduce font size
✅ **Check**: Open Text Box dialog and increase height

### Can't Resize Text Box?

✅ **Check**: Is text selected? (Must see handles)
✅ **Try**: Click text to select, then drag handles
✅ **Verify**: Blue dashed outline should be visible

### Word Wrap Ignores Line Breaks?

✅ **Expected**: Manual line breaks (`\n`) are preserved
✅ **Behavior**: Each paragraph wraps independently
✅ **Tip**: Use empty lines to separate paragraphs

## Examples

### Example 1: Simple Paragraph
```
Text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit..."
Width: 400px
Height: 200px
Result: Text wraps at 400px, multiple lines, left-aligned
```

### Example 2: Centered Caption
```
Text: "Figure 1: A beautiful sunset over the mountains"
Width: 300px
Height: 80px
Alignment: Center
Result: Text wraps and each line is centered
```

### Example 3: Narrow Column
```
Text: "This is a narrow column of text for magazine layouts"
Width: 180px
Height: 500px
Alignment: Justify
Result: Newspaper-style narrow column with justified text
```

## Future Enhancements

Potential future features:
- Auto-height (expand box to fit all text)
- Text overflow indicators (visual warning)
- Multiple columns within one text box
- Text box templates/presets
- Padding/margins within text box

## Related Features

- **Text Alignment**: `Format → Text Alignment`
- **Letter Spacing**: `Format → Typography → Letter Spacing`
- **Line Spacing**: `Format → Typography → Line Spacing`
- **Font Settings**: Property panel or Format menu
- **Text Effects**: `Format → Text Effects`

---

**Last Updated**: October 2024
**Version**: 1.0
