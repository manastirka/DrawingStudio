#!/bin/bash

# Download Stable Diffusion model for DrawingStudio

echo "======================================"
echo "Downloading Stable Diffusion Model"
echo "======================================"
echo ""

# Create models directory
mkdir -p models
cd models

echo "📦 Downloading Stable Diffusion v1.5 (Q4 quantized, ~2GB)..."
echo "This may take a few minutes depending on your internet speed."
echo ""

# Download SD 1.5 Q4 model (smaller, faster)
# Using public mirror
MODEL_URL="https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf-q4_0/resolve/main/sd_v1.5_q4_0.gguf"
MODEL_FILE="sd_v1.5_q4_0.gguf"

if [ -f "$MODEL_FILE" ]; then
    echo "✓ Model already exists: $MODEL_FILE"
else
    echo "Downloading from: $MODEL_URL"
    curl -L -o "$MODEL_FILE" "$MODEL_URL"
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✓ Model downloaded successfully!"
    else
        echo ""
        echo "❌ Download failed"
        exit 1
    fi
fi

echo ""
echo "======================================"
echo "✅ Setup Complete!"
echo "======================================"
echo ""
echo "Model location:"
echo "  $(pwd)/$MODEL_FILE"
echo ""
echo "Model size: $(du -h "$MODEL_FILE" | cut -f1)"
echo ""
echo "Next steps:"
echo "1. Rebuild DrawingStudio:"
echo "   cd ../build"
echo "   cmake .."
echo "   make"
echo ""
echo "2. Run DrawingStudio:"
echo "   ./DrawingStudio"
echo ""
echo "3. Try AI commands:"
echo "   'generate image of a cat'"
echo "   'create a sunset landscape'"
echo ""
