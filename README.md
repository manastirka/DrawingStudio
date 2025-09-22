# Guitar Builder CAD Application 🎸

A professional CAD application for guitar design and visualization built with Qt6 and C++20.

## 🚀 Features Implemented

### ✨ **New Guitar Component System**
- **Draw-First Workflow**: Use drawing tools to create shapes, then promote them to guitar components
- **Component Promotion**: Right-click selected shapes → "🎸 Promote to Guitar Component"
- **No More Placeholders**: Only explicitly promoted objects become real guitar components
- **Real Database Integration**: Access authentic guitar manufacturer specifications

### 🎨 **Drawing Tools**
- **Line Tool**: Create straight lines with snap-to-grid
- **Bezier Curve Tool**: Create smooth curves with control points
- **Rectangle Tool**: Draw rectangular shapes
- **Ellipse Tool**: Draw circular and elliptical shapes
- **Spline Tool**: Create flowing curves through multiple points
- **Selection Tool**: Select and modify existing elements

### 🎸 **Guitar Component Database**
**Real specifications from major manufacturers:**

#### **Guitar Bodies** (5 models):
- **Gibson Les Paul Body**: 350×480×45mm, mahogany with maple cap, 2.1kg
- **Fender Stratocaster Body**: 325×460×44mm, alder/ash, 1.8kg
- **Fender Telecaster Body**: 320×450×44mm, ash/alder, 1.9kg
- **PRS Custom 24 Body**: 340×470×50mm, mahogany with maple top, 2.0kg
- **Ibanez RG Body**: 315×445×43mm, basswood, 1.7kg

#### **Guitar Necks** (6 models):
- **Gibson Les Paul Neck**: 43mm nut, 628mm scale (24.75"), mahogany/rosewood
- **Fender Stratocaster Neck**: 42mm nut, 648mm scale (25.5"), maple
- **Fender Telecaster Neck**: 42.8mm nut, 648mm scale, maple
- **PRS Custom 24 Neck**: 43mm nut, 635mm scale (25"), mahogany/rosewood
- **Ibanez RG Neck**: 42mm nut, 648mm scale, thin Wizard profile
- **Martin D-28 Neck**: 44.5mm nut, 648mm scale, rosewood/ebony

#### **Guitar Headstocks** (7 models):
- **Gibson Les Paul**: 85×180mm, angled with crown inlay
- **Fender Stratocaster**: 89×175mm, straight headstock
- **PRS Custom 24**: 78×165mm, angled with bird logo
- And more from major manufacturers...

### 🔧 **Component Integration System**

#### **Property Panel Integration**:
- **Auto-display properties** after component promotion
- **Database-specific properties** instead of generic primitive properties
- **"Open Database Properties" button** for detailed specifications
- **Real manufacturer data** with dimensions, materials, prices

#### **Left Panel ↔ Canvas Sync**:
- **Bidirectional selection sync** between components tree and canvas
- **Click component in tree** → highlights on canvas
- **Select on canvas** → shows promoted component properties
- **Component tracking system** maintains relationships

### 🎯 **User Workflow**

1. **🎨 Draw Guitar Outline**: Use Bezier/Ellipse tools to create guitar body shape
2. **🎯 Select Shape**: Switch to selection tool and click the drawn shape
3. **🎸 Promote to Component**: Right-click → "Promote to Guitar Component" → "Main Parts" → "Body"
4. **✨ Automatic Properties**: Property panel opens showing database properties
5. **📋 Database Access**: Click "Open Database Properties" → Select "Gibson Les Paul Body"
6. **🔄 Perfect Sync**: Click "Body" in left tree → shape highlights on canvas

## 🛠 **Technical Architecture**

### **Core Classes**:
- **DrawingCanvas**: Main drawing surface with OpenGL rendering
- **ComponentDatabase**: Real guitar component specifications and database
- **PropertyPanel**: Dynamic property editor for components
- **MainWindow**: Application framework with docking panels
- **DrawingPrimitive**: Base class for all drawable objects

### **Component System**:
```cpp
struct ComponentInfo {
    ComponentType type;        // Body, Neck, Headstock, etc.
    QString subType;          // Specific model info
    QString displayName;      // Full component name
};

// Component tracking maps
std::map<DrawingPrimitive*, ComponentInfo> m_promotedComponents;
std::map<QString, DrawingPrimitive*> m_componentNameToPrimitive;
```

### **New Component Types**:
- **ComponentType::Body** - Guitar body components
- **ComponentType::Neck** - Guitar neck components  
- **ComponentType::Headstock** - Guitar headstock components
- **Plus all existing types**: Pickups, Hardware, Electronics

## 🎸 **Component Color System**
- **Body**: Brown (`#8B4513`) 
- **Neck/Headstock**: Saddle brown (`#A0522D`)
- **Pickups**: Dark gray (humbuckers) / Cream (single coils)
- **Hardware**: Silver, gold, bone colors by component type

## 📁 **Project Structure**
```
GuitarBuilder/
├── include/           # Header files
├── src/               # Source files  
├── build/             # Build artifacts (ignored)
├── CMakeLists.txt     # CMake configuration
├── README.md          # This file
└── .gitignore         # Git ignore rules
```

## 🔧 **Build Instructions**

### Prerequisites:
- Qt6 (with OpenGL support)
- CMake 3.20+
- C++20 compatible compiler

### Build:
```bash
mkdir build
cd build
cmake ..
make
./GuitarBuilder
```

## 🎯 **Recent Major Updates**

### **Component Promotion System** ✅
- Removed placeholder component creation
- Added "Promote to Guitar Component" context menu
- Only appears when drawing primitives are selected
- Real guitar components with database access

### **Property Panel Integration** ✅  
- Auto-display properties after promotion
- Database-specific property panels
- "Open Database Properties" button integration
- Real manufacturer specifications display

### **Left Panel Sync** ✅
- Bidirectional selection between tree and canvas
- Component name to primitive mapping system
- Visual selection feedback on canvas
- Seamless workflow integration

## 🚀 **Key Benefits**

- **🎨 Creative Freedom**: Draw any shape, then categorize as guitar component
- **🎯 Intentional Design**: Only promoted objects become guitar components
- **📋 Authentic Data**: Real guitar manufacturer specifications 
- **🔄 Seamless Workflow**: Perfect integration between drawing and component systems
- **🧹 Clean Interface**: No placeholder components cluttering the workspace

## 🎵 **Perfect for Guitar Designers**
Whether you're designing a custom Gibson Les Paul, Fender Stratocaster, or your own unique design, Guitar Builder provides the professional tools and authentic specifications you need to create accurate guitar blueprints.

---
**Built with ❤️ for guitar makers and designers** 🎸