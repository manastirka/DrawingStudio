# Mask Selection & Contour Editing Implementation Plan

## Overview
Add two major features:
1. **Mask Selection UI** - Browse and select from multiple SAM2 candidates
2. **Contour Editing** - Bezier-style editing with control points

## Current Status ✅
- SAM2 service now returns **top 10 candidate masks** (sorted by quality)
- Each mask includes: contour points, quality score, area, stability
- Stencil buffer rendering working (no rays)
- Smooth contours with Gaussian filtering

## Phase 1: Mask Selection UI

### Backend (DONE ✅)
- Modified `sam2_service.py` to return top 10 masks instead of just best
- Sorted by quality score (stability * iou * centrality)
- Each result includes:
  ```json
  {
    "id": 0,
    "contour": [[x1,y1], [x2,y2], ...],
    "score": 0.95,
    "stability": 0.97,
    "predicted_iou": 0.98,
    "area_percent": 25.3
  }
  ```

### Frontend (TODO)

#### 1. Store All Candidates in ImagePrimitive
```cpp
// ImagePrimitive.h
struct MaskCandidate {
    int id;
    QVector<QVector2D> contour;
    float score;
    float stability;
    float area_percent;
};

class ImagePrimitive {
private:
    QVector<MaskCandidate> m_maskCandidates;  // All candidates
    int m_selectedMaskIndex = 0;               // Currently selected
    
public:
    void setMaskCandidates(const QVector<MaskCandidate>& candidates);
    void selectMask(int index);
    int getMaskCount() const { return m_maskCandidates.size(); }
};
```

#### 2. Parse Multiple Masks from JSON Response
```cpp
// MainWindow.cpp or ImagePrimitive.cpp
void ImagePrimitive::handleSAM2Response(const QJsonObject& response) {
    QJsonArray objects = response["objects"].toArray();
    
    m_maskCandidates.clear();
    for (const QJsonValue& obj : objects) {
        MaskCandidate candidate;
        candidate.id = obj["id"].toInt();
        candidate.score = obj["score"].toDouble();
        candidate.stability = obj["stability"].toDouble();
        candidate.area_percent = obj["area_percent"].toDouble();
        
        // Parse contour points
        QJsonArray contour = obj["contour"].toArray();
        for (const QJsonValue& point : contour) {
            QJsonArray pt = point.toArray();
            candidate.contour.append(QVector2D(pt[0].toDouble(), pt[1].toDouble()));
        }
        
        m_maskCandidates.append(candidate);
    }
    
    // Select best (first) by default
    if (!m_maskCandidates.isEmpty()) {
        selectMask(0);
    }
}
```

#### 3. Add UI Controls for Mask Selection

**Option A: Slider + Info Display**
```cpp
// In MainWindow or a new MaskSelectorWidget
QSlider* maskSelector = new QSlider(Qt::Horizontal);
maskSelector->setRange(0, maskCount - 1);
maskSelector->setValue(0);

QLabel* maskInfo = new QLabel();
maskInfo->setText(QString("Mask %1/%2 - Quality: %3, Area: %4%")
    .arg(index + 1)
    .arg(total)
    .arg(score, 0, 'f', 2)
    .arg(area, 0, 'f', 1));

connect(maskSelector, &QSlider::valueChanged, [this](int index) {
    if (selectedPrimitive && selectedPrimitive->type() == ImagePrimitive) {
        static_cast<ImagePrimitive*>(selectedPrimitive)->selectMask(index);
        canvas->update();
    }
});
```

**Option B: Previous/Next Buttons**
```cpp
QPushButton* prevMask = new QPushButton("◀ Previous Mask");
QPushButton* nextMask = new QPushButton("Next Mask ▶");

connect(prevMask, &QPushButton::clicked, [this]() {
    int current = imagePrimitive->getSelectedMaskIndex();
    if (current > 0) {
        imagePrimitive->selectMask(current - 1);
    }
});
```

**Option C: Thumbnail Preview Grid**
```cpp
// Show small preview of each mask
QGridLayout* maskGrid = new QGridLayout();
for (int i = 0; i < candidates.size(); ++i) {
    QLabel* thumbnail = createMaskThumbnail(candidates[i]);
    thumbnail->setClickable(true);
    connect(thumbnail, &QLabel::clicked, [this, i]() {
        imagePrimitive->selectMask(i);
    });
    maskGrid->addWidget(thumbnail, i / 3, i % 3);
}
```

#### 4. Keyboard Shortcuts
```cpp
// In MainWindow::keyPressEvent
case Qt::Key_BracketLeft:  // [
    selectPreviousMask();
    break;
case Qt::Key_BracketRight: // ]
    selectNextMask();
    break;
```

## Phase 2: Contour Editing with Control Points

### Data Structure
```cpp
// ImagePrimitive.h
struct ControlPoint {
    QVector2D position;
    bool isSelected = false;
    int index;
};

class ImagePrimitive {
private:
    QVector<ControlPoint> m_controlPoints;
    bool m_editMode = false;
    int m_draggedPointIndex = -1;
    
public:
    void enterEditMode();
    void exitEditMode();
    void addControlPoint(const QVector2D& pos);
    void removeControlPoint(int index);
    void moveControlPoint(int index, const QVector2D& newPos);
    void updateContourFromControlPoints();
};
```

### Rendering Control Points
```cpp
// ImagePrimitive.cpp
void ImagePrimitive::drawControlPoints() {
    if (!m_editMode) return;
    
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    
    for (const auto& cp : m_controlPoints) {
        if (cp.isSelected) {
            glColor3f(1.0f, 0.0f, 0.0f);  // Red for selected
        } else {
            glColor3f(0.0f, 1.0f, 1.0f);  // Cyan for unselected
        }
        
        float y_flipped = m_size.y() - (cp.position.y() * scaleY);
        glVertex2f(cp.position.x() * scaleX, y_flipped);
    }
    
    glEnd();
    
    // Draw lines connecting points
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
    for (const auto& cp : m_controlPoints) {
        float y_flipped = m_size.y() - (cp.position.y() * scaleY);
        glVertex2f(cp.position.x() * scaleX, y_flipped);
    }
    glEnd();
}
```

### Mouse Interaction
```cpp
// DrawingCanvas.cpp
void DrawingCanvas::mousePressEvent(QMouseEvent* event) {
    if (m_selectedPrimitive && m_selectedPrimitive->isInEditMode()) {
        QVector2D clickPos = screenToWorld(event->pos());
        
        // Check if clicked on a control point
        int pointIndex = m_selectedPrimitive->findControlPointAt(clickPos, 10.0f);
        
        if (pointIndex >= 0) {
            // Start dragging
            m_selectedPrimitive->startDraggingPoint(pointIndex);
        } else if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl+Click: Add new control point
            m_selectedPrimitive->addControlPoint(clickPos);
        }
    }
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_selectedPrimitive && m_selectedPrimitive->isDraggingPoint()) {
        QVector2D newPos = screenToWorld(event->pos());
        m_selectedPrimitive->moveControlPoint(newPos);
        update();
    }
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (m_selectedPrimitive) {
        m_selectedPrimitive->stopDraggingPoint();
    }
}

void DrawingCanvas::keyPressEvent(QKeyEvent* event) {
    if (m_selectedPrimitive && m_selectedPrimitive->isInEditMode()) {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            m_selectedPrimitive->deleteSelectedControlPoints();
        }
    }
}
```

### Bezier Curve Smoothing
```cpp
// Utility function for Catmull-Rom spline (smooth curve through points)
QVector<QVector2D> ImagePrimitive::generateSmoothCurve(
    const QVector<ControlPoint>& controlPoints, 
    int segmentsPerPoint = 10
) {
    QVector<QVector2D> smoothCurve;
    
    for (int i = 0; i < controlPoints.size(); ++i) {
        QVector2D p0 = controlPoints[(i - 1 + controlPoints.size()) % controlPoints.size()].position;
        QVector2D p1 = controlPoints[i].position;
        QVector2D p2 = controlPoints[(i + 1) % controlPoints.size()].position;
        QVector2D p3 = controlPoints[(i + 2) % controlPoints.size()].position;
        
        for (int j = 0; j < segmentsPerPoint; ++j) {
            float t = float(j) / segmentsPerPoint;
            QVector2D point = catmullRomSpline(p0, p1, p2, p3, t);
            smoothCurve.append(point);
        }
    }
    
    return smoothCurve;
}

QVector2D ImagePrimitive::catmullRomSpline(
    const QVector2D& p0, const QVector2D& p1,
    const QVector2D& p2, const QVector2D& p3,
    float t
) {
    float t2 = t * t;
    float t3 = t2 * t;
    
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}
```

### UI Controls for Edit Mode
```cpp
// Toolbar or context menu
QAction* editContourAction = new QAction("Edit Contour", this);
editContourAction->setCheckable(true);
editContourAction->setShortcut(QKeySequence("E"));

connect(editContourAction, &QAction::toggled, [this](bool checked) {
    if (selectedPrimitive) {
        if (checked) {
            selectedPrimitive->enterEditMode();
        } else {
            selectedPrimitive->exitEditMode();
        }
        canvas->update();
    }
});
```

## Implementation Order

### Week 1: Mask Selection
1. ✅ Backend: Return top 10 masks
2. Store all candidates in ImagePrimitive
3. Add slider UI for mask selection
4. Wire up selection to update display
5. Add keyboard shortcuts ([/])

### Week 2: Basic Control Points
1. Convert contour to control points on edit mode entry
2. Render control points as circles
3. Implement click-to-select point
4. Implement drag-to-move point
5. Update contour when point moves

### Week 3: Advanced Editing
1. Add Catmull-Rom spline smoothing
2. Implement Ctrl+Click to add point
3. Implement Delete key to remove point
4. Add visual feedback (hover, selected states)
5. Add undo/redo for edits

### Week 4: Polish
1. Add point snapping (to grid, other points)
2. Add tangent handles for Bezier control
3. Optimize rendering for many points
4. Add "Reset to Original" button
5. Save edited contours

## Files to Modify

### Backend
- ✅ `sam2_service/sam2_service.py` - Return top 10 masks

### Frontend
- `src/ImagePrimitive.h` - Add control point data structures
- `src/ImagePrimitive.cpp` - Implement editing logic
- `src/DrawingCanvas.h/cpp` - Handle mouse/keyboard for editing
- `src/MainWindow.h/cpp` - Add UI controls for mask selection
- `ui/mainwindow.ui` - Add slider/buttons to UI

## Testing Plan
1. Import image with multiple subjects
2. Use slider to browse through 10 mask candidates
3. Select best mask
4. Enter edit mode (press 'E')
5. Click and drag control points
6. Ctrl+Click to add new points
7. Delete key to remove points
8. Exit edit mode - see smooth updated contour
9. Extract edited selection

## Future Enhancements
- Magnetic lasso (snap to edges)
- AI-assisted refinement (SAM2 with edited contour as input)
- Multiple contours per image (holes, multiple subjects)
- Export contour as SVG path
- Contour library (save/load common shapes)
