# Network Access Summary - DrawingStudio AI Systems

## Quick Answer

Your local AI systems request network access for **one-time model downloads only**. After downloading, everything runs 100% locally with no internet required.

## What Requests Network Access?

### 1. YOLO (Human Detection) ⚠️
- **When**: First time you use human detection feature
- **What**: Downloads YOLOv8s model weights
- **Size**: ~22 MB
- **From**: GitHub (ultralytics/assets)
- **Cached**: `~/.cache/ultralytics/yolov8s.pt`
- **After Download**: Runs 100% locally, no network needed

### 2. SAM2 (Segmentation) ✅
- **When**: Startup (if checkpoint missing)
- **What**: Expects pre-downloaded checkpoint
- **Size**: ~900 MB
- **From**: Facebook AI Research
- **Location**: `sam2_service/checkpoints/sam2_hiera_large.pt`
- **Status**: Manual download required (see instructions below)

### 3. Stable Diffusion ✅
- **Network**: None - uses local GGUF models
- **Location**: `models/sd_v1.5_f16.gguf`
- **Status**: Already configured for offline use

### 4. LLaMA (Text AI) ✅
- **Network**: None - uses local GGUF models
- **Location**: `models/*.gguf`
- **Status**: Already configured for offline use

## Current Network Usage

```
┌─────────────────────┬──────────────┬────────────────┐
│ Component           │ Network Use  │ Status         │
├─────────────────────┼──────────────┼────────────────┤
│ YOLO                │ First use    │ ⚠️  Downloads  │
│ SAM2                │ Never*       │ ✅ Local only  │
│ Stable Diffusion    │ Never        │ ✅ Local only  │
│ LLaMA               │ Never        │ ✅ Local only  │
│ Qt Network (unused) │ Never        │ ✅ Local only  │
└─────────────────────┴──────────────┴────────────────┘

* SAM2 expects checkpoint to be manually downloaded
```

## Solution: Pre-download Everything

### Option 1: Automated Download (Recommended)

```bash
cd /Users/Lukovic/Apps/DrawingStudio
./download_all_models.sh
```

This downloads:
- ✅ YOLO model (~22 MB)
- ✅ SAM2 checkpoint (~900 MB)
- ✅ Verifies Stable Diffusion models
- ✅ Verifies LLaMA models

### Option 2: Manual Download

#### YOLO Model
```bash
mkdir -p ~/.cache/ultralytics
cd ~/.cache/ultralytics
curl -L "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt" -o yolov8s.pt
```

#### SAM2 Checkpoint
```bash
cd /Users/Lukovic/Apps/DrawingStudio/sam2_service
mkdir -p checkpoints
cd checkpoints
curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" -o sam2_hiera_large.pt
```

## Enable Offline Mode

After downloading models, enable offline mode to prevent any network access:

```bash
# Add to ~/.zshrc
export YOLO_OFFLINE=true
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1

# Apply changes
source ~/.zshrc
```

## Verify Offline Operation

```bash
# 1. Check all models are present
ls -lh ~/.cache/ultralytics/yolov8s.pt
ls -lh sam2_service/checkpoints/sam2_hiera_large.pt
ls -lh models/*.gguf

# 2. Disconnect from internet

# 3. Start DrawingStudio
./build/DrawingStudio

# 4. Test features:
#    - Human detection (YOLO)
#    - Object segmentation (SAM2)
#    - Image generation (Stable Diffusion)
#    - AI assistant (LLaMA)
```

## Why Network Access Appears

### macOS Firewall Prompts

When you see firewall prompts for:
- **Python** (`sam2_service/venv/bin/python3`)
- **DrawingStudio** app

This is because:

1. **YOLO First Run**: Ultralytics library tries to download model
2. **Python HTTP Server**: Flask services listen on localhost:5001
3. **Localhost Communication**: Qt app talks to Python services via HTTP

### What's Actually Happening

```
DrawingStudio (Qt/C++)
    ↓ HTTP localhost:5001
sam2_service (Python Flask)
    ↓ Loads YOLO
ultralytics library
    ↓ Checks for model
    ↓ Downloads if missing (first time only)
GitHub/HuggingFace
```

## Firewall Configuration

### Allow (Required)
- ✅ Localhost connections (127.0.0.1:5001)
- ✅ Python Flask server binding to localhost

### Block (Optional)
- ❌ Outbound connections to github.com
- ❌ Outbound connections to huggingface.co
- ❌ Outbound connections to dl.fbaipublicfiles.com

**Note**: Blocking outbound only works if all models are pre-downloaded.

## Testing Network Usage

### Monitor Network Connections

```bash
# Terminal 1: Start DrawingStudio
./build/DrawingStudio

# Terminal 2: Monitor network connections
lsof -i -P | grep -E "(DrawingStudio|python3)"
```

You should only see:
```
python3   12345  user   TCP localhost:5001 (LISTEN)
python3   12345  user   TCP localhost:5001->localhost:xxxxx (ESTABLISHED)
```

No external connections should appear after models are downloaded.

## Disk Space Requirements

Total space needed for all models:

```
YOLO:              22 MB
SAM2:             900 MB
Stable Diffusion: 2-4 GB
LLaMA:            4-8 GB
─────────────────────────
Total:            7-13 GB
```

## Performance Impact

**Offline mode has ZERO performance impact.**

All AI models run locally on your M3 Pro:
- YOLO: ~50-100ms per detection (MPS accelerated)
- SAM2: ~200-500ms per segmentation (MPS accelerated)
- Stable Diffusion: ~30-60s per image (CPU only, stable)
- LLaMA: ~20-50 tokens/sec (CPU only)

Network is only used for initial downloads.

## Common Questions

### Q: Why does YOLO need network access?
**A**: Only for first-time model download (~22MB). After that, it's cached locally.

### Q: Can I run completely offline?
**A**: Yes! Pre-download models, set environment variables, and disconnect from internet.

### Q: What about updates?
**A**: Models don't auto-update. You control when to download newer versions.

### Q: Is my data sent anywhere?
**A**: No. All processing happens locally. No telemetry, no cloud APIs.

### Q: Why not bundle models with the app?
**A**: Models are large (7-13 GB). Separate download keeps app size small and lets you choose which models to use.

## Security Notes

✅ **No telemetry**: No usage data sent anywhere
✅ **No cloud APIs**: Everything runs on your Mac
✅ **No authentication**: No API keys or accounts needed
✅ **No data upload**: Images never leave your computer
✅ **Open source**: All model sources are public and verifiable

## Troubleshooting

### "Failed to download YOLO model"

1. Check internet connection
2. Try manual download (see above)
3. Check firewall isn't blocking Python

### "SAM2 checkpoint not found"

```bash
cd sam2_service/checkpoints
curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" -o sam2_hiera_large.pt
```

### "Human detector not initialized"

YOLO model missing. Run:
```bash
./download_all_models.sh
```

## Summary

✅ **After initial setup**: 100% offline operation
✅ **All processing**: Local on your M3 Pro
✅ **No cloud APIs**: Everything runs on-device
✅ **Total downloads**: ~1 GB (YOLO + SAM2)
✅ **One-time setup**: Download once, use forever

Run `./download_all_models.sh` and you're done! 🎉
