#!/bin/bash
set -e

echo "======================================"
echo "Installing Apple's Core ML SD"
echo "======================================"
echo ""

# Create virtual environment if not exists
if [ ! -d "venv" ]; then
    python3 -m venv venv
fi

source venv/bin/activate

# Upgrade pip
pip install --upgrade pip

# Install Apple's ml-stable-diffusion
echo "Installing Apple's official Core ML Stable Diffusion..."
pip install git+https://github.com/apple/ml-stable-diffusion.git

# Install dependencies
pip install Pillow numpy

echo ""
echo "======================================"
echo "✅ Installation Complete!"
echo "======================================"
echo ""
echo "Now download a Core ML model:"
echo ""
echo "Option 1: SD 1.5 (Fastest, ~2GB)"
echo "  Download from: https://huggingface.co/apple/coreml-stable-diffusion-v1-5"
echo "  Extract to: ~/coreml_models/coreml-stable-diffusion-v1-5"
echo ""
echo "Option 2: SD 2.1 (Better quality, ~4GB)"
echo "  Download from: https://huggingface.co/apple/coreml-stable-diffusion-2-1-base"
echo "  Extract to: ~/coreml_models/coreml-stable-diffusion-2-1-base"
echo ""
echo "These are NATIVE Core ML models optimized by Apple!"
echo "No conversion needed, no black images, pure GPU!"
echo ""
