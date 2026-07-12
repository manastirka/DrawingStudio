#!/usr/bin/env python3
"""Download U²-Net-P model for fast saliency detection"""
import os
import gdown

# Create models directory
os.makedirs('models', exist_ok=True)

# U²-Net-P (Portable) - smaller and faster version
# Google Drive link for U²-Net-P
url = 'https://drive.google.com/uc?id=1tNuFmLv0TSNDjYIkjEdeH1IWKQdUA4HR'
output = 'models/u2netp.pth'

if os.path.exists(output):
    print(f"✓ U²-Net-P model already exists at {output}")
else:
    print("Downloading U²-Net-P model...")
    print("This may take a few minutes...")
    gdown.download(url, output, quiet=False)
    print(f"✓ Downloaded to {output}")
    
# Check file size
if os.path.exists(output):
    size_mb = os.path.getsize(output) / (1024 * 1024)
    print(f"Model size: {size_mb:.1f} MB")
    
    if size_mb < 1:
        print("⚠ Warning: File seems too small, download may have failed")
    else:
        print("✓ Model ready!")
