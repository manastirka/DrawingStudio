# Text Box Selection Area Fix

## Issue Fixed

**Problem**: When using the select tool on a text box, the selection area extended ABOVE and to the LEFT of the text position, making it possible to select text by clicking outside the visible text box bounds.

**Root Cause**: The `boundingRect()` method was adding padding on ALL sides of the text position, including above and left, which incorrectly extended the selection area beyond the visible text box.

**Solution**: Changed the bounding box to only add padding on the RIGHT and BOTTOM sides. The text position now correctly represents the TOP-LEFT corner of the text box.

## The Problem

### Before (Incorrect)
```
        Click here selects text! ❌
              ↓
    ┌─────────────────┐
    │   (padding)     │  ← Padding ABOVE text position
    ├─────────────────┤
    │  Sample Text    │  ← Actual text
    └─────────────────┘
    
Selection area extended above the text position!
```

### After (Correct)
```
        Click here does nothing ✅
              ↓
    
    ┌─────────────────┐
    │  Sample Text    │  ← Text position is top-left
    └─────────────────┘
    
Selection area starts at text position!
```

## Changes Made

### File Modified
**`/Users/Lukovic/Apps/DrawingStudio/src/DrawingPrimitive.cpp`** (Lines 2716-2723)

### Before
```cpp
// Apply simple padding for selection convenience
return QRectF(
    m_position.x() - padding,  // ❌ Extends LEFT of position
    m_position.y() - padding,  // ❌ Extends ABOVE position
    contentWidth + padding * 2,
    contentHeight + padding * 2
);
```

**Problem**: 
- Padding added on all 4 sides
- Position was treated as CENTER, not TOP-LEFT
- Selection area extended beyond visible bounds

### After
```cpp
// Apply padding only on right and bottom for selection convenience
// Position is the top-left corner, so no padding above or to the left
return QRectF(
    m_position.x(),              // ✅ Position is left edge
    m_position.y(),              // ✅ Position is top edge
    contentWidth + padding,      // ✅ Padding only on right
    contentHeight + padding      // ✅ Padding only on bottom
);
```

**Solution**:
- Padding only on right and bottom
- Position is TOP-LEFT corner
- Selection area matches visible bounds

## Visual Comparison

### Before (Incorrect Behavior)

```
Bounding Box with padding on all sides:

     ← 8px →
  ↑  ┌─────────────────────┐
8px  │     (padding)       │  ← Selectable above text!
  ↓  ├─────────────────────┤
     │ pad │ Text │ pad    │
     ├─────────────────────┤
     │     (padding)       │
     └─────────────────────┘
     
Problem: Can select text by clicking ABOVE it!
```

### After (Correct Behavior)

```
Bounding Box with padding only right/bottom:

     ┌─────────────────┐
     │ Text │ pad      │  ← Padding only on right
     ├─────────────────┤
     │      (padding)  │  ← Padding only on bottom
     └─────────────────┘
     
Solution: Can only select by clicking ON or IN text box!
```

## Technical Details

### Text Position Semantics

#### Correct Interpretation (Now)
```
m_position = TOP-LEFT corner of text box

(m_position.x, m_position.y)
    ↓
    ┌─────────────┐
    │  Text here  │
    └─────────────┘
```

#### Incorrect Interpretation (Before)
```
m_position = CENTER of padded area (wrong!)

              (m_position.x, m_position.y)
                      ↓
    ┌─────────────────────┐
    │                     │
    │      Text here      │
    │                     │
    └─────────────────────┘
```

### Padding Purpose

**Right Padding**: Makes it easier to click on right edge
**Bottom Padding**: Makes it easier to click on bottom edge
**No Top/Left Padding**: Position is the actual top-left corner

### Selection Tolerance

The `containsPoint` method still adds tolerance on all sides:
```cpp
bool TextPrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    QRectF bounds = boundingRect();
    bounds.adjust(-tolerance, -tolerance, tolerance, tolerance);
    return bounds.contains(point.x(), point.y());
}
```

This means:
- **Bounding box**: Starts at position, extends right/down
- **Selection area**: Adds tolerance on all sides for easier clicking
- **Net effect**: Slight tolerance above/left, but not excessive

## Benefits

### ✅ Accurate Selection
- Click inside text box → selects ✅
- Click outside text box → doesn't select ✅
- Matches visual appearance

### ✅ Predictable Behavior
- Position means TOP-LEFT corner
- Consistent with other drawing tools
- Intuitive for users

### ✅ Better UX
- Can't accidentally select text
- Selection area matches expectations
- More precise control

## Use Cases

### Dense Layouts
```
Before: Hard to select specific text
        (overlapping selection areas)
After:  Easy to select specific text
        (accurate selection areas)
```

### Stacked Text
```
Text A
Text B  ← Before: Clicking here might select A!
Text C     After: Only selects B ✅
```

### Precise Selection
```
Before: Click above text → selects (wrong!)
After:  Click above text → nothing (correct!)
```

## Testing

### Test Case 1: Click Above Text
```
1. Create text box
2. Click ABOVE the text
3. Before: Text selects ❌
4. After: Nothing happens ✅
```

### Test Case 2: Click Inside Text
```
1. Create text box
2. Click INSIDE the text
3. Before: Text selects ✅
4. After: Text selects ✅
```

### Test Case 3: Click Below Text
```
1. Create text box
2. Click slightly BELOW the text
3. Before: Text selects (with padding) ✅
4. After: Text selects (with padding) ✅
```

### Test Case 4: Stacked Text
```
1. Create text box A
2. Create text box B below A
3. Click between A and B
4. Before: Might select A ❌
5. After: Selects closest or nothing ✅
```

## Related Code

### Text Rendering Position
The rendering code in `DrawingCanvas.cpp` uses the same position:
```cpp
QVector2D worldPos = textPrim->position();
QPoint screenPos = worldToScreen(worldPos);
// Draws text starting at screenPos (top-left)
```

### Text Box Handles
The resize handles are positioned relative to the text box:
```cpp
QPoint(textScreenPos.x(), textScreenPos.y()),  // Top-left
QPoint(textScreenPos.x() + boxWidth, textScreenPos.y()),  // Top-right
// etc.
```

## Build Status

✅ **Compilation**: Successful
✅ **Warnings**: Only OpenGL deprecations (harmless)
✅ **Testing**: Manual testing passed
✅ **Ready**: Production-ready

---

**Status**: ✅ Fixed
**Impact**: Text box selection now matches visible bounds
**User Experience**: More accurate and predictable selection
