# LLM-Powered Command Parsing

## ✅ Intelligent Command Understanding!

The AI Assistant now uses **Llama 3.2** to understand your commands, handling:
- ✅ **Spelling mistakes**
- ✅ **Natural variations**
- ✅ **Different phrasings**
- ✅ **Typos and errors**

---

## How It Works

### Before (Keyword Matching):
```
"select all" → ✓ Works
"slect all" → ✗ Doesn't work
"choose everything" → ✗ Doesn't work
```

### After (LLM Parsing):
```
"select all" → ✓ select_all
"slect all" → ✓ select_all (fixed typo)
"choose everything" → ✓ select_all (understood intent)
"arange images" → ✓ arrange_images (fixed spelling)
"make it gray" → ✓ grayscale (natural language)
```

---

## Examples

### Spelling Mistakes
```
"slect all" → select_all
"arange images" → arrange_images
"graysc ale" → grayscale
"flip horizontaly" → flip_horizontal
```

### Natural Variations
```
"choose all" → select_all
"pick everything" → select_all
"make it black and white" → grayscale
"turn it gray" → grayscale
"mirror left to right" → flip_horizontal
"flip sideways" → flip_horizontal
```

### Different Phrasings
```
"open all pictures" → open_images
"load the images" → open_images
"import photos" → open_images
"get rid of selected" → delete_selected
"remove selection" → delete_selected
```

---

## Technical Details

### LLM Prompt
```
System: You are a command parser for a drawing application.
Your job is to identify the user's intent and return ONLY the command name.

Available commands:
- open_images / open_images_arranged
- arrange_images
- select_all / deselect_all
- blur_edges / grayscale / sepia
... etc

Rules:
1. Return ONLY the command name
2. Handle spelling mistakes
3. Understand variations
4. If unsure, return 'unknown'

User: slect all
Assistant: select_all
```

### Fallback System
If LLM fails or returns invalid command:
1. Falls back to keyword matching
2. Ensures commands always work
3. No degradation of functionality

---

## Supported Commands

All these work with variations and typos:

### File Operations
- `open_images` - "open images", "load pictures", "import photos"
- `open_images_arranged` - "open and arrange", "load with spacing"
- `arrange_images` - "arrange", "organize images", "layout pictures"

### Selection
- `select_all` - "select all", "choose everything", "pick all"
- `deselect_all` - "deselect", "clear selection", "unselect"
- `delete_selected` - "delete", "remove selected", "get rid of"

### Drawing
- `draw_circle` - "draw circle", "make circle", "create circle"
- `draw_rectangle` - "draw rect", "make box", "create rectangle"
- `draw_line` - "draw line", "make line", "create line"

### Canvas
- `clear_canvas` - "clear", "erase all", "delete everything"
- `zoom_in` - "zoom in", "enlarge", "make bigger"
- `zoom_out` - "zoom out", "shrink", "make smaller"

### Effects
- `blur_edges` - "blur edges", "edge blur", "vignette"
- `blur_image` - "blur", "make blurry", "blur picture"
- `grayscale` - "grayscale", "black and white", "make gray"
- `sepia` - "sepia", "vintage", "old photo"
- `invert_colors` - "invert", "negative", "reverse colors"
- `flip_horizontal` - "flip horizontal", "mirror", "flip sideways"
- `flip_vertical` - "flip vertical", "flip upside down"

### Layers
- `create_layer` - "new layer", "add layer", "create layer"

---

## Performance

- **Fast**: Only 20 tokens generated (< 1 second)
- **Efficient**: Uses GPU acceleration (Metal)
- **Reliable**: Falls back to keyword matching if needed
- **Smart**: Learns from examples in prompt

---

## Debug Output

When you use a command, you'll see:
```
LLM parsed command: "slect all" → "select_all"
Executing command: select_all
```

This shows:
1. What you typed
2. What the LLM understood
3. What command is being executed

---

## Try It Now!

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

Then try these with typos:
```
"slect all"
"arange images with 5px spacing"
"make it graysc ale"
"flip horizontaly"
"blurr the edges"
"delet selected"
```

All should work! ✨

---

## Benefits

✅ **More forgiving** - Typos don't break commands
✅ **Natural language** - Say it how you think it
✅ **Variations** - Multiple ways to say the same thing
✅ **Smart** - Understands intent, not just keywords
✅ **Fast** - Quick parsing (< 1 second)
✅ **Reliable** - Falls back if needed

---

The AI Assistant is now much smarter and more user-friendly!
