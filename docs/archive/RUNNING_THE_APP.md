# Running DrawingStudio with SAM2 Subject Detection

## Quick Start

### 1. Start the SAM2 Service (Required for Subject Detection)

In a **separate terminal**, run:
```bash
cd /Users/Lukovic/Apps/DrawingStudio
./start_sam2_service.sh
```

Wait until you see:
```
✓ SAM2 loaded successfully on mps
✓ Service ready!
Listening on http://localhost:5001
```

### 2. Run DrawingStudio

In another terminal:
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
```

## Features

### Subject Detection with SAM2
1. **Import an image** (File → Open or drag & drop)
2. **Select the Image Tool** from the toolbar
3. **Click on the image** to select it
4. The app will **automatically detect the subject** using SAM2
5. You'll see a **precise outline** around the detected subject
6. Use **Edge Selection Tool** or **Focus Selection Tool** for refinement

### Tools Available
- **Line Tool**: Draw straight lines
- **Curve Tool**: Draw smooth curves
- **Bezier Tool**: Draw bezier curves with control points
- **Spline Tool**: Draw smooth splines
- **Image Tool**: Import and manipulate images with AI subject detection
- **Edge Selection Tool**: Select objects by clicking on edges
- **Focus Selection Tool**: Select objects by clicking on the subject area

### Layer Management
- Create, delete, and reorder layers
- Show/hide layers
- Lock/unlock layers
- Adjust layer opacity

## Troubleshooting

### SAM2 Service Not Starting
If the service fails to start, check:
1. **Virtual environment**: `cd sam2_service && source venv/bin/activate`
2. **Dependencies**: `pip install -r requirements.txt`
3. **SAM2 installation**: `cd segment-anything-2 && pip install -e .`

### Subject Detection Not Working
1. **Check if SAM2 service is running**:
   ```bash
   curl http://localhost:5001/health
   ```
   Should return: `{"device":"mps","sam2_loaded":true,"status":"ok"}`

2. **Check the app console** for error messages
3. **Restart the SAM2 service** if needed

### Port Already in Use
If port 5001 is already in use:
```bash
lsof -i :5001
kill -9 <PID>
```

## Performance

- **SAM2 with Metal (MPS)**: ~0.5-2 seconds per image on Apple M3 Pro
- **Accuracy**: Professional-grade subject detection (Adobe Lightroom quality)
- **Memory**: ~2-4GB for SAM2 service

## Building from Source

If you need to rebuild the app:
```bash
cd /Users/Lukovic/Apps/DrawingStudio
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## System Requirements

- **macOS**: 10.15 or later (tested on macOS with Apple Silicon)
- **Qt6**: Core, Widgets, OpenGLWidgets, Network
- **Python 3.11+**: For SAM2 service
- **Metal/MPS**: For GPU acceleration (Apple Silicon)
- **Memory**: 8GB+ recommended (4GB for app + 4GB for SAM2)

## Notes

- The SAM2 service must be running **before** you use subject detection features
- The service uses **Metal Performance Shaders (MPS)** for GPU acceleration on Apple Silicon
- Subject detection happens **automatically** when you import an image
- You can manually trigger detection by selecting the image and using the detection tools
