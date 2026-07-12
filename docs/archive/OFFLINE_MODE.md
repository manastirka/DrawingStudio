# Offline Mode Configuration for DrawingStudio

## Overview

DrawingStudio uses several local AI systems that may request network access for model downloads. This guide explains how to configure fully offline operation.

## Network Access Reasons

### 1. YOLO (Human Detection)
- **Location**: `sam2_service/human_detector.py`
- **Network Use**: Auto-downloads YOLOv8 model (~22MB) on first use
- **Cache Location**: `~/.cache/ultralytics/` or `~/.ultralytics/`
- **Model File**: `yolov8s.pt`

### 2. SAM2 (Segmentation)
- **Location**: `sam2_service/sam2_service.py`
- **Network Use**: Downloads model checkpoints if not present
- **Expected Location**: `sam2_service/checkpoints/sam2_hiera_large.pt`
- **Download URL**: https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt

### 3. Stable Diffusion
- **Location**: `src/DiffusionHelper.cpp`, `src/CoreMLDiffusionHelper.cpp`
- **Network Use**: Model downloads, tokenizer downloads
- **Models**: Local GGUF models in `models/` directory

### 4. LLaMA (Text Generation)
- **Location**: `src/LLMHelper.cpp`
- **Network Use**: Model downloads from HuggingFace
- **Models**: Local GGUF models in `models/` directory

## Solution: Enable Offline Mode

### Step 1: Pre-download All Models

Run this script to download all required models:

```bash
#!/bin/bash
# download_all_models.sh

echo "📦 Downloading all AI models for offline use..."

# 1. YOLO Model
echo "1️⃣ Downloading YOLOv8 model..."
mkdir -p ~/.cache/ultralytics
cd ~/.cache/ultralytics
if [ ! -f "yolov8s.pt" ]; then
    curl -L "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt" -o yolov8s.pt
    echo "✓ YOLOv8 downloaded"
else
    echo "✓ YOLOv8 already exists"
fi

# 2. SAM2 Model
echo "2️⃣ Downloading SAM2 model..."
cd /Users/Lukovic/Apps/DrawingStudio/sam2_service
mkdir -p checkpoints
cd checkpoints
if [ ! -f "sam2_hiera_large.pt" ]; then
    curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" -o sam2_hiera_large.pt
    echo "✓ SAM2 downloaded"
else
    echo "✓ SAM2 already exists"
fi

# 3. Stable Diffusion (already local)
echo "3️⃣ Stable Diffusion models..."
if [ -f "/Users/Lukovic/Apps/DrawingStudio/models/sd_v1.5_f16.gguf" ]; then
    echo "✓ Stable Diffusion model exists"
else
    echo "⚠️  Stable Diffusion model not found in models/ directory"
fi

# 4. LLaMA (already local)
echo "4️⃣ LLaMA models..."
if ls /Users/Lukovic/Apps/DrawingStudio/models/*.gguf 1> /dev/null 2>&1; then
    echo "✓ LLaMA models exist"
else
    echo "⚠️  LLaMA models not found in models/ directory"
fi

echo ""
echo "✅ Model download complete!"
echo "You can now run DrawingStudio in offline mode."
```

### Step 2: Configure Offline Mode

Set environment variables to disable network access:

```bash
# Add to your shell profile (~/.zshrc or ~/.bash_profile)
export YOLO_OFFLINE=true
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1
```

### Step 3: Verify Models Are Local

Check that all models are present:

```bash
# YOLO
ls -lh ~/.cache/ultralytics/yolov8s.pt

# SAM2
ls -lh /Users/Lukovic/Apps/DrawingStudio/sam2_service/checkpoints/sam2_hiera_large.pt

# Stable Diffusion
ls -lh /Users/Lukovic/Apps/DrawingStudio/models/*.gguf

# LLaMA
ls -lh /Users/Lukovic/Apps/DrawingStudio/models/*.gguf
```

## Modify Code for Offline Mode

### Update human_detector.py

Add offline mode check:

```python
def _load_yolo(self):
    """Load YOLOv8 model"""
    try:
        from ultralytics import YOLO
        print("Loading YOLOv8 model...")
        
        # Check for local model first
        local_model = os.path.expanduser("~/.cache/ultralytics/yolov8s.pt")
        if os.path.exists(local_model):
            self.yolo = YOLO(local_model)
            print(f"✓ Loaded local YOLOv8 from {local_model}")
        else:
            # Will auto-download if not in offline mode
            if os.getenv("YOLO_OFFLINE", "").lower() == "true":
                raise RuntimeError("YOLO model not found and offline mode is enabled")
            self.yolo = YOLO('yolov8s.pt')
        
        # Set device
        if self.device == 'mps' and torch.backends.mps.is_available():
            self.yolo.to('mps')
        elif self.device == 'cuda' and torch.cuda.is_available():
            self.yolo.to('cuda')
        else:
            self.yolo.to('cpu')
        
        print(f"✓ YOLOv8 loaded on {self.device}")
        
    except Exception as e:
        print(f"✗ Error loading YOLO: {e}")
        self.yolo = None
```

## Current Status

Based on your system:

- ✅ **Stable Diffusion**: Using local CPU-only GGUF models (no network)
- ✅ **LLaMA**: Using local GGUF models (no network)
- ⚠️  **YOLO**: Will download on first use (~22MB)
- ⚠️  **SAM2**: Expects local checkpoint (manual download)

## Network Firewall Rules

If you want to completely block network access for the Python services:

### macOS Little Snitch / Lulu Rules

Block these applications:
- `/Users/Lukovic/Apps/DrawingStudio/sam2_service/venv/bin/python3`
- `/Users/Lukovic/Apps/DrawingStudio/coreml_service/venv/bin/python3`

Allow only localhost (127.0.0.1) connections.

### Command Line Firewall

```bash
# Block outbound connections for Python services
sudo pfctl -e
sudo pfctl -f /etc/pf.conf
```

## Testing Offline Mode

1. Disconnect from internet
2. Set environment variables:
   ```bash
   export YOLO_OFFLINE=true
   export HF_HUB_OFFLINE=1
   ```
3. Start DrawingStudio
4. Test each AI feature:
   - Human detection (YOLO)
   - Object segmentation (SAM2)
   - Image generation (Stable Diffusion)
   - Text generation (LLaMA)

## Troubleshooting

### "Model not found" errors

1. Check model locations listed above
2. Run the download script
3. Verify file permissions

### "Network connection failed"

This is expected in offline mode. Ensure all models are pre-downloaded.

### YOLO still trying to connect

1. Verify `YOLO_OFFLINE=true` is set
2. Check that model exists at `~/.cache/ultralytics/yolov8s.pt`
3. Update `human_detector.py` with offline check

## Model Sizes

Total disk space required:

- YOLOv8s: ~22 MB
- SAM2 Large: ~900 MB
- Stable Diffusion: ~2-4 GB (depending on model)
- LLaMA: ~4-8 GB (depending on model)

**Total: ~7-13 GB**

## Performance Impact

Offline mode has **no performance impact** - all models run locally regardless. The only difference is preventing automatic downloads.
