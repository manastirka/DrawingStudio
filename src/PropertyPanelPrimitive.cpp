#include "PropertyPanel.h"
#include "DrawingPrimitive.h"
#include "DrawingCanvas.h"
#include "ImagePrimitive.h"
#include "SimpleTextPanel.h"
#include "ClassicTextTool.h"
#include "BrushStrokePrimitive.h"
#include "DrawingTool.h"

#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>
#include <QFontDatabase>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QMap>
#include <functional>
#include <cmath>
#include <algorithm>
#include <vector>

// Primitive / multi / canvas property views (refactor E17).

void PropertyPanel::showPrimitiveProperties(DrawingPrimitive* primitive, DrawingCanvas* canvas)
{
    qDebug() << "\n=== PROPERTY PANEL - SHOW PRIMITIVE PROPERTIES ===";
    qDebug() << "*** Primitive:" << (void*)primitive;
    qDebug() << "*** Primitive Type:" << (primitive ? (int)primitive->type() : -1);
    
    clearLayout();
    m_currentPrimitive = primitive;
    m_currentCanvas = canvas;
    
    if (!primitive) {
        qDebug() << "*** No primitive - returning";
        return;
    }
    
    m_currentObjectName = "Primitive";

    // Group title becomes the primitive type name — skip the redundant readonly "Object Type" row.
    QString primitiveTypeName = "Object";
    switch(primitive->type()) {
        case PrimitiveType::Line: primitiveTypeName = "Line"; break;
        case PrimitiveType::Curve: primitiveTypeName = "Curve"; break;
        case PrimitiveType::BezierCurve: primitiveTypeName = "Bézier Curve"; break;
        case PrimitiveType::Spline: primitiveTypeName = "Spline"; break;
        case PrimitiveType::Arc: primitiveTypeName = "Arc"; break;
        case PrimitiveType::Circle: primitiveTypeName = "Circle"; break;
        case PrimitiveType::Rectangle: primitiveTypeName = "Rectangle"; break;
        case PrimitiveType::Ellipse: primitiveTypeName = "Ellipse"; break;
        case PrimitiveType::Polygon: primitiveTypeName = "Polygon"; break;
        case PrimitiveType::Text: primitiveTypeName = "Text"; break;
        case PrimitiveType::Dimension: primitiveTypeName = "Dimension"; break;
        case PrimitiveType::Image: primitiveTypeName = "Image"; break;
    }
    addGroup(primitiveTypeName);
    
    // Type-specific properties
    if (auto line = dynamic_cast<LinePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Shadow");
        addPropertyEditor("shadowEnabled", "Enabled", QMetaType::Bool, primitive->shadowEnabled());
        addPropertyEditor("shadowColor", "Color", QMetaType::QColor, primitive->shadowColor());
        addPropertyEditor("shadowBlur", "Blur", QMetaType::Double, primitive->shadowBlur());
        addPropertyEditor("shadowOffsetX", "Offset X", QMetaType::Double, primitive->shadowOffsetX());
        addPropertyEditor("shadowOffsetY", "Offset Y", QMetaType::Double, primitive->shadowOffsetY());

        addGroup("Geometry");
        QVector2D start = line->startPoint();
        QVector2D end = line->endPoint();
        QVector2D delta = end - start;
        float length = delta.length();
        float angle = atan2(delta.y(), delta.x()) * 180.0f / M_PI;

        addPropertyEditor("length", "Length", QMetaType::Double, length);
        addPropertyEditor("angle", "Angle (°)", QMetaType::Double, angle);
    }
    else if (auto rect = dynamic_cast<RectanglePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Fill");
        addPropertyEditor("filled", "Enabled", QMetaType::Bool, rect->filled());
        if (rect->filled()) {
            addPropertyEditor("fillColor", "Color", QMetaType::QColor, rect->hasFillColor() ? rect->fillColor() : rect->color());
        }

        addGroup("Shadow");
        addPropertyEditor("shadowEnabled", "Enabled", QMetaType::Bool, primitive->shadowEnabled());
        addPropertyEditor("shadowColor", "Color", QMetaType::QColor, primitive->shadowColor());
        addPropertyEditor("shadowBlur", "Blur", QMetaType::Double, primitive->shadowBlur());
        addPropertyEditor("shadowOffsetX", "Offset X", QMetaType::Double, primitive->shadowOffsetX());
        addPropertyEditor("shadowOffsetY", "Offset Y", QMetaType::Double, primitive->shadowOffsetY());

        if (auto* se = qobject_cast<QCheckBox*>(m_propertyEditors.value("shadowEnabled"))) {
            const bool enabled = se->isChecked();
            if (m_propertyEditors.contains("shadowColor")) m_propertyEditors["shadowColor"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowBlur")) m_propertyEditors["shadowBlur"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowOffsetX")) m_propertyEditors["shadowOffsetX"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowOffsetY")) m_propertyEditors["shadowOffsetY"]->setEnabled(enabled);
            connect(se, &QCheckBox::toggled, this, [this](bool on){
                if (m_propertyEditors.contains("shadowColor")) m_propertyEditors["shadowColor"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowBlur")) m_propertyEditors["shadowBlur"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowOffsetX")) m_propertyEditors["shadowOffsetX"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowOffsetY")) m_propertyEditors["shadowOffsetY"]->setEnabled(on);
            });
        }

        addGroup("Geometry");
        QVector2D tl = rect->topLeft();
        QVector2D br = rect->bottomRight();
        float width = abs(br.x() - tl.x());
        float height = abs(br.y() - tl.y());
        
        addPropertyEditor("width", "Width", QMetaType::Double, width);
        addPropertyEditor("height", "Height", QMetaType::Double, height);
        addPropertyEditor("cornerRadius", "Corner Radius", QMetaType::Double, rect->cornerRadius());
        addPropertyEditor("maintainAspectRatio", "Lock Aspect", QMetaType::Bool, rect->maintainAspectRatio());
    }
    else if (auto ellipse = dynamic_cast<EllipsePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Fill");
        addPropertyEditor("filled", "Enabled", QMetaType::Bool, ellipse->filled());
        if (ellipse->filled()) {
            addPropertyEditor("fillColor", "Color", QMetaType::QColor, ellipse->hasFillColor() ? ellipse->fillColor() : ellipse->color());
        }

        addGroup("Shadow");
        addPropertyEditor("shadowEnabled", "Enabled", QMetaType::Bool, primitive->shadowEnabled());
        addPropertyEditor("shadowColor", "Color", QMetaType::QColor, primitive->shadowColor());
        addPropertyEditor("shadowBlur", "Blur", QMetaType::Double, primitive->shadowBlur());
        addPropertyEditor("shadowOffsetX", "Offset X", QMetaType::Double, primitive->shadowOffsetX());
        addPropertyEditor("shadowOffsetY", "Offset Y", QMetaType::Double, primitive->shadowOffsetY());

        if (auto* se = qobject_cast<QCheckBox*>(m_propertyEditors.value("shadowEnabled"))) {
            const bool enabled = se->isChecked();
            if (m_propertyEditors.contains("shadowColor")) m_propertyEditors["shadowColor"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowBlur")) m_propertyEditors["shadowBlur"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowOffsetX")) m_propertyEditors["shadowOffsetX"]->setEnabled(enabled);
            if (m_propertyEditors.contains("shadowOffsetY")) m_propertyEditors["shadowOffsetY"]->setEnabled(enabled);
            connect(se, &QCheckBox::toggled, this, [this](bool on){
                if (m_propertyEditors.contains("shadowColor")) m_propertyEditors["shadowColor"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowBlur")) m_propertyEditors["shadowBlur"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowOffsetX")) m_propertyEditors["shadowOffsetX"]->setEnabled(on);
                if (m_propertyEditors.contains("shadowOffsetY")) m_propertyEditors["shadowOffsetY"]->setEnabled(on);
            });
        }

        addGroup("Geometry");
        float radiusX = ellipse->radiusX();
        float radiusY = ellipse->radiusY();

        addPropertyEditor("radiusX", "Radius X", QMetaType::Double, radiusX);
        addPropertyEditor("radiusY", "Radius Y", QMetaType::Double, radiusY);
        addPropertyEditor("lockAspectRatio", "Lock Aspect", QMetaType::Bool, ellipse->lockAspectRatio());
    }
    else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());
    }
    else if (auto spline = dynamic_cast<SplinePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Shape");
        addPropertyEditor("closed", "Close Curve", QMetaType::Bool, spline->isClosed());
        addPropertyEditor("filled", "Fill", QMetaType::Bool, spline->filled());
        if (spline->filled()) {
            addPropertyEditor("fillColor", "Fill Color", QMetaType::QColor,
                              spline->hasFillColor() ? spline->fillColor() : spline->color());
        }
        addPropertyEditor("smoothness", "Smoothness", QMetaType::Double, spline->smoothness());
        addPropertyEditor("tension", "Tension", QMetaType::Double, spline->tension());
    }
    else if (auto curve = dynamic_cast<CurvePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Shape");
        addPropertyEditor("closed", "Close Curve", QMetaType::Bool, curve->isClosed());
    }
    else if (auto arc = dynamic_cast<ArcPrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Arc");
        float radius = arc->radius();
        float startAngle = arc->startAngle();
        float endAngle = arc->endAngle();

        addPropertyEditor("radius", "Radius (mm)", QMetaType::Double, radius);
        addPropertyEditor("startAngle", "Start Angle (°)", QMetaType::Double, startAngle);
        addPropertyEditor("endAngle", "End Angle (°)", QMetaType::Double, endAngle);

        // These are actions (one-shot), not persistent boolean properties.
        // If we show them as checkboxes they look "broken" because they reset to false.
        {
            QWidget* btn = new QPushButton("Make full circle");
            connect(static_cast<QPushButton*>(btn), &QPushButton::clicked, this, [this]() {
                emit propertyChanged(m_currentObjectName, "makeFullCircle", true);
            });
            // use the same card look as editors
            QHBoxLayout* layout = new QHBoxLayout();
            layout->setContentsMargins(3, 3, 3, 3);
            layout->setSpacing(7);
            QLabel* label = new QLabel("Action:");
            label->setMinimumWidth(70);
            label->setStyleSheet(R"(
                QLabel { color: rgba(231, 234, 240, 0.85); font-weight: 500; font-size: 10px; background: transparent; }
            )");
            layout->addWidget(label);
            layout->addWidget(btn, 1);
            QWidget* container = new QWidget();
            container->setLayout(layout);
            container->setMaximumWidth(QWIDGETSIZE_MAX);
            container->setStyleSheet(R"(
                QWidget {
                    background: rgba(255,255,255,0.03);
                    border: 1px solid rgba(255,255,255,0.08);
                    border-radius: 8px;
                    margin: 2px 0;
                }
                QWidget:hover {
                    background: rgba(255,255,255,0.05);
                    border: 1px solid rgba(138, 180, 255, 0.40);
                }
            )");
            m_contentLayout->addWidget(container);
        }

        {
            QWidget* btn = new QPushButton("Reverse direction");
            connect(static_cast<QPushButton*>(btn), &QPushButton::clicked, this, [this]() {
                emit propertyChanged(m_currentObjectName, "reverseDirection", true);
            });
            QHBoxLayout* layout = new QHBoxLayout();
            layout->setContentsMargins(3, 3, 3, 3);
            layout->setSpacing(7);
            QLabel* label = new QLabel("Action:");
            label->setMinimumWidth(70);
            label->setStyleSheet(R"(
                QLabel { color: rgba(231, 234, 240, 0.85); font-weight: 500; font-size: 10px; background: transparent; }
            )");
            layout->addWidget(label);
            layout->addWidget(btn, 1);
            QWidget* container = new QWidget();
            container->setLayout(layout);
            container->setMaximumWidth(QWIDGETSIZE_MAX);
            container->setStyleSheet(R"(
                QWidget {
                    background: rgba(255,255,255,0.03);
                    border: 1px solid rgba(255,255,255,0.08);
                    border-radius: 8px;
                    margin: 2px 0;
                }
                QWidget:hover {
                    background: rgba(255,255,255,0.05);
                    border: 1px solid rgba(138, 180, 255, 0.40);
                }
            )");
            m_contentLayout->addWidget(container);
        }
    }
    else if (auto circle = dynamic_cast<CirclePrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Fill");
        addPropertyEditor("filled", "Enabled", QMetaType::Bool, circle->filled());
        if (circle->filled()) {
            addPropertyEditor("fillColor", "Color", QMetaType::QColor, circle->hasFillColor() ? circle->fillColor() : circle->color());
        }

        addGroup("Geometry");
        addPropertyEditor("radius", "Radius", QMetaType::Double, circle->radius());
        addPropertyEditor("diameter", "Diameter", QMetaType::Double, 2 * circle->radius());
    }
    else if (auto polygon = dynamic_cast<PolygonPrimitive*>(primitive)) {
        addGroup("Appearance");
        addPropertyEditor("color", "Stroke", QMetaType::QColor, primitive->color());
        addPropertyEditor("lineWidth", "Width", QMetaType::Double, primitive->lineWidth());
        addPropertyEditor("lineStyle", "Style", QMetaType::Int, static_cast<int>(primitive->lineStyle()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        addGroup("Shape");
        addPropertyEditor("closed", "Close Shape", QMetaType::Bool, polygon->isClosed());
        addPropertyEditor("filled", "Fill", QMetaType::Bool, polygon->filled());
        if (polygon->filled()) {
            addPropertyEditor("fillColor", "Fill Color", QMetaType::QColor, polygon->hasFillColor() ? polygon->fillColor() : polygon->color());
        }
    }
    else if (auto imagePrim = dynamic_cast<ImagePrimitive*>(primitive)) {
        addGroup("Position");
        addPropertyEditor("imagePositionX", "X", QMetaType::Double, static_cast<double>(imagePrim->position().x()));
        addPropertyEditor("imagePositionY", "Y", QMetaType::Double, static_cast<double>(imagePrim->position().y()));

        addGroup("Size");
        addPropertyEditor("imageWidth", "Width", QMetaType::Double, static_cast<double>(imagePrim->size().x()));
        addPropertyEditor("imageHeight", "Height", QMetaType::Double, static_cast<double>(imagePrim->size().y()));
        addPropertyEditor("maintainAspectRatio", "Lock Aspect", QMetaType::Bool, imagePrim->maintainAspectRatio());

        addGroup("Transform");
        addPropertyEditor("imageRotation", "Rotation (°)", QMetaType::Double, static_cast<double>(imagePrim->rotation()));

        addGroup("Appearance");
        addPropertyEditor("opacity", "Opacity", QMetaType::Double, static_cast<double>(primitive->opacityMultiplier()));
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());

        if (imagePrim->getMaskCandidateCount() > 0 || imagePrim->isEditMode()) {
            addGroup("Mask");
            addPropertyEditor("maskOverlayVisible", "Show Overlay", QMetaType::Bool, imagePrim->isMaskOverlayVisible());
            addPropertyEditor("maskInverted", "Invert", QMetaType::Bool, imagePrim->isMaskInverted());
        }
    }
    else if (auto dimension = dynamic_cast<DimensionPrimitive*>(primitive)) {
        addGroup("Dimension Properties");
        addPropertyEditor("measurementValue", "Measured Value", QMetaType::Double,
                          static_cast<double>(dimension->measuredLength()));
        addPropertyEditor("unitsString", "Units", QMetaType::QString, dimension->getUnitsString());
    }
    else if (auto textPrim = dynamic_cast<TextPrimitive*>(primitive)) {
        qDebug() << "\n=== PROPERTY PANEL - SHOWING TEXT PRIMITIVE ===";
        qDebug() << "*** Text Primitive:" << (void*)textPrim;
        qDebug() << "*** SimpleTextPanel:" << (void*)m_simpleTextPanel;
        
        // Show the SimpleTextPanel for text editing
        if (m_simpleTextPanel) {
            m_simpleTextPanel->show();
            m_contentLayout->addWidget(m_simpleTextPanel);
            
            // Update the panel from the selected primitive
            m_simpleTextPanel->updateFromPrimitive(textPrim);
            
            qDebug() << "*** SimpleTextPanel shown and updated from primitive";
        } else {
            qDebug() << "*** ERROR: No SimpleTextPanel available!";
        }
        
        // Don't add common properties for text - all properties are in SimpleTextPanel
        m_contentLayout->addStretch();
        return;
    }
    
    // (Most primitives add their own Appearance/Shadow/etc. groups above)

    m_contentLayout->addStretch();
}

void PropertyPanel::showMultiplePrimitiveProperties(const std::vector<DrawingPrimitive*>& primitives)
{
    clearLayout();
    m_currentPrimitive = nullptr;
    m_currentPrimitives = primitives;
    
    if (primitives.empty()) {
        clearProperties();
        return;
    }
    
    m_currentObjectName = QString("Multiple Objects (%1)").arg(primitives.size());
    
    // Multi-selection info group
    addGroup("Selection Info");
    QLabel* countLabel = new QLabel(QString("Selected: %1 objects").arg(primitives.size()));
    countLabel->setStyleSheet("QLabel { color: #333; font-style: italic; }");
    m_contentLayout->addWidget(countLabel);
    
    // Find common properties that all selected objects share
    bool allVisible = true;
    bool allSameColor = true;
    bool allSameLineWidth = true;
    
    QColor commonColor = primitives[0]->color();
    float commonLineWidth = primitives[0]->lineWidth();
    
    for (size_t i = 1; i < primitives.size(); ++i) {
        if (primitives[i]->isVisible() != primitives[0]->isVisible()) {
            allVisible = false;
        }
        if (primitives[i]->color() != commonColor) {
            allSameColor = false;
        }
        if (abs(primitives[i]->lineWidth() - commonLineWidth) > 0.01f) {
            allSameLineWidth = false;
        }
    }
    
    // Common properties group
    addGroup("Common Properties");
    
    if (allVisible) {
        addPropertyEditor("visible", "Visible", QMetaType::Bool, primitives[0]->isVisible());
    }
    
    if (allSameColor) {
        addPropertyEditor("color", "Color", QMetaType::QColor, commonColor);
    }
    
    if (allSameLineWidth) {
        addPropertyEditor("lineWidth", "Line Width (px)", QMetaType::Double, commonLineWidth);
    }
    
    // Type-specific common properties
    bool allSameType = true;
    PrimitiveType commonType = primitives[0]->type();
    for (size_t i = 1; i < primitives.size(); ++i) {
        if (primitives[i]->type() != commonType) {
            allSameType = false;
            break;
        }
    }
    
    if (allSameType) {
        QString typeName = "Unknown";
        switch(commonType) {
            case PrimitiveType::Line: typeName = "Lines"; break;
            case PrimitiveType::Rectangle: typeName = "Rectangles"; break;
            case PrimitiveType::Ellipse: typeName = "Ellipses"; break;
            case PrimitiveType::Spline: typeName = "Splines"; break;
            case PrimitiveType::BezierCurve: typeName = "Bézier Curves"; break;
            default: break;
        }
        
        addGroup(QString("%1 Properties").arg(typeName));
        
        if (commonType == PrimitiveType::Rectangle) {
            // Check if all rectangles have same filled state
            bool allFilled = true;
            bool firstFilled = dynamic_cast<RectanglePrimitive*>(primitives[0])->filled();
            for (auto* primitive : primitives) {
                auto* rect = dynamic_cast<RectanglePrimitive*>(primitive);
                if (rect && rect->filled() != firstFilled) {
                    allFilled = false;
                    break;
                }
            }
            if (allFilled) {
                addPropertyEditor("filled", "Filled", QMetaType::Bool, firstFilled);
            }
        }
        else if (commonType == PrimitiveType::Spline) {
            // Check spline common properties
            bool allClosed = true;
            bool allFilled = true;
            bool allSameSmoothness = true;
            
            auto* firstSpline = dynamic_cast<SplinePrimitive*>(primitives[0]);
            bool firstClosed = firstSpline->isClosed();
            bool firstFilled = firstSpline->filled();
            float firstSmoothness = firstSpline->smoothness();
            
            for (auto* primitive : primitives) {
                auto* spline = dynamic_cast<SplinePrimitive*>(primitive);
                if (spline) {
                    if (spline->isClosed() != firstClosed) allClosed = false;
                    if (spline->filled() != firstFilled) allFilled = false;
                    if (abs(spline->smoothness() - firstSmoothness) > 0.01f) allSameSmoothness = false;
                }
            }
            
            if (allClosed) {
                addPropertyEditor("closed", "Closed", QMetaType::Bool, firstClosed);
            }
            if (allFilled) {
                addPropertyEditor("filled", "Filled", QMetaType::Bool, firstFilled);
            }
            if (allSameSmoothness) {
                addPropertyEditor("smoothness", "Smoothness", QMetaType::Double, firstSmoothness);
            }
        }
    }
    
    m_contentLayout->addStretch();
}

void PropertyPanel::showCanvasProperties(DrawingCanvas* canvas)
{
    clearLayout();
    m_currentPrimitive = nullptr;
    m_currentPrimitives.clear();
    m_currentCanvas = canvas;
    
    if (!canvas) {
        return;
    }
    
    m_currentObjectName = "Canvas";
    
    // Canvas settings group
    addGroup("Canvas Settings");
    
    // Background color
    addPropertyEditor("backgroundColor", "Background Color", QMetaType::QColor, canvas->backgroundColor());
    
    // Paper format selection
    addPropertyEditor("paperFormat", "Paper Format", QMetaType::QString, canvas->paperFormatName());
    
    // Grid settings
    addGroup("Grid Settings");
    addPropertyEditor("gridVisible", "Show Grid", QMetaType::Bool, canvas->isGridVisible());
    addPropertyEditor("gridColor", "Grid Color", QMetaType::QColor, canvas->gridColor());
    addPropertyEditor("gridSize", "Grid Size (mm)", QMetaType::Double, canvas->gridSize());
    addPropertyEditor("snapEnabled", "Snap to Grid", QMetaType::Bool, canvas->isSnapEnabled());
    
    // View settings
    addGroup("View Settings");
    addPropertyEditor("zoomSensitivity", "Zoom Sensitivity", QMetaType::Double, canvas->zoomSensitivity());
    addPropertyEditor("rulersVisible", "Show Rulers", QMetaType::Bool, canvas->areRulersVisible());
    
    m_contentLayout->addStretch();
}

void PropertyPanel::showTextControls()
{
    qDebug() << "*** SHOWING REAL-TIME TEXT CONTROLS ***";
    
    // Clear existing content
    clearLayout();
    m_currentPrimitive = nullptr;
    m_currentPrimitives.clear();
    m_currentObjectName = "TextTool";
    
    // Show the SimpleTextPanel for real-time text shadow controls
    if (m_simpleTextPanel) {
        m_simpleTextPanel->show();
        m_contentLayout->addWidget(m_simpleTextPanel);
        
        // Update the panel from the current text tool
        m_simpleTextPanel->updateFromTool();
        
        qDebug() << "*** SimpleTextPanel shown for real-time text controls";
    } else {
        qDebug() << "*** ERROR: No SimpleTextPanel available for real-time controls!";
    }
}

