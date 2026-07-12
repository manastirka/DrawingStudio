# How to Use Subject Detection in DrawingStudio

## Prerequisites

**The SAM2 service MUST be running before using subject detection!**

### Start SAM2 Service (in a separate terminal):
```bash
cd /Users/Lukovic/Apps/DrawingStudio
./start_sam2_service.sh
```

Wait for: `✓ Service ready! Listening on http://localhost:5001`

## Step-by-Step Guide

### 1. Launch DrawingStudio
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

### 2. Select the Image Tool
- Look at the **left toolbar**
- Find the **Image tool** icon (looks like a picture/photo icon)
- Click it, or press **`I`** on your keyboard

### 3. Import an Image
**Option A: Use File Menu**
- Go to **File → Open**
- Select an image file (JPG, PNG, etc.)

**Option B: Use Image Tool**
- With the Image tool selected, click on the canvas
- This will open a file dialog to select an image

### 4. Subject Detection UI Appears

Once the Image tool is selected, you'll see these options in the **Tool Settings panel** (right side):

#### Available Controls:
- **Scale slider**: Adjust image size (10-200%)
- **Detection Threshold slider**: Adjust detection sensitivity (1-100%)
- **Lock Aspect Ratio checkbox**: Keep image proportions
- **Use SAM2 (AI) checkbox**: ✅ Enable/disable AI-powered detection
- **Extract Selected Subject button**: Extract the detected subject as a new object

### 5. Automatic Subject Detection

When you import an image:
1. **SAM2 automatically detects the main subject**
2. You'll see a **green outline** around the detected subject
3. The console will show: `"SAM2: Found X objects, returning top 1"`

### 6. Extract the Subject

To extract the detected subject as a separate object:
1. Make sure the image is selected (click on it)
2. Click the **"Extract Selected Subject"** button in the Tool Settings panel
3. The subject will be extracted as a new, movable object
4. You can now move, scale, or manipulate it independently

## Troubleshooting

### "No option for subject detection"

**Solution**: Make sure you've selected the **Image tool** (press `I` or click the image icon in the toolbar). The SAM2 options only appear when the Image tool is active.

### Subject detection not working

**Check these:**

1. **Is SAM2 service running?**
   ```bash
   curl http://localhost:5001/health
   ```
   Should return: `{"device":"mps","sam2_loaded":true,"status":"ok"}`

2. **Is the "Use SAM2 (AI)" checkbox checked?**
   - It should be checked by default
   - If unchecked, it will use classical algorithm (less accurate)

3. **Check the app console for errors**
   - Look for messages starting with "SAM2:"
   - Common errors:
     - "Network error" → SAM2 service not running
     - "No objects detected" → Try a different image or adjust threshold

### SAM2 service not starting

```bash
cd /Users/Lukovic/Apps/DrawingStudio/sam2_service
source venv/bin/activate
python3 sam2_service.py
```

If errors occur, check:
- Virtual environment is activated
- Dependencies installed: `pip install -r requirements.txt`
- SAM2 installed: `cd segment-anything-2 && pip install -e .`

## Tips for Best Results

### Image Selection
- **Clear subjects**: Works best with images that have a clear main subject
- **Good contrast**: Subject should contrast with the background
- **Avoid clutter**: Too many objects may confuse the detector

### Detection Threshold
- **Lower threshold (1-30%)**: Detects more, may include background
- **Medium threshold (40-60%)**: Balanced, good for most images
- **Higher threshold (70-100%)**: More selective, only high-confidence areas

### Workflow
1. Import image with Image tool
2. Wait for automatic detection (~0.5-2 seconds)
3. Review the green outline
4. Adjust threshold if needed
5. Click "Extract Selected Subject" to create a new object
6. Switch to Select tool to move/manipulate the extracted subject

## Keyboard Shortcuts

- **`I`**: Select Image tool
- **`V`** or **`Esc`**: Return to Select tool
- **`Ctrl+Z`**: Undo
- **`Ctrl+Y`**: Redo

## Technical Details

- **AI Model**: Meta's Segment Anything Model 2 (SAM2)
- **Acceleration**: Metal Performance Shaders (MPS) on Apple Silicon
- **Detection Time**: ~0.5-2 seconds per image
- **Accuracy**: Professional-grade (comparable to Adobe Lightroom)
- **Service Port**: 5001
- **Memory Usage**: ~2-4GB for SAM2 service

## Example Workflow

```
1. Start SAM2 service → ./start_sam2_service.sh
2. Launch DrawingStudio → ./build/DrawingStudio
3. Press 'I' to select Image tool
4. Import an image (File → Open)
5. Wait for green outline to appear
6. Click "Extract Selected Subject"
7. Press 'V' to switch to Select tool
8. Move/scale the extracted subject
```

## Need Help?

Check the console output in both:
- **DrawingStudio terminal**: Shows app-level messages
- **SAM2 service terminal**: Shows detection progress and errors

Look for messages starting with:
- `SAM2:` - Detection status
- `ImagePrimitive:` - Image loading status
- `✓` or `✗` - Success/failure indicators
