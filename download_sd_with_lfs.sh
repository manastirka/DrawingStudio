#!/bin/bash

# Download Stable Diffusion model using git-lfs

echo "======================================"
echo "Downloading Stable Diffusion Model"
echo "Using git-lfs for reliable download"
echo "======================================"
echo ""

# Create models directory
mkdir -p models
cd models

MODEL_FILE="sd-v1-5-q4_0.gguf"

# Check if already downloaded
if [ -f "$MODEL_FILE" ]; then
    SIZE_BYTES=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE" 2>/dev/null)
    if [ "$SIZE_BYTES" -gt 100000000 ]; then
        SIZE=$(du -h "$MODEL_FILE" | cut -f1)
        echo "✓ Model already exists: $MODEL_FILE ($SIZE)"
        exit 0
    else
        echo "⚠ Removing incomplete download..."
        rm -f "$MODEL_FILE"
    fi
fi

echo "📦 Cloning model repository..."
echo "This will download ~2GB, please be patient..."
echo ""

# Clone with git-lfs (only the specific file we need)
GIT_LFS_SKIP_SMUDGE=1 git clone https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf sd-temp

if [ $? -ne 0 ]; then
    echo "❌ Failed to clone repository"
    rm -rf sd-temp
    exit 1
fi

cd sd-temp

# Download only the Q4 model file
echo ""
echo "📥 Downloading model file..."
git lfs pull --include="sd-v1-5-q4_0.gguf"

if [ -f "sd-v1-5-q4_0.gguf" ]; then
    SIZE_BYTES=$(stat -f%z "sd-v1-5-q4_0.gguf" 2>/dev/null || stat -c%s "sd-v1-5-q4_0.gguf" 2>/dev/null)
    
    if [ "$SIZE_BYTES" -gt 100000000 ]; then
        # Move to models directory
        mv "sd-v1-5-q4_0.gguf" "../$MODEL_FILE"
        cd ..
        rm -rf sd-temp
        
        SIZE=$(du -h "$MODEL_FILE" | cut -f1)
        echo ""
        echo "======================================"
        echo "✅ Download Complete!"
        echo "======================================"
        echo ""
        echo "Model: $MODEL_FILE"
        echo "Size: $SIZE"
        echo "Location: $(pwd)/$MODEL_FILE"
        echo ""
        echo "Next steps:"
        echo "1. Re-enable Stable Diffusion in CMakeLists.txt"
        echo "2. Rebuild: cd ../build && cmake .. && make"
        echo "3. Test: ./DrawingStudio"
        echo ""
        exit 0
    fi
fi

cd ..
rm -rf sd-temp

echo ""
echo "======================================"
echo "❌ Download failed"
echo "======================================"
echo ""
echo "Please try manual download:"
echo "1. Visit: https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf/tree/main"
echo "2. Download: sd-v1-5-q4_0.gguf"
echo "3. Move to: $(pwd)/"
echo ""
