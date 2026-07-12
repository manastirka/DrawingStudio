#!/bin/bash
set -e

echo "======================================"
echo "Installing Core ML Stable Diffusion"
echo "======================================"
echo ""

# Check Python version
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 not found. Please install Python 3.9 or later."
    exit 1
fi

PYTHON_VERSION=$(python3 --version | cut -d' ' -f2 | cut -d'.' -f1,2)
echo "✓ Found Python $PYTHON_VERSION"

# Create virtual environment
echo ""
echo "Creating virtual environment..."
python3 -m venv venv
source venv/bin/activate

# Upgrade pip
echo ""
echo "Upgrading pip..."
pip install --upgrade pip

# Install dependencies
echo ""
echo "Installing Core ML Stable Diffusion..."
pip install coremltools
pip install diffusers transformers accelerate
pip install Pillow numpy torch torchvision

echo ""
echo "======================================"
echo "✅ Installation Complete!"
echo "======================================"
echo ""
echo "Next steps:"
echo "1. Download a Core ML model:"
echo "   python -m python_coreml_stable_diffusion.torch2coreml \\"
echo "     --convert-unet --convert-text-encoder --convert-vae-decoder \\"
echo "     --model-version stabilityai/stable-diffusion-2-1-base \\"
echo "     -o ~/coreml_models"
echo ""
echo "2. This will take 10-20 minutes and download ~5GB"
echo ""
echo "3. Or download pre-converted models from:"
echo "   https://huggingface.co/apple/coreml-stable-diffusion-2-1-base"
echo ""
echo "4. Rebuild DrawingStudio to use Core ML backend"
echo ""
