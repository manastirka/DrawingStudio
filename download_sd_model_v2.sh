#!/bin/bash

# Download Stable Diffusion model - Alternative sources

echo "======================================"
echo "Stable Diffusion Model Download"
echo "======================================"
echo ""

# Create models directory
mkdir -p models
cd models

echo "📦 Downloading Stable Diffusion v1.5..."
echo ""
echo "Trying multiple sources..."
echo ""

# Model file
MODEL_FILE="sd-v1-5-q4_0.gguf"

if [ -f "$MODEL_FILE" ]; then
    SIZE=$(du -h "$MODEL_FILE" | cut -f1)
    if [ "$SIZE" != "29B" ] && [ "$SIZE" != "4.0K" ]; then
        echo "✓ Model already exists: $MODEL_FILE ($SIZE)"
        exit 0
    else
        echo "⚠ Removing incomplete download..."
        rm "$MODEL_FILE"
    fi
fi

# Try source 1: Direct HuggingFace CDN
echo "Trying HuggingFace CDN..."
curl -L --progress-bar \
    -H "User-Agent: Mozilla/5.0" \
    -o "$MODEL_FILE" \
    "https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf/resolve/main/sd-v1-5-q4_0.gguf?download=true"

# Check if download was successful
if [ -f "$MODEL_FILE" ]; then
    SIZE=$(du -h "$MODEL_FILE" | cut -f1)
    # Check if file is larger than 100MB (real model should be ~2GB)
    SIZE_BYTES=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE" 2>/dev/null)
    
    if [ "$SIZE_BYTES" -gt 100000000 ]; then
        echo ""
        echo "✓ Model downloaded successfully!"
        echo "  File: $MODEL_FILE"
        echo "  Size: $SIZE"
        echo ""
        echo "======================================"
        echo "✅ Download Complete!"
        echo "======================================"
        echo ""
        echo "Next steps:"
        echo "1. Rebuild DrawingStudio"
        echo "2. Test image generation"
        exit 0
    else
        echo "⚠ Download failed or incomplete (size: $SIZE)"
        rm "$MODEL_FILE"
    fi
fi

# Try source 2: Alternative quantization
echo ""
echo "Trying alternative model (Q8)..."
MODEL_FILE="sd-v1-5-q8_0.gguf"

curl -L --progress-bar \
    -H "User-Agent: Mozilla/5.0" \
    -o "$MODEL_FILE" \
    "https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf/resolve/main/sd-v1-5-q8_0.gguf?download=true"

if [ -f "$MODEL_FILE" ]; then
    SIZE_BYTES=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE" 2>/dev/null)
    if [ "$SIZE_BYTES" -gt 100000000 ]; then
        SIZE=$(du -h "$MODEL_FILE" | cut -f1)
        echo ""
        echo "✓ Model downloaded successfully!"
        echo "  File: $MODEL_FILE"
        echo "  Size: $SIZE"
        exit 0
    fi
fi

echo ""
echo "======================================"
echo "❌ Automatic download failed"
echo "======================================"
echo ""
echo "Manual download instructions:"
echo ""
echo "1. Visit: https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf"
echo "2. Download one of these files:"
echo "   - sd-v1-5-q4_0.gguf (~2GB, faster)"
echo "   - sd-v1-5-q8_0.gguf (~4GB, better quality)"
echo "3. Move to: $(pwd)/"
echo ""
echo "Or use git-lfs:"
echo "  git lfs install"
echo "  git clone https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf"
echo "  cp Stable-Diffusion-v1-5-gguf/sd-v1-5-q4_0.gguf ."
echo ""
