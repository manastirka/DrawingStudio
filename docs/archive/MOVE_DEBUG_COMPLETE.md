# Complete Move Tool Debug Guide

## Maximum Debug Output Added!

I've added extensive logging to track EVERY step of the move operation.

---

## Run with Full Debug

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

---

## What You'll See

### When You Click (Mouse Press):
```
Move tool called! button: 1
Move tool: Left button pressed, selected objects count: 6
Skipping control point check for ImagePrimitive
Clicked on already-selected object
Clicked object type: 5
Is ImagePrimitive: YES
=== MOVE OPERATION STARTED ===
m_isMoving = true
m_moveStartPos = 150.5, 200.3
Selected objects count: 6
```

### When You Drag (Mouse Move):
```
mouseMoveEvent: m_isMoving=true, calling handleMoveOperation
handleMoveOperation called, worldPos: 160.7, 205.6
deltaOffset: 10.2, 5.3
selected objects: 6
Calling translate on object
ImagePrimitive::translate: offset 10.2, 5.3 old pos: 100, 100
  new pos: 110.2, 105.3
Calling translate on object
ImagePrimitive::translate: offset 10.2, 5.3 old pos: 200, 100
  new pos: 210.2, 105.3
... (for each selected object)
Canvas updated
```

---

## Diagnostic Steps

### Step 1: Check if Move Operation Starts
Look for:
```
=== MOVE OPERATION STARTED ===
m_isMoving = true
```

**If you DON'T see this:**
- Move tool isn't detecting the click
- Object might not be found at click position
- Check: "Found primitive: YES" should appear

### Step 2: Check if Mouse Move is Detected
Look for:
```
mouseMoveEvent: m_isMoving=true, calling handleMoveOperation
```

**If you DON'T see this:**
- Mouse move events aren't being captured
- Something is blocking mouseMoveEvent
- Check if another tool/mode is active

### Step 3: Check if translate() is Called
Look for:
```
Calling translate on object
ImagePrimitive::translate: offset ...
```

**If you DON'T see this:**
- m_selectedObjects might be empty
- Objects might be null pointers

### Step 4: Check if Canvas Updates
Look for:
```
Canvas updated
```

**If you see this but no visual change:**
- Rendering issue
- Texture not updating
- OpenGL context problem

---

## Test Sequence

### Test 1: Basic Move
```
1. Open app from terminal
2. "Find all .jpg on desktop"
3. "Select all"
4. Press M (Move tool)
5. Click on an image
6. Drag slowly
7. Watch terminal output
```

### Test 2: Check Each Stage
```
Stage 1: Click
- Should see "=== MOVE OPERATION STARTED ==="

Stage 2: Drag
- Should see "mouseMoveEvent: m_isMoving=true"
- Should see "handleMoveOperation called"

Stage 3: Translate
- Should see "Calling translate on object"
- Should see "ImagePrimitive::translate"

Stage 4: Visual
- Should see "Canvas updated"
- Should see image move on screen
```

---

## Common Problems

### Problem 1: "Found primitive: NO"
**Cause**: Click not hitting image
**Solution**:
- Click directly on image center
- Check if layer is locked
- Check if image is visible

### Problem 2: No "mouseMoveEvent" messages
**Cause**: Mouse move not captured
**Solution**:
- Make sure you're dragging (not just clicking)
- Check if another mode is active (panning, etc.)
- Try clicking and holding, then dragging

### Problem 3: translate() called but no movement
**Cause**: Position update not working
**Solution**:
- Check if old pos and new pos are different
- Check if offset is non-zero
- Verify canvas.update() is called

### Problem 4: Canvas updated but no visual change
**Cause**: Rendering issue
**Solution**:
- Try zooming in/out
- Try panning
- Check OpenGL context

---

## What to Share

If it still doesn't work, run this and share the output:

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio 2>&1 | tee move_debug.log
```

Then:
1. Load images
2. Select all
3. Try to move
4. Share the move_debug.log file

---

## Expected Full Output

Here's what a WORKING move operation looks like:

```
Move tool called! button: 1
Move tool: Left button pressed, selected objects count: 6
Skipping control point check for ImagePrimitive
Clicked on already-selected object
Clicked object type: 5
Is ImagePrimitive: YES
=== MOVE OPERATION STARTED ===
m_isMoving = true
m_moveStartPos = 150.5, 200.3
Selected objects count: 6
mouseMoveEvent: m_isMoving=true, calling handleMoveOperation
handleMoveOperation called, worldPos: 151.2, 201.1
deltaOffset: 0.7, 0.8
selected objects: 6
Calling translate on object
ImagePrimitive::translate: offset 0.7, 0.8 old pos: 100, 100
  new pos: 100.7, 100.8
Canvas updated
mouseMoveEvent: m_isMoving=true, calling handleMoveOperation
handleMoveOperation called, worldPos: 152.5, 202.3
deltaOffset: 1.3, 1.2
... (continues as you drag)
```

---

This debug output will tell us EXACTLY where the problem is!
