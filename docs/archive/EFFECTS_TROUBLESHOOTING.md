# Effects Troubleshooting Guide

## Issue: Effects Applied But No Visual Change

### What's Happening
The effect is being applied to the image data, but you might not see the change immediately due to texture caching.

### Solution 1: Force Refresh
After applying an effect, try:
1. Zoom in/out (mouse wheel)
2. Pan the canvas
3. Click somewhere else and back

### Solution 2: Check Debug Output
Run the app from terminal to see debug messages:
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

Look for messages like:
```
applyEdgeBlurToSelected: Processing 6 selected objects
Found ImagePrimitive, applying edge blur with radius: 15
Edge blur applied successfully to image
Updating canvas after applying edge blur to 6 images
```

### Solution 3: Try Different Effect
If edge blur doesn't show, try a more obvious effect:
```
"Select all"
"Make grayscale"
```

Grayscale is very obvious and should be immediately visible.

### Solution 4: Check Selection
Make sure images are actually selected:
```
"Select all"
```

Then check if images have selection handles (blue boxes around them).

### Common Issues

#### Issue: "No images selected"
**Fix**: Make sure to run "Select all" first

#### Issue: Effect applied but not visible
**Fix**: 
- Try zooming in/out
- Try a more obvious effect (grayscale)
- Check terminal output for errors

#### Issue: Wrong objects selected
**Fix**: Click on canvas background first, then "Select all"

---

## Testing Workflow

### Test 1: Grayscale (Most Obvious)
```
1. "Find all .jpg on desktop"
2. "Select all"
3. "Make grayscale"
```
Result: Images should turn black & white immediately

### Test 2: Sepia
```
1. "Select all"
2. "Apply sepia"
```
Result: Images should turn brown/vintage

### Test 3: Flip
```
1. "Select all"
2. "Flip horizontal"
```
Result: Images should mirror left-right

### Test 4: Edge Blur
```
1. "Select all"
2. "Blur edges 20"
```
Result: Edges should be blurry (may be subtle)

---

## Debug Mode

Run with debug output:
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio 2>&1 | grep -E "apply|blur|Edge|Image"
```

This will show only effect-related messages.

---

## Next Steps

If effects still don't work:
1. Check terminal output
2. Try grayscale first (most obvious)
3. Make sure images are loaded (you can see them)
4. Make sure images are selected (blue handles visible)
