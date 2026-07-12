#!/bin/bash
# Start SAM2 Service for DrawingStudio
# This script starts the SAM2 segmentation service on port 5001

echo "=================================================="
echo "Starting SAM2 Service for DrawingStudio"
echo "=================================================="

cd "$(dirname "$0")/sam2_service"

# Check if virtual environment exists
if [ ! -d "venv" ]; then
    echo "Error: Virtual environment not found!"
    echo "Please run: cd sam2_service && python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt"
    exit 1
fi

# Check if checkpoint exists
if [ ! -f "checkpoints/sam2_hiera_large.pt" ]; then
    echo "Error: SAM2 checkpoint not found!"
    echo "Please download the checkpoint:"
    echo "  mkdir -p checkpoints"
    echo "  cd checkpoints"
    echo "  wget https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt"
    exit 1
fi

# Activate virtual environment and start service
echo "Activating virtual environment..."
source venv/bin/activate

echo "Starting SAM2 service on port 5001..."
echo "Press Ctrl+C to stop the service"
echo ""

python3 sam2_service.py
