# Customizable Canvas Colors

## Overview
This document describes the implementation of customizable background and paper colors in the DrawingStudio application.

## Problem Solved
Previously, the canvas had a fixed white background color, and the A4 paper format area had a fixed gray color that couldn't be changed by users. This feature makes both colors customizable through the user interface.

## Implementation Details

### Background vs Paper Colors
- **Background Color**: The overall canvas background (area outside the paper format)
- **Paper Color**: The A4/paper format area color (the actual drawing surface)

### Code Changes

#### DrawingCanvas.h
- Added `m_paperColor` member variable
- Added `setPaperColor(const QColor &color)` method
- Added `paperColor()` const getter method

#### DrawingCanvas.cpp
- **Constructor**: Initialize paper color to light gray `QColor(250, 250, 250)`
- **setPaperColor()**: Sets the paper color and triggers an update
- **renderPaper()**: Updated to use `m_paperColor` instead of hardcoded gray value

#### MainWindow.h
- Added `changeBackgroundColor()` slot
- Added `changePaperColor()` slot

#### MainWindow.cpp
- Added `#include <QColorDialog>` for color picker dialog
- Added "Colors" submenu to View menu with two options:
  - "Background Color..." - Changes canvas background
  - "Paper Color..." - Changes A4 paper area color
- Implemented color picker dialogs with current color preview

### User Interface

The color customization options are accessible through:

```
Menu: View > Colors > Background Color...
Menu: View > Colors > Paper Color...
```

### Usage Instructions

1. **Change Background Color**:
   - Go to View > Colors > Background Color...
   - Select desired color from color picker dialog
   - Click OK to apply

2. **Change Paper Color**:
   - Go to View > Colors > Paper Color...
   - Select desired color from color picker dialog
   - Click OK to apply

### Default Colors

- **Background**: Pure white `#FFFFFF` (255, 255, 255)
- **Paper**: Light gray `#FAFAFA` (250, 250, 250) - slightly off-white

### Technical Details

#### Color Rendering
- Background color is applied through OpenGL's `glClearColor()`
- Paper color is applied in `renderPaper()` using `glColor4f()` with 90% opacity
- Colors are stored as `QColor` objects for consistency with Qt's color system

#### Color Picker Integration
- Uses Qt's `QColorDialog::getColor()` for native color selection
- Shows current color as default in picker
- Validates color before applying (checks `isValid()`)
- Provides user feedback through status bar messages

### Code Example

```cpp
// Change background color programmatically
canvas->setBackgroundColor(QColor(240, 240, 240)); // Light gray background

// Change paper color programmatically  
canvas->setPaperColor(QColor(255, 255, 255)); // Pure white paper
```

### Future Enhancements

Potential improvements could include:
- Save/load color preferences
- Color presets/themes
- Gradient backgrounds
- Paper texture options
- Color picker with recently used colors
- Real-time color preview while selecting

### Testing

To test the functionality:

1. Launch DrawingStudio
2. Go to View > Colors > Background Color...
3. Select a different color (e.g., light blue)
4. Notice the canvas background changes
5. Go to View > Colors > Paper Color...
6. Select a different color (e.g., cream/beige)
7. Notice the A4 paper area changes color
8. Create some drawing primitives to see contrast with new colors

### Build Instructions

```bash
cd /Users/Lukovic/Apps/DrawingStudio
mkdir -p build && cd build
cmake ..
make
./DrawingStudio
```

## Files Modified

1. `include/DrawingCanvas.h` - Added paper color property
2. `src/DrawingCanvas.cpp` - Implemented paper color functionality
3. `include/MainWindow.h` - Added color change method declarations
4. `src/MainWindow.cpp` - Added UI controls and color picker implementation

This feature significantly improves user customization options and allows for better visual contrast and personal preferences when working with drawings.