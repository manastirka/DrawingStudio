#!/bin/bash
# Download all AI models for offline use

set -e

echo "📦 Downloading all AI models for offline use..."
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 1. YOLO Model
echo "1️⃣  Downloading YOLOv8 model..."
YOLO_DIR="$HOME/.cache/ultralytics"
mkdir -p "$YOLO_DIR"
cd "$YOLO_DIR"

if [ -f "yolov8s.pt" ]; then
    echo -e "${GREEN}✓ YOLOv8 already exists${NC}"
else
    echo "  Downloading from GitHub releases..."
    curl -L "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt" -o yolov8s.pt
    echo -e "${GREEN}✓ YOLOv8 downloaded ($(du -h yolov8s.pt | cut -f1))${NC}"
fi
echo ""

# 2. SAM2 Model
echo "2️⃣  Downloading SAM2 model..."
SAM2_DIR="$(dirname "$0")/sam2_service/checkpoints"
mkdir -p "$SAM2_DIR"
cd "$SAM2_DIR"

if [ -f "sam2_hiera_large.pt" ]; then
    echo -e "${GREEN}✓ SAM2 already exists${NC}"
else
    echo "  Downloading from Facebook AI (this may take a while, ~900MB)..."
    curl -L "https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt" -o sam2_hiera_large.pt
    echo -e "${GREEN}✓ SAM2 downloaded ($(du -h sam2_hiera_large.pt | cut -f1))${NC}"
fi
echo ""

# 3. Stable Diffusion (check if exists)
echo "3️⃣  Checking Stable Diffusion models..."
MODELS_DIR="$(dirname "$0")/models"
if [ -f "$MODELS_DIR/sd_v1.5_f16.gguf" ] || [ -f "$MODELS_DIR/sd1.5-f16.gguf" ]; then
    echo -e "${GREEN}✓ Stable Diffusion model exists${NC}"
else
    echo -e "${YELLOW}⚠️  Stable Diffusion model not found in models/ directory${NC}"
    echo "  Expected location: $MODELS_DIR/sd_v1.5_f16.gguf"
    echo "  Download from: https://huggingface.co/stabilityai/stable-diffusion-v1-5"
fi
echo ""

# 4. LLaMA (check if exists)
echo "4️⃣  Checking LLaMA models..."
if ls "$MODELS_DIR"/*.gguf 1> /dev/null 2>&1; then
    echo -e "${GREEN}✓ LLaMA models exist:${NC}"
    ls -lh "$MODELS_DIR"/*.gguf | awk '{print "  - " $9 " (" $5 ")"}'
else
    echo -e "${YELLOW}⚠️  LLaMA models not found in models/ directory${NC}"
    echo "  Expected location: $MODELS_DIR/*.gguf"
    echo "  Download from: https://huggingface.co/models"
fi
echo ""

# Summary
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 Model Download Summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check each model
YOLO_STATUS="${RED}✗ Missing${NC}"
[ -f "$HOME/.cache/ultralytics/yolov8s.pt" ] && YOLO_STATUS="${GREEN}✓ Ready${NC}"

SAM2_STATUS="${RED}✗ Missing${NC}"
[ -f "$(dirname "$0")/sam2_service/checkpoints/sam2_hiera_large.pt" ] && SAM2_STATUS="${GREEN}✓ Ready${NC}"

SD_STATUS="${RED}✗ Missing${NC}"
[ -f "$MODELS_DIR/sd_v1.5_f16.gguf" ] || [ -f "$MODELS_DIR/sd1.5-f16.gguf" ] && SD_STATUS="${GREEN}✓ Ready${NC}"

LLAMA_STATUS="${RED}✗ Missing${NC}"
ls "$MODELS_DIR"/*.gguf 1> /dev/null 2>&1 && LLAMA_STATUS="${GREEN}✓ Ready${NC}"

echo -e "YOLO (Human Detection):  $YOLO_STATUS"
echo -e "SAM2 (Segmentation):     $SAM2_STATUS"
echo -e "Stable Diffusion:        $SD_STATUS"
echo -e "LLaMA (Text AI):         $LLAMA_STATUS"
echo ""

# Offline mode instructions
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔒 Enable Offline Mode"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Add these to your ~/.zshrc or ~/.bash_profile:"
echo ""
echo "  export YOLO_OFFLINE=true"
echo "  export HF_HUB_OFFLINE=1"
echo "  export TRANSFORMERS_OFFLINE=1"
echo ""
echo "Then run: source ~/.zshrc"
echo ""

echo -e "${GREEN}✅ Setup complete!${NC}"
echo "See OFFLINE_MODE.md for more details."
