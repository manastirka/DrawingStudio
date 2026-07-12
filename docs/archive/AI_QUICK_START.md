# AI Quick Start Guide - DrawingStudio

## 🚀 First Time Setup

### 1. Download All AI Models (One-Time)
```bash
cd /Users/Lukovic/Apps/DrawingStudio
./download_all_models.sh
```

This downloads:
- ✅ YOLO model (~22 MB) - for human detection
- ✅ SAM2 checkpoint (~900 MB) - for object segmentation
- ✅ Verifies Stable Diffusion and LLaMA models

**Time**: ~5-10 minutes depending on internet speed

---

### 2. Enable Offline Mode (Optional)
```bash
# Add to ~/.zshrc
export YOLO_OFFLINE=true
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1

# Apply changes
source ~/.zshrc
```

---

### 3. Start SAM2 Service
```bash
# Regular mode
./start_sam2_service.sh

# Offline mode (with checks)
./start_sam2_service_offline.sh
```

**Keep this terminal open** - the service must run while using DrawingStudio.

---

### 4. Launch DrawingStudio
```bash
# In a new terminal
./build/DrawingStudio
```

---

## 🎯 Using AI Features

### 🔍 Object Detection (SAM2)

**Automatic Detection**:
1. Import an image (File → Open Image)
2. Select the image
3. Menu: `Masks → Detect All Objects` (or press `Ctrl+D`)
4. Wait for detection (~2-5 seconds)
5. Use `[` and `]` keys to cycle through detected masks
6. Click "Extract" to create a new layer

**Manual Point Detection**:
1. Select image
2. Hold `Shift` and click on object
3. SAM2 segments the clicked object
4. Adjust mask if needed

**Best For**:
- General object detection
- Multiple objects in scene
- Complex shapes
- Product photos

---

### 🧑 Human Detection (YOLO + SAM2)

1. Import image with people
2. Select the image
3. Menu: `Masks → Detect Humans (YOLO)` (or press `Ctrl+Shift+D`)
4. Wait for detection (~1-3 seconds)
5. Automatically selects largest/most confident person
6. Extract or edit as needed

**Best For**:
- Portrait photos
- Group photos
- Human subjects
- Faster than general detection

---

### 🎨 Image Generation (Stable Diffusion)

**Simple Generation**:
1. Menu: `AI → Generate Image`
2. Enter prompt (e.g., "red sports car, photorealistic")
3. Click Generate
4. Wait ~30-60 seconds
5. Image appears on canvas

**Context-Aware Generation** (Advanced):
1. Select an image
2. Use SAM2 to create a mask where you want to generate
3. Menu: `AI → Generate Image`
4. Enter prompt
5. LLaMA analyzes context and enhances prompt
6. Stable Diffusion generates matching image
7. Wait ~25-35 seconds

**Tips**:
- Be specific: "red Ferrari F40, sunset lighting" not just "car"
- Use negative prompts: "blurry, low quality, distorted"
- Adjust steps: 20-30 for quality, 10-15 for speed
- Try different samplers in settings

---

### 💬 AI Assistant (LLaMA)

1. Find AI Assistant panel at bottom of window
2. Type question or request
3. Press Enter or click Ask
4. Wait for response (~5-10 seconds)

**Example Questions**:
- "How do I extract a subject from an image?"
- "What's the best way to remove backgrounds?"
- "Explain the layer system"
- "Give me ideas for a sci-fi illustration"

**Best For**:
- Learning features
- Getting help
- Creative suggestions
- Technical questions

---

### 📝 Text Extraction (OCR)

1. Import image with text
2. Select the image
3. Menu: `Edit → Extract Text (OCR)`
4. Wait ~1-2 seconds
5. Text appears in dialog with confidence score
6. Copy text to clipboard

**Tips**:
- Use high-contrast images
- Crop to text area first
- Works best with printed text
- Confidence >80% is good

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+D` | Detect All Objects (SAM2) |
| `Ctrl+Shift+D` | Detect Humans (YOLO) |
| `[` | Previous mask candidate |
| `]` | Next mask candidate |
| `Shift+Click` | Point-based segmentation |
| `E` | Toggle edit mode |
| `Delete` | Delete selected mask |

---

## 🔧 Troubleshooting

### "SAM2 service not available"
```bash
# Check if service is running
lsof -i :5001

# If not running, start it
./start_sam2_service.sh
```

### "YOLO model not found"
```bash
# Download YOLO model
./download_all_models.sh

# Or manually
mkdir -p ~/.cache/ultralytics
curl -L "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt" \
  -o ~/.cache/ultralytics/yolov8s.pt
```

### "SAM2 checkpoint not found"
```bash
# Download SAM2 checkpoint
cd sam2_service
mkdir -p checkpoints
cd checkpoints
curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" \
  -o sam2_hiera_large.pt
```

### "Image generation is slow"
- **Normal**: 30-60 seconds per image on CPU
- **Why**: Metal GPU backend crashes, using stable CPU mode
- **Solution**: None needed - this is expected behavior
- **Quality**: Excellent despite slower speed

### "AI Assistant not responding"
```bash
# Check LLaMA model exists
ls models/Llama-*.gguf

# If missing, download from HuggingFace
# Search for "Llama-3.2-1B-Instruct-Q4_K_M.gguf"
```

### "OCR not working"
```bash
# Install Tesseract
brew install tesseract

# Verify installation
tesseract --version
ls /opt/homebrew/share/tessdata/eng.traineddata
```

---

## 📊 Performance Expectations

| Feature | Time | Quality |
|---------|------|---------|
| Object Detection | 2-5s | Excellent |
| Human Detection | 1-3s | Excellent |
| Image Generation | 30-60s | Excellent |
| AI Assistant | 5-10s | Good |
| OCR | 1-2s | Good |

**Hardware**: Apple M3 Pro  
**All processing**: 100% local, no cloud

---

## 💡 Pro Tips

### 1. Mask Editing
- After detection, use edit mode (`E` key) to refine masks
- Add points: Click on missed areas
- Remove points: Click on over-segmented areas
- Smooth edges: Use morphological operations

### 2. Layer Workflow
1. Import base image
2. Detect and extract subject (SAM2)
3. Generate background (Stable Diffusion)
4. Arrange layers
5. Export final composition

### 3. Context-Aware Generation
- Select area with SAM2 mask
- LLaMA analyzes colors and style
- Stable Diffusion matches context
- Result: Seamless integration

### 4. Batch Processing
- Import multiple images
- Use same detection settings
- Extract all subjects
- Arrange in composition

---

## 🔒 Privacy & Offline Use

✅ **All AI runs locally** - no cloud APIs  
✅ **No data upload** - images stay on your Mac  
✅ **No telemetry** - no tracking or analytics  
✅ **Works offline** - after initial model download  

---

## 📚 Learn More

- **LOCAL_AI_SYSTEMS.md** - Complete AI system documentation
- **OFFLINE_MODE.md** - Detailed offline configuration
- **NETWORK_ACCESS_SUMMARY.md** - Network usage explanation
- **BUILD_SUCCESS_SUMMARY.md** - Build and setup guide

---

## 🆘 Getting Help

1. **AI Assistant**: Ask questions in the app
2. **Console**: Check for error messages
3. **Logs**: Look in terminal where you started services
4. **Documentation**: Read the detailed guides above

---

## ✅ Checklist

Before using AI features:

- [ ] Downloaded all models (`./download_all_models.sh`)
- [ ] SAM2 service is running (`./start_sam2_service.sh`)
- [ ] DrawingStudio is launched
- [ ] Tested with sample image
- [ ] Keyboard shortcuts learned
- [ ] Read troubleshooting section

---

**Ready to go!** Import an image and try `Ctrl+D` to detect objects. 🎉
