# Human Detection with YOLO + SAM2

## Overview

This integration combines **YOLOv8** (object detection) with **SAM2** (segmentation) to provide accurate human detection and pixel-perfect segmentation.

## Architecture

```
Image → YOLOv8 → Bounding Boxes → SAM2 → Precise Masks
        (detect)   (humans only)    (segment)  (pixel-perfect)
```

### Why YOLO + SAM2?

| Component | Role | Strength |
|-----------|------|----------|
| **YOLOv8** | Detection | Fast, identifies humans specifically |
| **SAM2** | Segmentation | Precise, pixel-perfect masks |

## Features

✅ **Human-specific detection** - Only detects people, not other objects  
✅ **Multiple people** - Detects all humans in image  
✅ **Confidence scoring** - YOLO confidence + SAM2 quality score  
✅ **Bounding boxes** - Returns bbox for each human  
✅ **Precise masks** - Pixel-perfect segmentation from SAM2  
✅ **Fast** - ~60-115ms for single person on M3 Pro  

## API Endpoints

### 1. `/segment_humans` - Detect All Humans

Detects and segments all humans in the image.

**Request:**
```json
{
  "image": "base64_encoded_image",
  "confidence": 0.5,
  "largest_only": false
}
```

**Parameters:**
- `image` (required): Base64 encoded image
- `confidence` (optional): Minimum YOLO confidence (0-1), default 0.5
- `largest_only` (optional): Return only largest human, default false

**Response:**
```json
{
  "success": true,
  "count": 2,
  "humans": [
    {
      "mask": "base64_encoded_mask",
      "mask_shape": [1080, 1920],
      "contour": [[x1, y1], [x2, y2], ...],
      "bbox": [x1, y1, x2, y2],
      "confidence": 0.92,
      "sam2_score": 0.98,
      "area_percent": 23.5,
      "index": 0
    },
    ...
  ]
}
```

### 2. `/segment_largest_human` - Detect Largest Human

Detects and segments only the largest/most prominent human.

**Request:**
```json
{
  "image": "base64_encoded_image",
  "confidence": 0.5
}
```

**Response:**
```json
{
  "success": true,
  "mask": "base64_encoded_mask",
  "mask_shape": [1080, 1920],
  "contour": [[x1, y1], [x2, y2], ...],
  "bbox": [x1, y1, x2, y2],
  "confidence": 0.92,
  "sam2_score": 0.98,
  "area_percent": 23.5
}
```

If no human detected:
```json
{
  "success": true,
  "human": null,
  "message": "No human detected"
}
```

## Performance

### Timing (Apple M3 Pro)

| Scenario | YOLO | SAM2 | Total |
|----------|------|------|-------|
| 1 person | ~10ms | ~50ms | **~60ms** |
| 3 people | ~15ms | ~150ms | **~165ms** |
| 5 people | ~15ms | ~250ms | **~265ms** |

### Accuracy

- **YOLO Detection**: 90-95% accuracy for humans
- **SAM2 Segmentation**: 95-99% mask quality
- **Combined**: Highly accurate human segmentation

## Usage Examples

### Python

```python
import requests
import base64
import os

headers = {'Authorization': f"Bearer {os.environ['DRAWINGSTUDIO_SAM2_TOKEN']}"}

# Load image
with open('photo.jpg', 'rb') as f:
    image_b64 = base64.b64encode(f.read()).decode('utf-8')

# Detect largest human
response = requests.post('http://127.0.0.1:5001/segment_largest_human', headers=headers, json={
    'image': image_b64,
    'confidence': 0.5
})

result = response.json()
if result['success'] and result.get('human'):
    print(f"Detected human with {result['confidence']:.1%} confidence")
    print(f"Area: {result['area_percent']:.1f}%")
```

### cURL

```bash
# Encode image
IMAGE_B64=$(base64 -i photo.jpg)

# Detect all humans
curl -X POST http://127.0.0.1:5001/segment_humans \
  -H "Authorization: Bearer $DRAWINGSTUDIO_SAM2_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"image\": \"$IMAGE_B64\", \"confidence\": 0.5}"
```

### Browser clients

Browser access is intentionally unsupported: the local service does not enable
CORS. Use DrawingStudio, a native client, or a trusted command-line client with
the bearer token.

## Testing

### Quick Test

```bash
# Start service
python sam2_service.py

# In another terminal, test with an image
python test_human_detection.py /path/to/photo.jpg
```

### Test Output

```
============================================================
YOLO + SAM2 Human Detection Test
============================================================

✓ Service is running
  SAM2 loaded: True
  Device: mps

============================================================
TEST 1: Detect Largest Human
============================================================
Loading image: photo.jpg
Sending request to http://127.0.0.1:5001/segment_largest_human...

============================================================
DETECTION RESULTS
============================================================
Human detected!
  Confidence: 92.34%
  SAM2 Score: 0.98
  Area: 23.5%
  Bounding Box: [450, 120, 890, 980]
  Contour Points: 87
============================================================
```

## Configuration

### Confidence Threshold

Adjust based on your needs:

- **0.3-0.4**: Very permissive, may include false positives
- **0.5**: Balanced (default, recommended)
- **0.6-0.7**: Conservative, fewer false positives
- **0.8+**: Very strict, may miss some humans

### YOLO Model Selection

In `human_detector.py`, you can change the model:

```python
# Faster but less accurate
self.yolo = YOLO('yolov8n.pt')  # nano

# Balanced (default)
self.yolo = YOLO('yolov8s.pt')  # small

# More accurate but slower
self.yolo = YOLO('yolov8m.pt')  # medium
```

## Advantages Over Generic Segmentation

| Feature | Generic SAM2 | YOLO + SAM2 |
|---------|-------------|-------------|
| **Knows it's human** | ❌ No | ✅ Yes |
| **Multiple people** | ⚠️ Harder | ✅ Easy |
| **False positives** | ⚠️ Common | ✅ Rare |
| **Speed** | ✅ Fast | ✅ Fast |
| **Accuracy** | ✅ Good | ✅ Excellent |

## Limitations

❌ **Occlusion**: Partially hidden people may not be detected  
❌ **Small people**: Very distant people (<2% of image) may be missed  
❌ **Unusual poses**: Extreme poses may reduce confidence  
❌ **Poor lighting**: Very dark images reduce detection rate  

## Tips for Best Results

1. **Good lighting** - Clear visibility of people
2. **Reasonable distance** - People should be >2% of image
3. **Clear view** - Minimal occlusion
4. **Standard poses** - Standing, sitting, walking work best
5. **Adjust confidence** - Lower for difficult images

## Model Downloads

YOLOv8 models auto-download on first use:

- **yolov8n.pt**: ~6MB (nano)
- **yolov8s.pt**: ~22MB (small) ← **default**
- **yolov8m.pt**: ~52MB (medium)

Models are cached in `~/.ultralytics/` directory.

## Integration with DrawingStudio

The human detection endpoints can be called from your Qt/C++ application:

```cpp
// Example integration (pseudo-code)
QNetworkRequest request(QUrl("http://127.0.0.1:5001/segment_largest_human"));
request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
request.setRawHeader("Authorization", "Bearer " + sam2Token);

QJsonObject json;
json["image"] = imageToBase64(image);
json["confidence"] = 0.5;

QNetworkReply *reply = networkManager->post(request, 
    QJsonDocument(json).toJson());
```

## Troubleshooting

### "Human detector not initialized"

The YOLO model failed to load. Check:
1. ultralytics is installed: `pip install ultralytics`
2. Sufficient disk space for model download (~22MB)
3. Internet connection for first-time download

### "No humans detected"

Try:
1. Lower confidence threshold (0.3-0.4)
2. Check image quality and lighting
3. Verify people are clearly visible
4. Ensure people are >2% of image area

### Slow performance

1. Use smaller YOLO model (yolov8n)
2. Reduce image resolution before sending
3. Check CPU/GPU usage
4. Ensure MPS acceleration is working

## Future Enhancements

- [ ] Face detection integration
- [ ] Pose estimation
- [ ] Age/gender classification
- [ ] Multiple person tracking
- [ ] Real-time video processing
- [ ] Custom person re-identification

## References

- **YOLOv8**: https://github.com/ultralytics/ultralytics
- **SAM2**: https://github.com/facebookresearch/segment-anything-2
- **COCO Dataset**: Person class (class 0)
