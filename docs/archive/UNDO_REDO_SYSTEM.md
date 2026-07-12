# Comprehensive Undo/Redo System

This document describes the complete undo/redo system implemented for all operations in the DrawingStudio application.

## Architecture Overview

The system uses a **Command Pattern** architecture with two complementary undo/redo managers:

1. **CommandManager** - For precise command-based operations (new)
2. **UndoRedoManager** - For snapshot-based operations (legacy fallback)

## Supported Operations

### ✅ Primitive Creation
- **All primitive types**: Line, Rectangle, Circle, Ellipse, Polygon, Curve, Bezier Curve, Spline, Arc, Text, Dimension, Image, Brush Strokes
- **Implementation**: `AddPrimitiveCommand` via `addPrimitiveWithCommand()`
- **Undo behavior**: Removes the primitive from canvas
- **Redo behavior**: Re-adds the primitive to canvas

### ✅ Primitive Deletion
- **Multiple selection deletion**: Delete key or menu command
- **Implementation**: `DeletePrimitivesCommand` via `deleteSelectedPrimitivesWithCommand()`
- **Undo behavior**: Restores deleted primitives to their original layers
- **Redo behavior**: Removes primitives again

### ✅ Property Modifications
- **All primitive properties**: Color, line width, fill settings, visibility, etc.
- **Implementation**: `ModifyPrimitiveCommand` via PropertyPanel changes
- **Undo behavior**: Restores previous property values
- **Redo behavior**: Re-applies property changes
- **Features**: 
  - Supports batch property changes for multiple selected objects
  - Command merging for consecutive property changes

### ✅ Move/Transform Operations
- **Object movement**: Drag and drop operations
- **Implementation**: `MovePrimitivesCommand` via `finishMoveOperation()`
- **Undo behavior**: Moves objects back to original positions
- **Redo behavior**: Re-applies the movement
- **Features**: 
  - Command merging for continuous drag operations
  - Supports multiple object selection movement

### ✅ Control Point Modifications
- **Control point editing**: Direct manipulation of curve points, rectangle corners, etc.
- **Implementation**: `ModifyControlPointCommand` via `finishControlPointEdit()`
- **Undo behavior**: Restores original control point position
- **Redo behavior**: Re-applies control point changes
- **Features**:
  - Command merging for continuous control point dragging
  - Precise position tracking

### ✅ Layer Operations
- **Layer management**: Create, delete, move objects between layers, reorder layers
- **Implementation**: `LayerCommand` for layer-specific operations
- **Undo behavior**: Reverses layer operations
- **Redo behavior**: Re-applies layer operations

### ✅ Brush Stroke Operations
- **Special handling**: Continuous brush/blur strokes
- **Implementation**: `BrushStrokeCommand` with stroke merging
- **Undo behavior**: Removes entire brush stroke
- **Redo behavior**: Re-adds brush stroke
- **Features**: 
  - Command merging for continuous brush strokes
  - Point-by-point stroke building

### ✅ Mask Editing Operations (ImagePrimitive)
- **Mask control point modifications**: Move mask contour points
- **Implementation**: `ModifyMaskControlPointCommand`
- **Undo behavior**: Restores original mask point position
- **Redo behavior**: Re-applies mask point movement
- **Features**: Command merging for continuous mask point dragging

- **Mask point insertion**: Add new points to mask contour
- **Implementation**: `InsertMaskControlPointCommand`
- **Undo behavior**: Removes the inserted point
- **Redo behavior**: Re-inserts the point

- **Mask point deletion**: Remove points from mask contour
- **Implementation**: `DeleteMaskControlPointCommand`
- **Undo behavior**: Restores the deleted point at original position
- **Redo behavior**: Re-deletes the point

- **Mask candidate selection**: Switch between different mask candidates
- **Implementation**: `SelectMaskCandidateCommand`
- **Undo behavior**: Reverts to previous mask candidate
- **Redo behavior**: Re-selects the new mask candidate

## Command Classes

### Core Commands
- **`AddPrimitiveCommand`** - Adding primitives to canvas
- **`DeletePrimitivesCommand`** - Removing primitives from canvas
- **`ModifyPrimitiveCommand`** - Property changes with merging support
- **`MovePrimitivesCommand`** - Object movement with merging
- **`ModifyControlPointCommand`** - Control point modifications
- **`TransformPrimitivesCommand`** - Scale, rotate, resize operations
- **`LayerCommand`** - Layer operations
- **`BrushStrokeCommand`** - Brush stroke operations
- **`CompoundCommand`** - Grouping multiple operations

### Mask Editing Commands
- **`ModifyMaskControlPointCommand`** - Moving mask contour points with merging
- **`InsertMaskControlPointCommand`** - Adding new mask contour points
- **`DeleteMaskControlPointCommand`** - Removing mask contour points
- **`SelectMaskCandidateCommand`** - Switching between mask candidates

### Key Features
- **Command Merging**: Consecutive similar operations are merged for cleaner undo/redo
- **JSON Serialization**: State preservation using primitive serialization
- **Memory Safety**: Proper ownership transfer with unique_ptr
- **Signal-based Architecture**: Commands are requested via Qt signals for decoupling

## Usage

### For Users
- **Undo**: `Ctrl+Z` or Edit → Undo
- **Redo**: `Ctrl+Y` or Edit → Redo  
- **Hybrid System**: Commands are tried first, falls back to snapshots for legacy operations

### For Developers

#### Adding New Undoable Operations
1. Create a new command class inheriting from `Command`
2. Implement `execute()`, `undo()`, and `description()` methods
3. Emit `commandRequested` signal from canvas or call `executeCommand()` from MainWindow
4. Optional: Implement `canMergeWith()` and `mergeWith()` for command merging

#### Example
```cpp
// Create a command
auto command = std::make_unique<MyCustomCommand>(parameters);

// Execute via MainWindow
mainWindow->executeCommand(command.release());

// Or emit from canvas
emit commandRequested(command.release());
```

## Implementation Details

### Signal Flow
1. **Canvas Operations** → `commandRequested` signal → **MainWindow** → **CommandManager**
2. **Property Changes** → `propertyChanged` signal → **MainWindow** → **CommandManager**
3. **Undo/Redo UI** → **MainWindow** → **CommandManager** (primary) → **UndoRedoManager** (fallback)

### Files Modified/Created
- `include/Command.h` - Base command interface
- `include/Commands.h` - All command declarations  
- `src/Commands.cpp` - Command implementations
- `include/CommandManager.h` - Command stack manager
- `src/CommandManager.cpp` - Command execution logic
- `MainWindow.h/.cpp` - Integrated dual undo/redo system
- `DrawingCanvas.h/.cpp` - Command-based operations
- `CMakeLists.txt` - Added new source files

### Memory Management
- **Commands**: Managed via `std::unique_ptr` with proper ownership transfer
- **Primitives**: JSON serialization for state preservation instead of object cloning
- **Signal Safety**: Raw pointers passed via signals, ownership transferred to CommandManager

## Testing

The system has been successfully tested for:
- ✅ Primitive creation and deletion
- ✅ Property modifications
- ✅ Move operations  
- ✅ Control point editing
- ✅ Multiple object operations
- ✅ Command merging
- ✅ Memory safety
- ✅ UI integration

## Future Enhancements

- **Group Operations**: Compound commands for complex multi-step operations
- **Macro Recording**: Record and replay command sequences
- **Selective Undo**: Cherry-pick specific operations to undo
- **Command History UI**: Visual command history with branching
- **Persistence**: Save/load command history with projects