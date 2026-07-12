# AI Systems Summary - DrawingStudio

## 📋 Overview

DrawingStudio integrates **5 local AI systems** that run entirely on your Mac with **100% offline capability** after initial setup.

---

## 🤖 AI Systems Integrated

| # | AI System | Purpose | Technology | Size | Status |
|---|-----------|---------|------------|------|--------|
| 1 | **SAM2** | Object Segmentation | Meta SAM2 + MPS | 900 MB | ✅ Local |
| 2 | **YOLO v8** | Human Detection | Ultralytics + MPS | 22 MB | ✅ Local |
| 3 | **Stable Diffusion** | Image Generation | SD 1.5 + GGML | 2-4 GB | ✅ Local |
| 4 | **LLaMA 3.2** | AI Assistant | llama.cpp + Metal | 4-8 GB | ✅ Local |
| 5 | **Tesseract** | Text Recognition | Tesseract 5.x | ~10 MB | ✅ Local |

**Total Disk Space**: ~7-13 GB

---

## 🎯 Key Features

### 1. SAM2 - Object Segmentation
- **Menu**: `Masks → Detect All Objects` (Ctrl+D)
- **Features**:
  - Multi-object detection
  - Point-based segmentation (Shift+Click)
  - Box-based segmentation
  - Automatic mask generation
  - Focus-aware detection
  - GrabCut refinement
- **Performance**: 200-500ms per segmentation
- **Acceleration**: Apple MPS (Metal)

### 2. YOLO v8 - Human Detection
- **Menu**: `Masks → Detect Humans (YOLO)` (Ctrl+Shift+D)
- **Features**:
  - Fast human detection
  - Precise SAM2 segmentation
  - Multi-person support
  - Confidence filtering
- **Performance**: 50-100ms detection + 200ms segmentation
- **Acceleration**: Apple MPS (Metal)

### 3. Stable Diffusion - Image Generation
- **Menu**: `AI → Generate Image`
- **Features**:
  - Text-to-image generation
  - Context-aware generation (with SAM2 mask)
  - Multiple samplers
  - Negative prompts
  - Configurable steps/CFG
- **Performance**: 30-60 seconds per 512x512 image
- **Acceleration**: CPU-only (Metal disabled for stability)

### 4. LLaMA 3.2 - AI Assistant
- **Location**: AI Assistant panel (bottom)
- **Features**:
  - Natural language Q&A
  - Context analysis
  - Prompt enhancement
  - Drawing help
- **Performance**: 20-50 tokens/second
- **Acceleration**: Metal GPU (99 layers)

### 5. Tesseract - OCR
- **Menu**: `Edit → Extract Text (OCR)`
- **Features**:
  - Multi-language support
  - Automatic segmentation
  - Confidence scoring
  - Bounding boxes
- **Performance**: 100-500ms per image
- **Acceleration**: CPU

---

## 🔒 Offline Mode

### Current Status

✅ **YOLO**: Offline mode implemented  
✅ **SAM2**: Manual download, no auto-fetch  
✅ **Stable Diffusion**: Local GGUF models only  
✅ **LLaMA**: Local GGUF models only  
✅ **Tesseract**: System-installed data  

### Implementation Details

#### YOLO (human_detector.py)
```python
local_model = os.path.expanduser("~/.cache/ultralytics/yolov8s.pt")
offline_mode = os.getenv("YOLO_OFFLINE", "").lower() == "true"

if os.path.exists(local_model):
    self.yolo = YOLO(local_model)  # Use cached
elif offline_mode:
    raise RuntimeError("YOLO model not found and offline mode enabled")
else:
    self.yolo = YOLO('yolov8s.pt')  # Auto-download
```

#### Stable Diffusion (DiffusionHelper.cpp)
```cpp
// Disable Metal GPU to prevent crashes
setenv("GGML_METAL_DISABLE", "1", 1);
```

#### LLaMA (LLMHelper.cpp)
```cpp
// Use Metal GPU for acceleration
llama_model_params model_params = llama_model_default_params();
model_params.n_gpu_layers = 99;  // Offload to Metal
```

### Environment Variables

```bash
export YOLO_OFFLINE=true          # Prevent YOLO auto-download
export HF_HUB_OFFLINE=1           # Disable HuggingFace Hub
export TRANSFORMERS_OFFLINE=1     # Disable Transformers downloads
export GGML_METAL_DISABLE=1       # Force CPU for Stable Diffusion
```

---

## 📦 Setup Instructions

### Quick Setup (Recommended)
```bash
# 1. Download all models
./download_all_models.sh

# 2. Enable offline mode
echo 'export YOLO_OFFLINE=true' >> ~/.zshrc
echo 'export HF_HUB_OFFLINE=1' >> ~/.zshrc
echo 'export TRANSFORMERS_OFFLINE=1' >> ~/.zshrc
source ~/.zshrc

# 3. Start SAM2 service
./start_sam2_service_offline.sh

# 4. Launch app (in new terminal)
./build/DrawingStudio
```

### Verify Setup
```bash
# Check all models present
ls -lh ~/.cache/ultralytics/yolov8s.pt
ls -lh sam2_service/checkpoints/sam2_hiera_large.pt
ls -lh models/*.gguf
ls -lh /opt/homebrew/share/tessdata/eng.traineddata

# Test offline mode
# Disconnect from internet, then:
./start_sam2_service_offline.sh
./build/DrawingStudio
# Try all AI features
```

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DrawingStudio (Qt/C++)                   │
│                         Main Process                         │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │DiffusionHelper│ │  LLMHelper   │  │  OCRHelper   │     │
│  │  (SD 1.5)    │  │ (LLaMA 3.2)  │  │ (Tesseract)  │     │
│  │  CPU-only    │  │  Metal GPU   │  │    CPU       │     │
│  │  ~30-60s     │  │  20-50 tok/s │  │  100-500ms   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         SAM2Client (HTTP Client)                      │  │
│  │         Connects to localhost:5001                    │  │
│  └──────────────────────────────────────────────────────┘  │
└───────────────────────────┬──────────────────────────────────┘
                            │ HTTP localhost:5001
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              sam2_service (Python Flask)                     │
│                    Separate Process                          │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ SAM2Predictor│  │HumanDetector │  │  MaskCache   │     │
│  │   (SAM2)     │  │(YOLO + SAM2) │  │   (Disk)     │     │
│  │  Metal MPS   │  │  Metal MPS   │  │              │     │
│  │  200-500ms   │  │  50-100ms    │  │              │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘

All processing: 100% Local on M3 Pro
No cloud APIs, no data uploads, no telemetry
```

---

## 📊 Performance Benchmarks

### Detection & Segmentation
- **SAM2 (single object)**: 200-300ms
- **SAM2 (multi-object)**: 2-5 seconds
- **YOLO detection**: 50-100ms
- **YOLO + SAM2 (per person)**: 250-400ms

### Generation
- **Stable Diffusion (512x512)**: 30-60 seconds
- **Context analysis (LLaMA)**: 5-10 seconds
- **Total context-aware generation**: 35-70 seconds

### Text Processing
- **OCR (single image)**: 100-500ms
- **LLaMA (100 tokens)**: 2-5 seconds

### Hardware
- **CPU**: Apple M3 Pro
- **GPU**: Metal (MPS for SAM2/YOLO, Metal for LLaMA)
- **RAM**: Varies by model (2-8 GB per model)

---

## 🔐 Privacy & Security

✅ **No Cloud APIs**: Everything runs on your Mac  
✅ **No Data Upload**: Images never leave your computer  
✅ **No Telemetry**: No usage tracking or analytics  
✅ **No Authentication**: No API keys or accounts needed  
✅ **Open Source Models**: All models are publicly available  
✅ **Offline Capable**: Works without internet after setup  
✅ **Local Storage**: All data stays on your disk  

---

## 📚 Documentation Files

1. **LOCAL_AI_SYSTEMS.md** - Complete technical documentation
2. **AI_QUICK_START.md** - User-friendly getting started guide
3. **OFFLINE_MODE.md** - Detailed offline configuration
4. **NETWORK_ACCESS_SUMMARY.md** - Network usage explanation
5. **AI_SYSTEMS_SUMMARY.md** - This file (overview)

### Scripts
- **download_all_models.sh** - Download all AI models
- **start_sam2_service.sh** - Start SAM2 service (normal)
- **start_sam2_service_offline.sh** - Start with offline checks

---

## 🎯 Use Cases

### 1. Product Photography
- Import product photo
- Use SAM2 to detect product
- Extract to transparent background
- Generate new background with Stable Diffusion

### 2. Portrait Editing
- Import portrait
- Use YOLO to detect person
- Extract subject
- Generate artistic background

### 3. Document Processing
- Import document image
- Use OCR to extract text
- Edit text in external editor
- Regenerate with corrections

### 4. Creative Composition
- Import base image
- Detect multiple objects (SAM2)
- Extract each object to layer
- Rearrange and compose
- Generate fill areas with Stable Diffusion

### 5. AI-Assisted Design
- Ask AI Assistant for ideas
- Generate base image with Stable Diffusion
- Refine with manual edits
- Use SAM2 for precise selections

---

## 🛠️ Troubleshooting Quick Reference

| Issue | Solution |
|-------|----------|
| SAM2 service not available | `./start_sam2_service.sh` |
| YOLO model not found | `./download_all_models.sh` |
| Slow image generation | Normal - CPU mode takes 30-60s |
| AI Assistant not responding | Check `models/Llama-*.gguf` exists |
| OCR not working | `brew install tesseract` |
| Network access prompts | Run `./download_all_models.sh` first |

---

## 📈 Future Enhancements

### Planned
- [ ] GPU acceleration for Stable Diffusion (when Metal is fixed)
- [ ] ControlNet support for guided generation
- [ ] Real-time video segmentation
- [ ] Multi-language OCR
- [ ] Custom model fine-tuning

### Under Consideration
- [ ] Batch processing automation
- [ ] Plugin system for custom AI models
- [ ] Advanced mask editing tools
- [ ] AI-powered color correction
- [ ] Style transfer

---

## ✅ Verification Checklist

Before reporting issues:

- [ ] All models downloaded (`./download_all_models.sh`)
- [ ] SAM2 service running (`lsof -i :5001`)
- [ ] Environment variables set (check `echo $YOLO_OFFLINE`)
- [ ] Sufficient disk space (~13 GB free)
- [ ] Sufficient RAM (~8 GB free)
- [ ] macOS permissions granted (if prompted)
- [ ] Console checked for errors

---

## 📞 Support

1. **Check Documentation**: Read the guides above
2. **Check Console**: Look for error messages
3. **Check Logs**: Terminal where services started
4. **Ask AI Assistant**: Built-in help system
5. **Verify Setup**: Run verification checklist

---

## 🎉 Summary

DrawingStudio integrates **5 powerful AI systems** that run **100% locally** on your Mac:

1. ✅ **SAM2** - Best-in-class object segmentation
2. ✅ **YOLO v8** - Fast human detection
3. ✅ **Stable Diffusion** - High-quality image generation
4. ✅ **LLaMA 3.2** - Intelligent AI assistant
5. ✅ **Tesseract** - Reliable text recognition

**Total Setup Time**: ~10 minutes  
**Total Disk Space**: ~7-13 GB  
**Network Required**: Only for initial download  
**Privacy**: 100% local, no cloud, no tracking  
**Performance**: Optimized for Apple Silicon  

**Ready to use!** Follow **AI_QUICK_START.md** to get started. 🚀
