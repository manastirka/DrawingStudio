# Complete AI Command Guide

## All Supported Commands

### File Search Commands

#### 1. Find All Images in a Directory
```
"find all images on desktop"
"find images in downloads"
"find pictures in documents"
```

#### 2. Find Specific File Types in Directory
```
"find all .jpg on desktop"
"find all .png in downloads"
"*.jpg on desktop"
```

#### 3. Find by Keyword
```
"find guitar images"
"find vacation photos"
"find screenshot"
```

#### 4. Find by Keyword in Specific Directory
```
"find guitars in desktop"
"find vacation images in pictures"
"find logo in downloads"
```

#### 5. Wildcard Search
```
"*.jpg"          (current directory)
"*.png"          (current directory)
```

---

## How It Works

### Directory Detection
Recognizes these keywords:
- **desktop** → ~/Desktop
- **downloads** → ~/Downloads  
- **pictures** → ~/Pictures
- **documents** → ~/Documents

### File Type Detection
Supports:
- jpg, jpeg, png, gif, bmp, svg, webp

### Keyword Search
Searches filenames for your keyword across all specified directories

---

## Examples

### Example 1: All JPGs on Desktop
```
You: "find all .jpg on desktop"
Assistant: ✓ Opened 12 image(s) successfully!
           Files: photo1.jpg, photo2.jpg, ...
```

### Example 2: Keyword Search
```
You: "find guitar images"
Assistant: ✓ Opened 3 image(s) successfully!
           Files: guitar_red.jpg, electric_guitar.png, ...
```

### Example 3: Specific Directory + Keyword
```
You: "find guitars in desktop"
Assistant: ✓ Opened 5 image(s) successfully!
           Files: ~/Desktop/guitar1.jpg, ~/Desktop/my_guitar.png, ...
```

### Example 4: All Images in Directory
```
You: "find images in downloads"
Assistant: ✓ Opened 25 image(s) successfully!
           (Opens ALL images from Downloads folder)
```

---

## Drawing Commands

### Tools
- "Draw a circle"
- "Draw a rectangle"
- "Draw a line"
- "Draw text"

### Selection
- "Select all"
- "Deselect"
- "Delete selected"

### Canvas
- "Clear canvas"
- "Zoom in"
- "Zoom out"

### Layers
- "Create a new layer"

---

## Tips

1. **Be Specific**: "find guitars in desktop" is better than "find guitars"
2. **Use Extensions**: ".jpg on desktop" finds only JPG files
3. **Keywords Work**: Filename must contain the keyword
4. **Multiple Results**: All matching files will be opened

---

## Status

✅ System-wide search
✅ Directory-specific search  
✅ Keyword search
✅ Extension filtering
✅ 20+ commands
✅ Ready to use!
