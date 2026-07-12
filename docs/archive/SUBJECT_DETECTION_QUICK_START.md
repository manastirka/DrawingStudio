# Subject Detection - Quick Start Guide

## 🚀 3-Step Quick Start

### Step 1: Start SAM2 Service
```bash
./start_sam2_service.sh
```
✅ Wait for: `Listening on http://localhost:5001`

### Step 2: Launch App & Select Image Tool
```bash
./build/DrawingStudio
```
Then press **`I`** key (or click Image tool icon in left toolbar)

### Step 3: Import Image
- **File → Open** or drag & drop an image
- Subject detection happens **automatically**
- Look for the **green outline** around the subject

---

## 📍 Where to Find Subject Detection Options

### Left Toolbar (Tool Selection)
```
┌─────────────┐
│   Select    │  ← Default tool
│   Line      │
│   Curve     │
│   Bezier    │
│   Spline    │
│   Rectangle │
│   Ellipse   │
├─────────────┤
│   Eraser    │
│   Fill      │
│   Brush     │
│   Blur      │
├─────────────┤
│   Measure   │
│ ► IMAGE ◄   │  ← **SELECT THIS** (or press 'I')
│   Hand      │
├─────────────┤
│   Text      │
└─────────────┘
```

### Right Panel (Tool Settings) - Appears when Image tool is selected
```
┌──────────────────────────────────┐
│     Tool Settings                │
├──────────────────────────────────┤
│                                  │
│  Scale: [========] 100%          │
│                                  │
│  Detection Threshold:            │
│         [====] 50%               │
│                                  │
│  ☑ Lock Aspect Ratio             │
│                                  │
│  ☑ Use SAM2 (AI)  ← **AI Toggle**│
│                                  │
│  ┌──────────────────────────┐   │
│  │ Extract Selected Subject │   │
│  └──────────────────────────┘   │
│                                  │
└──────────────────────────────────┘
```

---

## ✨ What Happens Automatically

When you import an image with the Image tool:

1. **Image loads** → Shows on canvas
2. **SAM2 detects subject** → Takes 0.5-2 seconds
3. **Green outline appears** → Shows detected subject boundary
4. **Ready to extract** → Click "Extract Selected Subject" button

---

## 🎯 Common Use Cases

### Extract Person from Photo
1. Press `I` (Image tool)
2. Open photo with person
3. Wait for green outline around person
4. Click "Extract Selected Subject"
5. Person is now a separate, movable object

### Remove Background
1. Import image with Image tool
2. Subject is automatically detected
3. Extract subject
4. Delete original image
5. Subject remains without background

### Composite Images
1. Import background image
2. Import subject image (on new layer)
3. Extract subject from second image
4. Move/scale extracted subject over background
5. Professional composite!

---

## ⚙️ Settings Explained

### Scale (10-200%)
- Resize the image on canvas
- Does not affect detection quality

### Detection Threshold (1-100%)
- **Low (1-30%)**: Includes more area, may grab background
- **Medium (40-60%)**: Balanced, recommended
- **High (70-100%)**: Very selective, only obvious subject

### Lock Aspect Ratio
- ☑ Checked: Image maintains proportions when resizing
- ☐ Unchecked: Can stretch/squash image

### Use SAM2 (AI)
- ☑ Checked: Uses AI (Meta SAM2) - **Recommended**
- ☐ Unchecked: Uses classical algorithm (less accurate)

---

## 🔍 Visual Indicators

### Subject Detected Successfully
```
✓ Green outline around subject
✓ Status bar: "SAM2: Found X objects"
✓ "Extract Selected Subject" button enabled
```

### Detection in Progress
```
⏳ No outline yet
⏳ Status bar: "SAM2: Sending image..."
⏳ Wait 0.5-2 seconds
```

### Detection Failed
```
✗ No outline appears
✗ Status bar: "No objects detected" or "Network error"
✗ Check SAM2 service is running
```

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| No "Use SAM2" checkbox visible | Press `I` to select Image tool |
| "Network error" | Start SAM2 service: `./start_sam2_service.sh` |
| No outline appears | Check SAM2 service: `curl http://localhost:5001/health` |
| Outline wrong | Adjust "Detection Threshold" slider |
| Can't extract | Click on image first to select it |

---

## 📝 Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `I` | Select Image tool |
| `V` or `Esc` | Return to Select tool |
| `Ctrl+O` | Open image file |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |

---

## ✅ Checklist Before Using

- [ ] SAM2 service is running (`./start_sam2_service.sh`)
- [ ] DrawingStudio is launched
- [ ] Image tool is selected (press `I`)
- [ ] "Use SAM2 (AI)" checkbox is checked
- [ ] Image is imported

---

## 🎓 Pro Tips

1. **Best Images**: Clear subject, good contrast, uncluttered background
2. **Multiple Subjects**: SAM2 detects the most prominent one
3. **Adjust Threshold**: If outline is wrong, try different threshold values
4. **Extract Multiple**: Import same image multiple times with different thresholds
5. **Layer Management**: Use layers to organize extracted subjects

---

## 📞 Still Need Help?

See detailed guide: `HOW_TO_USE_SUBJECT_DETECTION.md`

Check service status:
```bash
curl http://localhost:5001/health
```

Should return:
```json
{"device":"mps","sam2_loaded":true,"status":"ok"}
```
