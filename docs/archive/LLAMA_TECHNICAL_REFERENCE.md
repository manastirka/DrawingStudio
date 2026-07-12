# DrawingStudio - Technical Reference for LLM Assistant

## Code Architecture Deep Dive

### Class Hierarchy

```
QMainWindow
└── MainWindow
    ├── DrawingCanvas (QOpenGLWidget)
    ├── LayerPanel (QDockWidget)
    ├── PropertyPanel (QDockWidget)
    └── ColorPalette (QDockWidget)

DrawingPrimitive (Abstract Base)
├── LinePrimitive
├── RectanglePrimitive
├── CirclePrimitive
├── CurvePrimitive
├── PolygonPrimitive
├── BezierPrimitive
├── SplinePrimitive
├── ArcPrimitive
├── TextPrimitive
├── ImagePrimitive
└── BrushStrokePrimitive

Command (Abstract Base)
├── AddPrimitiveCommand
├── DeletePrimitivesCommand
├── MovePrimitivesCommand
├── ModifyPrimitiveCommand
├── TransformPrimitivesCommand
├── ModifyControlPointCommand
├── LayerCommand
├── BrushStrokeCommand
├── ModifyMaskControlPointCommand
├── InsertMaskControlPointCommand
├── DeleteMaskControlPointCommand
├── SelectMaskCandidateCommand
├── ExtractSubjectCommand
├── EditTextCommand
├── ImportImageCommand
├── SetContourSmoothnessCommand
├── ResizeTextCommand
└── CompoundCommand
```

---

## Drawing Tools Implementation

### Tool Enum
```cpp
enum class DrawingTool {
    Select,      // Selection and manipulation
    Move,        // Move objects
    Line,        // Draw lines
    Rectangle,   // Draw rectangles
    Circle,      // Draw circles
    Curve,       // Draw curves
    Polygon,     // Draw polygons
    Bezier,      // Draw bezier curves
    Spline,      // Draw splines
    Arc,         // Draw arcs
    Text,        // Add text
    Image,       // Import images
    Brush,       // Freehand brush
    Eraser,      // Erase
    Measure,     // Measure distances
    AngleLine,   // Draw angled lines
    Blur         // Blur tool
};
```

### Tool Switching Logic
```cpp
// In DrawingCanvas
void setCurrentTool(DrawingTool tool) {
    m_currentTool = tool;
    // Reset tool-specific state
    m_isDrawing = false;
    m_currentPrimitive.reset();
    // Update cursor
    updateCursor();
    emit toolChanged(tool);
}
```

---

## Primitive System Details

### Primitive Creation Flow

1. **User clicks with tool active**
   ```cpp
   // DrawingCanvas::mousePressEvent
   switch (m_currentTool) {
       case DrawingTool::Line:
           handleLineTool(event);
           break;
       // ... other tools
   }
   ```

2. **Tool handler creates primitive**
   ```cpp
   void handleLineTool(QMouseEvent* event) {
       m_drawStartPos = screenToWorld(event->pos());
       m_currentPrimitive = std::make_unique<LinePrimitive>(
           m_drawStartPos, m_drawStartPos
       );
       m_isDrawing = true;
   }
   ```

3. **Mouse move updates primitive**
   ```cpp
   void mouseMoveEvent(QMouseEvent* event) {
       if (m_isDrawing && m_currentPrimitive) {
           // Update primitive based on tool
           update(); // Trigger repaint
       }
   }
   ```

4. **Mouse release finalizes primitive**
   ```cpp
   void mouseReleaseEvent(QMouseEvent* event) {
       if (m_isDrawing && m_currentPrimitive) {
           addPrimitiveWithCommand(std::move(m_currentPrimitive));
           m_isDrawing = false;
       }
   }
   ```

### Primitive Rendering

All primitives implement:
```cpp
virtual void render(bool selected = false) const = 0;
```

Example (LinePrimitive):
```cpp
void LinePrimitive::render(bool selected) const {
    glColor4f(m_color.redF(), m_color.greenF(), 
              m_color.blueF(), m_color.alphaF());
    glLineWidth(m_lineWidth);
    
    // Apply line style
    applyLineStyle(m_lineStyle);
    
    glBegin(GL_LINES);
    glVertex2f(m_start.x(), m_start.y());
    glVertex2f(m_end.x(), m_end.y());
    glEnd();
    
    if (selected) {
        renderSelectionHandles();
    }
}
```

---

## Layer System Implementation

### Layer Class
```cpp
class Layer {
    QUuid m_id;
    QString m_name;
    bool m_visible;
    bool m_locked;
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
    
public:
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void removePrimitive(DrawingPrimitive* primitive);
    void render();
    bool isVisible() const { return m_visible; }
    bool isLocked() const { return m_locked; }
};
```

### Layer Rendering Order
```cpp
// In DrawingCanvas::paintGL()
for (const auto& layer : m_layerManager->layers()) {
    if (layer->isVisible()) {
        layer->render();
    }
}
```

### Layer Z-Order
- Layers stored in vector (bottom to top)
- First layer = bottom (drawn first)
- Last layer = top (drawn last)
- Reordering changes vector order

---

## Command System Implementation

### Command Pattern
```cpp
class Command {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual QString description() const = 0;
    virtual bool canMergeWith(const Command* other) const { return false; }
    virtual void mergeWith(const Command* other) {}
};
```

### Command Manager
```cpp
class CommandManager {
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    
public:
    void executeCommand(std::unique_ptr<Command> command) {
        command->execute();
        m_undoStack.push_back(std::move(command));
        m_redoStack.clear(); // Clear redo on new command
    }
    
    void undo() {
        if (!m_undoStack.empty()) {
            auto cmd = std::move(m_undoStack.back());
            m_undoStack.pop_back();
            cmd->undo();
            m_redoStack.push_back(std::move(cmd));
        }
    }
    
    void redo() {
        if (!m_redoStack.empty()) {
            auto cmd = std::move(m_redoStack.back());
            m_redoStack.pop_back();
            cmd->execute();
            m_undoStack.push_back(std::move(cmd));
        }
    }
};
```

### Command Execution Flow
```cpp
// 1. Create command
auto command = std::make_unique<AddPrimitiveCommand>(canvas, primitive);

// 2. Execute via manager
commandManager->executeCommand(std::move(command));

// 3. Undo
commandManager->undo(); // Calls command->undo()

// 4. Redo
commandManager->redo(); // Calls command->execute() again
```

---

## AI Features Implementation

### SAM2 Integration

#### Architecture
```
DrawingStudio (Qt/C++)
    ↓ HTTP Request
SAM2 Service (Python Flask)
    ↓ Process
SAM2 Model (PyTorch)
    ↓ Return
Masks (JSON)
    ↓ Parse
DrawingStudio (Display)
```

#### SAM2Client Class
```cpp
class SAM2Client : public QObject {
    Q_OBJECT
    
public:
    void detectSubjects(const QImage& image);
    
signals:
    void subjectsDetected(const QJsonArray& masks);
    void error(const QString& message);
    
private:
    QNetworkAccessManager* m_networkManager;
    QString m_serverUrl = "http://localhost:5000";
};
```

#### Detection Flow
```cpp
// 1. User selects image and clicks "Detect Subjects"
void MainWindow::detectSubjects() {
    auto* imgPrim = getSelectedImagePrimitive();
    if (imgPrim) {
        m_sam2Client->detectSubjects(imgPrim->image());
    }
}

// 2. SAM2Client sends HTTP request
void SAM2Client::detectSubjects(const QImage& image) {
    // Convert image to base64
    QByteArray imageData = imageToBase64(image);
    
    // Create JSON request
    QJsonObject request;
    request["image"] = QString(imageData);
    
    // Send POST request
    m_networkManager->post(url, jsonData);
}

// 3. Response received
void SAM2Client::onResponse(QNetworkReply* reply) {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray masks = doc["masks"].toArray();
    emit subjectsDetected(masks);
}

// 4. ImagePrimitive stores masks
void ImagePrimitive::setMaskCandidates(const QJsonArray& masks) {
    m_maskCandidates.clear();
    for (const auto& mask : masks) {
        MaskCandidate candidate;
        candidate.contour = parseContour(mask);
        candidate.score = mask["score"].toDouble();
        m_maskCandidates.push_back(candidate);
    }
}
```

### Mask Editing

#### Edit Mode
```cpp
// Enter edit mode
void ImagePrimitive::setEditMode(bool enabled) {
    m_editMode = enabled;
    if (enabled) {
        // Show control points
        // Enable point dragging
    }
}
```

#### Control Point Manipulation
```cpp
// Move control point
void ImagePrimitive::moveControlPoint(int index, const QVector2D& position) {
    if (index >= 0 && index < m_detectedSubject.contour.size()) {
        m_detectedSubject.contour[index] = QPointF(position.x(), position.y());
        emit maskModified();
    }
}

// Insert control point
void ImagePrimitive::insertControlPoint(int afterIndex, const QVector2D& position) {
    m_detectedSubject.contour.insert(
        m_detectedSubject.contour.begin() + afterIndex + 1,
        QPointF(position.x(), position.y())
    );
}

// Delete control point
void ImagePrimitive::deleteControlPoint(int index) {
    if (m_detectedSubject.contour.size() > 3) { // Keep minimum 3 points
        m_detectedSubject.contour.erase(
            m_detectedSubject.contour.begin() + index
        );
    }
}
```

#### Contour Smoothing
```cpp
// Catmull-Rom spline interpolation
std::vector<QPointF> ImagePrimitive::getSmoothedContour() const {
    if (m_contourSmoothness == 0) {
        return m_detectedSubject.contour;
    }
    
    std::vector<QPointF> smoothed;
    int n = m_detectedSubject.contour.size();
    
    for (int i = 0; i < n; ++i) {
        QPointF p0 = m_detectedSubject.contour[(i - 1 + n) % n];
        QPointF p1 = m_detectedSubject.contour[i];
        QPointF p2 = m_detectedSubject.contour[(i + 1) % n];
        QPointF p3 = m_detectedSubject.contour[(i + 2) % n];
        
        smoothed.push_back(p1);
        
        // Interpolate between p1 and p2
        for (int j = 1; j <= m_contourSmoothness; ++j) {
            float t = static_cast<float>(j) / (m_contourSmoothness + 1);
            QPointF interpolated = catmullRomSpline(p0, p1, p2, p3, t);
            smoothed.push_back(interpolated);
        }
    }
    
    return smoothed;
}
```

### Subject Extraction

```cpp
std::unique_ptr<ImagePrimitive> ImagePrimitive::extractDetectedSubject() {
    if (m_detectedSubject.contour.empty()) {
        return nullptr;
    }
    
    // 1. Get smoothed contour
    std::vector<QPointF> contour = getSmoothedContour();
    
    // 2. Create mask from contour
    QImage mask = createMaskFromContour(contour);
    
    // 3. Apply mask refinements (feather, blur, expand)
    if (m_maskExpand != 0 || m_maskBlur > 0 || m_maskFeather > 0) {
        mask = applyMaskRefinements(mask);
    }
    
    // 4. Apply mask to image
    QImage extracted = applyMask(m_image, mask);
    
    // 5. Create new ImagePrimitive
    auto result = std::make_unique<ImagePrimitive>(
        extracted, m_position, m_size
    );
    result->setRotation(m_rotation);
    
    return result;
}
```

---

## Text System Implementation

### TextPrimitive Properties
```cpp
class TextPrimitive : public DrawingPrimitive {
    QString m_text;
    QString m_fontFamily;
    int m_fontSize;
    bool m_bold;
    bool m_italic;
    bool m_underline;
    TextAlignment m_alignment;
    float m_textBoxWidth;
    float m_textBoxHeight;
    
    // Effects
    bool m_hasShadow;
    QPointF m_shadowOffset;
    float m_shadowBlur;
    QColor m_shadowColor;
    
    bool m_hasOutline;
    float m_outlineWidth;
    QColor m_outlineColor;
    
    bool m_hasBackground;
    QColor m_backgroundColor;
};
```

### Text Rendering
```cpp
void TextPrimitive::render(bool selected) const {
    // 1. Setup font
    QFont font(m_fontFamily, m_fontSize);
    font.setBold(m_bold);
    font.setItalic(m_italic);
    font.setUnderline(m_underline);
    
    // 2. Render background
    if (m_hasBackground) {
        renderBackground();
    }
    
    // 3. Render shadow
    if (m_hasShadow) {
        renderShadow(font);
    }
    
    // 4. Render outline
    if (m_hasOutline) {
        renderOutline(font);
    }
    
    // 5. Render text
    renderText(font);
    
    // 6. Render selection handles
    if (selected) {
        renderSelectionHandles();
    }
}
```

### Advanced Text Editor
```cpp
class AdvancedTextEditor : public QDialog {
    QTextEdit* m_textEditor;
    QComboBox* m_fontFamily;
    QSpinBox* m_fontSize;
    QPushButton* m_boldButton;
    QPushButton* m_italicButton;
    QPushButton* m_underlineButton;
    // ... shadow, outline, background controls
    
public:
    void setTextPrimitive(TextPrimitive* primitive);
    void applyToTextPrimitive(TextPrimitive* primitive);
};
```

---

## Coordinate Systems

### Screen Space
- Origin: Top-left corner of window
- Units: Pixels
- Used by: Qt mouse events

### World Space
- Origin: Center of canvas or custom
- Units: Arbitrary (user-defined)
- Used by: Primitives, drawing logic

### Conversion
```cpp
QVector2D DrawingCanvas::screenToWorld(const QPoint& screenPos) const {
    // Account for zoom and pan
    float worldX = (screenPos.x() - m_panOffset.x()) / m_zoomFactor;
    float worldY = (screenPos.y() - m_panOffset.y()) / m_zoomFactor;
    return QVector2D(worldX, worldY);
}

QPoint DrawingCanvas::worldToScreen(const QVector2D& worldPos) const {
    int screenX = worldPos.x() * m_zoomFactor + m_panOffset.x();
    int screenY = worldPos.y() * m_zoomFactor + m_panOffset.y();
    return QPoint(screenX, screenY);
}
```

---

## Serialization (Save/Load)

### JSON Format
```json
{
    "version": "1.0",
    "canvas": {
        "width": 1920,
        "height": 1080,
        "backgroundColor": "#FFFFFF"
    },
    "layers": [
        {
            "id": "uuid-1",
            "name": "Background",
            "visible": true,
            "locked": false,
            "primitives": [
                {
                    "type": "Line",
                    "id": "uuid-2",
                    "start": {"x": 0, "y": 0},
                    "end": {"x": 100, "y": 100},
                    "color": "#000000",
                    "lineWidth": 2,
                    "lineStyle": "Solid"
                }
            ]
        }
    ]
}
```

### Primitive Serialization
```cpp
QJsonObject LinePrimitive::toJson() const {
    QJsonObject obj = DrawingPrimitive::toJson(); // Base properties
    obj["start"] = pointToJson(m_start);
    obj["end"] = pointToJson(m_end);
    return obj;
}

void LinePrimitive::fromJson(const QJsonObject& obj) {
    DrawingPrimitive::fromJson(obj); // Base properties
    m_start = jsonToPoint(obj["start"]);
    m_end = jsonToPoint(obj["end"]);
}
```

---

## Performance Optimizations

### Rendering Optimizations
1. **Frustum Culling**: Don't render off-screen primitives
2. **Level of Detail**: Simplify distant objects
3. **Batch Rendering**: Group similar primitives
4. **VBO Usage**: Use Vertex Buffer Objects for complex shapes

### Memory Optimizations
1. **Smart Pointers**: Use `std::unique_ptr` for ownership
2. **Move Semantics**: Avoid unnecessary copies
3. **Image Caching**: Cache scaled/processed images
4. **Lazy Loading**: Load images on demand

### AI Optimizations
1. **Image Downscaling**: Scale large images before SAM2
2. **Mask Caching**: Cache generated masks
3. **Async Processing**: Run SAM2 in background thread
4. **Connection Pooling**: Reuse HTTP connections

---

## Error Handling

### Common Error Patterns
```cpp
// 1. Null pointer checks
if (!primitive) {
    qDebug() << "Error: Null primitive";
    return;
}

// 2. Bounds checking
if (index < 0 || index >= m_primitives.size()) {
    qDebug() << "Error: Index out of bounds";
    return;
}

// 3. Resource validation
if (image.isNull()) {
    QMessageBox::warning(this, "Error", "Failed to load image");
    return;
}

// 4. Network errors
connect(reply, &QNetworkReply::errorOccurred, [](QNetworkReply::NetworkError error) {
    qDebug() << "Network error:" << error;
});
```

---

## Debug Output

### Debug Macros
```cpp
#define DEBUG_TOOLS 1
#define DEBUG_RENDERING 0
#define DEBUG_COMMANDS 1

#if DEBUG_TOOLS
    qDebug() << "Tool changed to:" << toolName;
#endif
```

### Useful Debug Points
- Tool switching
- Primitive creation/deletion
- Command execution/undo
- Layer operations
- AI request/response
- Mouse events
- Rendering pipeline

---

## Extension Points

### Adding New Tool
1. Add enum value to `DrawingTool`
2. Create tool handler in `DrawingCanvas`
3. Add to `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`
4. Add toolbar button in `MainWindow`
5. Add keyboard shortcut

### Adding New Primitive
1. Inherit from `DrawingPrimitive`
2. Implement `render()`, `toJson()`, `fromJson()`
3. Add to `PrimitiveType` enum
4. Add to `DrawingPrimitive::createFromJson()`
5. Create corresponding tool

### Adding New Command
1. Inherit from `Command`
2. Implement `execute()`, `undo()`, `description()`
3. Optionally implement `canMergeWith()`, `mergeWith()`
4. Use in appropriate operation

---

## Testing Strategies

### Unit Testing
- Test primitive creation
- Test coordinate conversion
- Test JSON serialization
- Test command undo/redo

### Integration Testing
- Test tool workflows
- Test layer operations
- Test AI features end-to-end
- Test save/load

### Manual Testing
- Draw complex scenes
- Test all tools
- Test undo/redo extensively
- Test AI with various images
- Test performance with many objects

---

*This technical reference provides deep implementation details for the LLM assistant to answer advanced questions about DrawingStudio's architecture and code.*
