#!/bin/bash
# Start SAM3 Service for DrawingStudio
# This script starts the SAM3 segmentation service on port 5002

echo "=================================================="
echo "Starting SAM3 Service for DrawingStudio"
echo "=================================================="

cd "$(dirname "$0")/sam3_service"

# Check if virtual environment exists
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    source venv/bin/activate
    echo "Installing dependencies..."
    pip install -r requirements.txt
else
    source venv/bin/activate
fi

# Check for checkpoints (placeholder logic)
if [ ! -d "checkpoints" ]; then
    mkdir -p checkpoints
    echo "⚠ Note: You may need to download SAM3 checkpoints manually into sam3_service/checkpoints/"
fi

echo "Starting SAM3 service on port 5002..."
python3 sam3_service.py
