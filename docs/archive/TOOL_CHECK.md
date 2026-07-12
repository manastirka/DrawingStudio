# Tool Selection Check

## Issue Found!
Your debug output shows `containsPoint` being called (which is good!), but NO "Move tool called!" messages. This means **the Move tool isn't actually active**.

## What the Output Shows

Your output:
```
ImagePrimitive::containsPoint: HIT on opaque image
Horizontal alignment guide: 3 at x= 150
Vertical alignment guide: 6 at y= 150
```

**Missing:**
```
=== MOUSE PRESS EVENT ===
Current tool: 1
Move tool called!
```

This means you're probably in **Select tool** (tool 0), not **Move tool** (tool 1).

---

## How to Fix

### Method 1: Use Keyboard Shortcut
Press **M** key to activate Move tool

### Method 2: Use Toolbar
Click the Move tool icon in the toolbar

### Method 3: Use AI Command
Try: `"Switch to move tool"` (if implemented)

---

## Test Again

After activating Move tool, you should see:

```
=== MOUSE PRESS EVENT ===
Current tool: 1          ← This should be 1 for Move tool
Button: 1
Move tool called! button: 1
```

---

## Tool Numbers Reference

- 0 = Select
- 1 = Move  ← You want this!
- 2 = Line
- 3 = Curve
- etc.

---

## Quick Test

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

Then:
```
1. Load images: "Find all .jpg on desktop"
2. Select images: "Select all"
3. Press M key (Move tool)
4. Click on an image
5. Watch terminal - should see "Current tool: 1"
```

---

## Why Select Tool Doesn't Move

The Select tool (tool 0) can also move objects, but it has different behavior:
- It prioritizes selection over movement
- It checks control points first
- It might be blocked by other interactions

The **Move tool** (tool 1) is specifically designed for moving and should work better.

---

Try pressing **M** and then moving an image!
