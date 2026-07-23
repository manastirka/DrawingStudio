# Drawing Studio 🎨

A professional drawing and design application built with Qt6 and C++20. Create precise technical drawings, artistic illustrations, and complex diagrams with an intuitive interface and powerful tools.

## 🚀 Features

### 🎨 **Comprehensive Drawing Tools**
- **Selection Tool (S)**: Select, move, and modify drawing objects
- **Line Tool (L)**: Create precise straight lines with angle constraints
- **Bezier Curve Tool (B)**: Create smooth curves with full control point manipulation
- **Spline Tool (P)**: Create flowing curves through multiple points with tension control
- **Rectangle Tool (R)**: Draw rectangles with optional rounded corners
- **Ellipse Tool (E)**: Create circles and ellipses with precision
- **Curve Tool (C)**: Advanced curve creation with multiple interpolation types
- **Measure Tool (M)**: Add dimensions and measurements to your drawings
- **Eraser Tool (X)**: Remove unwanted elements

### 📐 **Precision Features**
- **Snap to Grid**: Automatic alignment to customizable grid
- **Magnetic Connection**: Automatically connect line endpoints
- **Multiple Grid Sizes**: Fine (1mm), Medium (2mm), Coarse (10mm)
- **Multiple Units**: Millimeters, Centimeters, Inches
- **Zoom Controls**: In, Out, Fit to View, Actual Size
- **Coordinate Display**: Real-time cursor position tracking

### 🎯 **Advanced Object Properties**

#### **Universal Properties**
- Color, Line Width, Visibility
- Layer assignment and management
- Transform controls (position, rotation, scale)

#### **Shape-Specific Properties**
- **Lines**: Length, angle adjustment, endpoint snapping
- **Rectangles**: Width/height, corner radius, aspect ratio lock, fill options
- **Ellipses**: Radius control, circular conversion, axes display, subdivision quality
- **Bezier Curves**: Control point manipulation, tangent modes, subdivision quality
- **Splines**: Smoothness, tension, interpolation type, point visibility
- **Arcs**: Radius, start/end angles, direction control

### 🖼️ **Blueprint Support**
- **Load Background Images**: Import PNG, JPG, PDF as reference blueprints
- **Opacity Control**: Adjust blueprint transparency for tracing
- **Scale Adjustment**: Resize blueprints to match your project scale
- **Auto-trace**: Generate outlines from blueprint images

### 📋 **Project Management**
- **Layer System**: Organize drawing elements across multiple layers
- **Object Tree**: Hierarchical view of all drawing objects
- **Save/Load Projects**: Custom .drawing file format
- **Export Options**: Multiple format support for sharing

### 🔧 **Professional Interface**
- **Dockable Panels**: Objects, Properties, Layers
- **Customizable Workspace**: Arrange panels to suit your workflow
- **Status Bar**: Real-time coordinates, zoom level, and tool status
- **Keyboard Shortcuts**: Speed up your workflow with hotkeys
- **Dark Theme**: Professional appearance optimized for long work sessions

## 🛠 **Technical Architecture**

### **Core Classes**:
- **DrawingCanvas**: OpenGL-accelerated drawing surface with hardware rendering
- **DrawingProject**: Project management with layer support and serialization
- **PropertyPanel**: Dynamic property editor for all object types
- **MainWindow**: Modern Qt application framework with docking system
- **DrawingPrimitive**: Polymorphic base class for all drawable objects

### **Drawing System**:
```cpp
enum class PrimitiveType {
    Line, Curve, BezierCurve, Spline,
    Arc, Circle, Rectangle, Ellipse,
    Polygon, Text, Dimension
};

// Project structure
class DrawingProject {
    std::vector<std::unique_ptr<DrawingPrimitive>> primitives;
    std::vector<std::pair<QString, QColor>> layers;
    DrawingUnit units = DrawingUnit::Millimeters;
    GridSize gridSize = GridSize::Medium;
};
```

### **Key Design Patterns**:
- **Command Pattern**: Full undo/redo support for all operations
- **Observer Pattern**: Automatic UI updates when objects change
- **Factory Pattern**: Extensible primitive creation system
- **Strategy Pattern**: Multiple rendering and export strategies

## 📁 **Project Structure**
```
DrawingStudio/
├── include/           # Header files
│   ├── DrawingCanvas.h
│   ├── DrawingProject.h
│   ├── DrawingPrimitive.h
│   ├── PropertyPanel.h
│   └── MainWindow.h
├── src/               # Source files
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── DrawingCanvas.cpp
│   ├── DrawingProject.cpp
│   └── PropertyPanel.cpp
├── build/             # Build artifacts (ignored)
├── CMakeLists.txt     # CMake configuration
├── README.md          # This file
└── .gitignore         # Git ignore rules
```

## 🔧 **Build Instructions**

### Prerequisites:
- **Qt6** (6.2 or later) with OpenGL support
- **CMake** 3.20 or later
- **C++20** compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- **OpenGL** 3.3 or later support

### Build:
```bash
# Clone the repository
git clone <repository-url>
cd DrawingStudio

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)  # Linux/macOS
# or
make             # Single-threaded

# Run
./DrawingStudio
```

### Authenticated automation API

The localhost automation server is disabled by default. Enable it for a trusted
session with a strong bearer token:

```bash
DRAWINGSTUDIO_AUTOMATION_TOKEN='<at-least-16-random-bytes>' \
  ./DrawingStudio --enable-automation

curl -H "Authorization: Bearer $DRAWINGSTUDIO_AUTOMATION_TOKEN" \
  http://127.0.0.1:19100/api/status
```

`GET /api/commands` returns catalog version 2. Each action declares
`requiredParams`, `optionalParams`, and whether it needs filesystem access;
`params` remains the backward-compatible union of both parameter lists. Unknown
actions and commands missing required parameters are rejected before dispatch.

Opening, importing, saving, and exporting through automation are blocked unless
the app is also launched with `--automation-allow-filesystem`. The server binds
only to `127.0.0.1`, does not enable browser CORS, and enforces bounded requests.
The environment variable is preferred over `--automation-token`, because command-line
arguments can be visible to other local processes.

### Local SAM2 service security

DrawingStudio launches the optional SAM2 helper on `127.0.0.1:5001` with a random
per-session bearer token. Every health, progress, and segmentation request is
authenticated, and responses from older unauthenticated services are rejected.
The helper does not enable browser CORS and rejects request bodies larger than
64 MiB.

SAM mask-cache entries are written atomically and validated before use. Corrupt,
outdated, hash-mismatched, oversized, or dimensionally invalid entries are
discarded so they cannot masquerade as successful detection results.

For an externally managed helper, give both processes the same strong token:

```bash
export DRAWINGSTUDIO_SAM2_TOKEN='<at-least-16-random-bytes>'
sam2_service/venv/bin/python3 sam2_service/sam2_service.py
./build/DrawingStudio
```

### AI provider connection tests

**Test Connection** uses the values currently shown in the AI settings dialog
without saving those draft API keys or endpoints. Only **Save** persists the
form. Remote Stable Diffusion defaults to `http://127.0.0.1:8000`; probes accept
only HTTP(S) URLs, refuse redirects, and time out after 15 seconds.

AI generation and edit requests time out after three minutes, result downloads
after one minute, and network responses are capped at 128 MiB. Requests carrying
provider credentials and Remote SD prompts do not follow redirects; downloaded
result images may follow only redirects that do not downgrade HTTPS.
The legacy Remote SD generation helper also stays on its Qt owner thread, uses
a two-minute request deadline, caps generation responses at 96 MiB, and strictly
validates returned base64 images before decoding them.

### Project persistence and recovery

Project saves and two-minute recovery snapshots use atomic replacement, so a
failed or interrupted write does not truncate the previous file. Incoming
projects are fully validated and parsed before the current document is replaced.
After restoring a crash snapshot, DrawingStudio keeps that snapshot until the
project is explicitly saved or the application closes cleanly.
Project persistence also enforces a 256 MiB file limit, 4,096-layer limit, and
100,000-primitive limit on both save and load to avoid unbounded JSON memory use.
Vector geometry is limited to 10,000 points per primitive and 100,000 points per
project; coordinates must be finite and within the supported canvas range.
Scalar geometry for lines, shapes, dimensions, text, and images follows the
same finite ±1 billion canvas range; radii cannot be negative and serialized
image dimensions must be positive.
Project metadata is validated before replacing the open document: layer
opacity stays within 0–1, layer names are limited to 4,096 characters, layer
IDs must be valid UUIDs, and canvas colors and boolean settings use strict
types.
Primitive, layer, group, and object-reference UUIDs use strict syntax, and
projects with duplicate layer or primitive identities are rejected on both
load and save.
Shared primitive styling is normalized at the model boundary: opacity stays
within 0–1, line width and shadow blur within 0–1,000, shadow offsets within
±10,000, and shape rotation within ±360 degrees. Invalid serialized colors,
pen styles, or non-numeric style fields reject the primitive.
Serialized text is limited to one million characters and uses strict bounded
font, transform, alignment, spacing, box, shadow, stroke, and gradient fields.
Direct geometry/text setters ignore non-finite coordinates and clamp supported
ranges. Automation parameters are recursively bounded by depth, collection
size, string length, and finite ±1-billion numeric values before dispatch.

Image resizing keeps dimensions finite and positive, prevents handles from
crossing their opposite edges, and preserves the source ratio when aspect lock
is enabled. Aspect-lock state is also retained in saved projects.

Saved images retain mask candidates, multi-selection, inversion, overlay, and
refinement settings. Undo/redo and project loading clear optional mask data
before restoration, preventing stale contours from leaking between snapshots;
refinement values are bounded to their supported UI ranges.
Serialized masks use strict finite point/candidate schemas, at most 4,096
candidates and 100,000 total contour points per image; mask points also count
toward the project geometry budget.
Embedded project images use strict base64/PNG decoding and are limited to
64 MiB compressed, 16,384 pixels per dimension, and 64 megapixels decoded.
An invalid embedded image rejects the project before the open document changes.
SAM2 HTTP calls use endpoint-specific deadlines, loopback-only authenticated
requests, 1 MiB health/progress and 128 MiB segmentation response caps, strict
base64 decoding, and bounded candidate, mask, and contour parsing.

### Windows (Visual Studio):
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
.\Release\DrawingStudio.exe
```

## 🎯 **Usage Guide**

### **Getting Started**
1. **Create New Project**: File → New or Ctrl+N
2. **Select Drawing Tool**: Use toolbar or keyboard shortcuts (S, L, B, etc.)
3. **Draw Objects**: Click and drag on canvas to create shapes
4. **Modify Properties**: Select objects to view/edit properties in right panel
5. **Organize with Layers**: Use layers panel to organize complex drawings
6. **Save Project**: File → Save or Ctrl+S

### **Advanced Techniques**
- **Multi-Selection**: Hold Ctrl and click multiple objects
- **Precision Drawing**: Enable grid snapping for accurate placement
- **Blueprint Tracing**: Load reference image and trace over it
- **Measurement**: Use measure tool for technical drawings
- **Layer Management**: Group related elements on separate layers

## 🚀 **Key Benefits**

- **🎨 Versatile**: Perfect for technical drawings, artistic illustrations, and diagrams
- **⚡ Performance**: Hardware-accelerated OpenGL rendering for smooth interaction
- **🔧 Professional**: Industry-standard tools and precision controls
- **💾 Reliable**: Robust project format with full undo/redo support
- **🎯 Intuitive**: Clean interface that doesn't get in your way
- **🔍 Precise**: Sub-pixel accuracy with multiple coordinate systems

## 🎨 **Perfect For**
- **Technical Illustrations**: Engineering drawings, schematics, diagrams
- **Architectural Sketches**: Floor plans, elevations, detail drawings  
- **Product Design**: Concept sketches, specification drawings
- **Educational Materials**: Geometric diagrams, scientific illustrations
- **Artistic Work**: Vector art, logo design, creative illustrations
- **Documentation**: User manuals, instruction guides, workflows

## 🛣️ **Roadmap**

### **Upcoming Features**
- **Text Tool**: Rich text with formatting options
- **Symbol Library**: Reusable drawing components
- **Advanced Export**: SVG, DXF, PDF export options
- **Collaboration**: Real-time collaborative editing
- **Scripting**: Python API for automation
- **Templates**: Pre-built templates for common use cases

---
**Built with ❤️ for designers, engineers, and artists** 🎨
