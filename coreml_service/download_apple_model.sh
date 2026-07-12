#!/bin/bash
set -e

echo "======================================"
echo "Downloading Apple Core ML Model"
echo "======================================"
echo ""
echo "This will download ~2GB (SD 1.5 Core ML)"
echo ""

# Create directory
mkdir -p ~/coreml_models
cd ~/coreml_models

# Check if already exists
if [ -d "coreml-stable-diffusion-v1-5" ]; then
    echo "✓ Model already exists at ~/coreml_models/coreml-stable-diffusion-v1-5"
    exit 0
fi

echo "Downloading Apple's Core ML Stable Diffusion 1.5..."
echo "This may take 5-10 minutes..."
echo ""

# Install git-lfs if not installed
if ! command -v git-lfs &> /dev/null; then
    echo "Installing git-lfs..."
    brew install git-lfs
    git lfs install
fi

# Clone the model
git clone https://huggingface.co/apple/coreml-stable-diffusion-v1-5

echo ""
echo "======================================"
echo "✅ Download Complete!"
echo "======================================"
echo ""
echo "Model location: ~/coreml_models/coreml-stable-diffusion-v1-5"
echo ""
echo "Now run DrawingStudio and enjoy GPU-accelerated image generation!"
echo ""
