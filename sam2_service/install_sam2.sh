#!/bin/bash
# SAM2 Installation Script for Apple M3 Pro

echo "=========================================="
echo "Installing SAM2 for DrawingStudio"
echo "Optimized for Apple M3 Pro"
echo "=========================================="

# Check Python version
python3 --version
if [ $? -ne 0 ]; then
    echo "✗ Python 3 not found. Please install Python 3.8 or later."
    exit 1
fi

echo ""
echo "Step 1: Installing Python dependencies..."
pip3 install flask flask-cors torch torchvision opencv-python pillow numpy

echo ""
echo "Step 2: Cloning SAM2 repository..."
if [ ! -d "segment-anything-2" ]; then
    git clone https://github.com/facebookresearch/segment-anything-2.git
    cd segment-anything-2
    pip3 install -e .
    cd ..
else
    echo "  SAM2 repository already exists"
fi

echo ""
echo "Step 3: Creating checkpoints directory..."
mkdir -p checkpoints

echo ""
echo "Step 4: Downloading SAM2 model checkpoint..."
echo "  Model: sam2_hiera_large.pt (~900MB)"
echo "  This may take a few minutes..."

cd checkpoints
if [ ! -f "sam2_hiera_large.pt" ]; then
    curl -L -o sam2_hiera_large.pt https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt
    echo "  ✓ Model downloaded"
else
    echo "  Model already exists"
fi
cd ..

echo ""
echo "Step 5: Testing MPS (Metal) support..."
python3 -c "import torch; print('MPS Available:', torch.backends.mps.is_available()); print('MPS Built:', torch.backends.mps.is_built())"

echo ""
echo "=========================================="
echo "✓ Installation complete!"
echo "=========================================="
echo ""
echo "To start the service:"
echo "  cd sam2_service"
echo "  python3 sam2_service.py"
echo ""
echo "The service will run on http://localhost:5000"
echo "=========================================="
