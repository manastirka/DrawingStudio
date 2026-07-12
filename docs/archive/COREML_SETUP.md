# Core ML Stable Diffusion Setup

## Why Core ML?

Core ML provides **native Apple Silicon optimization** with:
- ✅ **10-20x faster** than CPU (2-18 seconds vs 150 seconds)
- ✅ **No crashes** - stable Metal GPU acceleration
- ✅ Uses **Neural Engine + GPU** efficiently
- ✅ Lower power consumption than ggml Metal backend

## Installation

### Step 1: Install Core ML Dependencies

```bash
cd coreml_service
./install_coreml.sh
```

### Step 2: Download Core ML Model

**Option A: Convert from HuggingFace (Recommended)**
```bash
source coreml_service/venv/bin/activate
python -m python_coreml_stable_diffusion.torch2coreml \
  --convert-unet --convert-text-encoder --convert-vae-decoder \
  --model-version stabilityai/stable-diffusion-2-1-base \
  -o ~/coreml_models
```

This takes 10-20 minutes and downloads ~5GB.

**Option B: Download Pre-converted Model**
```bash
# Download from Apple's HuggingFace
# https://huggingface.co/apple/coreml-stable-diffusion-2-1-base
# Extract to ~/coreml_models/
```

### Step 3: Update CMakeLists.txt

Add Core ML helper to your build:

```cmake
set(SOURCES
    # ... existing sources ...
    src/CoreMLDiffusionHelper.cpp
)

set(HEADERS
    # ... existing headers ...
    include/CoreMLDiffusionHelper.h
)
```

### Step 4: Update MainWindow to Use Core ML

In `MainWindow.cpp`, replace DiffusionHelper with CoreMLDiffusionHelper:

```cpp
#include "CoreMLDiffusionHelper.h"

// In constructor:
m_diffusionHelper = new CoreMLDiffusionHelper(this);

// In initializeAIModelsAsync():
if (m_diffusionHelper->initialize()) {
    qDebug() << "✓ Core ML Stable Diffusion initialized";
    // Connect signals...
}
```

### Step 5: Rebuild

```bash
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Performance Expectations

| Mac Model | Image Size | Steps | Time |
|-----------|------------|-------|------|
| M1 (16GB) | 512x512 | 20 | ~18s |
| M1 Pro/Max | 512x512 | 20 | ~8-12s |
| M2 Pro/Max | 512x512 | 20 | ~6-10s |
| M3/M4 | 512x512 | 20 | ~2-5s |

## Troubleshooting

### Python not found
```bash
brew install python@3.11
```

### Model download fails
Download manually from HuggingFace and extract to `~/coreml_models/`

### Service won't start
Check Python dependencies:
```bash
source coreml_service/venv/bin/activate
python -c "import coremltools; print('OK')"
```

## Models

### Recommended Models

1. **SD 2.1 Base** (Default)
   - Best balance of speed and quality
   - 512x512 optimal resolution

2. **SDXL** (Advanced)
   - Better quality
   - Requires 16GB+ RAM
   - Slower but better results

3. **SD 1.5** (Lightweight)
   - Fastest option
   - Good for quick iterations

## Notes

- Core ML uses **Metal + Neural Engine** automatically
- No manual GPU configuration needed
- Works offline after model download
- Privacy-friendly (all local)
