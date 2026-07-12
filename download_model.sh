#!/bin/bash

# Download Llama 3.2 1B model for DrawingStudio AI Assistant

echo "Downloading Llama 3.2 1B Instruct model (Q4_K_M quantization - 700MB)..."
echo "This model provides the best balance of quality and speed."
echo ""

cd models

# Check if model already exists
if [ -f "Llama-3.2-1B-Instruct-Q4_K_M.gguf" ]; then
    echo "Model already exists. Skipping download."
    echo "If you want to re-download, delete the file first:"
    echo "  rm models/Llama-3.2-1B-Instruct-Q4_K_M.gguf"
    exit 0
fi

# Download using wget or curl
if command -v wget &> /dev/null; then
    wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf
elif command -v curl &> /dev/null; then
    curl -L -O https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf
else
    echo "Error: Neither wget nor curl found. Please install one of them."
    echo "  brew install wget"
    exit 1
fi

echo ""
echo "✓ Model downloaded successfully!"
echo "  Location: models/Llama-3.2-1B-Instruct-Q4_K_M.gguf"
echo "  Size: ~700MB"
echo ""
echo "The AI Assistant will automatically use this model when you run DrawingStudio."
