#!/bin/bash
# Start SAM2 Service for DrawingStudio in OFFLINE MODE
# This script starts the SAM2 segmentation service with offline mode enabled

echo "=================================================="
echo "Starting SAM2 Service (OFFLINE MODE)"
echo "=================================================="

cd "$(dirname "$0")/sam2_service"

# Check if virtual environment exists
if [ ! -d "venv" ]; then
    echo "❌ Error: Virtual environment not found!"
    echo "Please run: cd sam2_service && python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt"
    exit 1
fi

# Check if checkpoint exists
if [ ! -f "checkpoints/sam2_hiera_large.pt" ]; then
    echo "❌ Error: SAM2 checkpoint not found!"
    echo "Please download the checkpoint:"
    echo "  cd sam2_service"
    echo "  mkdir -p checkpoints"
    echo "  cd checkpoints"
    echo "  curl -L https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt -o sam2_hiera_large.pt"
    echo ""
    echo "Or run: ./download_all_models.sh"
    exit 1
fi

# Check if YOLO model exists
if [ ! -f "$HOME/.cache/ultralytics/yolov8s.pt" ]; then
    echo "⚠️  Warning: YOLO model not found!"
    echo "Human detection will not work. Download with:"
    echo "  ./download_all_models.sh"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Set offline mode environment variables
echo "🔒 Enabling offline mode..."
export YOLO_OFFLINE=true
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1

echo "✓ YOLO_OFFLINE=true"
echo "✓ HF_HUB_OFFLINE=1"
echo "✓ TRANSFORMERS_OFFLINE=1"
echo ""

# Activate virtual environment and start service
echo "Activating virtual environment..."
source venv/bin/activate

echo "Starting SAM2 service on port 5001..."
echo "Press Ctrl+C to stop the service"
echo ""
echo "🔒 Running in OFFLINE MODE - no network access"
echo ""

python3 sam2_service.py
