# Image Object Detection & Selection - User Guide

## Overview
When using the Image tool, you now have special capabilities to detect and select objects within imported images. This feature helps you identify distinct regions, objects, or elements in your images.

## Features

### 1. Object Detection
Automatically detect distinct objects/regions within an image based on color and shape.

### 2. Object Selection
Click on detected objects to select them individually within the image.

### 3. Adjustable Sensitivity
Control the detection threshold to find more or fewer objects.

## How to Use

### Step 1: Import an Image
1. Select the **Image Tool** from the left toolbar
2. Click on the canvas to place an image
3. Choose an image file to import

### Step 2: Enable Object Detection
1. With the image selected, look at the tool settings panel
2. Check the **"Enable Object Detection"** checkbox
3. A message will appear confirming detection is enabled

### Step 3: Adjust Detection Settings

#### Scale (10% - 200%)
- Controls the display size of the image
- Does not affect detection

#### Detection Threshold (1% - 100%)
- **Lower values (1-30%)**: Detect more objects, including small regions
- **Medium values (30-70%)**: Balanced detection
- **Higher values (70-100%)**: Only detect large, distinct objects

**Tip**: Start with 50% and adjust based on your image

### Step 4: Detect Objects
1. Adjust the **Detection Threshold** slider
2. The system automatically scans the image for:
   - Distinct color regions
   - Connected areas
   - Shapes and boundaries

### Step 5: Select Objects
1. Click on any detected object in the image
2. The object will be highlighted
3. Object information is displayed in the console

## Detection Algorithm

### How It Works
The system uses a **color-based region detection** algorithm:

1. **Scans the image** for distinct color regions
2. **Groups connected pixels** with similar colors
3. **Identifies boundaries** of each region
4. **Filters by size** based on the threshold
5. **Creates bounding boxes** around each object

### What Gets Detected
- ✅ Distinct color regions
- ✅ Separate objects with different colors
- ✅ Connected shapes
- ✅ Large enough regions (minimum 20x20 pixels)

### What Doesn't Get Detected
- ❌ Very small details (< 20x20 pixels)
- ❌ Objects with the same color as background
- ❌ Overlapping objects with similar colors
- ❌ Transparent or very faint regions

## Visual Feedback

### Detected Objects
Each detected object is assigned:
- **Bounding Box**: Rectangle around the object
- **Label**: "Object 1", "Object 2", etc.
- **Highlight Color**: Red, Green, Blue, Yellow, Cyan, or Magenta
- **Confidence**: Detection confidence score (typically 85%)

### Selection Highlight
When you click on an object:
- The object is highlighted
- Object information appears in the console
- You can see the object's label and bounds

## Use Cases

### 1. Photo Analysis
- Identify people, objects, or regions in photos
- Select specific elements for editing
- Analyze composition

### 2. Diagram Parsing
- Detect shapes in technical diagrams
- Identify components in schematics
- Select individual elements

### 3. Art & Design
- Find distinct color regions in artwork
- Select specific design elements
- Analyze color distribution

### 4. Document Processing
- Detect text blocks or images in scanned documents
- Identify logos or graphics
- Select specific sections

## Tips for Best Results

### Image Preparation
1. **High Contrast**: Images with clear color differences work best
2. **Clean Backgrounds**: Solid or simple backgrounds improve detection
3. **Distinct Objects**: Objects with different colors are easier to detect
4. **Good Resolution**: Higher resolution images provide better results

### Threshold Adjustment
- **Too many objects detected?** → Increase threshold
- **Missing objects?** → Decrease threshold
- **Only want large objects?** → Set threshold to 70-100%
- **Want to find everything?** → Set threshold to 10-30%

### Performance
- Detection runs when you adjust the threshold
- Large images may take a few seconds
- The system samples every 10 pixels for speed
- Maximum 10,000 pixels per region to prevent slowdown

## Technical Details

### Detection Parameters
- **Minimum Object Size**: 20x20 pixels
- **Sampling Rate**: Every 10 pixels
- **Color Similarity Threshold**: RGB difference < 100
- **Max Pixels Per Region**: 10,000
- **Max Objects**: Unlimited (but practical limit ~50-100)

### Coordinate Systems
- **Image Coordinates**: Pixel positions in the original image
- **World Coordinates**: Position on the canvas
- **Bounding Boxes**: Stored in image coordinates
- **Selection**: Converts click from world to image coordinates

### Object Data Structure
Each detected object contains:
```cpp
struct DetectedObject {
    QRectF boundingBox;     // Position and size in image
    QString label;          // "Object 1", "Object 2", etc.
    float confidence;       // 0.0 to 1.0
    QColor highlightColor;  // Visualization color
};
```

## Keyboard Shortcuts
- **Image Tool**: I
- **Select Tool**: S (to select the image first)

## Limitations

### Current Implementation
- **Basic Algorithm**: Uses color-based region detection, not AI
- **No Classification**: Doesn't identify what objects are (e.g., "cat", "car")
- **2D Only**: Doesn't understand depth or 3D structure
- **Color-Based**: Relies on color differences

### Future Enhancements
Potential improvements:
- AI-based object detection (YOLO, R-CNN)
- Object classification (identify what objects are)
- Semantic segmentation (pixel-level classification)
- Edge detection algorithms
- Machine learning integration
- Custom object training

## Troubleshooting

### No Objects Detected
**Problem**: Detection finds nothing
**Solutions**:
- Lower the detection threshold
- Check if image has distinct color regions
- Ensure image is not all one color
- Try a different image

### Too Many Objects
**Problem**: Hundreds of tiny regions detected
**Solutions**:
- Increase the detection threshold
- Use images with clearer object boundaries
- Filter out small regions manually

### Wrong Objects Selected
**Problem**: Clicking selects wrong object
**Solutions**:
- Zoom in for more precise clicking
- Adjust threshold to get better boundaries
- Try clicking in the center of the object

### Slow Performance
**Problem**: Detection takes too long
**Solutions**:
- Use smaller images
- Increase threshold (fewer objects to process)
- Close other applications
- The system is optimized but very large images (> 4000x4000) may be slow

## Example Workflow

### Detecting Objects in a Photo
1. Import a photo using Image Tool
2. Select the image
3. Enable "Object Detection"
4. Set threshold to 40%
5. Click on different people/objects to select them
6. Adjust threshold if needed

### Analyzing a Diagram
1. Import technical diagram
2. Enable object detection
3. Set threshold to 60% (for main components)
4. Click on each component to identify it
5. Use selection for further processing

## Integration with Other Tools

### With Select Tool
- Use Select tool to move the entire image
- Switch to Image tool for object detection
- Objects move with the image

### With AI Generation
- Detect objects in AI-generated images
- Select specific generated elements
- Analyze AI output quality

### With Layers
- Each image is on a layer
- Object detection works per-image
- Multiple images can have detection enabled

## Console Output

When detecting objects, you'll see:
```
Detected 5 objects in image
```

When selecting an object:
```
Selected object 2: Object 2
```

This helps you track what's happening during detection and selection.

## Summary

The Image Object Detection feature provides:
- ✅ Automatic object detection in images
- ✅ Click-to-select functionality
- ✅ Adjustable sensitivity
- ✅ Visual feedback with bounding boxes
- ✅ Simple, intuitive interface
- ✅ Fast, real-time processing

Perfect for analyzing images, selecting specific elements, and understanding image composition!
