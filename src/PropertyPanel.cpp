#include "PropertyPanel.h"
#include "GuitarComponent.h"
#include "DrawingPrimitive.h"
#include <QColorDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>

// ColorButton Implementation
ColorButton::ColorButton(const QColor& color, QWidget* parent)
    : QPushButton(parent), m_color(color)
{
    setFixedSize(60, 25);
    updateStyle();
}

void ColorButton::setColor(const QColor& color)
{
    if (m_color != color) {
        m_color = color;
        updateStyle();
        emit colorChanged(color);
    }
}

void ColorButton::updateStyle()
{
    setStyleSheet(QString("ColorButton { background-color: %1; border: 1px solid #666; border-radius: 3px; }")
                  .arg(m_color.name()));
}

void ColorButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect colorRect = rect().adjusted(2, 2, -2, -2);
    painter.fillRect(colorRect, m_color);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(colorRect);
}

void ColorButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QColor newColor = QColorDialog::getColor(m_color, this, "Select Color");
        if (newColor.isValid()) {
            setColor(newColor);
        }
    }
    QPushButton::mousePressEvent(event);
}

// PropertyPanel Implementation
PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentComponent(nullptr)
    , m_currentPrimitive(nullptr)
{
    setupUI();
}

void PropertyPanel::setupUI()
{
    setFixedWidth(250);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Title
    QLabel* titleLabel = new QLabel("Properties");
    titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; margin-bottom: 10px; }");
    mainLayout->addWidget(titleLabel);
    
    // Scroll area for properties
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(5, 5, 5, 5);
    m_contentLayout->setSpacing(5);
    
    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);
    
    // Initial message
    QLabel* emptyLabel = new QLabel("Select an object to view properties");
    emptyLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();
}

void PropertyPanel::showComponentProperties(GuitarComponent* component)
{
    clearLayout();
    m_currentComponent = component;
    m_currentPrimitive = nullptr;
    
    if (!component) {
        return;
    }
    
    m_currentObjectName = component->name();
    
    // Component info group
    addGroup("Component Info");
    
    addPropertyEditor("name", "Name", QMetaType::QString, component->name());
    QString componentTypeName = "Unknown";
    switch(component->type()) {
        case ComponentType::Body: componentTypeName = "Body"; break;
        case ComponentType::Neck: componentTypeName = "Neck"; break;
        case ComponentType::Headstock: componentTypeName = "Headstock"; break;
        case ComponentType::Pickup: componentTypeName = "Pickup"; break;
        case ComponentType::Bridge: componentTypeName = "Bridge"; break;
        case ComponentType::Tuner: componentTypeName = "Tuner"; break;
        case ComponentType::Nut: componentTypeName = "Nut"; break;
        case ComponentType::Fret: componentTypeName = "Fret"; break;
        case ComponentType::Inlay: componentTypeName = "Inlay"; break;
        case ComponentType::SoundHole: componentTypeName = "Sound Hole"; break;
        case ComponentType::Electronics: componentTypeName = "Electronics"; break;
        case ComponentType::Custom: componentTypeName = "Custom"; break;
    }
    addPropertyEditor("type", "Type", QMetaType::QString, componentTypeName);
    
    // Transform group
    addGroup("Transform");
    
    QVector2D pos = component->position();
    addPropertyEditor("posX", "X Position", QMetaType::Double, pos.x());
    addPropertyEditor("posY", "Y Position", QMetaType::Double, pos.y());
    addPropertyEditor("rotation", "Rotation", QMetaType::Double, component->rotation());
    
    QVector2D scale = component->scale();
    addPropertyEditor("scaleX", "Scale X", QMetaType::Double, scale.x());
    addPropertyEditor("scaleY", "Scale Y", QMetaType::Double, scale.y());
    
    // Visibility
    addPropertyEditor("visible", "Visible", QMetaType::Bool, component->isVisible());
    
    // Component-specific properties
    addGroup("Component Properties");
    
    if (auto pickup = dynamic_cast<PickupComponent*>(component)) {
        addPropertyEditor("pickupType", "Pickup Type", QMetaType::Int, 
                          static_cast<int>(pickup->pickupType()));
        addPropertyEditor("color", "Color", QMetaType::QColor, pickup->color());
    }
    else if (auto bridge = dynamic_cast<BridgeComponent*>(component)) {
        addPropertyEditor("bridgeType", "Bridge Type", QMetaType::Int, 
                          static_cast<int>(bridge->bridgeType()));
        addPropertyEditor("stringCount", "String Count", QMetaType::Int, bridge->stringCount());
    }
    else if (auto tuner = dynamic_cast<TunerComponent*>(component)) {
        addPropertyEditor("tunerType", "Tuner Type", QMetaType::Int, 
                          static_cast<int>(tuner->tunerType()));
        addPropertyEditor("tunerCount", "Tuner Count", QMetaType::Int, tuner->tunerCount());
    }
    
    m_contentLayout->addStretch();
}

void PropertyPanel::showPromotedComponentProperties(DrawingPrimitive* primitive, const QString& componentType, const QString& componentName)
{
    clearLayout();
    m_currentComponent = nullptr;
    m_currentPrimitive = primitive;
    
    if (!primitive) {
        return;
    }
    
    m_currentObjectName = componentName;
    
    // Component info group
    addGroup("Guitar Component Info");
    
    addPropertyEditor("name", "Component Name", QMetaType::QString, componentName);
    addPropertyEditor("type", "Component Type", QMetaType::QString, componentType);
    
    // Get database information for this component
    QString manufacturer = "Unknown";
    QString model = "Unknown";
    QString description = "No database information available";
    QString dimensions = "Not specified";
    QString weight = "Not specified";
    QString material = "Not specified";
    QString priceRange = "Not specified";
    
    // Parse componentType to extract type information
    if (componentType.contains("Body") || componentType.contains("Neck") || componentType.contains("Headstock")) {
        // For main parts, we can access the database
        // This would be better implemented by connecting to the DrawingCanvas to get the ComponentInfo
        addGroup("Database Properties");
        addPropertyEditor("manufacturer", "Manufacturer", QMetaType::QString, "Gibson, Fender, PRS, etc.");
        addPropertyEditor("description", "Description", QMetaType::QString, "Click 'Open Database Properties' to select specific model");
        
        // Add a button to open the detailed database dialog
        QPushButton* dbButton = new QPushButton("Open Database Properties");
        connect(dbButton, &QPushButton::clicked, [this, componentType, componentName]() {
            qDebug() << "Opening database properties for" << componentName;
            // This will trigger the detailed component properties dialog
            emit propertyChanged(componentName, "openDatabase", componentType);
        });
        m_contentLayout->addWidget(dbButton);
    }
    
    // Transform group (showing primitive properties)
    addGroup("Transform & Visual Properties");
    
    QRectF bounds = primitive->boundingRect();
    addPropertyEditor("centerX", "Center X", QMetaType::Double, bounds.center().x());
    addPropertyEditor("centerY", "Center Y", QMetaType::Double, bounds.center().y());
    addPropertyEditor("width", "Width", QMetaType::Double, bounds.width());
    addPropertyEditor("height", "Height", QMetaType::Double, bounds.height());
    
    // Color
    addPropertyEditor("color", "Color", QMetaType::QColor, primitive->color());
    addPropertyEditor("lineWidth", "Line Width", QMetaType::Double, primitive->lineWidth());
    addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());
    
    m_contentLayout->addStretch();
}

void PropertyPanel::showPrimitiveProperties(DrawingPrimitive* primitive)
{
    clearLayout();
    m_currentComponent = nullptr;
    m_currentPrimitive = primitive;
    
    if (!primitive) {
        return;
    }
    
    m_currentObjectName = "Primitive";
    
    // Object identification group
    addGroup("Object Information");
    
    QString primitiveTypeName = "Unknown";
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
    }
    addPropertyEditor("type", "Object Type", QMetaType::QString, primitiveTypeName);
    
    // Appearance group
    addGroup("Appearance");
    addPropertyEditor("visible", "Visible", QMetaType::Bool, primitive->isVisible());
    addPropertyEditor("color", "Color", QMetaType::QColor, primitive->color());
    addPropertyEditor("lineWidth", "Line Width (px)", QMetaType::Double, primitive->lineWidth());
    
    // Type-specific properties
    if (auto line = dynamic_cast<LinePrimitive*>(primitive)) {
        addGroup("Line Properties");
        QVector2D start = line->startPoint();
        QVector2D end = line->endPoint();
        QVector2D delta = end - start;
        float length = delta.length();
        float angle = atan2(delta.y(), delta.x()) * 180.0f / M_PI;
        
        addPropertyEditor("length", "Length (mm)", QMetaType::Double, length);
        addPropertyEditor("angle", "Angle (°)", QMetaType::Double, angle);
    }
    else if (auto rect = dynamic_cast<RectanglePrimitive*>(primitive)) {
        addGroup("Rectangle Properties");
        QVector2D tl = rect->topLeft();
        QVector2D br = rect->bottomRight();
        float width = abs(br.x() - tl.x());
        float height = abs(br.y() - tl.y());
        
        addPropertyEditor("width", "Width (mm)", QMetaType::Double, width);
        addPropertyEditor("height", "Height (mm)", QMetaType::Double, height);
        addPropertyEditor("filled", "Filled", QMetaType::Bool, rect->filled());
        addPropertyEditor("cornerRadius", "Corner Radius (mm)", QMetaType::Double, rect->cornerRadius());
        addPropertyEditor("maintainAspectRatio", "Lock Aspect Ratio", QMetaType::Bool, rect->maintainAspectRatio());
        addPropertyEditor("centerOnResize", "Center on Resize", QMetaType::Bool, rect->centerOnResize());
        addPropertyEditor("convertToRoundedRect", "Rounded Corners", QMetaType::Bool, rect->cornerRadius() > 0.0f);
    }
    else if (auto ellipse = dynamic_cast<EllipsePrimitive*>(primitive)) {
        addGroup("Ellipse Properties");
        float radiusX = ellipse->radiusX();
        float radiusY = ellipse->radiusY();
        
        addPropertyEditor("radiusX", "Radius X (mm)", QMetaType::Double, radiusX);
        addPropertyEditor("radiusY", "Radius Y (mm)", QMetaType::Double, radiusY);
        addPropertyEditor("filled", "Filled", QMetaType::Bool, ellipse->filled());
        addPropertyEditor("makeCircle", "Force Circular", QMetaType::Bool, radiusX == radiusY);
        addPropertyEditor("lockAspectRatio", "Lock Aspect Ratio", QMetaType::Bool, ellipse->lockAspectRatio());
        addPropertyEditor("showAxes", "Show Axes", QMetaType::Bool, ellipse->showAxes());
        addPropertyEditor("subdivisions", "Curve Quality", QMetaType::Int, ellipse->subdivisions());
    }
    else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive)) {
        addGroup("Bézier Curve Properties");
        
        const auto& controlPoints = bezier->controlPoints();
        
        if (controlPoints.size() >= 2) {
            QVector2D start = controlPoints.front();
            QVector2D end = controlPoints.back();
            float directDistance = (end - start).length();
            addPropertyEditor("directDistance", "Direct Distance (mm)", QMetaType::Double, directDistance);
        }
        
        addPropertyEditor("showControlLines", "Show Control Lines", QMetaType::Bool, bezier->showControlLines());
        addPropertyEditor("autoTangents", "Auto Tangents", QMetaType::Bool, bezier->autoTangents());
        addPropertyEditor("symmetricHandles", "Symmetric Handles", QMetaType::Bool, bezier->symmetricHandles());
        addPropertyEditor("subdivisionLevel", "Subdivision Quality", QMetaType::Int, bezier->subdivisionLevel());
    }
    else if (auto spline = dynamic_cast<SplinePrimitive*>(primitive)) {
        addGroup("Spline Properties");
        
        addPropertyEditor("smoothness", "Smoothness", QMetaType::Double, spline->smoothness());
        addPropertyEditor("closed", "Close Curve", QMetaType::Bool, spline->isClosed());
        addPropertyEditor("filled", "Fill Shape", QMetaType::Bool, spline->filled());
        addPropertyEditor("interpolationType", "Interpolation", QMetaType::Int, spline->interpolationType());
        addPropertyEditor("tension", "Curve Tension", QMetaType::Double, spline->tension());
        addPropertyEditor("showPoints", "Show Control Points", QMetaType::Bool, spline->showPoints());
        addPropertyEditor("autoSmooth", "Auto Smooth", QMetaType::Bool, spline->autoSmooth());
    }
    else if (auto curve = dynamic_cast<CurvePrimitive*>(primitive)) {
        addGroup("Curve Properties");
        
        addPropertyEditor("closed", "Close Curve", QMetaType::Bool, curve->isClosed());
        addPropertyEditor("curveType", "Curve Type", QMetaType::Int, curve->curveType());
        addPropertyEditor("showControlPolygon", "Show Control Polygon", QMetaType::Bool, curve->showControlPolygon());
    }
    else if (auto arc = dynamic_cast<ArcPrimitive*>(primitive)) {
        addGroup("Arc Properties");
        float radius = arc->radius();
        float startAngle = arc->startAngle();
        float endAngle = arc->endAngle();
        
        addPropertyEditor("radius", "Radius (mm)", QMetaType::Double, radius);
        addPropertyEditor("startAngle", "Start Angle (°)", QMetaType::Double, startAngle);
        addPropertyEditor("endAngle", "End Angle (°)", QMetaType::Double, endAngle);
        addPropertyEditor("makeFullCircle", "Convert to Circle", QMetaType::Bool, false);
        addPropertyEditor("reverseDirection", "Reverse Direction", QMetaType::Bool, false);
    }
    else if (auto circle = dynamic_cast<CirclePrimitive*>(primitive)) {
        addGroup("Circle Properties");
        float radius = circle->radius();
        float diameter = 2 * radius;
        
        addPropertyEditor("radius", "Radius (mm)", QMetaType::Double, radius);
        addPropertyEditor("diameter", "Diameter (mm)", QMetaType::Double, diameter);
        addPropertyEditor("filled", "Filled", QMetaType::Bool, circle->filled());
        addPropertyEditor("showCenterPoint", "Show Center Point", QMetaType::Bool, circle->showCenterPoint());
        addPropertyEditor("showQuadrants", "Show Quadrants", QMetaType::Bool, circle->showQuadrants());
    }
    else if (auto polygon = dynamic_cast<PolygonPrimitive*>(primitive)) {
        addGroup("Polygon Properties");
        
        addPropertyEditor("closed", "Close Shape", QMetaType::Bool, polygon->isClosed());
        addPropertyEditor("filled", "Fill Shape", QMetaType::Bool, polygon->filled());
    }
    else if (auto dimension = dynamic_cast<DimensionPrimitive*>(primitive)) {
        addGroup("Dimension Properties");
        
        float measurementValue = dimension->getMeasurementValue();
        
        addPropertyEditor("measurementValue", "Measured Value", QMetaType::Double, measurementValue);
        addPropertyEditor("unitsString", "Units", QMetaType::QString, dimension->getUnitsString());
    }
    
    m_contentLayout->addStretch();
}

void PropertyPanel::clearProperties()
{
    clearLayout();
    m_currentComponent = nullptr;
    m_currentPrimitive = nullptr;
    
    QLabel* emptyLabel = new QLabel("Select an object to view properties");
    emptyLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();
}

void PropertyPanel::clearLayout()
{
    // Clear existing layout
    QLayoutItem* child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_propertyEditors.clear();
}

void PropertyPanel::addGroup(const QString& title)
{
    QLabel* groupLabel = new QLabel(title);
    groupLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; margin-top: 10px; margin-bottom: 5px; }");
    m_contentLayout->addWidget(groupLabel);
}

void PropertyPanel::addSeparator()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("QFrame { color: #ccc; }");
    m_contentLayout->addWidget(line);
}

void PropertyPanel::addPropertyEditor(const QString& propertyName, 
                                     const QString& displayName,
                                     QMetaType::Type type, const QVariant& value)
{
    QWidget* editor = createEditor(propertyName, type, value);
    if (!editor) return;
    
    QHBoxLayout* layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* label = new QLabel(displayName + ":");
    label->setMinimumWidth(80);
    label->setStyleSheet("QLabel { color: #555; }");
    
    layout->addWidget(label);
    layout->addWidget(editor, 1);
    
    QWidget* container = new QWidget();
    container->setLayout(layout);
    m_contentLayout->addWidget(container);
    
    m_propertyEditors[propertyName] = editor;
}

QWidget* PropertyPanel::createEditor(const QString& propertyName, QMetaType::Type type, const QVariant& value)
{
    // Check if this is a readonly/calculated property
    bool isReadonly = (propertyName == "type");
    
    switch (type) {
        case QMetaType::QString: {
            QLineEdit* edit = new QLineEdit();
            edit->setText(value.toString());
            if (isReadonly) {
                edit->setReadOnly(true);
                edit->setStyleSheet("QLineEdit { background-color: #f0f0f0; color: #666; }");
            } else {
                connect(edit, &QLineEdit::textChanged, this, &PropertyPanel::onPropertyValueChanged);
            }
            return edit;
        }
        
        case QMetaType::Int: {
            QSpinBox* spin = new QSpinBox();
            
            // Set appropriate ranges based on property type
            if (propertyName.contains("Count", Qt::CaseInsensitive) || 
                propertyName.contains("vertices", Qt::CaseInsensitive) ||
                propertyName.contains("subdivisions", Qt::CaseInsensitive) ||
                propertyName.contains("quality", Qt::CaseInsensitive)) {
                // Count properties - must be positive
                spin->setRange(1, 1000);
            } else if (propertyName.contains("precision", Qt::CaseInsensitive)) {
                // Precision for measurements
                spin->setRange(0, 10);
            } else {
                // Default range
                spin->setRange(-10000, 10000);
            }
            
            spin->setValue(value.toInt());
            
            if (isReadonly) {
                spin->setReadOnly(true);
                spin->setStyleSheet("QSpinBox { background-color: #f0f0f0; color: #666; }");
            } else {
                connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), 
                       this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Double: {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            
            // Set appropriate ranges based on property type
            if (propertyName.contains("radius") || propertyName.contains("Width") || 
                propertyName.contains("Height") || propertyName.contains("length", Qt::CaseInsensitive)) {
                // Geometric properties - must be positive
                spin->setRange(0.1, 10000.0);
            } else if (propertyName.contains("angle", Qt::CaseInsensitive)) {
                // Angle properties
                spin->setRange(-360.0, 360.0);
            } else if (propertyName.contains("opacity", Qt::CaseInsensitive) || 
                      propertyName.contains("smoothness", Qt::CaseInsensitive) ||
                      propertyName.contains("tension", Qt::CaseInsensitive)) {
                // Normalized properties (0-1 or 0-2)
                spin->setRange(0.0, 2.0);
            } else {
                // Default range
                spin->setRange(-10000.0, 10000.0);
            }
            
            spin->setDecimals(2);
            spin->setValue(value.toDouble());
            
            if (isReadonly) {
                spin->setReadOnly(true);
                spin->setStyleSheet("QDoubleSpinBox { background-color: #f0f0f0; color: #666; }");
            } else {
                connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
                       this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Bool: {
            QCheckBox* check = new QCheckBox();
            check->setChecked(value.toBool());
            connect(check, &QCheckBox::toggled, this, &PropertyPanel::onPropertyValueChanged);
            return check;
        }
        
        case QMetaType::QColor: {
            ColorButton* colorBtn = new ColorButton(value.value<QColor>());
            connect(colorBtn, &ColorButton::colorChanged, this, &PropertyPanel::onPropertyValueChanged);
            return colorBtn;
        }
        
        default:
            return nullptr;
    }
}

void PropertyPanel::showMultiplePrimitiveProperties(const std::vector<DrawingPrimitive*>& primitives)
{
    clearLayout();
    m_currentComponent = nullptr;
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

void PropertyPanel::refreshCurrentProperties()
{
    // Refresh the current object's properties without changing selection
    if (m_currentComponent) {
        // Re-show component properties to refresh values
        GuitarComponent* component = m_currentComponent;
        m_currentComponent = nullptr; // Reset to force refresh
        showComponentProperties(component);
    }
    else if (m_currentPrimitive) {
        // Re-show primitive properties to refresh values
        DrawingPrimitive* primitive = m_currentPrimitive;
        m_currentPrimitive = nullptr; // Reset to force refresh
        showPrimitiveProperties(primitive);
    }
    else if (!m_currentPrimitives.empty()) {
        // Re-show multiple primitive properties
        auto primitives = m_currentPrimitives;
        m_currentPrimitives.clear();
        showMultiplePrimitiveProperties(primitives);
    }
}

void PropertyPanel::onPropertyValueChanged()
{
    QObject* sender = this->sender();
    QString propertyName;
    QVariant value;
    
    // Find the property name
    for (auto it = m_propertyEditors.begin(); it != m_propertyEditors.end(); ++it) {
        if (it.value() == sender) {
            propertyName = it.key();
            break;
        }
    }
    
    if (propertyName.isEmpty()) return;
    
    // Get the value based on widget type
    if (auto edit = qobject_cast<QLineEdit*>(sender)) {
        value = edit->text();
    }
    else if (auto spin = qobject_cast<QSpinBox*>(sender)) {
        value = spin->value();
    }
    else if (auto spin = qobject_cast<QDoubleSpinBox*>(sender)) {
        value = spin->value();
    }
    else if (auto check = qobject_cast<QCheckBox*>(sender)) {
        value = check->isChecked();
    }
    else if (auto colorBtn = qobject_cast<ColorButton*>(sender)) {
        value = colorBtn->color();
    }
    
    emit propertyChanged(m_currentObjectName, propertyName, value);
    
    qDebug() << "Property changed:" << propertyName << "=" << value;
}

