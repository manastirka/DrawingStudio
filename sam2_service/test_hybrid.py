#!/usr/bin/env python3
"""Test hybrid endpoint locally"""
import sys
import numpy as np
from PIL import Image
import cv2
import time

# Test image
print("Creating test image...")
img = np.random.randint(0, 255, (1024, 681, 3), dtype=np.uint8)

print("\n1. Testing fast saliency...")
from fast_saliency import FastSaliency, get_largest_component, extract_roi_bbox
sal = FastSaliency()
mask, meta = sal.detect(img)
print(f"   Saliency: {meta['time_ms']:.1f}ms")

print("\n2. Testing component extraction...")
clean = get_largest_component(mask, min_area_percent=1.0)
if clean is None:
    print("   No component found - using full mask")
    clean = mask

print("\n3. Testing ROI extraction...")
roi_bbox = extract_roi_bbox(clean, pad_px=32, min_size=64)
if roi_bbox:
    x, y, w, h = roi_bbox
    print(f"   ROI: x={x}, y={y}, w={w}, h={h}")
    roi_img = img[y:y+h, x:x+w]
    print(f"   ROI size: {roi_img.shape}")
else:
    print("   No valid ROI")
    sys.exit(1)

print("\n4. Testing SAM2 on ROI...")
try:
    # Import SAM2
    from sam2.build_sam import build_sam2
    from sam2.sam2_image_predictor import SAM2ImagePredictor
    from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
    import torch
    
    print("   Loading SAM2...")
    device = "mps" if torch.backends.mps.is_available() else "cpu"
    sam2_checkpoint = "checkpoints/sam2_hiera_large.pt"
    model_cfg = "sam2_hiera_l.yaml"
    
    sam2_model = build_sam2(model_cfg, sam2_checkpoint, device=device)
    mask_gen = SAM2AutomaticMaskGenerator(sam2_model)
    
    print(f"   Running on ROI ({roi_img.shape})...")
    start = time.time()
    
    with torch.inference_mode():
        masks = mask_gen.generate(roi_img)
    
    elapsed = (time.time() - start) * 1000
    print(f"   SAM2 time: {elapsed:.1f}ms")
    print(f"   Found {len(masks)} masks")
    
    if masks:
        largest = max(masks, key=lambda x: np.sum(x['segmentation']))
        print(f"   Largest mask area: {np.sum(largest['segmentation'])} pixels")
        print("\n✓ Hybrid pipeline working!")
    else:
        print("\n⚠ No masks generated")
        
except Exception as e:
    print(f"\n✗ SAM2 failed: {e}")
    import traceback
    traceback.print_exc()
