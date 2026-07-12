#!/bin/bash
set -e

echo "======================================"
echo "Downloading Stable Diffusion Model"
echo "======================================"
echo ""
echo "This will download ~4GB and take 5-10 minutes."
echo "This is a ONE-TIME download."
echo ""

# Activate virtual environment
source venv/bin/activate

# Download model using Python
python3 << 'EOF'
import torch
from diffusers import StableDiffusionPipeline

print("Downloading Stable Diffusion 2.1 Base...")
print("This may take 5-10 minutes depending on your internet speed.")
print("")

model_id = "stabilityai/stable-diffusion-2-1-base"

# Download and cache the model
pipe = StableDiffusionPipeline.from_pretrained(
    model_id,
    torch_dtype=torch.float16,
    use_safetensors=True
)

print("")
print("✓ Model downloaded successfully!")
print("✓ Cached in: ~/.cache/huggingface/")
print("")
print("You can now use image generation in DrawingStudio!")
EOF

echo ""
echo "======================================"
echo "✅ Setup Complete!"
echo "======================================"
