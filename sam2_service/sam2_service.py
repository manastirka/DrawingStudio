#!/usr/bin/env python3
"""
SAM2 Segmentation Service for DrawingStudio
Optimized for Apple M3 Pro with MPS acceleration
Enhanced accuracy with multi-scale detection and iterative refinement
"""

from flask import Flask, request, jsonify
import torch
import numpy as np
from PIL import Image, ImageEnhance, ImageFilter
import io
import base64
import cv2
import sys
import os
from functools import lru_cache
import hashlib
import hmac

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 64 * 1024 * 1024

SAM2_PROTOCOL_VERSION = '2'
SAM2_AUTH_TOKEN = os.environ.get('DRAWINGSTUDIO_SAM2_TOKEN', '').strip().encode('utf-8')
if len(SAM2_AUTH_TOKEN) < 16:
    raise RuntimeError(
        'DRAWINGSTUDIO_SAM2_TOKEN must contain at least 16 bytes; '
        'start the service through DrawingStudio or set it explicitly')


@app.before_request
def require_authentication():
    """Reject every request that does not carry the per-session bearer token."""
    supplied = request.headers.get('Authorization', '').encode('utf-8')
    expected = b'Bearer ' + SAM2_AUTH_TOKEN
    if not hmac.compare_digest(supplied, expected):
        return jsonify({'status': 'error', 'error': 'Unauthorized'}), 401
    content_length = request.content_length
    if content_length is not None and content_length > app.config['MAX_CONTENT_LENGTH']:
        return jsonify({'status': 'error', 'error': 'Request body too large'}), 413


@app.after_request
def secure_response(response):
    response.headers['Cache-Control'] = 'no-store'
    response.headers['X-Content-Type-Options'] = 'nosniff'
    response.headers['X-DrawingStudio-SAM2-Protocol'] = SAM2_PROTOCOL_VERSION
    return response


@app.errorhandler(413)
def request_too_large(_error):
    return jsonify({'status': 'error', 'error': 'Request body too large'}), 413

# Global variables
predictor = None
device = None
mask_generator = None
human_detector = None

# Performance optimization caches
image_cache = {}
embedding_cache = {}  # Cache image embeddings
MAX_CACHE_SIZE = 10  # Keep last 10 embeddings

# Mask cache for instant repeated images
from mask_cache import MaskCache
mask_cache = MaskCache(max_memory_items=10, cache_dir='cache')

# Progress tracking for real-time updates
progress_tracker = {
    'current_task': None,
    'progress': 0,
    'message': '',
    'timestamp': 0
}

def update_progress(progress, message):
    """Update global progress tracker"""
    import time
    progress_tracker['progress'] = progress
    progress_tracker['message'] = message
    progress_tracker['timestamp'] = time.time()
    print(f"Progress: {progress}% - {message}")

def get_focus_mask(image):
    """
    Detect in-focus (sharp) areas of the image using edge detection
    Returns a binary mask where 1 = in-focus, 0 = blurry/background
    """
    # Convert to grayscale
    gray = cv2.cvtColor(image, cv2.COLOR_RGB2GRAY)
    
    # Apply Laplacian to detect edges (sharp areas have more edges)
    laplacian = cv2.Laplacian(gray, cv2.CV_64F)
    laplacian_abs = np.abs(laplacian)
    
    # Blur to get local sharpness map
    sharpness = cv2.GaussianBlur(laplacian_abs, (21, 21), 0)
    
    # Threshold to get binary focus mask
    # Use Otsu's method to automatically find threshold
    _, focus_mask = cv2.threshold(sharpness.astype(np.uint8), 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    
    # Dilate slightly to include edges of focused objects
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (15, 15))
    focus_mask = cv2.dilate(focus_mask, kernel, iterations=2)
    
    return focus_mask.astype(bool)

def get_image_hash(image):
    """Generate hash for image caching"""
    return hashlib.md5(image.tobytes()).hexdigest()

def preprocess_image(image, enhance=False):  # DISABLED enhancement for speed
    """
    Preprocess image for better SAM2 detection
    """
    if not enhance:
        return image
    
    try:
        # Convert to PIL for enhancement
        pil_image = Image.fromarray(image)
        
        # Enhance contrast
        enhancer = ImageEnhance.Contrast(pil_image)
        pil_image = enhancer.enhance(1.2)
        
        # Enhance sharpness
        enhancer = ImageEnhance.Sharpness(pil_image)
        pil_image = enhancer.enhance(1.3)
        
        # Convert back to numpy
        enhanced = np.array(pil_image)
        return enhanced
    except Exception as e:
        print(f"Enhancement failed: {e}, returning original")
        return image

def refine_mask_with_grabcut(image_np, initial_mask, iterations=3):
    """
    Refine mask using GrabCut for better boundary accuracy
    """
    try:
        # Create mask for GrabCut
        gc_mask = np.where(initial_mask > 0, cv2.GC_PR_FGD, cv2.GC_PR_BGD).astype(np.uint8)
        
        # Create background and foreground models
        bgd_model = np.zeros((1, 65), np.float64)
        fgd_model = np.zeros((1, 65), np.float64)
        
        # Apply GrabCut
        cv2.grabCut(image_np, gc_mask, None, bgd_model, fgd_model, 
                    iterations, cv2.GC_INIT_WITH_MASK)
        
        # Extract refined mask
        refined_mask = np.where((gc_mask == cv2.GC_FGD) | (gc_mask == cv2.GC_PR_FGD), 1, 0).astype(np.uint8)
        
        return refined_mask
    except Exception as e:
        print(f"GrabCut refinement failed: {e}, using initial mask")
        return initial_mask

def refine_mask_edges(mask, image_np=None):
    """
    Refine mask edges using morphological operations and alpha matting
    """
    try:
        # Fill small holes
        kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel_small)
        
        # Remove small noise
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel_small)
        
        # Smooth boundaries with larger kernel
        kernel_large = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel_large)
        
        # Apply Gaussian blur for smoother edges
        mask_float = mask.astype(np.float32) / 255.0
        mask_float = cv2.GaussianBlur(mask_float, (5, 5), 1.0)
        
        # Threshold back to binary
        mask = (mask_float > 0.5).astype(np.uint8)
        
        return mask
    except Exception as e:
        print(f"Edge refinement failed: {e}")
        return mask

def initialize_sam2():
    """Initialize SAM2 model with Apple Silicon optimization"""
    global predictor, device, mask_generator
    
    try:
        # Import SAM2
        from sam2.build_sam import build_sam2
        from sam2.sam2_image_predictor import SAM2ImagePredictor
        from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
        
        # Detect best device
        if torch.backends.mps.is_available():
            device = torch.device("mps")
            print("✓ Using Apple Metal (GPU acceleration)")
        elif torch.cuda.is_available():
            device = torch.device("cuda")
            print("✓ Using CUDA")
        else:
            device = torch.device("cpu")
            print("⚠ Using CPU only")
        
        # Model configuration - try different models for better accuracy
        checkpoint_path = os.path.join(os.path.dirname(__file__), "checkpoints", "sam2_hiera_large.pt")
        model_cfg = "sam2_hiera_l.yaml"
        
        # Check if checkpoint exists
        if not os.path.exists(checkpoint_path):
            print(f"⚠ Checkpoint not found at {checkpoint_path}")
            # Try smaller model
            checkpoint_path = os.path.join(os.path.dirname(__file__), "checkpoints", "sam2_hiera_base_plus.pt")
            model_cfg = "sam2_hiera_b+.yaml"
            
            if not os.path.exists(checkpoint_path):
                print("Please download SAM2 checkpoint:")
                print("  mkdir -p checkpoints")
                print("  cd checkpoints")
                print("  wget https://dl.fbaipublicfiles.com/segment_anything_2/072824/sam2_hiera_large.pt")
                return False
        
        print(f"Loading SAM2 model from {checkpoint_path}...")
        sam2_model = build_sam2(model_cfg, checkpoint_path, device=device)
        
        global predictor, mask_generator, human_detector
        predictor = SAM2ImagePredictor(sam2_model)
        
        # Initialize automatic mask generator with balanced settings for multi-object detection
        mask_generator = SAM2AutomaticMaskGenerator(
            model=sam2_model,
            points_per_side=32,  # Dense grid for boundary accuracy
            pred_iou_thresh=0.86,  # Prefer higher-quality masks
            stability_score_thresh=0.90,  # Prefer stable boundaries
            crop_n_layers=1,  # Multi-scale detection
            crop_n_points_downscale_factor=2,
            min_mask_region_area=100,  # Filter small artifacts
        )
        
        # Initialize human detector (YOLO + SAM2)
        try:
            from human_detector import HumanDetector
            human_detector = HumanDetector(predictor, device=device)
            print(f"✓ Human detector initialized")
        except Exception as e:
            print(f"⚠ Human detector not available: {e}")
            human_detector = None
        
        print(f"✓ SAM2 loaded successfully on {device}")
        return True
        
    except ImportError as e:
        print(f"⚠ SAM2 not installed: {e}")
        print("Please install SAM2:")
        print("  git clone https://github.com/facebookresearch/segment-anything-2.git")
        print("  cd segment-anything-2")
        print("  pip install -e .")
        return False
    except Exception as e:
        print(f"✗ Error initializing SAM2: {e}")
        import traceback
        traceback.print_exc()
        return False

def extract_contour(mask, min_points=30, max_points=500):
    """
    Extract a precise contour from a binary mask.
    Keeps denser boundaries than before so subject outlines follow edges more closely.
    """
    try:
        print(f"extract_contour called: mask shape={mask.shape}, dtype={mask.dtype}", flush=True)
        # Convert to uint8 (handle both bool and numeric types)
        if mask.dtype.kind == 'b':  # 'b' = boolean kind
            mask_uint8 = mask.astype(np.uint8) * 255
        else:
            mask_bool = mask.astype(bool)
            mask_uint8 = mask_bool.astype(np.uint8) * 255
        print(f"Converted to uint8: min={mask_uint8.min()}, max={mask_uint8.max()}", flush=True)

        # Full boundary chain preserves fine edge detail (hair, fingers, corners)
        contours, _ = cv2.findContours(
            mask_uint8,
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_NONE
        )

        if not contours:
            print("No contours found!", flush=True)
            return []

        print(f"Found {len(contours)} external contours", flush=True)

        largest = max(contours, key=cv2.contourArea)
        contour_points = largest.reshape(-1, 2).astype(np.float32)

        try:
            from scipy.ndimage import gaussian_filter1d
            # Very light smoothing — remove pixel stair-steps without rounding off detail
            smoothed_x = gaussian_filter1d(contour_points[:, 0], sigma=0.8, mode='wrap')
            smoothed_y = gaussian_filter1d(contour_points[:, 1], sigma=0.8, mode='wrap')
            smoothed_contour = np.column_stack([smoothed_x, smoothed_y]).astype(np.float32)
            print(f"Applied Gaussian smoothing (σ=0.8) for precise edges")
        except Exception:
            smoothed_contour = contour_points
            print(f"Gaussian smoothing skipped (scipy not available)")

        perimeter = cv2.arcLength(largest, True)
        epsilon = 0.0004 * perimeter  # 0.04% — denser control points
        smoothed_contour_int = smoothed_contour.astype(np.int32).reshape(-1, 1, 2)
        simplified = cv2.approxPolyDP(smoothed_contour_int, epsilon, True)

        # If oversimplified, retry with a milder epsilon
        if len(simplified) < min_points and len(smoothed_contour_int) >= min_points:
            milder = max(0.00015 * perimeter, 0.5)
            simplified = cv2.approxPolyDP(smoothed_contour_int, milder, True)
            epsilon = milder

        # Cap point count for UI editability while keeping even coverage
        if len(simplified) > max_points:
            idx = np.linspace(0, len(simplified) - 1, max_points, dtype=np.int32)
            simplified = simplified[idx]

        print(f"Contour: {len(largest)} points -> {len(simplified)} points "
              f"(ε={epsilon:.3f}, {100.0 * epsilon / max(perimeter, 1e-6):.3f}% of perimeter)")

        if len(simplified) >= 4:
            points = simplified.reshape(-1, 2).astype(np.float32)
            min_x, min_y = points.min(axis=0)
            max_x, max_y = points.max(axis=0)
            print(f"Contour coordinate ranges: X=[{min_x:.1f}, {max_x:.1f}], Y=[{min_y:.1f}, {max_y:.1f}]", flush=True)
            print(f"First 3 points: {points[:3].tolist()}", flush=True)
            print(f"Mask shape was: {mask.shape} (height x width)", flush=True)
            return points.tolist()

        return []

    except Exception as e:
        print(f"Error extracting contour: {e}")
        import traceback
        traceback.print_exc()
        return []

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    cache_stats = mask_cache.get_stats()
    return jsonify({
        'status': 'ok',
        'auth_required': True,
        'protocol_version': SAM2_PROTOCOL_VERSION,
        'sam2_loaded': predictor is not None,
        'device': str(device) if device else 'none',
        'cache': cache_stats
    })

@app.route('/progress', methods=['GET'])
def get_progress():
    """Get current progress of ongoing detection"""
    return jsonify({
        'progress': progress_tracker['progress'],
        'message': progress_tracker['message'],
        'timestamp': progress_tracker['timestamp']
    })

@app.route('/cache/clear', methods=['POST'])
def clear_cache():
    """Clear all caches"""
    # Get stats before clearing
    stats_before = mask_cache.get_stats()
    print(f"Cache before clear: {stats_before}")
    
    # Clear cache
    mask_cache.clear()
    
    # Get stats after clearing
    stats_after = mask_cache.get_stats()
    print(f"Cache after clear: {stats_after}")
    print(f"✓ Cache cleared: removed {stats_before['disk_masks']} masks, {stats_before['disk_embeddings']} embeddings")
    
    return jsonify({
        'status': 'cleared',
        'removed': {
            'masks': stats_before['disk_masks'],
            'embeddings': stats_before['disk_embeddings'],
            'memory_items': stats_before['memory_items']
        }
    })

@app.route('/segment_all', methods=['POST'])
def segment_all_objects():
    """
    Detect ALL objects in the image using SAM2's automatic mask generator
    Input: Base64 encoded image
    Output: Array of all detected objects with masks and contours
    """
    try:
        if mask_generator is None:
            return jsonify({'error': 'SAM2 not initialized'}), 500
        
        data = request.json
        if 'image' not in data:
            return jsonify({'error': 'No image provided'}), 400
        
        image_b64 = data['image']
        
        # Decode image
        update_progress(35, "Decoding image...")
        image_bytes = base64.b64decode(image_b64)
        image = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        image_np = np.array(image)
        
        h, w = image_np.shape[:2]
        print(f"Processing image for ALL objects: {image_np.shape}")
        
        # Check cache first
        update_progress(40, "Checking cache...")
        image_hash = mask_cache.get_image_hash(image_np)
        cached_masks = mask_cache.get_masks(image_hash)
        
        if cached_masks is not None:
            print(f"✓ CACHE HIT! Returning {len(cached_masks)} cached masks instantly")
            update_progress(100, "Loaded from cache")
            return jsonify({
                'success': True,
                'objects': cached_masks,
                'total_found': len(cached_masks),
                'cached': True
            })
        
        print(f"Cache miss - processing image (hash: {image_hash[:8]}...)")
        
        # Get focus mask to identify sharp/in-focus areas (subjects)
        update_progress(45, "Analyzing image focus...")
        print(f"Computing focus mask to identify subjects...")
        focus_mask = get_focus_mask(image_np)
        focus_area_percent = (np.sum(focus_mask) / (w * h)) * 100
        print(f"Focus area: {focus_area_percent:.1f}% of image")
        
        # Preprocess for better accuracy (but keep original if it fails)
        try:
            image_np_enhanced = preprocess_image(image_np, enhance=True)
            if image_np_enhanced.shape == image_np.shape:
                image_np = image_np_enhanced
        except Exception as e:
            print(f"Preprocessing failed, using original: {e}")
        
        # Use automatic mask generator with optimized parameters
        update_progress(50, "Running SAM2 detection...")
        print(f"Using automatic mask generator...")
        
        # Start a progress simulation thread for the SAM2 detection phase
        import threading
        stop_simulation = threading.Event()
        
        def simulate_sam2_progress():
            """Simulate progress during SAM2 detection (which is blocking)"""
            progress = 50
            while not stop_simulation.is_set() and progress < 69:
                stop_simulation.wait(0.5)  # Update every 500ms
                if not stop_simulation.is_set():
                    progress = min(69, progress + 2)
                    update_progress(progress, f"SAM2 analyzing image... {progress}%")
        
        progress_thread = threading.Thread(target=simulate_sam2_progress, daemon=True)
        progress_thread.start()
        
        try:
            with torch.inference_mode():
                auto_masks = mask_generator.generate(image_np)
        finally:
            stop_simulation.set()
            progress_thread.join(timeout=1.0)
        
        print(f"Generated {len(auto_masks)} masks")
        update_progress(70, f"Processing {len(auto_masks)} detected masks...")
        
        # MULTI-CANDIDATE STRATEGY: Collect ALL good masks for user selection
        print(f"Multi-candidate mode: collecting all quality masks in focus")
        
        # Collect all quality masks that overlap with focus area
        candidate_masks = []
        
        for i, mask_data in enumerate(auto_masks):
            # Update progress for each mask processed
            if i % 10 == 0:
                progress = 70 + int((i / len(auto_masks)) * 15)
                update_progress(progress, f"Analyzing mask {i+1}/{len(auto_masks)}...")
            
            mask = mask_data['segmentation']
            area = mask_data['area']
            area_percent = (area / (w * h)) * 100
            stability = mask_data.get('stability_score', 0)
            predicted_iou = mask_data.get('predicted_iou', 0)
            
            # Skip background (>90%) and tiny masks (<1%)
            if area_percent > 90.0 or area_percent < 1.0:
                continue
            
            # Check overlap with focus area (subjects are usually in focus)
            mask_bool = mask.astype(bool)
            overlap_with_focus = np.sum(mask_bool & focus_mask)
            mask_area = np.sum(mask_bool)
            focus_ratio = overlap_with_focus / mask_area if mask_area > 0 else 0
            
            # Quality score: stability * iou * area_factor
            # Prefer masks around 5-60% area (wider range for subjects)
            area_factor = 1.0
            if 5.0 < area_percent < 60.0:
                area_factor = 1.3  # Boost subject-sized masks
            
            # Boost masks with some focus (but don't require it)
            focus_factor = 1.0 + (focus_ratio * 0.3)
            
            quality_score = stability * predicted_iou * area_factor * focus_factor
            
            # Collect quality masks (relaxed thresholds for more detections)
            if stability > 0.70 and predicted_iou > 0.70:
                candidate_masks.append({
                    'mask': mask_bool,
                    'score': quality_score,
                    'area_percent': area_percent,
                    'stability': stability,
                    'predicted_iou': predicted_iou,
                    'focus_ratio': focus_ratio,
                    'index': i
                })
                print(f"  Candidate {len(candidate_masks)}: mask {i}, area={area_percent:.1f}%, quality={quality_score:.3f}, focus={focus_ratio:.1%}")
        
        # Sort by quality score (best first) and take top 12 (reduced for speed)
        candidate_masks.sort(key=lambda x: x['score'], reverse=True)
        candidate_masks = candidate_masks[:12]
        
        print(f"Collected {len(candidate_masks)} candidates (top 12)")
        
        # Use the best one as the primary mask
        if candidate_masks:
            best_candidate = candidate_masks[0]
            best_mask = best_candidate['mask']
            best_score = best_candidate['score']
            best_area_percent = best_candidate['area_percent']
            print(f"Best mask: area={best_area_percent:.1f}%, score={best_score:.3f}")
        
        else:
            # Fallback: find the largest mask
            print(f"No person-sized masks found, using largest mask...")
            best_mask = None
            best_area_percent = 0
            best_score = 0
            
            for i, mask_data in enumerate(auto_masks):
                mask = mask_data['segmentation']
                area = mask_data['area']
                area_percent = (area / (w * h)) * 100
                stability = mask_data.get('stability_score', 0)
                predicted_iou = mask_data.get('predicted_iou', 0)
                
                if stability > 0.7 and predicted_iou > 0.7:
                    if area_percent > best_area_percent:
                        best_area_percent = area_percent
                        best_mask = mask
                        best_score = stability
                        print(f"  Mask {i}: area={area_percent:.1f}%, stability={stability:.3f}, iou={predicted_iou:.3f} ← BEST")
        
        print(f"Selected mask: area={best_area_percent:.1f}%, score={best_score:.3f}")
        
        # Dummy condition to skip the old merging code
        if False and best_mask is not None and best_area_percent < 50.0:
            print(f"Single detection too small, trying to merge overlapping objects...")
            
            # Collect all good masks from all points
            all_good_masks = []
            for point_x, point_y in test_points:
                masks, scores, _ = predictor.predict(
                    point_coords=np.array([[point_x, point_y]]),
                    point_labels=np.array([1]),
                    multimask_output=True
                )
                
                for mask, score in zip(masks, scores):
                    area = np.sum(mask)
                    area_percent = (area / (w * h)) * 100
                    
                    # Keep masks that are reasonable size and good quality
                    if area_percent > 3.0 and area_percent < 60.0 and score > 0.6:
                        all_good_masks.append(mask.astype(bool))
            
            print(f"Found {len(all_good_masks)} good masks to potentially merge")
            
            # Only merge masks that overlap with the best mask (same subject)
            if len(all_good_masks) > 1:
                best_mask_bool = best_mask.astype(bool)
                overlapping_masks = [best_mask_bool]
                
                for mask in all_good_masks:
                    # Check if this mask overlaps with best mask
                    overlap = np.sum(mask & best_mask_bool)
                    mask_area = np.sum(mask)
                    overlap_ratio = overlap / mask_area if mask_area > 0 else 0
                    
                    # If >20% overlap, it's likely the same subject
                    if overlap_ratio > 0.2:
                        overlapping_masks.append(mask)
                
                print(f"Found {len(overlapping_masks)} overlapping masks (same subject)")
                
                # Merge only overlapping masks
                if len(overlapping_masks) > 1:
                    merged_mask = np.zeros_like(best_mask_bool, dtype=bool)
                    for mask in overlapping_masks:
                        merged_mask = merged_mask | mask
                    
                    merged_area = np.sum(merged_mask)
                    merged_area_percent = (merged_area / (w * h)) * 100
                    
                    print(f"Merged mask: area={merged_area_percent:.1f}%")
                    
                    # Use merged mask if it's larger but not too much larger (avoid background)
                    if merged_area_percent > best_area_percent * 1.2 and merged_area_percent < 70.0:
                        print(f"Using merged mask (larger but controlled)")
                        best_mask = merged_mask
                        best_area_percent = merged_area_percent
                        best_score = 0.8
                    else:
                        print(f"Merged mask rejected (too large or not enough improvement)")
        
        # Clean up mask to remove artifacts (rays, thin lines)
        if best_mask is not None and best_area_percent > 3.0:
            print(f"Cleaning mask to remove artifacts and smooth boundaries...")
            
            # Convert to uint8 for morphological operations
            mask_uint8 = (best_mask.astype(bool) * 255).astype(np.uint8)
            
            # Minimal processing for single subject (fast & precise)
            # Just keep largest component and light smoothing
            num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(mask_uint8, connectivity=8)
            if num_labels > 1:
                largest_label = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
                cleaned_mask = np.where(labels == largest_label, 255, 0).astype(np.uint8)
                print(f"Kept largest connected component")
            else:
                cleaned_mask = mask_uint8
            
            # Light morphological close to seal tiny gaps without blurring edges
            kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
            cleaned_mask = cv2.morphologyEx(cleaned_mask, cv2.MORPH_CLOSE, kernel, iterations=1)
            print(f"Applied morphological close (3x3) for clean edges")
            
            cleaned_mask_bool = cleaned_mask.astype(bool)
            cleaned_area = np.sum(cleaned_mask_bool)
            cleaned_area_percent = (cleaned_area / (w * h)) * 100
            
            print(f"Original: {best_area_percent:.1f}%")
            print(f"After morphological cleaning: {cleaned_area_percent:.1f}%")
            
            # NO inversion check - we already filtered out background masks
            print(f"Inversion check: DISABLED (background already filtered out)")
            
            # Use cleaned mask if it's reasonable
            if cleaned_area_percent > 5.0 and cleaned_area_percent < 80.0:
                print(f"Using cleaned mask")
                best_mask = cleaned_mask_bool
                best_area_percent = cleaned_area_percent
            else:
                print(f"Cleaning removed too much or detected background, using original")
            
            print(f"Final result: area={best_area_percent:.1f}%, score={best_score:.3f}")
            
            # Create auto_masks from ALL candidates with cleaning
            auto_masks = []
            for idx, candidate in enumerate(candidate_masks):
                # Clean each candidate mask
                mask_uint8 = (candidate['mask'].astype(bool) * 255).astype(np.uint8)
                
                # Keep largest connected component
                num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(mask_uint8, connectivity=8)
                if num_labels > 1:
                    largest_label = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
                    cleaned_mask = np.where(labels == largest_label, 255, 0).astype(np.uint8)
                else:
                    cleaned_mask = mask_uint8
                
                # Light close — preserve boundary detail vs blur+hard-threshold
                kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
                cleaned_mask = cv2.morphologyEx(cleaned_mask, cv2.MORPH_CLOSE, kernel, iterations=1)
                
                cleaned_mask_bool = cleaned_mask.astype(bool)
                
                auto_masks.append({
                    'segmentation': cleaned_mask_bool,
                    'area': int(np.sum(cleaned_mask_bool)),
                    'stability_score': candidate['stability'],
                    'predicted_iou': candidate['predicted_iou']
                })
            
            print(f"Created and cleaned {len(auto_masks)} mask entries from candidates")
        else:
            print(f"All detection strategies failed, falling back to automatic mask generator")
            auto_masks = mask_generator.generate(image_np)
        
        print(f"Total masks to process: {len(auto_masks)}")
        
        # Return TOP candidate masks for user selection
        # Filter and rank masks, return top 12 for user to choose from (reduced from 20)
        update_progress(85, "Extracting contours...")
        valid_objects = []
        filtered_count = {'size': 0, 'quality': 0, 'contour': 0}
        
        for idx, mask_data in enumerate(auto_masks):
            if idx % 5 == 0:
                progress = 85 + int((idx / len(auto_masks)) * 10)
                update_progress(progress, f"Extracting contour {idx+1}/{len(auto_masks)}...")
            mask = mask_data['segmentation']
            area = mask_data['area']
            stability = mask_data.get('stability_score', 0)
            predicted_iou = mask_data.get('predicted_iou', 0)
            
            # Filter by size (accept reasonable range)
            area_percent = (area / (w * h)) * 100
            print(f"  Mask: area={area_percent:.1f}%, stability={stability:.3f}, iou={predicted_iou:.3f}", flush=True)
            
            # Filter: 1-90% area (skip tiny and background)
            if area_percent < 1.0 or area_percent > 90.0:
                filtered_count['size'] += 1
                continue
            
            # Filter: decent quality
            if stability < 0.7 or predicted_iou < 0.7:
                filtered_count['quality'] += 1
                continue
            
            # Calculate centrality (prefer objects near center)
            y_coords, x_coords = np.where(mask)
            if len(y_coords) > 0:
                center_y, center_x = np.mean(y_coords), np.mean(x_coords)
                center_dist = np.sqrt((center_x - w/2)**2 + (center_y - h/2)**2)
                centrality = 1.0 / (1.0 + center_dist / (min(w, h) * 0.5))
            else:
                centrality = 0
            
            # Skip edge refinement - it's destroying the masks
            # Just use the original mask from SAM2
            # Convert boolean to uint8 (0 or 255, not 0 or 1!)
            mask_refined = (mask.astype(bool) * 255).astype(np.uint8)
            
            # Extract high-quality contour
            contour = extract_contour(mask_refined)
            
            if len(contour) < 4:  # Skip invalid contours (need at least 4 for a polygon)
                filtered_count['contour'] += 1
                print(f"  Filtered by contour: only {len(contour)} points (need >=4)", flush=True)
                continue
            
            print(f"  ✓ Valid contour with {len(contour)} points", flush=True)
            
            # Combined quality score
            quality_score = stability * predicted_iou * centrality * (area_percent / 100)
            
            valid_objects.append({
                'mask': mask_refined,
                'contour': contour,
                'score': float(quality_score),
                'stability': float(stability),
                'predicted_iou': float(predicted_iou),
                'area': int(np.sum(mask_refined)),
                'area_percent': float(area_percent),
                'centrality': float(centrality)
            })
        
        print(f"After filtering: {len(valid_objects)} high-quality objects")
        print(f"Filtered out: {filtered_count['size']} by size, {filtered_count['quality']} by quality, {filtered_count['contour']} by contour")
        
        # Sort by quality score (best first) for user selection
        valid_objects.sort(key=lambda x: x['score'], reverse=True)
        
        print(f"Returning top 12 candidates for user selection")
        
        # Return top 12 objects for user to choose from (reduced for speed)
        results = []
        for i, obj in enumerate(valid_objects[:12]):
            # Encode as binary 0/255 — mask is already uint8 0/255, do NOT multiply again
            mask_uint8 = np.where(obj['mask'].astype(bool), np.uint8(255), np.uint8(0))
            mask_b64 = base64.b64encode(np.ascontiguousarray(mask_uint8)).decode('utf-8')
            results.append({
                'id': i,
                'mask': mask_b64,
                'mask_shape': list(obj['mask'].shape),
                'contour': obj['contour'],
                'score': obj['score'],
                'stability': obj['stability'],
                'predicted_iou': obj['predicted_iou'],
                'area_percent': obj['area_percent']
            })
            
            print(f"  Object {i}: score={obj['score']:.3f}, "
                  f"stability={obj['stability']:.3f}, "
                  f"iou={obj['predicted_iou']:.3f}, "
                  f"area={obj['area_percent']:.1f}%, "
                  f"contour_points={len(obj['contour'])}")
        
        # Cache the results for instant future access
        update_progress(95, "Caching results...")
        mask_cache.set_masks(image_hash, results)
        print(f"✓ Cached {len(results)} masks for future use")
        update_progress(100, "Detection complete!")
        
        return jsonify({
            'success': True,
            'objects': results,
            'total_found': len(valid_objects)
        })
        
    except Exception as e:
        print(f"Error in segment_all_objects: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

@app.route('/segment', methods=['POST'])
def segment_image():
    """
    Automatic subject segmentation
    Input: Base64 encoded image
    Output: Binary mask + contour points
    """
    try:
        if predictor is None:
            return jsonify({'error': 'SAM2 not initialized'}), 500
        
        data = request.json
        if 'image' not in data:
            return jsonify({'error': 'No image provided'}), 400
        
        image_b64 = data['image']
        
        # Decode image
        image_bytes = base64.b64decode(image_b64)
        image = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        image_np = np.array(image)
        
        print(f"Processing image: {image_np.shape}")
        
        # Set image for SAM2
        with torch.inference_mode():
            predictor.set_image(image_np)
            
            # Advanced subject detection strategy
            h, w = image_np.shape[:2]
            
            # Strategy 1: Try automatic mask generation first (best for main subjects)
            try:
                from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
                mask_generator = SAM2AutomaticMaskGenerator(predictor.model)
                auto_masks = mask_generator.generate(image_np)
                
                if auto_masks:
                    # Find the largest, most central mask
                    best_mask = None
                    best_score = 0
                    
                    for mask_data in auto_masks:
                        mask = mask_data['segmentation']
                        area = np.sum(mask)
                        stability = mask_data.get('stability_score', 0)
                        
                        # Calculate centrality (how close to center)
                        y_coords, x_coords = np.where(mask)
                        if len(y_coords) > 0:
                            center_y, center_x = np.mean(y_coords), np.mean(x_coords)
                            center_dist = np.sqrt((center_x - w/2)**2 + (center_y - h/2)**2)
                            centrality = 1.0 / (1.0 + center_dist / min(w, h))
                        else:
                            centrality = 0
                        
                        # Combined score: area + stability + centrality
                        combined_score = (area / (w * h)) * stability * centrality
                        
                        if combined_score > best_score:
                            best_score = combined_score
                            best_mask = mask
                    
                    if best_mask is not None:
                        print(f"Using automatic mask generation. Score: {best_score:.3f}")
                        masks = np.array([best_mask])
                        scores = np.array([best_score])
                    else:
                        raise Exception("No good automatic mask found")
                        
            except Exception as e:
                print(f"Automatic mask generation failed: {e}")
                print("Falling back to multi-strategy detection...")
                
                # Strategy 2: Try multiple single points and pick the best
                test_points = [
                    [w // 2, h // 2],      # Center
                    [w // 3, h // 3],      # Upper left
                    [2 * w // 3, h // 3],  # Upper right  
                    [w // 3, 2 * h // 3],  # Lower left
                    [2 * w // 3, 2 * h // 3], # Lower right
                    [w // 2, h // 3],      # Top center
                    [w // 2, 2 * h // 3],  # Bottom center
                ]
                
                best_masks = None
                best_scores = None
                best_overall_score = 0
                
                for point in test_points:
                    try:
                        test_masks, test_scores, _ = predictor.predict(
                            point_coords=np.array([point]),
                            point_labels=np.array([1]),
                            multimask_output=True
                        )
                        
                        max_score = np.max(test_scores)
                        if max_score > best_overall_score:
                            best_overall_score = max_score
                            best_masks = test_masks
                            best_scores = test_scores
                            print(f"Better result at point {point}: score {max_score:.3f}")
                            
                    except Exception as point_error:
                        print(f"Point {point} failed: {point_error}")
                        continue
                
                if best_masks is not None:
                    masks, scores = best_masks, best_scores
                    print(f"Multi-point detection complete. Best score: {best_overall_score:.3f}")
                else:
                    # Final fallback: simple center point
                    masks, scores, logits = predictor.predict(
                        point_coords=np.array([[w // 2, h // 2]]),
                        point_labels=np.array([1]),
                        multimask_output=True
                    )
                    print(f"Fallback center point. Score: {np.max(scores):.3f}")
        
        # Select best mask (highest score)
        best_idx = np.argmax(scores)
        mask = masks[best_idx]
        score = float(scores[best_idx])
        
        print(f"Segmentation complete. Score: {score:.3f}")
        
        # Extract contour
        contour = extract_contour(mask)
        
        # Encode mask as base64
        mask_uint8 = (mask * 255).astype(np.uint8)
        mask_b64 = base64.b64encode(mask_uint8.tobytes()).decode('utf-8')
        
        return jsonify({
            'success': True,
            'mask': mask_b64,
            'mask_shape': list(mask.shape),
            'contour': contour,
            'score': score
        })
        
    except Exception as e:
        print(f"Error in segment_image: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

@app.route('/segment_point', methods=['POST'])
def segment_with_point():
    """
    Enhanced point-based segmentation with accuracy improvements
    Input: Image + point coordinates + optional refinement settings
    Output: Best mask for clicked object with post-processing
    """
    try:
        if predictor is None:
            return jsonify({'error': 'SAM2 not initialized'}), 500
        
        data = request.json
        if 'image' not in data or 'point_x' not in data or 'point_y' not in data:
            return jsonify({'error': 'Missing required fields'}), 400
        
        image_b64 = data['image']
        point_x = int(data['point_x'])
        point_y = int(data['point_y'])
        
        # Optional refinement settings
        use_preprocessing = data.get('preprocess', True)
        use_grabcut = data.get('use_grabcut', True)
        use_edge_refinement = data.get('refine_edges', True)
        
        # Decode image
        image_bytes = base64.b64decode(image_b64)
        image = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        image_np = np.array(image)
        
        print(f"\n{'='*60}")
        print(f"Point-based segmentation: ({point_x}, {point_y})")
        print(f"Preprocessing: {use_preprocessing}, GrabCut: {use_grabcut}, Edge refinement: {use_edge_refinement}")
        
        # Preprocess image
        h, w = image_np.shape[:2]
        processed_np = preprocess_image(image_np, enhance=use_preprocessing)
        
        # Set image for SAM2
        with torch.inference_mode():
            predictor.set_image(processed_np)
            
            # Strategy 1: Multi-point sampling around the click
            sample_points = []
            sample_labels = []
            
            # Main click point
            sample_points.append([point_x, point_y])
            sample_labels.append(1)
            
            # Add nearby points in a small radius
            radius = min(w, h) * 0.015  # 1.5% of image size
            for angle in [0, 60, 120, 180, 240, 300]:
                angle_rad = np.radians(angle)
                offset_x = int(radius * np.cos(angle_rad))
                offset_y = int(radius * np.sin(angle_rad))
                
                new_x = max(0, min(w-1, point_x + offset_x))
                new_y = max(0, min(h-1, point_y + offset_y))
                
                sample_points.append([new_x, new_y])
                sample_labels.append(1)
            
            # Try multiple strategies and combine results
            best_mask = None
            best_score = 0
            all_masks = []
            all_scores = []
            
            # Strategy 1: Single point (baseline)
            try:
                masks1, scores1, _ = predictor.predict(
                    point_coords=np.array([[point_x, point_y]]),
                    point_labels=np.array([1]),
                    multimask_output=True
                )
                
                for i, (mask, score) in enumerate(zip(masks1, scores1)):
                    all_masks.append(mask)
                    all_scores.append(score)
                    if score > best_score:
                        best_score = score
                        best_mask = mask
                        print(f"Strategy 1 (single point) - mask {i}: score {score:.3f}")
            except Exception as e:
                print(f"Single point strategy failed: {e}")
            
            # Strategy 2: Multi-point sampling
            try:
                masks2, scores2, _ = predictor.predict(
                    point_coords=np.array(sample_points),
                    point_labels=np.array(sample_labels),
                    multimask_output=True
                )
                
                for i, (mask, score) in enumerate(zip(masks2, scores2)):
                    all_masks.append(mask)
                    all_scores.append(score)
                    if score > best_score:
                        best_score = score
                        best_mask = mask
                        print(f"Strategy 2 (multi-point) - mask {i}: score {score:.3f}")
            except Exception as e:
                print(f"Multi-point strategy failed: {e}")
            
            # Strategy 3: Foreground + background points
            try:
                # Add background points at corners and edges
                bg_points = [
                    [w//10, h//10], [9*w//10, h//10],
                    [9*w//10, 9*h//10], [w//10, 9*h//10]
                ]
                
                all_points = [[point_x, point_y]] + bg_points
                all_labels = [1] + [0, 0, 0, 0]
                
                masks3, scores3, _ = predictor.predict(
                    point_coords=np.array(all_points),
                    point_labels=np.array(all_labels),
                    multimask_output=True
                )
                
                for i, (mask, score) in enumerate(zip(masks3, scores3)):
                    all_masks.append(mask)
                    all_scores.append(score)
                    if score > best_score:
                        best_score = score
                        best_mask = mask
                        print(f"Strategy 3 (fg+bg) - mask {i}: score {score:.3f}")
            except Exception as e:
                print(f"Foreground+background strategy failed: {e}")
        
        if best_mask is None:
            return jsonify({'error': 'All segmentation strategies failed'}), 500
        
        mask = best_mask
        score = float(best_score)
        
        print(f"\n🎯 Best mask selected: score {score:.3f}")
        
        # Post-processing refinements
        if use_grabcut:
            print("🔄 Applying GrabCut refinement...")
            mask = refine_mask_with_grabcut(image_np, mask, iterations=3)
        
        if use_edge_refinement:
            print("✨ Refining edges...")
            mask = refine_mask_edges(mask, image_np)
        
        # Extract high-quality contour
        print("📐 Extracting contour...")
        contour = extract_contour(mask, min_points=30, max_points=500)
        
        print(f"✓ Complete! {len(contour)} contour points")
        print(f"{'='*60}\n")
        
        # Encode mask
        mask_uint8 = (mask * 255).astype(np.uint8)
        mask_b64 = base64.b64encode(mask_uint8.tobytes()).decode('utf-8')
        
        return jsonify({
            'success': True,
            'mask': mask_b64,
            'mask_shape': list(mask.shape),
            'contour': contour,
            'score': score,
            'num_strategies_tried': len(all_masks),
            'preprocessing_used': use_preprocessing,
            'grabcut_used': use_grabcut,
            'edge_refinement_used': use_edge_refinement
        })
        
    except Exception as e:
        print(f"Error in segment_with_point: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

@app.route('/segment_box', methods=['POST'])
def segment_with_box():
    """
    Segment based on bounding box
    Input: Image + box coordinates (x1, y1, x2, y2)
    Output: Mask for object in box
    """
    try:
        if predictor is None:
            return jsonify({'error': 'SAM2 not initialized'}), 500
        
        data = request.json
        required = ['image', 'x1', 'y1', 'x2', 'y2']
        if not all(k in data for k in required):
            return jsonify({'error': 'Missing required fields'}), 400
        
        image_b64 = data['image']
        box = np.array([data['x1'], data['y1'], data['x2'], data['y2']])
        
        # Decode image
        image_bytes = base64.b64decode(image_b64)
        image = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        image_np = np.array(image)
        
        print(f"Processing image with box: {box}")
        
        # Set image
        with torch.inference_mode():
            predictor.set_image(image_np)
            
            # Predict with box prompt
            masks, scores, logits = predictor.predict(
                point_coords=None,
                point_labels=None,
                box=box[None, :],
                multimask_output=False
            )
        
        mask = masks[0]
        score = float(scores[0])
        
        print(f"Segmentation complete. Score: {score:.3f}")
        
        # Extract contour
        contour = extract_contour(mask)
        
        # Encode mask
        mask_uint8 = (mask * 255).astype(np.uint8)
        mask_b64 = base64.b64encode(mask_uint8.tobytes()).decode('utf-8')
        
        return jsonify({
            'success': True,
            'mask': mask_b64,
            'mask_shape': list(mask.shape),
            'contour': contour,
            'score': score
        })
        
    except Exception as e:
        print(f"Error in segment_with_box: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

# ============================================================================
# FAST HYBRID ENDPOINT - Optimized for speed (<150ms target)
# ============================================================================

@app.route('/segment_fast', methods=['POST'])
def segment_fast():
    """
    Hybrid segmentation: Quick saliency ROI + SAM2 refinement
    Target: 1-2 seconds (faster than full SAM2, better quality than pure saliency)
    """
    import time
    
    try:
        start_total = time.time()
        timings = {}
        
        # Decode image
        data = request.json
        image_b64 = data['image']
        image_bytes = base64.b64decode(image_b64)
        image_pil = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        image_np = np.array(image_pil)
        
        orig_h, orig_w = image_np.shape[:2]
        
        # Step 1: Quick saliency to find ROI (very small image for speed)
        t = time.time()
        max_side = 256  # Very small for fast ROI detection
        scale = min(max_side / max(orig_h, orig_w), 1.0)
        new_w, new_h = int(orig_w * scale), int(orig_h * scale)
        img_tiny = cv2.resize(image_np, (new_w, new_h))
        
        # Improved saliency: combine edges + color contrast
        gray = cv2.cvtColor(img_tiny, cv2.COLOR_RGB2GRAY)
        
        # Edge detection
        edges = cv2.Canny(gray, 50, 150)
        edge_sal = cv2.GaussianBlur(edges.astype(float), (15, 15), 0)
        
        # Color contrast (helps find subjects)
        img_bgr = cv2.cvtColor(img_tiny, cv2.COLOR_RGB2BGR)
        img_lab = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2LAB)
        l_channel = img_lab[:,:,0]
        
        # Local contrast
        blur = cv2.GaussianBlur(l_channel.astype(float), (21, 21), 0)
        contrast = np.abs(l_channel.astype(float) - blur)
        
        # Combine edge and contrast saliency
        if edge_sal.max() > 0:
            edge_sal = edge_sal / edge_sal.max()
        if contrast.max() > 0:
            contrast = contrast / contrast.max()
        
        sal_map = (0.6 * edge_sal + 0.4 * contrast) * 255
        sal_map = sal_map.astype(np.uint8)
        
        _, binary_mask = cv2.threshold(sal_map, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        
        # Clean up
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        binary_mask = cv2.morphologyEx(binary_mask, cv2.MORPH_CLOSE, kernel)
        
        # Get largest component
        num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(binary_mask, connectivity=8)
        if num_labels > 1:
            areas = stats[1:, cv2.CC_STAT_AREA]
            largest_idx = np.argmax(areas) + 1
            binary_mask = np.where(labels == largest_idx, 255, 0).astype(np.uint8)
        
        # Extract ROI bbox with LARGE padding (to avoid cutting subject)
        contours, _ = cv2.findContours(binary_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            # Fallback to center 80% of image
            pad = int(min(new_w, new_h) * 0.1)
            roi_x, roi_y = pad, pad
            roi_w, roi_h = new_w - 2*pad, new_h - 2*pad
        else:
            x, y, w, h = cv2.boundingRect(np.vstack(contours))
            # LARGE padding: 30% of bbox size to ensure we don't cut the subject
            pad_w = int(w * 0.3)
            pad_h = int(h * 0.3)
            roi_x = max(0, x - pad_w)
            roi_y = max(0, y - pad_h)
            roi_w = min(new_w - roi_x, w + 2*pad_w)
            roi_h = min(new_h - roi_y, h + 2*pad_h)
        
        # Scale ROI back to original image
        roi_x_orig = int(roi_x / scale)
        roi_y_orig = int(roi_y / scale)
        roi_w_orig = int(roi_w / scale)
        roi_h_orig = int(roi_h / scale)
        
        timings['roi_detection'] = (time.time() - t) * 1000
        
        # Step 2: Run SAM2 on ROI only (much faster!)
        t = time.time()
        roi_image = image_np[roi_y_orig:roi_y_orig+roi_h_orig, roi_x_orig:roi_x_orig+roi_w_orig]
        
        # Resize ROI to balanced size (448px - good speed/quality tradeoff)
        max_roi_side = 448  # Balanced between speed and quality
        roi_scale = min(max_roi_side / max(roi_h_orig, roi_w_orig), 1.0)
        if roi_scale < 1.0:
            roi_w_sam = int(roi_w_orig * roi_scale)
            roi_h_sam = int(roi_h_orig * roi_scale)
            roi_sam = cv2.resize(roi_image, (roi_w_sam, roi_h_sam))
        else:
            roi_sam = roi_image
        
        # Use SAM2 on the small ROI
        if predictor is None or mask_generator is None:
            return jsonify({
                'success': False,
                'error': 'SAM2 not initialized',
                'timings': timings
            })
        
        # Run SAM2 automatic mask generation on ROI with FAST settings
        with torch.inference_mode():
            try:
                # Create a balanced mask generator for ROI
                from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
                fast_generator = SAM2AutomaticMaskGenerator(
                    model=predictor.model,
                    points_per_side=24,  # More points for better coverage
                    pred_iou_thresh=0.82,  # Slightly lower to get more candidates
                    stability_score_thresh=0.88,  # Slightly lower for more options
                    crop_n_layers=0,  # No multi-scale for speed
                    min_mask_region_area=100,
                )
                auto_masks = fast_generator.generate(roi_sam)
                
                if not auto_masks:
                    # Fallback: use whole ROI as mask
                    roi_mask = np.ones((roi_sam.shape[0], roi_sam.shape[1]), dtype=bool)
                else:
                    # Take the best mask using multiple criteria
                    h_roi, w_roi = roi_sam.shape[:2]
                    best_mask = None
                    best_score = 0
                    
                    for mask_data in auto_masks:
                        mask = mask_data['segmentation']
                        area = np.sum(mask)
                        stability = mask_data.get('stability_score', 0.5)
                        predicted_iou = mask_data.get('predicted_iou', 0.5)
                        
                        # Calculate centrality
                        y_coords, x_coords = np.where(mask)
                        if len(y_coords) > 0:
                            center_y, center_x = np.mean(y_coords), np.mean(x_coords)
                            center_dist = np.sqrt((center_x - w_roi/2)**2 + (center_y - h_roi/2)**2)
                            centrality = 1.0 / (1.0 + center_dist / min(w_roi, h_roi))
                        else:
                            centrality = 0
                        
                        # Area score (prefer 20-80% of ROI)
                        area_ratio = area / (w_roi * h_roi)
                        if area_ratio < 0.2 or area_ratio > 0.8:
                            area_score = 0.5  # Penalize too small or too large
                        else:
                            area_score = 1.0
                        
                        # Combined score: quality + centrality + size
                        score = stability * predicted_iou * centrality * area_score
                        
                        if score > best_score:
                            best_score = score
                            best_mask = mask
                    
                    roi_mask = best_mask if best_mask is not None else auto_masks[0]['segmentation']
                    
            except Exception as e:
                print(f"SAM2 on ROI failed: {e}, using fallback")
                # Fallback: use whole ROI
                roi_mask = np.ones((roi_sam.shape[0], roi_sam.shape[1]), dtype=bool)
        
        # Upscale mask back to ROI size
        if roi_scale < 1.0:
            roi_mask_full = cv2.resize(roi_mask.astype(np.uint8), (roi_w_orig, roi_h_orig))
            roi_mask_full = (roi_mask_full > 0.5).astype(bool)
        else:
            roi_mask_full = roi_mask
        
        timings['sam2_roi'] = (time.time() - t) * 1000
        
        # Step 3: Place ROI mask back into full image
        t = time.time()
        full_mask = np.zeros((orig_h, orig_w), dtype=np.uint8)
        full_mask[roi_y_orig:roi_y_orig+roi_h_orig, roi_x_orig:roi_x_orig+roi_w_orig] = (roi_mask_full * 255).astype(np.uint8)
        
        # Clean up
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        full_mask = cv2.morphologyEx(full_mask, cv2.MORPH_CLOSE, kernel)
        
        timings['merge'] = (time.time() - t) * 1000
        
        final_mask = full_mask
        timings['upscale'] = (time.time() - t) * 1000
        
        # Step 6: Extract contour
        t = time.time()
        contours, _ = cv2.findContours(final_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return jsonify({
                'success': False,
                'error': 'No contour found',
                'timings': timings
            })
        
        largest = max(contours, key=cv2.contourArea)
        epsilon = 0.002 * cv2.arcLength(largest, True)
        approx = cv2.approxPolyDP(largest, epsilon, True)
        contour_points = approx.reshape(-1, 2).tolist()
        
        # Limit to 50 points
        if len(contour_points) > 50:
            step = len(contour_points) // 50
            contour_points = contour_points[::step]
        
        timings['contour'] = (time.time() - t) * 1000
        timings['total'] = (time.time() - start_total) * 1000
        
        # Calculate stats
        area = np.sum(final_mask > 0)
        area_percent = (area / (orig_w * orig_h)) * 100
        
        # Encode mask
        mask_b64 = base64.b64encode(np.ascontiguousarray(final_mask)).decode('utf-8')
        
        print(f"✓ Hybrid segmentation: {timings['total']:.1f}ms (target: <2000ms)")
        print(f"  Breakdown: ROI detection={timings.get('roi_detection', 0):.1f}ms, "
              f"SAM2 on ROI={timings.get('sam2_roi', 0):.1f}ms, "
              f"merge={timings.get('merge', 0):.1f}ms, "
              f"contour={timings.get('contour', 0):.1f}ms")
        
        return jsonify({
            'success': True,
            'mask': mask_b64,
            'mask_shape': list(final_mask.shape),
            'contour': contour_points,
            'score': 0.95,
            'area_percent': float(area_percent),
            'timings': timings,
            'method': 'fast_hybrid_saliency'
        })
        
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/segment_humans', methods=['POST'])
def segment_humans():
    """
    Detect and segment all humans in image using YOLO + SAM2
    Input: Base64 encoded image
    Output: Array of human masks with bounding boxes and scores
    """
    try:
        if human_detector is None:
            return jsonify({'error': 'Human detector not initialized'}), 500
        
        data = request.json
        if 'image' not in data:
            return jsonify({'error': 'No image provided'}), 400
        
        # Decode image
        image_data = base64.b64decode(data['image'])
        image = Image.open(io.BytesIO(image_data))
        image_np = np.array(image.convert('RGB'))
        
        # Get parameters
        confidence_threshold = float(data.get('confidence', 0.5))
        return_largest_only = data.get('largest_only', False)
        
        print(f"Detecting humans (confidence >= {confidence_threshold})...")
        
        # Detect humans
        humans = human_detector.detect_humans(
            image_np,
            confidence_threshold=confidence_threshold,
            return_largest_only=return_largest_only
        )
        
        if not humans:
            return jsonify({
                'success': True,
                'humans': [],
                'count': 0,
                'message': 'No humans detected'
            })
        
        # Prepare response
        results = []
        for human in humans:
            # Encode as binary 0/255 regardless of bool vs uint8 input
            mask_uint8 = np.where(human['mask'].astype(bool), np.uint8(255), np.uint8(0))
            
            # Extract contour from mask
            contour_points = extract_contour(mask_uint8)
            
            # Encode mask
            mask_b64 = base64.b64encode(np.ascontiguousarray(mask_uint8)).decode('utf-8')
            
            results.append({
                'mask': mask_b64,
                'mask_shape': list(mask_uint8.shape),
                'contour': contour_points,
                'bbox': [float(x) for x in human['bbox']],  # Convert to Python float
                'confidence': float(human['confidence']),
                'sam2_score': float(human['sam2_score']),
                'area_percent': float(human['area_percent']),
                'index': int(human['index'])
            })
        
        print(f"✓ Detected {len(results)} human(s)")
        
        return jsonify({
            'success': True,
            'humans': results,
            'count': len(results)
        })
        
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/segment_largest_human', methods=['POST'])
def segment_largest_human():
    """
    Detect and segment only the largest/most prominent human
    Input: Base64 encoded image
    Output: Single human mask with contour
    """
    try:
        if human_detector is None:
            return jsonify({'error': 'Human detector not initialized'}), 500
        
        data = request.json
        if 'image' not in data:
            return jsonify({'error': 'No image provided'}), 400
        
        # Decode image
        image_data = base64.b64decode(data['image'])
        image = Image.open(io.BytesIO(image_data))
        image_np = np.array(image.convert('RGB'))
        
        # Get parameters
        confidence_threshold = float(data.get('confidence', 0.5))
        
        print(f"Detecting largest human (confidence >= {confidence_threshold})...")
        
        # Detect largest human
        human = human_detector.detect_largest_human(
            image_np,
            confidence_threshold=confidence_threshold
        )
        
        if human is None:
            return jsonify({
                'success': True,
                'human': None,
                'message': 'No human detected'
            })
        
        # Convert mask to uint8 first
        mask_uint8 = np.where(human['mask'].astype(bool), np.uint8(255), np.uint8(0))
        
        # Extract contour from mask
        contour_points = extract_contour(mask_uint8)
        
        # Encode mask
        mask_b64 = base64.b64encode(np.ascontiguousarray(mask_uint8)).decode('utf-8')
        
        print(f"✓ Detected human: confidence={human['confidence']:.2f}, area={human['area_percent']:.1f}%")
        
        return jsonify({
            'success': True,
            'mask': mask_b64,
            'mask_shape': list(mask_uint8.shape),
            'contour': contour_points,
            'bbox': [float(x) for x in human['bbox']],  # Convert to Python float
            'confidence': float(human['confidence']),
            'sam2_score': float(human['sam2_score']),
            'area_percent': float(human['area_percent'])
        })
        
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500

if __name__ == '__main__':
    print("=" * 60)
    print("SAM2 Segmentation Service for DrawingStudio")
    print("Optimized for Apple M3 Pro")
    print("=" * 60)
    
    # Initialize SAM2
    if initialize_sam2():
        print("\n✓ Service ready!")
        print("Listening on authenticated http://127.0.0.1:5001")
        print("\nEndpoints:")
        print("  GET  /health                  - Health check")
        print("  POST /segment                 - Automatic segmentation (slow, high quality)")
        print("  POST /segment_fast            - ⚡ FAST hybrid segmentation (<150ms)")
        print("  POST /segment_point           - Point-based segmentation")
        print("  POST /segment_box             - Box-based segmentation")
        print("  POST /segment_humans          - 🧑 Detect ALL humans (YOLO + SAM2)")
        print("  POST /segment_largest_human   - 🧑 Detect LARGEST human (YOLO + SAM2)")
        print("\nPress Ctrl+C to stop")
        print("=" * 60)
        
        app.run(host='127.0.0.1', port=5001, debug=False)
    else:
        print("\n✗ Failed to initialize SAM2")
        print("Please check the error messages above")
        sys.exit(1)
