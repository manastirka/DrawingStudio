# Move Tool Debugging Guide

## Issue
Images are selected but won't move when using the Move tool.

## Debug Build Complete
Added extensive debug logging to track what's happening.

---

## How to Debug

### 1. Run from Terminal
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

### 2. Test Workflow
```
1. Open images: "Find all .jpg on desktop"
2. Select images: "Select all"
3. Switch to Move tool (toolbar or press M)
4. Try to drag an image
```

### 3. Watch Debug Output
You'll see messages like:
```
Move tool called! button: 1
Searching for primitive at 150.5, 200.3
Found primitive: YES
Clicked object type: 5
Is ImagePrimitive: YES
Move tool: Selected object before moving
Started moving object(s)
ImagePrimitive::translate: offset 10.2, 5.3 old pos: 100, 100
  new pos: 110.2, 105.3
```

---

## What the Debug Output Tells Us

### If you see "Found primitive: NO"
**Problem**: Click detection isn't finding the image
**Possible causes:**
- Image is on a locked layer
- Image is not visible
- Click is outside image bounds
- containsPoint() is returning false

### If you see "Found primitive: YES" but no movement
**Problem**: Move operation isn't starting
**Check for:**
- Is `m_isMoving` being set to true?
- Is `handleMoveOperation` being called?

### If you see translate() calls but no visual change
**Problem**: Rendering issue
**Check for:**
- Is canvas.update() being called?
- Is texture being regenerated?

---

## Common Issues & Solutions

### Issue 1: Layer is Locked
**Symptom**: "Found primitive: NO" even when clicking on image
**Solution**: 
```
Check Layers panel
Unlock the layer containing images
```

### Issue 2: Wrong Tool Selected
**Symptom**: Nothing happens when clicking
**Solution**:
```
Make sure Move tool is active (not Select tool)
Press M key or click Move tool in toolbar
```

### Issue 3: Images Not Selectable
**Symptom**: Can't select images at all
**Solution**:
```
Use Select tool first (S key)
Click on image to select it
Then switch to Move tool (M key)
```

---

## Expected Debug Flow

### Successful Move Operation:
```
1. Move tool called! button: 1
2. Searching for primitive at X, Y
3. Found primitive: YES
4. Clicked object type: 5 (Image)
5. Is ImagePrimitive: YES
6. Started moving object(s)
7. [Mouse move events]
8. ImagePrimitive::translate: offset ...
9. Finished moving objects. Total offset: ...
```

---

## Quick Test

### Test 1: Can you select images?
```
1. Switch to Select tool (S)
2. Click on an image
3. Should see blue selection handles
```

### Test 2: Can you move with Select tool?
```
1. Select tool active
2. Image selected
3. Drag the image
4. Should move
```

### Test 3: Can you move with Move tool?
```
1. Move tool active (M)
2. Click and drag image
3. Should move
```

---

## Next Steps

Run the app from terminal and share the debug output when you try to move an image. This will tell us exactly where the issue is!

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio 2>&1 | grep -E "Move tool|Found primitive|translate|ImagePrimitive"
```

This filters output to show only move-related messages.
