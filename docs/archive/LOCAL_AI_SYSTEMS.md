# Local AI Systems in DrawingStudio

## Overview

DrawingStudio integrates **5 local AI systems** that run entirely on your Mac (M3 Pro). All processing happens on-device with no cloud APIs or data uploads.

---

## 🤖 Integrated AI Systems

### 1. **SAM2 (Segment Anything Model 2)** - Object Segmentation
- **Purpose**: Detect and segment objects in images
- **Technology**: Meta's SAM2 with Hiera Large backbone
- **Location**: `sam2_service/sam2_service.py`
- **Model**: `sam2_service/checkpoints/sam2_hiera_large.pt` (~900 MB)
- **Acceleration**: Apple MPS (Metal Performance Shaders)
- **Features**:
  - Multi-object detection (detect all objects in image)
  - Point-based segmentation (click to segment)
  - Box-based segmentation (draw box to segment)
  - Automatic mask generation
  - Focus-aware detection (prioritizes sharp/in-focus objects)
  - GrabCut refinement for better boundaries
- **Performance**: ~200-500ms per segmentation
- **Network**: Manual download required (one-time)
- **Status**: ✅ 100% Local

**Usage in App**:
- Menu: `Masks → Detect All Objects` (Ctrl+D)
- Automatically detects subjects when entering edit mode
- Provides multiple mask candidates to choose from

---

### 2. **YOLO v8** - Human Detection
- **Purpose**: Detect and segment humans specifically
- **Technology**: Ultralytics YOLOv8s + SAM2 hybrid
- **Location**: `sam2_service/human_detector.py`
- **Model**: `~/.cache/ultralytics/yolov8s.pt` (~22 MB)
- **Acceleration**: Apple MPS
- **Features**:
  - Fast human detection using YOLO
  - Precise segmentation using SAM2
  - Confidence threshold filtering
  - Multi-person detection
  - Bounding box + mask output
- **Performance**: ~50-100ms YOLO + ~200ms SAM2 per person
- **Network**: Auto-downloads on first use (can be pre-downloaded)
- **Status**: ✅ 100% Local (after download)

**Offline Mode**:
```python
# Already implemented in human_detector.py
local_model = os.path.expanduser("~/.cache/ultralytics/yolov8s.pt")
offline_mode = os.getenv("YOLO_OFFLINE", "").lower() == "true"

if os.path.exists(local_model):
    self.yolo = YOLO(local_model)  # Use cached model
elif offline_mode:
    raise RuntimeError("YOLO model not found and offline mode is enabled")
else:
    self.yolo = YOLO('yolov8s.pt')  # Auto-download
```

**Usage in App**:
- Menu: `Masks → Detect Humans (YOLO)` (Ctrl+Shift+D)
- Optimized for human subjects
- Returns largest/most confident human by default

---

### 3. **Stable Diffusion 1.5** - Image Generation
- **Purpose**: Generate images from text prompts
- **Technology**: stable-diffusion.cpp (GGML backend)
- **Location**: `src/DiffusionHelper.cpp`
- **Model**: `models/v1-5-pruned-emaonly.ckpt` or GGUF format (~2-4 GB)
- **Acceleration**: CPU-only (Metal GPU disabled due to crashes)
- **Features**:
  - Text-to-image generation
  - Context-aware generation (analyzes existing image colors/style)
  - Multiple samplers (Euler, DPM++, etc.)
  - Configurable steps, CFG scale, seed
  - Negative prompts
- **Performance**: ~30-60 seconds per 512x512 image (CPU)
- **Network**: None - uses local GGUF models
- **Status**: ✅ 100% Local

**Why CPU-only**:
```cpp
// CRITICAL: Disable Metal GPU to prevent crashes
// Metal backend has bugs that cause ggml_abort errors
// Force CPU-only mode by setting environment variable
setenv("GGML_METAL_DISABLE", "1", 1);
```

**Context-Aware Generation**:
- Analyzes mask area colors and dimensions
- Uses LLaMA to create enhanced prompt matching image style
- Adapts generation size to mask dimensions
- Total time: ~25-35 seconds (analysis + generation)

**Usage in App**:
- Menu: `AI → Generate Image`
- Works with or without SAM2 mask selection
- Integrates with LLaMA for intelligent prompts

---

### 4. **LLaMA 3.2** - Text Generation & AI Assistant
- **Purpose**: Natural language understanding and generation
- **Technology**: llama.cpp with Metal GPU acceleration
- **Location**: `src/LLMHelper.cpp`
- **Model**: `models/Llama-3.2-1B-Instruct-Q4_K_M.gguf` (~4-8 GB)
- **Acceleration**: Metal GPU (99 layers offloaded)
- **Features**:
  - AI assistant for drawing help
  - Context analysis for image generation
  - Prompt enhancement
  - Documentation-aware responses
  - Streaming token generation
- **Performance**: ~20-50 tokens/second
- **Network**: None - uses local GGUF models
- **Status**: ✅ 100% Local

**GPU Acceleration**:
```cpp
llama_model_params model_params = llama_model_default_params();
model_params.n_gpu_layers = 99; // Use Metal GPU acceleration on macOS
```

**Usage in App**:
- AI Assistant panel (bottom of window)
- Type questions about drawing, features, or get help
- Automatically enhances Stable Diffusion prompts

---

### 5. **Tesseract OCR** - Text Recognition
- **Purpose**: Extract text from images
- **Technology**: Tesseract 5.x with Leptonica
- **Location**: `src/OCRHelper.cpp`
- **Data**: Tessdata language files (system-installed)
- **Acceleration**: CPU
- **Features**:
  - Multi-language support
  - Automatic page segmentation
  - Confidence scoring
  - Preprocessing (grayscale, threshold, denoise)
  - Bounding box detection
- **Performance**: ~100-500ms depending on image size
- **Network**: None - uses system-installed tessdata
- **Status**: ✅ 100% Local

**Tessdata Locations**:
```cpp
QStringList possiblePaths = {
    QCoreApplication::applicationDirPath() + "/tessdata",
    QCoreApplication::applicationDirPath() + "/../Resources/tessdata",
    "/usr/local/share/tessdata",
    "/opt/homebrew/share/tessdata",
    "/usr/share/tessdata",
    QDir::homePath() + "/.local/share/tessdata"
};
```

**Usage in App**:
- Select image primitive
- Menu: `Edit → Extract Text (OCR)`
- Text appears in dialog with confidence score

---

## 🔒 Offline Mode Configuration

### Quick Setup

```bash
# 1. Download all models
cd /Users/Lukovic/Apps/DrawingStudio
./download_all_models.sh

# 2. Enable offline mode
export YOLO_OFFLINE=true
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1
export GGML_METAL_DISABLE=1  # For Stable Diffusion stability

# 3. Add to shell profile
echo 'export YOLO_OFFLINE=true' >> ~/.zshrc
echo 'export HF_HUB_OFFLINE=1' >> ~/.zshrc
echo 'export TRANSFORMERS_OFFLINE=1' >> ~/.zshrc
source ~/.zshrc
```

### Verify All Models Present

```bash
# YOLO
ls -lh ~/.cache/ultralytics/yolov8s.pt

# SAM2
ls -lh sam2_service/checkpoints/sam2_hiera_large.pt

# Stable Diffusion
ls -lh models/*.ckpt models/*.gguf

# LLaMA
ls -lh models/Llama-*.gguf

# Tesseract (system)
ls -lh /opt/homebrew/share/tessdata/eng.traineddata
```

---

## 📊 System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DrawingStudio (Qt/C++)                   │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ DiffusionHelper│ │  LLMHelper   │  │  OCRHelper   │     │
│  │ (Stable Diff) │  │  (LLaMA)     │  │ (Tesseract)  │     │
│  │   CPU-only    │  │  Metal GPU   │  │    CPU       │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            SAM2Client (HTTP Client)                   │  │
│  └──────────────────────────────────────────────────────┘  │
└───────────────────────────┬──────────────────────────────────┘
                            │ HTTP localhost:5001
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              sam2_service (Python Flask)                     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ SAM2Predictor│  │HumanDetector │  │  MaskCache   │     │
│  │  (SAM2)      │  │(YOLO + SAM2) │  │              │     │
│  │  Metal MPS   │  │  Metal MPS   │  │              │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘

All processing: 100% Local on M3 Pro
No cloud APIs, no data uploads, no telemetry
```

---

## 🚀 Performance Summary

| AI System          | Acceleration | Speed                  | Model Size |
|--------------------|--------------|------------------------|------------|
| SAM2               | Metal MPS    | 200-500ms/segment      | 900 MB     |
| YOLO v8            | Metal MPS    | 50-100ms/detection     | 22 MB      |
| Stable Diffusion   | CPU          | 30-60s/image           | 2-4 GB     |
| LLaMA 3.2          | Metal GPU    | 20-50 tokens/sec       | 4-8 GB     |
| Tesseract OCR      | CPU          | 100-500ms/image        | ~10 MB     |

**Total Disk Space**: ~7-13 GB

---

## 🔐 Privacy & Security

✅ **No Cloud APIs**: Everything runs on your Mac  
✅ **No Data Upload**: Images never leave your computer  
✅ **No Telemetry**: No usage tracking or analytics  
✅ **No Authentication**: No API keys or accounts needed  
✅ **Open Source**: All models are publicly available  
✅ **Offline Capable**: Works without internet (after setup)  

---

## 🛠️ Troubleshooting

### "YOLO model not found"
```bash
./download_all_models.sh
# or manually:
mkdir -p ~/.cache/ultralytics
curl -L "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt" \
  -o ~/.cache/ultralytics/yolov8s.pt
```

### "SAM2 checkpoint not found"
```bash
mkdir -p sam2_service/checkpoints
cd sam2_service/checkpoints
curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" \
  -o sam2_hiera_large.pt
```

### "Stable Diffusion crashes"
- This is expected with Metal GPU backend
- App automatically uses CPU-only mode (stable and reliable)
- Performance: ~30-60s per image (acceptable for quality)

### "AI Assistant not responding"
- Check LLaMA model exists: `ls models/Llama-*.gguf`
- Check console for initialization errors
- Model should be Q4_K_M quantization for best speed/quality

### "OCR not working"
```bash
# Install Tesseract (if not already)
brew install tesseract

# Verify installation
tesseract --version
ls /opt/homebrew/share/tessdata/eng.traineddata
```

---

## 📚 Related Documentation

- **OFFLINE_MODE.md** - Detailed offline configuration
- **NETWORK_ACCESS_SUMMARY.md** - Network usage explanation
- **BUILD_SUCCESS_SUMMARY.md** - Build and setup guide
- **download_all_models.sh** - Automated model download script

---

## 🎯 Feature Roadmap

### Current Features
- ✅ Multi-object detection (SAM2)
- ✅ Human-specific detection (YOLO)
- ✅ Image generation (Stable Diffusion)
- ✅ AI assistant (LLaMA)
- ✅ Text extraction (OCR)
- ✅ Context-aware generation
- ✅ Mask editing and refinement

### Potential Enhancements
- [ ] GPU acceleration for Stable Diffusion (when Metal backend is fixed)
- [ ] ControlNet support for guided generation
- [ ] Real-time video segmentation
- [ ] Multi-language OCR
- [ ] Custom model fine-tuning
- [ ] Batch processing

---

## 💡 Usage Tips

1. **For Best Segmentation**:
   - Use high-resolution images (but not too large, ~1024px max)
   - Ensure good contrast between subject and background
   - Try "Detect Humans" for people (faster and more accurate)
   - Use "Detect All Objects" for general subjects

2. **For Best Image Generation**:
   - Use SAM2 mask to define generation area
   - Let LLaMA enhance your prompt (automatic)
   - Be specific in prompts (e.g., "red sports car" not just "car")
   - Adjust steps (20-30 for quality, 10-15 for speed)

3. **For Best OCR**:
   - Use clear, high-contrast text
   - Avoid skewed or rotated text
   - Crop to text area before OCR
   - Check confidence score (>80% is good)

4. **For Best AI Assistant**:
   - Ask specific questions
   - Provide context about your task
   - Use for drawing help, feature explanations, or creative ideas

---

## 🔧 Developer Notes

### Adding New AI Models

1. **Choose Integration Method**:
   - C++ direct integration (like Stable Diffusion, LLaMA)
   - Python service (like SAM2, YOLO)

2. **For C++ Integration**:
   ```cpp
   // Create helper class
   class NewAIHelper : public QObject {
       Q_OBJECT
   public:
       bool initialize(const QString& modelPath);
       QVariant process(const QVariant& input);
   signals:
       void processingComplete(const QVariant& result);
   };
   ```

3. **For Python Service**:
   ```python
   # Add endpoint to sam2_service.py
   @app.route('/new_ai_endpoint', methods=['POST'])
   def new_ai_endpoint():
       # Process request
       return jsonify(result)
   ```

4. **Update Documentation**:
   - Add to this file
   - Update OFFLINE_MODE.md
   - Update download_all_models.sh

### Model Selection Criteria

- ✅ Runs locally (no cloud API)
- ✅ Reasonable size (<10 GB)
- ✅ Good performance on M3 Pro
- ✅ Open source / permissive license
- ✅ Active maintenance
- ✅ GGUF/ONNX/PyTorch format

---

**Last Updated**: 2024  
**DrawingStudio Version**: 1.0  
**Platform**: macOS (Apple Silicon)
