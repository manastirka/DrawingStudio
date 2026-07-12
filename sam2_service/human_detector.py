"""
Human Detection using YOLO + SAM2
Combines YOLOv8 for human detection with SAM2 for precise segmentation
"""
import numpy as np
import torch
from typing import List, Dict, Optional, Tuple
import time


class HumanDetector:
    """Detects and segments humans using YOLO + SAM2"""
    
    def __init__(self, sam2_predictor, device='mps'):
        """
        Initialize human detector
        
        Args:
            sam2_predictor: SAM2ImagePredictor instance
            device: 'mps', 'cuda', or 'cpu'
        """
        self.sam2 = sam2_predictor
        self.device = device
        self.yolo = None
        self._load_yolo()
    
    def _load_yolo(self):
        """Load YOLOv8 model"""
        try:
            import os
            from ultralytics import YOLO
            print("Loading YOLOv8 model...")
            
            # Check for local model first (offline mode support)
            local_model = os.path.expanduser("~/.cache/ultralytics/yolov8s.pt")
            offline_mode = os.getenv("YOLO_OFFLINE", "").lower() == "true"
            
            if os.path.exists(local_model):
                # Use local cached model
                print(f"Using local model: {local_model}")
                self.yolo = YOLO(local_model)
            elif offline_mode:
                # Offline mode but model not found
                raise RuntimeError(
                    f"YOLO model not found at {local_model} and offline mode is enabled.\n"
                    "Run: ./download_all_models.sh to download models for offline use."
                )
            else:
                # Online mode - will auto-download (~22MB)
                print("Downloading YOLOv8s model (~22MB)...")
                self.yolo = YOLO('yolov8s.pt')
            
            # Set device
            if self.device == 'mps' and torch.backends.mps.is_available():
                self.yolo.to('mps')
            elif self.device == 'cuda' and torch.cuda.is_available():
                self.yolo.to('cuda')
            else:
                self.yolo.to('cpu')
            
            print(f"✓ YOLOv8 loaded on {self.device}")
            
        except Exception as e:
            print(f"✗ Error loading YOLO: {e}")
            self.yolo = None
    
    def detect_humans(
        self, 
        image: np.ndarray,
        confidence_threshold: float = 0.5,
        use_box_prompt: bool = True,
        return_largest_only: bool = False
    ) -> List[Dict]:
        """
        Detect and segment all humans in image
        
        Args:
            image: RGB image (H, W, 3)
            confidence_threshold: Minimum YOLO confidence (0-1)
            use_box_prompt: Use YOLO box as SAM2 prompt (faster, more accurate)
            return_largest_only: Return only the largest/most confident human
            
        Returns:
            List of dicts with keys:
                - mask: Binary mask (H, W) bool
                - bbox: [x1, y1, x2, y2]
                - confidence: YOLO confidence score
                - sam2_score: SAM2 quality score
                - area_percent: Mask area as % of image
        """
        if self.yolo is None:
            raise RuntimeError("YOLO not initialized")
        
        start_time = time.time()
        
        # 1. YOLO Detection
        print(f"Running YOLO detection...")
        yolo_start = time.time()
        
        # Class 0 = person in COCO dataset
        results = self.yolo(image, classes=[0], verbose=False)
        
        yolo_time = (time.time() - yolo_start) * 1000
        print(f"YOLO detection: {yolo_time:.1f}ms")
        
        if len(results) == 0 or len(results[0].boxes) == 0:
            print("No humans detected")
            return []
        
        detections = results[0].boxes
        print(f"Found {len(detections)} human(s)")
        
        # 2. Filter by confidence
        valid_detections = []
        for det in detections:
            conf = float(det.conf[0])
            if conf >= confidence_threshold:
                bbox = det.xyxy[0].cpu().numpy()
                valid_detections.append({
                    'bbox': bbox,
                    'confidence': conf
                })
        
        if not valid_detections:
            print(f"No humans above confidence threshold {confidence_threshold}")
            return []
        
        print(f"{len(valid_detections)} human(s) above confidence threshold")
        
        # Sort by confidence (best first)
        valid_detections.sort(key=lambda x: x['confidence'], reverse=True)
        
        # 3. SAM2 Segmentation for each human
        humans = []
        h, w = image.shape[:2]
        
        # Set image once for all predictions
        self.sam2.set_image(image)
        
        for i, det in enumerate(valid_detections):
            print(f"Segmenting human {i+1}/{len(valid_detections)}...")
            sam2_start = time.time()
            
            bbox = det['bbox']
            x1, y1, x2, y2 = bbox
            
            try:
                if use_box_prompt:
                    # Use YOLO box as prompt (recommended)
                    masks, scores, _ = self.sam2.predict(
                        box=np.array([x1, y1, x2, y2]),
                        multimask_output=False
                    )
                else:
                    # Use center point as prompt
                    center_x = (x1 + x2) / 2
                    center_y = (y1 + y2) / 2
                    masks, scores, _ = self.sam2.predict(
                        point_coords=np.array([[center_x, center_y]]),
                        point_labels=np.array([1]),
                        multimask_output=False
                    )
                
                mask = masks[0]
                score = float(scores[0])
                
                sam2_time = (time.time() - sam2_start) * 1000
                
                # Calculate area
                area = np.sum(mask)
                area_percent = (area / (h * w)) * 100
                
                print(f"  Human {i+1}: confidence={det['confidence']:.2f}, "
                      f"sam2_score={score:.2f}, area={area_percent:.1f}%, "
                      f"time={sam2_time:.1f}ms")
                
                humans.append({
                    'mask': mask.astype(bool),
                    'bbox': bbox.tolist(),
                    'confidence': det['confidence'],
                    'sam2_score': score,
                    'area_percent': area_percent,
                    'index': i
                })
                
            except Exception as e:
                print(f"  Error segmenting human {i+1}: {e}")
                continue
        
        total_time = (time.time() - start_time) * 1000
        print(f"Total detection + segmentation: {total_time:.1f}ms for {len(humans)} human(s)")
        
        # Return only largest if requested
        if return_largest_only and humans:
            # Sort by area and return largest
            humans.sort(key=lambda x: x['area_percent'], reverse=True)
            return [humans[0]]
        
        return humans
    
    def detect_largest_human(
        self,
        image: np.ndarray,
        confidence_threshold: float = 0.5
    ) -> Optional[Dict]:
        """
        Detect and segment only the largest/most prominent human
        
        Args:
            image: RGB image (H, W, 3)
            confidence_threshold: Minimum YOLO confidence (0-1)
            
        Returns:
            Dict with mask, bbox, etc. or None if no human found
        """
        humans = self.detect_humans(
            image,
            confidence_threshold=confidence_threshold,
            return_largest_only=True
        )
        
        return humans[0] if humans else None
    
    def get_combined_mask(
        self,
        image: np.ndarray,
        confidence_threshold: float = 0.5
    ) -> Tuple[Optional[np.ndarray], int]:
        """
        Get a single combined mask of all detected humans
        
        Args:
            image: RGB image (H, W, 3)
            confidence_threshold: Minimum YOLO confidence (0-1)
            
        Returns:
            (combined_mask, num_humans) or (None, 0) if no humans found
        """
        humans = self.detect_humans(image, confidence_threshold)
        
        if not humans:
            return None, 0
        
        # Combine all masks with OR operation
        h, w = image.shape[:2]
        combined_mask = np.zeros((h, w), dtype=bool)
        
        for human in humans:
            combined_mask |= human['mask']
        
        return combined_mask, len(humans)
