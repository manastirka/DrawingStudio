# DrawingStudio - Complete Application Documentation for LLM Assistant

## Table of Contents
1. [Application Overview](#application-overview)
2. [Core Architecture](#core-architecture)
3. [Drawing Tools](#drawing-tools)
4. [Primitives System](#primitives-system)
5. [Layer Management](#layer-management)
6. [AI Features](#ai-features)
7. [Undo/Redo System](#undoredo-system)
8. [Keyboard Shortcuts](#keyboard-shortcuts)
9. [Common User Tasks](#common-user-tasks)
10. [Troubleshooting](#troubleshooting)

---

## Application Overview

**DrawingStudio** is a professional Qt6-based drawing application for macOS with advanced AI-powered features including subject detection, mask editing, and AI image generation.

### Key Features
- **Vector Drawing Tools**: Lines, rectangles, circles, curves, polygons, bezier curves, splines
- **AI Subject Detection**: SAM2-powered automatic subject detection and masking
- **Layer System**: Professional layer management with reordering, visibility, locking
- **Advanced Text Editing**: Rich text with formatting, shadows, outlines, gradients
- **AI Image Generation**: Gemini API integration for AI-powered image creation
- **Undo/Redo**: Comprehensive command-based undo system
- **Color Palette**: Quick access to common colors
- **Property Panel**: Dynamic tool-specific settings

### Technology Stack
- **Framework**: Qt6 (Widgets, OpenGL)
- **Language**: C++20
- **AI Backend**: SAM2 (Python service), Gemini API
- **Build System**: CMake
- **Platform**: macOS (Apple Silicon optimized)

---

## Core Architecture

### Main Components

#### 1. MainWindow (`MainWindow.h/cpp`)
- **Purpose**: Main application window, menu management, tool coordination
- **Key Responsibilities**:
  - Menu bar and toolbar setup
  - Tool selection and switching
  - File operations (New, Open, Save)
  - AI feature integration
  - Command manager coordination

#### 2. DrawingCanvas (`DrawingCanvas.h/cpp`)
- **Purpose**: Main drawing surface, handles all drawing operations
- **Key Responsibilities**:
  - Rendering primitives using OpenGL
  - Mouse event handling for drawing
  - Tool-specific behavior implementation
  - Selection management
  - Grid and snap functionality
  - Zoom and pan controls

#### 3. DrawingPrimitive (`DrawingPrimitive.h/cpp`)
- **Purpose**: Base class for all drawable objects
- **Derived Classes**:
  - `LinePrimitive` - Straight lines
  - `RectanglePrimitive` - Rectangles and squares
  - `CirclePrimitive` - Circles and ellipses
  - `CurvePrimitive` - Smooth curves
  - `PolygonPrimitive` - Multi-point polygons
  - `BezierPrimitive` - Bezier curves
  - `SplinePrimitive` - Spline curves
  - `ArcPrimitive` - Circular arcs
  - `TextPrimitive` - Text with rich formatting
  - `ImagePrimitive` - Images with AI features
  - `BrushStrokePrimitive` - Freehand brush strokes

#### 4. LayerManager (`LayerManager.h/cpp`)
- **Purpose**: Manages drawing layers
- **Features**:
  - Create, delete, rename layers
  - Reorder layers (z-order)
  - Lock/unlock layers
  - Show/hide layers
  - Active layer management

#### 5. CommandManager (`CommandManager.h/cpp`)
- **Purpose**: Undo/redo system implementation
- **Features**:
  - Command pattern implementation
  - Undo stack management
  - Redo stack management
  - Command merging for continuous operations

---

## Drawing Tools

### Available Tools

#### 1. **Select Tool** (Default)
- **Shortcut**: `V` or Escape
- **Purpose**: Select and manipulate objects
- **Actions**:
  - Click to select single object
  - Drag to select multiple objects (rectangle selection)
  - Click and drag selected objects to move
  - Resize handles for scaling
  - Rotation handle for rotating

#### 2. **Move Tool**
- **Shortcut**: `M`
- **Purpose**: Move selected objects
- **Actions**:
  - Click and drag to move selected objects
  - Automatically activates after subject extraction

#### 3. **Line Tool**
- **Shortcut**: `L`
- **Purpose**: Draw straight lines
- **Actions**:
  - Click start point
  - Click end point
  - Line is created with current color and width

#### 4. **Rectangle Tool**
- **Shortcut**: `R`
- **Purpose**: Draw rectangles
- **Actions**:
  - Click and drag from corner to corner
  - Hold Shift for perfect squares
  - Filled or outline based on settings

#### 5. **Circle Tool**
- **Shortcut**: `C`
- **Purpose**: Draw circles and ellipses
- **Actions**:
  - Click center point
  - Drag to set radius
  - Hold Shift for perfect circles

#### 6. **Curve Tool**
- **Shortcut**: `U`
- **Purpose**: Draw smooth curves
- **Actions**:
  - Click to add control points
  - Right-click to finish
  - Click near start to close curve

#### 7. **Polygon Tool**
- **Shortcut**: `P`
- **Purpose**: Draw multi-sided polygons
- **Actions**:
  - Click to add vertices
  - Right-click or double-click to finish
  - Click near start to close polygon

#### 8. **Text Tool**
- **Shortcut**: `T`
- **Purpose**: Add text
- **Actions**:
  - Click to place text
  - Type text directly
  - Double-click to open Advanced Text Editor
  - Drag handles to resize text box

#### 9. **Image Tool**
- **Shortcut**: `I`
- **Purpose**: Import images
- **Actions**:
  - Click to open file dialog
  - Select image file
  - Image is placed at click location
  - Supports: PNG, JPG, JPEG, BMP, GIF, TIFF, WEBP

#### 10. **Brush Tool**
- **Shortcut**: `B`
- **Purpose**: Freehand drawing
- **Actions**:
  - Click and drag to draw
  - Adjustable brush size and hardness
  - Pressure sensitivity (if supported)

#### 11. **Eraser Tool**
- **Shortcut**: `E`
- **Purpose**: Erase parts of drawings
- **Actions**:
  - Click and drag to erase
  - Adjustable eraser size

#### 12. **Measure Tool**
- **Shortcut**: `Shift+M`
- **Purpose**: Measure distances
- **Actions**:
  - Click start point
  - Click end point
  - Shows distance in current units

---

## Primitives System

### Primitive Properties

All primitives share these common properties:
- **Position**: (x, y) coordinates
- **Color**: RGB color with alpha
- **Line Width**: Stroke thickness
- **Line Style**: Solid, Dashed, Dotted, Dash-Dot
- **Selected**: Selection state
- **Layer**: Parent layer reference
- **ID**: Unique identifier (UUID)

### Primitive-Specific Properties

#### TextPrimitive
- **Text Content**: The actual text string
- **Font Family**: Arial, Helvetica, Times, etc.
- **Font Size**: Point size
- **Bold**: Boolean
- **Italic**: Boolean
- **Underline**: Boolean
- **Alignment**: Left, Center, Right, Justify
- **Text Box Size**: Width and height
- **Shadow**: Offset, blur, color
- **Outline**: Width, color
- **Background**: Color with alpha

#### ImagePrimitive (Special Features)
- **Image Data**: QImage
- **Size**: Width and height
- **Rotation**: Angle in degrees
- **Detected Subjects**: SAM2 mask candidates
- **Active Mask**: Currently selected mask
- **Mask Inversion**: Boolean
- **Contour Smoothness**: 0-10 (spline interpolation)
- **Edit Mode**: Boolean for mask editing

---

## Layer Management

### Layer System

Layers organize primitives in a z-order stack:
- **Bottom layer** = drawn first (behind)
- **Top layer** = drawn last (in front)

### Layer Operations

#### Create Layer
```
Menu: Layer → New Layer
Shortcut: Ctrl+Shift+N
```

#### Delete Layer
```
Menu: Layer → Delete Layer
Shortcut: Ctrl+Shift+D
```

#### Rename Layer
```
Right-click layer → Rename
```

#### Reorder Layers
```
Drag layer in Layer Panel
```

#### Lock/Unlock Layer
```
Click lock icon in Layer Panel
Locked layers cannot be edited
```

#### Show/Hide Layer
```
Click eye icon in Layer Panel
Hidden layers are not rendered
```

#### Move Objects Between Layers
```
Select objects → Right-click → Move to Layer
```

---

## AI Features

### 1. Subject Detection (SAM2)

**Purpose**: Automatically detect and isolate subjects in images

**How to Use**:
1. Import an image (Image Tool or File → Import)
2. Select the image
3. Menu: Masks → Detect Subjects
4. Wait for SAM2 processing
5. Green outlines appear around detected subjects
6. Click outline to select a subject
7. Use slider to switch between candidates

**Keyboard Shortcuts**:
- `Ctrl+D` - Detect subjects
- `Ctrl+E` - Enter edit mode
- `Ctrl+R` - Refine mask (smoothness)
- `Ctrl+I` - Invert mask
- `Ctrl+X` - Extract selected subject

### 2. Mask Editing

**Purpose**: Manually refine detected masks

**How to Use**:
1. Select image with detected subject
2. Press `Ctrl+E` or Menu: Masks → Edit Mask
3. Green control points appear
4. Drag control points to adjust mask
5. Right-click on contour to add point
6. Click control point + Delete to remove
7. Press `Ctrl+E` again to exit edit mode

**Mask Refinement**:
- **Contour Smoothness**: 0-10 (higher = smoother)
- **Feather**: Soft edge transition
- **Expand/Contract**: Grow or shrink mask
- **Blur**: Blur mask edges

### 3. Subject Extraction (Cutout)

**Purpose**: Cut out subject from image

**How to Use**:
1. Select image with detected/edited subject
2. Press `Ctrl+X` or click "Cut Subject" button
3. Subject is extracted as new image
4. Original image remains
5. Move tool automatically activates
6. Drag to position cutout

**Undo**: `Ctrl+Z` to undo extraction

### 4. AI Image Generation (Gemini)

**Purpose**: Generate images using AI

**Setup Required**:
1. Get Gemini API key from Google AI Studio
2. Menu: File → AI Settings
3. Enter API key
4. Click "Test Connection"

**How to Use**:
1. Menu: AI → Generate Image
2. Enter text prompt
3. Click "Generate"
4. Wait for generation
5. Image appears on canvas

**Advanced Options**:
- **Negative Prompt**: What to avoid
- **Aspect Ratio**: Square, Portrait, Landscape
- **Number of Images**: 1-4

---

## Undo/Redo System

### Command-Based Undo

All operations are tracked as commands:
- **Add Primitive**: Creating any object
- **Delete Primitive**: Deleting objects
- **Move Primitives**: Moving objects
- **Transform**: Scale, rotate, resize
- **Modify Properties**: Color, width, etc.
- **Text Editing**: Text changes
- **Subject Extraction**: Cutout operations
- **Mask Refinement**: Smoothness changes
- **Layer Operations**: Create, delete, reorder

### Keyboard Shortcuts
- **Undo**: `Ctrl+Z` or `Cmd+Z`
- **Redo**: `Ctrl+Shift+Z` or `Cmd+Shift+Z`

### Undo Stack
- Unlimited undo history (memory permitting)
- Stack cleared on new document
- Stack preserved on save/load

---

## Keyboard Shortcuts

### File Operations
- `Ctrl+N` - New Project
- `Ctrl+O` - Open Project
- `Ctrl+S` - Save Project
- `Ctrl+Shift+S` - Save As
- `Ctrl+Q` - Quit

### Edit Operations
- `Ctrl+Z` - Undo
- `Ctrl+Shift+Z` - Redo
- `Ctrl+X` - Cut (or Extract Subject)
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Delete` - Delete Selected

### Tool Selection
- `V` or `Esc` - Select Tool
- `M` - Move Tool
- `L` - Line Tool
- `R` - Rectangle Tool
- `C` - Circle Tool
- `U` - Curve Tool
- `P` - Polygon Tool
- `T` - Text Tool
- `I` - Image Tool
- `B` - Brush Tool
- `E` - Eraser Tool

### View Operations
- `Ctrl++` - Zoom In
- `Ctrl+-` - Zoom Out
- `Ctrl+0` - Reset Zoom
- `Ctrl+G` - Toggle Grid
- `Ctrl+;` - Toggle Snap to Grid

### Layer Operations
- `Ctrl+Shift+N` - New Layer
- `Ctrl+Shift+D` - Delete Layer

### AI/Mask Operations
- `Ctrl+D` - Detect Subjects
- `Ctrl+E` - Edit Mask
- `Ctrl+R` - Refine Mask
- `Ctrl+I` - Invert Mask
- `Ctrl+X` - Extract Subject

### Text Operations
- `Ctrl+Shift+T` - Advanced Text Editor

---

## Common User Tasks

### Task 1: Draw a Simple Shape
1. Select tool (e.g., Rectangle Tool - press `R`)
2. Click and drag on canvas
3. Release to create shape
4. Shape appears with current color/settings

### Task 2: Change Object Color
1. Select object (Select Tool - press `V`)
2. Click object to select
3. Click color in Color Palette panel
4. Object color changes immediately

### Task 3: Add and Format Text
1. Select Text Tool (press `T`)
2. Click on canvas
3. Type text
4. Press `Ctrl+Shift+T` for Advanced Editor
5. Change font, size, color, effects
6. Click OK to apply

### Task 4: Import and Edit Image
1. Select Image Tool (press `I`)
2. Choose image file
3. Image appears on canvas
4. Press `Ctrl+D` to detect subjects
5. Wait for green outlines
6. Click outline to select subject
7. Press `Ctrl+E` to edit mask
8. Drag control points to refine
9. Press `Ctrl+X` to extract subject

### Task 5: Create Multi-Layer Drawing
1. Create new layer (`Ctrl+Shift+N`)
2. Name layer (e.g., "Background")
3. Draw background elements
4. Create another layer (e.g., "Foreground")
5. Draw foreground elements
6. Reorder layers by dragging in Layer Panel
7. Toggle visibility with eye icon

### Task 6: Use AI to Generate Image
1. Menu: AI → Generate Image
2. Enter prompt: "A red dragon flying over mountains"
3. Click Generate
4. Wait for generation
5. Image appears on canvas
6. Use as reference or incorporate into drawing

### Task 7: Measure Distance
1. Select Measure Tool (`Shift+M`)
2. Click start point
3. Click end point
4. Distance is displayed
5. Measurement line remains on canvas

### Task 8: Create Complex Path
1. Select Curve Tool (`U`)
2. Click to add first point
3. Click to add more points
4. Right-click to finish
5. Curve is smoothly interpolated
6. Edit control points if needed

---

## Troubleshooting

### Subject Detection Not Working

**Problem**: "Detect Subjects" does nothing or shows error

**Solutions**:
1. Check SAM2 service is running:
   ```bash
   cd sam2_service
   python sam2_server.py
   ```
2. Verify image is selected
3. Check image format (must be PNG, JPG, etc.)
4. Check terminal for error messages
5. Restart SAM2 service

### Undo Not Working

**Problem**: Ctrl+Z doesn't undo operation

**Solutions**:
1. Check if operation is undoable (some aren't)
2. Verify command manager is initialized
3. Check debug output in terminal
4. Try Menu: Edit → Undo instead of shortcut

### Text Not Appearing

**Problem**: Text tool creates text but it's invisible

**Solutions**:
1. Check text color (might be same as background)
2. Check font size (might be too small)
3. Check layer visibility
4. Check if text is behind other objects

### Image Import Fails

**Problem**: Image tool doesn't import image

**Solutions**:
1. Check file format (supported: PNG, JPG, JPEG, BMP, GIF, TIFF, WEBP)
2. Check file size (very large images are scaled)
3. Check file permissions
4. Try different image file

### Mask Editing Issues

**Problem**: Can't edit mask or control points don't appear

**Solutions**:
1. Ensure subject is detected first (`Ctrl+D`)
2. Ensure image is selected
3. Press `Ctrl+E` to enter edit mode
4. Check if mask exists (green outline visible)
5. Exit and re-enter edit mode

### AI Generation Fails

**Problem**: AI image generation shows error

**Solutions**:
1. Check API key is set (File → AI Settings)
2. Test connection in AI Settings
3. Check internet connection
4. Verify Gemini API quota
5. Try simpler prompt
6. Check terminal for error details

---

## Advanced Features

### Grid and Snap

**Grid**:
- Toggle: `Ctrl+G`
- Adjustable spacing in settings
- Visual guide for alignment

**Snap to Grid**:
- Toggle: `Ctrl+;`
- Objects snap to grid intersections
- Helps create precise drawings

### Line Styles

Available styles:
- **Solid**: Continuous line
- **Dashed**: Long dashes
- **Dotted**: Small dots
- **Dash-Dot**: Alternating dash and dot

Apply to: Lines, rectangles, circles, curves, polygons

### Brush Settings

**Brush Size**: 1-100 pixels
**Brush Hardness**: 0-100%
- 0% = Very soft edges
- 100% = Hard edges

### Text Effects

**Shadow**:
- Offset X/Y
- Blur radius
- Color and opacity

**Outline**:
- Width
- Color

**Background**:
- Color with transparency
- Padding

### Transform Operations

**Move**: Drag with Move tool or Select tool
**Scale**: Drag resize handles
**Rotate**: Drag rotation handle
**Flip**: Menu → Transform → Flip Horizontal/Vertical

---

## File Format

### Project Files (.drawing)

DrawingStudio saves projects in JSON format:
- All primitives with properties
- Layer structure
- Canvas settings
- Metadata (creation date, modified date)

### Export Options

Currently supports:
- Save Project (.drawing)
- Future: Export to PNG, SVG, PDF

---

## Performance Tips

### For Large Projects
1. Use layers to organize (faster than many objects on one layer)
2. Lock layers you're not editing
3. Hide layers you're not viewing
4. Delete unused objects

### For AI Features
1. Resize large images before detection (faster processing)
2. Use lower resolution for quick tests
3. Keep SAM2 service running (avoid restart overhead)

### For Smooth Drawing
1. Close unnecessary applications
2. Use hardware acceleration (enabled by default)
3. Reduce brush size for complex strokes
4. Use fewer control points on curves

---

## API Reference for LLM

### When User Asks About...

**"How do I [draw/create] X?"**
→ Recommend appropriate tool and steps

**"How do I change [property]?"**
→ Explain selection + property panel or menu

**"How do I use AI features?"**
→ Guide through SAM2 or Gemini workflow

**"Something is not working"**
→ Check Troubleshooting section

**"What's the shortcut for X?"**
→ Refer to Keyboard Shortcuts section

**"How do I organize my drawing?"**
→ Explain layer system

**"Can I undo X?"**
→ Check if operation is in Undo/Redo System list

---

## Version Information

**Current Version**: 1.0
**Qt Version**: 6.x
**Platform**: macOS (Apple Silicon)
**Build System**: CMake
**License**: [Your License]

---

## Support and Resources

**Documentation**: This file
**Build Instructions**: BUILD_STATUS.md
**SAM2 Setup**: SAM2_SETUP_INSTRUCTIONS.md
**AI Features**: AI_IMAGE_GENERATION_GUIDE.md
**Development**: DEVELOPMENT_PLAN.md

---

*This documentation is designed to be used by an LLM assistant to help users with DrawingStudio. The LLM should use this as context to answer user questions, provide guidance, and suggest solutions.*
