# Font Family Selection Feature

## Overview

The Font Family feature provides easy access to all system fonts through a convenient dropdown menu and a comprehensive font browser dialog. Users can quickly change fonts for selected text or set the default font for new text.

## Features

### ✅ Quick Font Menu
- **15 common fonts** available in dropdown menu
- One-click font selection
- Instant preview on selected text
- Updates default font for new text

### ✅ Font Browser Dialog
- **All system fonts** accessible
- **Live preview** with sample text
- **Search functionality** to filter fonts
- **Font rendering** in list (each font shown in its own typeface)

### ✅ Smart Application
- Applies to **selected text** objects
- Updates **ClassicTextTool** for new text
- **Status bar feedback** confirms changes
- **Instant canvas update**

## How to Use

### Method 1: Quick Font Menu (Fastest)

1. **Select text** on canvas (or none for new text default)
2. **Open menu**: `Format → Font Family`
3. **Click font name** from list:
   - Arial
   - Helvetica
   - Times New Roman
   - Georgia
   - Courier New
   - Verdana
   - Trebuchet MS
   - Comic Sans MS
   - Impact
   - Palatino
   - Garamond
   - Bookman
   - Avant Garde
   - Futura
   - Optima
4. **Done!** Font applied instantly

### Method 2: Font Browser (All Fonts)

1. **Select text** on canvas (optional)
2. **Open browser**: `Format → Font Family → More Fonts...`
3. **Browse or search**:
   - Scroll through complete font list
   - Type in search box to filter (e.g., "mono", "sans", "serif")
4. **Preview**: Click any font to see live preview below
5. **Select**: Click OK to apply

## Font Browser Dialog

### Layout

```
┌─────────────────────────────────────────┐
│ 🔤 Select Font Family                   │
├─────────────────────────────────────────┤
│ Choose a font family from all...        │
├─────────────────────────────────────────┤
│ 🔍 Search fonts...                      │
├─────────────────────────────────────────┤
│ ┌─────────────────────────────────────┐ │
│ │ Arial                               │ │
│ │ Courier New                         │ │
│ │ Georgia                             │ │
│ │ Helvetica                           │ │
│ │ ...                                 │ │
│ └─────────────────────────────────────┘ │
├─────────────────────────────────────────┤
│ The quick brown fox jumps over...      │
│ (Preview in selected font)              │
├─────────────────────────────────────────┤
│                          [OK] [Cancel]  │
└─────────────────────────────────────────┘
```

### Features

#### 🔍 Search Box
- **Real-time filtering** as you type
- **Case-insensitive** search
- Searches font family names
- Example searches:
  - "mono" → finds all monospace fonts
  - "sans" → finds sans-serif fonts
  - "times" → finds Times variants

#### 📋 Font List
- **All system fonts** displayed
- **Each font rendered in its own typeface**
- **Scrollable** list
- **Current font pre-selected** (if text selected)
- Click to select

#### 👁️ Live Preview
- **Sample text**: "The quick brown fox jumps over the lazy dog"
- **Updates instantly** when you click a font
- **16pt size** for clear preview
- Shows actual font rendering

## Common Fonts Included

### Sans-Serif Fonts
- **Arial** - Clean, modern, widely used
- **Helvetica** - Classic Swiss design
- **Verdana** - Optimized for screen readability
- **Trebuchet MS** - Humanist sans-serif
- **Avant Garde** - Geometric sans-serif
- **Futura** - Geometric, modern
- **Optima** - Humanist sans-serif

### Serif Fonts
- **Times New Roman** - Classic book font
- **Georgia** - Screen-optimized serif
- **Palatino** - Elegant old-style serif
- **Garamond** - Classic old-style serif
- **Bookman** - Friendly serif

### Display/Decorative
- **Comic Sans MS** - Casual, informal
- **Impact** - Bold, condensed

### Monospace
- **Courier New** - Classic typewriter font

## Use Cases

### 📝 Professional Documents
```
Font: Times New Roman or Georgia
Use: Reports, articles, formal text
```

### 🎨 Modern Designs
```
Font: Helvetica or Arial
Use: UI text, clean layouts
```

### 💻 Code/Technical
```
Font: Courier New
Use: Code snippets, technical content
```

### 🎭 Creative/Artistic
```
Font: Impact or custom fonts
Use: Headers, posters, emphasis
```

### 📖 Long-Form Reading
```
Font: Georgia or Palatino
Use: Articles, books, extended text
```

## Tips & Tricks

### 💡 Font Discovery
1. Open "More Fonts..." dialog
2. Scroll through entire list
3. Click fonts to preview
4. Discover hidden gems on your system

### 💡 Font Combinations
- **Heading**: Impact or Futura
- **Body**: Georgia or Helvetica
- **Code**: Courier New

### 💡 Search Shortcuts
- Type "mono" → Find monospace fonts
- Type "sans" → Find sans-serif fonts
- Type "script" → Find script/handwriting fonts
- Type "bold" → Find fonts with "bold" in name

### 💡 Quick Font Testing
1. Create sample text
2. Use quick menu to try different fonts
3. See instant results
4. Choose favorite

### 💡 System Font Availability
- Font list shows **all installed fonts**
- Different systems have different fonts
- macOS, Windows, Linux have different defaults
- Install custom fonts to expand options

## Keyboard Workflow

While there's no direct keyboard shortcut for font selection, you can:

1. **Select text**: Click or `Cmd+A`
2. **Open menu**: `Alt+F` (Format) → `F` (Font Family)
3. **Navigate**: Arrow keys
4. **Select**: Enter

Or use the dialog:
1. `Format → Font Family → More Fonts...`
2. Type to search
3. Arrow keys to navigate
4. Enter to select

## Technical Details

### Font Database
- Uses Qt's `QFontDatabase`
- Queries all system fonts
- Returns font families (not individual styles)
- Fonts rendered using Qt's font engine

### Font Application
```cpp
// Apply to selected text
textPrimitive->setFontFamily("Arial");

// Apply to tool (for new text)
classicTextTool->setFontFamily("Arial");
```

### Font Rendering
- Each list item rendered in its own font
- Preview uses 16pt size
- Antialiasing enabled
- Respects font hinting

## Compatibility

### Supported Platforms
✅ **macOS** - Full support, native fonts
✅ **Windows** - Full support, native fonts  
✅ **Linux** - Full support, system fonts

### Font Formats
✅ **TrueType (.ttf)**
✅ **OpenType (.otf)**
✅ **System fonts**
✅ **User-installed fonts**

## Limitations

### Current Limitations
- Shows font **families** only (not individual weights/styles)
- Bold/italic applied separately via formatting options
- No font preview in quick menu (only in dialog)
- No font favorites/recent fonts (yet)

### Future Enhancements
Potential improvements:
- Font favorites list
- Recently used fonts
- Font categories (serif, sans, mono, etc.)
- Font preview in dropdown menu
- Custom font collections
- Font weight/style selection in same dialog

## Related Features

This feature works with:
- **Text Tool** - Create new text with selected font
- **Bold/Italic** - Style text independently of font
- **Font Size** - Adjust size separately
- **Text Box** - Word wrap works with all fonts
- **Text Effects** - Shadows, strokes, gradients apply to any font

## Troubleshooting

### Font Not Appearing?

✅ **Check**: Is font installed on your system?
✅ **Try**: Search for font name in "More Fonts..." dialog
✅ **Verify**: Font might be under different name

### Font Looks Different?

✅ **Cause**: Different font weights/styles
✅ **Solution**: Use Bold/Italic buttons for styling
✅ **Note**: Font family is base font, styles applied separately

### Can't Find Specific Font?

✅ **Use search**: Type partial name in search box
✅ **Check spelling**: Font names are case-sensitive in search
✅ **Install font**: Font might not be on your system

### Preview Not Updating?

✅ **Click font**: Must click font in list to update preview
✅ **Restart dialog**: Close and reopen if stuck
✅ **Check selection**: Make sure font is actually selected

## Examples

### Example 1: Change Heading Font
```
1. Select heading text
2. Format → Font Family → Impact
3. Result: Bold, impactful heading
```

### Example 2: Set Default for New Text
```
1. Don't select anything
2. Format → Font Family → Georgia
3. Create new text → Uses Georgia
```

### Example 3: Find Monospace Font
```
1. Format → Font Family → More Fonts...
2. Search: "mono"
3. Select: Monaco or Courier New
4. OK → Applied
```

### Example 4: Professional Document
```
1. Heading: Impact or Futura
2. Body: Georgia or Times New Roman
3. Code: Courier New
4. Result: Professional, readable layout
```

---

**Status**: ✅ Fully Implemented
**Build**: ✅ Successful
**Ready**: Production-ready
**Version**: 1.0
