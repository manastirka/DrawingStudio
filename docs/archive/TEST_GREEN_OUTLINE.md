# Testing Green Outline - Debug Guide

## Current Status

The app has been rebuilt with:
1. ✅ Green outline rendering code
2. ✅ Qt signals for canvas updates
3. ✅ Improved SAM2 detection
4. ✅ Debug output added

## Step-by-Step Test

### 1. Check SAM2 Service is Running
```bash
curl http://localhost:5001/health
```
**Expected**: `{"device":"mps","sam2_loaded":true,"status":"ok"}`

### 2. Launch App (Already Running)
The app should be running now. If not:
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

### 3. Import an Image
1. Press **`I`** key (select Image tool)
2. **File → Open**
3. Choose any image file (JPG, PNG, etc.)

### 4. Watch Console Output

**Look for these messages in order:**

#### When Image Loads:
```
Loading image from: "/path/to/image.jpg"
Image loaded successfully: true
Image size: QSize(width, height)
ImagePrimitive: Starting auto-detection of subject...
SAM2: Sending image for ALL objects detection...
```

#### During Detection (in SAM2 terminal):
```
Processing image for ALL objects: (height, width, 3)
SAM2 automatic mask generator found X masks
After filtering: Y high-quality objects
  Object 0: score=0.XXX, stability=0.9XX, iou=0.9XX, area=XX.X%
```

#### When Detection Completes:
```
SAM2: Found X objects, returning top 1
ImagePrimitive: SAM2 segmentation complete, contour points: XXX
ImagePrimitive: Subject detection complete and ready for extraction
ImagePrimitive: Green outline should now be visible on canvas
DrawingCanvas: Detection complete signal received, updating canvas
```

#### When Rendering:
```
RENDERING GREEN OUTLINE - Contour points: XXX
```
**This message should appear repeatedly as the canvas redraws!**

## Diagnostic Checklist

### ❓ No "Starting auto-detection" message?
- Image didn't load properly
- Check file path and permissions

### ❓ No "SAM2: Sending image" message?
- SAM2Client not initialized
- Check SAM2 service is running

### ❓ No "segmentation complete" message?
- Detection failed or timed out
- Check SAM2 service terminal for errors
- Try a different image

### ❓ No "Detection complete signal received" message?
- Signal not connected
- This shouldn't happen with latest build

### ❓ No "RENDERING GREEN OUTLINE" message?
- Contour is empty (detection failed)
- Canvas not calling render()
- Check if image is visible on canvas

### ❓ "RENDERING GREEN OUTLINE" appears but no visible outline?
- OpenGL rendering issue
- Check if other primitives render correctly
- Try zooming in/out

## Quick Test Commands

### Check if app is running:
```bash
ps aux | grep DrawingStudio | grep -v grep
```

### Check app console output:
```bash
tail -f /tmp/drawingstudio.log
```

### Check SAM2 service:
```bash
lsof -i :5001
```

### Restart SAM2 service if needed:
```bash
pkill -f sam2_service.py
cd /Users/Lukovic/Apps/DrawingStudio/sam2_service
source venv/bin/activate && python3 sam2_service.py
```

## What Should Happen

### Timeline:
1. **T+0s**: Import image → Image appears on canvas
2. **T+0.1s**: "Starting auto-detection" message
3. **T+0.2s**: "SAM2: Sending image" message
4. **T+1-3s**: SAM2 processes (service terminal shows progress)
5. **T+1-3s**: "segmentation complete" message
6. **T+1-3s**: "Detection complete signal received" message
7. **T+1-3s**: Canvas updates → render() called
8. **T+1-3s**: "RENDERING GREEN OUTLINE" message
9. **T+1-3s**: **GREEN OUTLINE VISIBLE ON SCREEN** ✅

## If Still No Green Outline

### Possible Issues:

1. **Detection Failed**
   - Contour is empty
   - Check SAM2 service logs
   - Try a simpler image (clear subject, good contrast)

2. **Render Not Called**
   - Canvas not updating
   - Check if "RENDERING GREEN OUTLINE" appears in console
   - If not, canvas update() not working

3. **OpenGL Issue**
   - Rendering code has bug
   - Check if image itself renders
   - Check if other shapes render

4. **Signal Not Connected**
   - "Detection complete signal received" not appearing
   - Rebuild app: `cd build && make`

5. **Wrong Build Running**
   - Old executable still running
   - Kill all: `pkill DrawingStudio`
   - Launch new: `./build/DrawingStudio`

## Next Steps

**Please provide:**
1. Console output from app (first 50 lines after importing image)
2. SAM2 service output (when detection runs)
3. Does "RENDERING GREEN OUTLINE" appear? How many times?
4. Is the image visible on canvas?
5. Are other shapes (lines, rectangles) visible?

This will help identify exactly where the issue is.
