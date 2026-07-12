"""
Fast Saliency Detection
Uses OpenCV spectral residual for speed (<50ms)
"""
import cv2
import numpy as np
from typing import Tuple, Optional

class FastSaliency:
    """Ultra-fast saliency detection using OpenCV spectral residual"""
    
    def __init__(self):
        try:
            self.saliency = cv2.saliency.StaticSaliencySpectralResidual_create()
            self.use_spectral = True
        except:
            print("Warning: Spectral residual not available, using edge-based fallback")
            self.saliency = None
            self.use_spectral = False
    
    def detect(self, image: np.ndarray, threshold: str = "otsu") -> Tuple[np.ndarray, dict]:
        """
        Detect salient regions in image
        
        Args:
            image: RGB image (H, W, 3)
            threshold: "otsu" or float 0-1
            
        Returns:
            mask: Binary mask (H, W) uint8
            metadata: Dict with timing and quality info
        """
        import time
        start = time.time()
        
        # Convert to BGR for OpenCV
        if image.shape[2] == 3:
            image_bgr = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
        else:
            image_bgr = image
        
        # Compute saliency map
        if self.use_spectral:
            success, saliency_map = self.saliency.computeSaliency(image_bgr)
            if not success:
                # Fallback to edge-based
                saliency_map = self._edge_based_saliency(image_bgr)
        else:
            # Use edge-based fallback
            saliency_map = self._edge_based_saliency(image_bgr)
        
        # Normalize to 0-255
        saliency_map = (saliency_map * 255).astype(np.uint8)
        
        # Threshold
        if threshold == "otsu":
            _, binary_mask = cv2.threshold(
                saliency_map, 0, 255, 
                cv2.THRESH_BINARY + cv2.THRESH_OTSU
            )
        else:
            thresh_val = int(float(threshold) * 255)
            _, binary_mask = cv2.threshold(
                saliency_map, thresh_val, 255, 
                cv2.THRESH_BINARY
            )
        
        # Morphological cleaning
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        binary_mask = cv2.morphologyEx(binary_mask, cv2.MORPH_CLOSE, kernel)
        binary_mask = cv2.morphologyEx(binary_mask, cv2.MORPH_OPEN, kernel)
        
        elapsed = (time.time() - start) * 1000
        
        metadata = {
            'time_ms': elapsed,
            'method': 'spectral_residual',
            'threshold': threshold
        }
        
        return binary_mask, metadata
    
    def _edge_based_saliency(self, image_bgr: np.ndarray) -> np.ndarray:
        """Fast edge-based saliency fallback"""
        gray = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2GRAY)
        edges = cv2.Canny(gray, 50, 150)
        sal_map = cv2.GaussianBlur(edges.astype(float), (21, 21), 0)
        if sal_map.max() > 0:
            sal_map = sal_map / sal_map.max()
        return sal_map


def get_largest_component(mask: np.ndarray, min_area_percent: float = 1.0) -> Optional[np.ndarray]:
    """
    Extract largest connected component from mask
    
    Args:
        mask: Binary mask uint8
        min_area_percent: Minimum area as % of image
        
    Returns:
        Cleaned mask with only largest component, or None if too small
    """
    # Find connected components
    num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(
        mask, connectivity=8
    )
    
    if num_labels <= 1:  # Only background
        return None
    
    # Get largest component (skip background at index 0)
    areas = stats[1:, cv2.CC_STAT_AREA]
    largest_idx = np.argmax(areas) + 1
    largest_area = areas[largest_idx - 1]
    
    # Check minimum size
    total_pixels = mask.shape[0] * mask.shape[1]
    area_percent = (largest_area / total_pixels) * 100
    
    if area_percent < min_area_percent:
        return None
    
    # Create mask with only largest component
    result = np.zeros_like(mask)
    result[labels == largest_idx] = 255
    
    return result


def extract_roi_bbox(mask: np.ndarray, pad_px: int = 32, min_size: int = 64) -> Optional[Tuple[int, int, int, int]]:
    """
    Extract bounding box from mask with padding
    
    Args:
        mask: Binary mask uint8
        pad_px: Padding in pixels
        min_size: Minimum bbox size
        
    Returns:
        (x, y, w, h) or None if invalid
    """
    # Find contours
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    if not contours:
        return None
    
    # Get bounding box
    x, y, w, h = cv2.boundingRect(np.vstack(contours))
    
    # Check minimum size
    if w < min_size or h < min_size:
        return None
    
    # Add padding
    img_h, img_w = mask.shape
    x = max(0, x - pad_px)
    y = max(0, y - pad_px)
    w = min(img_w - x, w + 2 * pad_px)
    h = min(img_h - y, h + 2 * pad_px)
    
    return (x, y, w, h)
