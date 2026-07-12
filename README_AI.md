# DrawingStudio AI Documentation

## 📚 Documentation Index

Welcome to DrawingStudio's AI documentation! This app integrates **5 local AI systems** that run entirely on your Mac.

---

## 🚀 Getting Started

### New Users - Start Here!
1. **[AI_QUICK_START.md](AI_QUICK_START.md)** ⭐
   - First-time setup guide
   - Download models
   - Start using AI features
   - Keyboard shortcuts
   - Troubleshooting

### Quick Setup (3 Steps)
```bash
# 1. Download models (~1 GB, one-time)
./download_all_models.sh

# 2. Start SAM2 service
./start_sam2_service.sh

# 3. Launch app (new terminal)
./build/DrawingStudio
```

---

## 📖 Complete Documentation

### Overview & Summary
- **[AI_SYSTEMS_SUMMARY.md](AI_SYSTEMS_SUMMARY.md)**
  - Complete overview of all 5 AI systems
  - Architecture diagram
  - Performance benchmarks
  - Quick reference tables

### Technical Details
- **[LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md)**
  - In-depth technical documentation
  - Implementation details
  - Code examples
  - Developer notes

### Offline Mode
- **[OFFLINE_MODE.md](OFFLINE_MODE.md)**
  - Complete offline configuration
  - Environment variables
  - Model locations
  - Firewall settings

- **[NETWORK_ACCESS_SUMMARY.md](NETWORK_ACCESS_SUMMARY.md)**
  - Why network access is requested
  - What gets downloaded
  - How to verify offline operation

---

## 🤖 AI Systems Reference

### 1. SAM2 - Object Segmentation
- **What**: Detect and segment objects in images
- **How**: `Masks → Detect All Objects` (Ctrl+D)
- **Speed**: 2-5 seconds
- **Docs**: [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md#1-sam2-segment-anything-model-2---object-segmentation)

### 2. YOLO v8 - Human Detection
- **What**: Fast human detection and segmentation
- **How**: `Masks → Detect Humans (YOLO)` (Ctrl+Shift+D)
- **Speed**: 1-3 seconds
- **Docs**: [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md#2-yolo-v8---human-detection)

### 3. Stable Diffusion - Image Generation
- **What**: Generate images from text prompts
- **How**: `AI → Generate Image`
- **Speed**: 30-60 seconds
- **Docs**: [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md#3-stable-diffusion-15---image-generation)

### 4. LLaMA 3.2 - AI Assistant
- **What**: Natural language Q&A and help
- **How**: AI Assistant panel (bottom)
- **Speed**: 5-10 seconds
- **Docs**: [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md#4-llama-32---text-generation--ai-assistant)

### 5. Tesseract - OCR
- **What**: Extract text from images
- **How**: `Edit → Extract Text (OCR)`
- **Speed**: 1-2 seconds
- **Docs**: [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md#5-tesseract-ocr---text-recognition)

---

## 🎯 Common Tasks

### Extract Subject from Photo
1. Import image
2. Press `Ctrl+D` (Detect All Objects)
3. Use `[` `]` to cycle through masks
4. Click "Extract" button
5. Subject appears on new layer

### Generate Background
1. Create mask with SAM2
2. Menu: `AI → Generate Image`
3. Enter prompt: "sunset beach, photorealistic"
4. Wait ~30-60 seconds
5. Generated image appears

### Detect People
1. Import photo with people
2. Press `Ctrl+Shift+D` (Detect Humans)
3. Largest person auto-selected
4. Extract or edit as needed

### Ask for Help
1. Find AI Assistant panel (bottom)
2. Type: "How do I remove backgrounds?"
3. Press Enter
4. Get instant help

### Extract Text
1. Import document/screenshot
2. Select image
3. Menu: `Edit → Extract Text (OCR)`
4. Copy text from dialog

---

## 🔒 Privacy & Offline Use

✅ **100% Local Processing** - All AI runs on your Mac  
✅ **No Cloud APIs** - No internet required after setup  
✅ **No Data Upload** - Images never leave your computer  
✅ **No Telemetry** - No tracking or analytics  
✅ **No Accounts** - No API keys or sign-ups  

**See**: [OFFLINE_MODE.md](OFFLINE_MODE.md) for complete offline setup

---

## 📦 Model Downloads

### Automatic Download (Recommended)
```bash
./download_all_models.sh
```

Downloads:
- ✅ YOLO model (~22 MB)
- ✅ SAM2 checkpoint (~900 MB)
- ✅ Verifies Stable Diffusion models
- ✅ Verifies LLaMA models

### Manual Downloads
See [NETWORK_ACCESS_SUMMARY.md](NETWORK_ACCESS_SUMMARY.md#option-2-manual-download)

---

## 🛠️ Scripts & Tools

### Service Management
- **start_sam2_service.sh** - Start SAM2 service (normal mode)
- **start_sam2_service_offline.sh** - Start with offline checks

### Model Management
- **download_all_models.sh** - Download all AI models
- **download_model.sh** - Download specific model
- **download_sd_model.sh** - Download Stable Diffusion model

### Other Tools
- **install_ocr.sh** - Install Tesseract OCR
- **test_sd_connection.sh** - Test Stable Diffusion setup

---

## 📊 System Requirements

### Minimum
- **OS**: macOS 12+ (Monterey or later)
- **CPU**: Apple Silicon (M1 or later)
- **RAM**: 8 GB
- **Disk**: 15 GB free (for models)

### Recommended
- **OS**: macOS 14+ (Sonoma or later)
- **CPU**: Apple M3 Pro or better
- **RAM**: 16 GB
- **Disk**: 20 GB free

### Tested On
- **Hardware**: Apple M3 Pro
- **OS**: macOS Sonoma
- **RAM**: 18 GB
- **Performance**: Excellent

---

## ⚡ Performance Guide

### Fast Operations (<1s)
- YOLO human detection: 50-100ms
- Single object segmentation: 200-500ms
- OCR text extraction: 100-500ms

### Medium Operations (1-10s)
- Multi-object detection: 2-5s
- AI Assistant response: 5-10s

### Slow Operations (>10s)
- Image generation: 30-60s
- Context-aware generation: 35-70s

**Note**: Stable Diffusion uses CPU-only mode for stability. GPU acceleration would be faster but causes crashes on current Metal backend.

---

## 🐛 Troubleshooting

### Quick Fixes

| Problem | Solution |
|---------|----------|
| SAM2 not working | `./start_sam2_service.sh` |
| YOLO not found | `./download_all_models.sh` |
| Slow generation | Normal - CPU takes 30-60s |
| AI not responding | Check model files exist |
| OCR not working | `brew install tesseract` |

### Detailed Troubleshooting
See [AI_QUICK_START.md](AI_QUICK_START.md#-troubleshooting)

### Check Service Status
```bash
# Is SAM2 service running?
lsof -i :5001

# Check environment variables
echo $YOLO_OFFLINE
echo $HF_HUB_OFFLINE

# Verify models
ls -lh ~/.cache/ultralytics/yolov8s.pt
ls -lh sam2_service/checkpoints/sam2_hiera_large.pt
ls -lh models/*.gguf
```

---

## 📈 Feature Comparison

| Feature | SAM2 | YOLO | Stable Diff | LLaMA | Tesseract |
|---------|------|------|-------------|-------|-----------|
| Speed | ⚡⚡⚡ | ⚡⚡⚡⚡ | ⚡ | ⚡⚡⚡ | ⚡⚡⚡⚡ |
| Accuracy | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| GPU Use | ✅ MPS | ✅ MPS | ❌ CPU | ✅ Metal | ❌ CPU |
| Model Size | 900 MB | 22 MB | 2-4 GB | 4-8 GB | 10 MB |
| Offline | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## 🎓 Learning Path

### Beginner
1. Read [AI_QUICK_START.md](AI_QUICK_START.md)
2. Download models
3. Try object detection (Ctrl+D)
4. Try human detection (Ctrl+Shift+D)
5. Ask AI Assistant questions

### Intermediate
1. Read [AI_SYSTEMS_SUMMARY.md](AI_SYSTEMS_SUMMARY.md)
2. Try image generation
3. Use context-aware generation
4. Experiment with mask editing
5. Try OCR on documents

### Advanced
1. Read [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md)
2. Configure offline mode
3. Optimize performance settings
4. Create complex compositions
5. Explore batch workflows

---

## 🔗 External Resources

### Model Sources
- **YOLO**: [Ultralytics YOLOv8](https://github.com/ultralytics/ultralytics)
- **SAM2**: [Meta Segment Anything 2](https://github.com/facebookresearch/segment-anything-2)
- **Stable Diffusion**: [stable-diffusion.cpp](https://github.com/leejet/stable-diffusion.cpp)
- **LLaMA**: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- **Tesseract**: [Tesseract OCR](https://github.com/tesseract-ocr/tesseract)

### Model Downloads
- **HuggingFace**: [huggingface.co/models](https://huggingface.co/models)
- **Stable Diffusion Models**: [huggingface.co/stabilityai](https://huggingface.co/stabilityai)
- **LLaMA Models**: [huggingface.co/meta-llama](https://huggingface.co/meta-llama)

---

## 💡 Tips & Best Practices

### For Best Results
1. **Use high-quality images** - Better input = better output
2. **Start with defaults** - Adjust settings only if needed
3. **Be specific in prompts** - Detail improves generation
4. **Use keyboard shortcuts** - Much faster workflow
5. **Ask AI Assistant** - Built-in help is very useful

### Workflow Optimization
1. **Keep SAM2 service running** - Avoid restart delays
2. **Pre-download models** - Enable offline mode
3. **Use YOLO for humans** - Faster than general detection
4. **Batch similar tasks** - Process multiple images at once
5. **Save frequently** - AI operations can take time

### Performance Tips
1. **Close other apps** - Free up RAM for AI models
2. **Use smaller images** - Faster processing
3. **Reduce SD steps** - 15-20 is often enough
4. **Cache is your friend** - Repeated operations are faster
5. **Monitor Activity Monitor** - Check resource usage

---

## ✅ Quick Verification

Before using AI features, verify:

```bash
# 1. Models downloaded
./download_all_models.sh

# 2. SAM2 service running
./start_sam2_service.sh

# 3. App launched
./build/DrawingStudio

# 4. Test detection
# Import image → Press Ctrl+D

# 5. Test generation
# Menu: AI → Generate Image

# 6. Test assistant
# Type question in AI Assistant panel
```

---

## 🎉 You're Ready!

**Quick Start**: Follow [AI_QUICK_START.md](AI_QUICK_START.md)  
**Full Docs**: Read [LOCAL_AI_SYSTEMS.md](LOCAL_AI_SYSTEMS.md)  
**Offline Mode**: See [OFFLINE_MODE.md](OFFLINE_MODE.md)  

**Have fun creating with AI!** 🚀🎨

---

**Last Updated**: 2024  
**DrawingStudio Version**: 1.0  
**Platform**: macOS (Apple Silicon)
