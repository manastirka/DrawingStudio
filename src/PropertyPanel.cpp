#include "PropertyPanel.h"
#include "DrawingPrimitive.h"
#include "DrawingCanvas.h"
#include "ImagePrimitive.h"
#include "SimpleTextPanel.h"
#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>
#include <QFontDatabase>

// ColorButton Implementation
ColorButton::ColorButton(const QColor& color, QWidget* parent)
    : QPushButton(parent), m_color(color)
{
    setFixedSize(28, 18);
    setCursor(Qt::PointingHandCursor);
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
    setStyleSheet(QString(R"(
        ColorButton {
            background-color: %1;
            border: 1px solid rgba(255,255,255,0.14);
            border-radius: 5px;
            padding: 0;
        }
        ColorButton:hover {
            border: 1px solid rgba(138, 180, 255, 0.85);
        }
    )").arg(m_color.name()));
}

void ColorButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF colorRect = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);

    // Transparency checker
    if (m_color.alpha() < 255) {
        const int s = 3;
        QColor c1(255, 255, 255, 28);
        QColor c2(0, 0, 0, 28);
        for (int y = (int)colorRect.top(); y < (int)colorRect.bottom(); y += s) {
            for (int x = (int)colorRect.left(); x < (int)colorRect.right(); x += s) {
                const bool odd = ((x / s) + (y / s)) % 2;
                painter.fillRect(QRect(x, y, s, s), odd ? c1 : c2);
            }
        }
    }

    QPainterPath path;
    path.addRoundedRect(colorRect, 3.5, 3.5);
    painter.fillPath(path, m_color);
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
    , m_currentPrimitive(nullptr)
    , m_currentCanvas(nullptr)
    , m_simpleTextPanel(nullptr)
    , m_textTool(nullptr)
{
    setupUI();

    // Create simplified text panel but keep hidden until needed
    m_simpleTextPanel = new SimpleTextPanel(this);
    m_simpleTextPanel->hide();
}

void PropertyPanel::setTextTool(ClassicTextTool* textTool)
{
    m_textTool = textTool;
    if (m_simpleTextPanel) {
        m_simpleTextPanel->setTextTool(textTool);
    }
}

void PropertyPanel::showToolProperties(DrawingTool tool, DrawingCanvas* canvas)
{
    m_currentPrimitive = nullptr;
    m_currentPrimitives.clear();
    m_currentCanvas = canvas;

    clearLayout();
    m_propertyEditors.clear();

    // Title
    QLabel* title = new QLabel("Tool");
    title->setStyleSheet("font-size: 12px; font-weight: 700; margin: 3px 0 8px 0; color: rgba(231,234,240,0.9);");
    m_contentLayout->addWidget(title);

    auto addBool = [&](const QString& key, const QString& label, bool value,
                       std::function<void(bool)> apply) {
        QCheckBox* cb = new QCheckBox(label);
        cb->setChecked(value);
        connect(cb, &QCheckBox::toggled, this, [this, apply](bool v){
            if (apply) apply(v);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(cb);
        m_propertyEditors[key] = cb;
    };

    auto addIntSlider = [&](const QString& key, const QString& label, int minV, int maxV, int value,
                            std::function<void(int)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QSlider* s = new QSlider(Qt::Horizontal);
        s->setRange(minV, maxV);
        s->setValue(value);
        QLabel* v = new QLabel(QString::number(value));
        v->setMinimumWidth(32);
        h->addWidget(l);
        h->addWidget(s, 1);
        h->addWidget(v);
        connect(s, &QSlider::valueChanged, this, [this, v, apply](int nv){
            v->setText(QString::number(nv));
            if (apply) apply(nv);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = s;
    };

    auto addFloatSpin = [&](const QString& key, const QString& label, double minV, double maxV,
                            double step, double value,
                            std::function<void(double)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QDoubleSpinBox* sp = new QDoubleSpinBox();
        sp->setRange(minV, maxV);
        sp->setSingleStep(step);
        sp->setDecimals(1);
        sp->setValue(value);
        h->addWidget(l);
        h->addWidget(sp, 1);
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, apply](double nv){
                    if (apply) apply(nv);
                    if (m_currentCanvas) m_currentCanvas->update();
                });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = sp;
    };

    auto addLineStyleCombo = [&](const QString& key, const QString& label, Qt::PenStyle current,
                                 std::function<void(Qt::PenStyle)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QComboBox* combo = new QComboBox();
        struct Item { const char* name; Qt::PenStyle style; };
        const Item items[] = {
            {"Solid", Qt::SolidLine},
            {"Dash", Qt::DashLine},
            {"Dot", Qt::DotLine},
            {"Dash Dot", Qt::DashDotLine},
            {"Dash Dot Dot", Qt::DashDotDotLine}
        };
        int idx = 0;
        int currentIdx = 0;
        for (const auto& it : items) {
            combo->addItem(it.name, static_cast<int>(it.style));
            if (it.style == current) currentIdx = idx;
            ++idx;
        }
        combo->setCurrentIndex(currentIdx);
        h->addWidget(l);
        h->addWidget(combo, 1);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, combo, apply](int){
                    Qt::PenStyle st = static_cast<Qt::PenStyle>(combo->currentData().toInt());
                    if (apply) apply(st);
                    if (m_currentCanvas) m_currentCanvas->update();
                });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = combo;
    };

    auto addColorButton = [&](const QString& key, const QString& label, const QColor& color,
                              std::function<void(const QColor&)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        ColorButton* btn = new ColorButton(color);
        btn->setMinimumWidth(44);
        h->addWidget(l);
        h->addWidget(btn, 1);
        connect(btn, &ColorButton::colorChanged, this, [this, apply](const QColor& c){
            if (apply) apply(c);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = btn;
    };

    const bool isStrokeTool = (tool == DrawingTool::Line || tool == DrawingTool::Rectangle ||
                               tool == DrawingTool::Ellipse || tool == DrawingTool::Circle ||
                               tool == DrawingTool::Polygon || tool == DrawingTool::Arc ||
                               tool == DrawingTool::Curve || tool == DrawingTool::BezierCurve ||
                               tool == DrawingTool::Spline);

    // Only show Fill controls for tools that actually apply fill on finalize.
    // (Arc + Line don't support fill in the current backend.)
    const bool toolSupportsFill = (tool == DrawingTool::Rectangle || tool == DrawingTool::Ellipse ||
                                   tool == DrawingTool::Circle || tool == DrawingTool::Polygon ||
                                   tool == DrawingTool::Spline);

    if (isStrokeTool && canvas) {
        addGroup("Stroke");
        addColorButton("tool.strokeColor", "Color", canvas->defaultDrawingColor(),
                       [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
        addFloatSpin("tool.strokeWidth", "Width", 0.5, 40.0, 0.5, canvas->defaultLineWidth(),
                     [canvas](double w){ canvas->setDefaultLineWidth(static_cast<float>(w)); });
        addLineStyleCombo("tool.strokeStyle", "Style", canvas->defaultLineStyle(),
                          [canvas](Qt::PenStyle st){ canvas->setDefaultLineStyle(st); });

        if (toolSupportsFill) {
            addGroup("Fill");
            addBool("tool.fillEnabled", "Enabled", canvas->defaultFillEnabled(),
                    [canvas](bool on){ canvas->setDefaultFillEnabled(on); });
            addColorButton("tool.fillColor", "Color", canvas->defaultFillColor(),
                           [canvas](const QColor& c){ canvas->setDefaultFillColor(c); });

            // Disable fill color when fill is off
            if (auto* fe = qobject_cast<QCheckBox*>(m_propertyEditors.value("tool.fillEnabled"))) {
                const bool on = fe->isChecked();
                if (m_propertyEditors.contains("tool.fillColor")) m_propertyEditors["tool.fillColor"]->setEnabled(on);
                connect(fe, &QCheckBox::toggled, this, [this](bool on2){
                    if (m_propertyEditors.contains("tool.fillColor")) m_propertyEditors["tool.fillColor"]->setEnabled(on2);
                });
            }
        }

    } else {
        // Keep non-drawing tools simple and working
        switch (tool) {
        case DrawingTool::Select: {
            addGroup("Options");
            if (canvas && canvas->selectionManager()) {
                SelectionManager* sm = canvas->selectionManager();
                addBool("select.pipetteMode", "Pipette mode", sm->isPipetteMode(),
                        [sm](bool on){ sm->setPipetteMode(on); });
                addIntSlider("select.pipetteTolerance", "Color tolerance", 0, 100,
                             sm->pipetteTolerance(),
                             [sm](int t){ sm->setPipetteTolerance(t); });
            }
            break;
        }
        case DrawingTool::Move: {
            addGroup("Options");
            if (canvas) {
                addBool("move.snap", "Snap to grid", canvas->isSnapEnabled(),
                        [canvas](bool on){ canvas->setSnapEnabled(on); });
                addBool("move.magnetic", "Magnetic connection", canvas->isMagneticConnectionEnabled(),
                        [canvas](bool on){ canvas->setMagneticConnectionEnabled(on); });
            }
            break;
        }
        case DrawingTool::Brush:
        case DrawingTool::Blur: {
            addGroup("Brush");
            if (canvas) {
                addIntSlider("brush.size", "Size", 1, 100,
                             static_cast<int>(canvas->brushSize()),
                             [canvas](int s){ canvas->setBrushSize(s); });
                addIntSlider("brush.hardness", tool == DrawingTool::Blur ? "Strength" : "Hardness",
                             0, 100,
                             static_cast<int>(canvas->brushHardness() * 100.0f),
                             [canvas](int h){ canvas->setBrushHardness(h / 100.0f); });
                addColorButton("brush.color", "Color", canvas->defaultDrawingColor(),
                               [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
            }
            break;
        }
        case DrawingTool::Eraser: {
            addGroup("Eraser");
            if (canvas) {
                addIntSlider("eraser.size", "Size", 1, 100,
                             static_cast<int>(canvas->eraserSize()),
                             [canvas](int s){ canvas->setEraserSize(static_cast<float>(s)); });
            }
            break;
        }
        case DrawingTool::Fill: {
            addGroup("Fill");
            if (canvas) {
                addColorButton("fill.color", "Color", canvas->defaultDrawingColor(),
                               [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
                addBool("fill.splash", "Splash mode",
                        canvas->fillMode() == DrawingCanvas::FillMode::Splash,
                        [canvas](bool on){
                            canvas->setFillMode(on ? DrawingCanvas::FillMode::Splash
                                                   : DrawingCanvas::FillMode::Normal);
                        });
            }
            break;
        }
        case DrawingTool::Image: {
            QLabel* info = new QLabel("Import an image, then select it to edit mask and transform properties.");
            info->setWordWrap(true);
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        case DrawingTool::Measure: {
            QLabel* info = new QLabel("Click-drag to measure distance on the canvas.");
            info->setWordWrap(true);
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        default: {
            QLabel* info = new QLabel("(No basic tool properties)");
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        }
    }

    m_contentLayout->addStretch();
}

void PropertyPanel::setupUI()
{
    // Ensure a comfortable default width; still resizable by the user
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    // macOS-style Light Theme for PropertyPanel
    setStyleSheet(R"(
        PropertyPanel {
            background: #17191c;
            border-left: 1px solid rgba(255,255,255,0.08);
            padding: 0px;
        }

        QScrollArea {
            border: none;
            background: transparent;
            margin: 0px;
        }

        QScrollBar:vertical {
            background: rgba(255,255,255,0.06);
            width: 12px;
            border-radius: 6px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(138, 180, 255, 0.55);
            border-radius: 6px;
            min-height: 24px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(138, 180, 255, 0.75);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: none;
            background: none;
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QLabel {
            color: #e7eaf0;
            font-size: 10px;
        }

        QToolTip {
            background-color: rgba(20, 22, 26, 0.92);
            color: #e7eaf0;
            border: 1px solid rgba(138, 180, 255, 0.25);
            border-radius: 8px;
            padding: 8px 10px;
            font-size: 10px;
        }

        QLineEdit, QComboBox {
            background-color: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 7px;
            font-size: 10px;
        }

        QSpinBox, QDoubleSpinBox {
            background-color: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 10px;
        }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
            border-color: rgba(138, 180, 255, 0.45);
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: rgba(138, 180, 255, 0.85);
            background-color: #252a31;
        }

        QComboBox::drop-down {
            border: none;
            width: 18px;
        }
        QComboBox QAbstractItemView {
            background: #1e2126;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            selection-background-color: rgba(138, 180, 255, 0.22);
        }

        QCheckBox {
            color: #e7eaf0;
            spacing: 8px;
        }

        QSlider::groove:horizontal {
            height: 6px;
            background: rgba(255,255,255,0.10);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            border-radius: 7px;
            background: rgba(138, 180, 255, 0.95);
        }

        QPushButton {
            background: #22252b;
            color: #e7eaf0;
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 6px;
            padding: 5px 9px;
            font-size: 10px;
        }
        QPushButton:hover {
            border-color: rgba(138, 180, 255, 0.55);
            background: #262b33;
        }
        QPushButton:pressed {
            background: #1d2026;
        }


        /* Spinbox buttons are removed programmatically via setButtonSymbols(NoButtons). */
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    // Dock widget already provides a title bar — don't duplicate it here.
    
    // Scroll area for properties
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // Always show scrollbar
    
    m_contentWidget = new QWidget();
    // Let content expand with the dock width (avoid right-side "dead space")
    m_contentWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(10, 10, 10, 10);
    m_contentLayout->setSpacing(6);
    
    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea, 1);
    
    // Initial message: subtle, no placeholder-style card
    QLabel* emptyLabel = new QLabel("No selection");
    emptyLabel->setStyleSheet(R"(
        QLabel {
            color: rgba(231,234,240,0.35);
            font-size: 11px;
            padding: 32px 8px;
            background: transparent;
        }
    )");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();
}


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

void PropertyPanel::clearProperties()
{
    qDebug() << "PropertyPanel::clearProperties called";
    qDebug() << "  m_contentLayout:" << (void*)m_contentLayout;
    qDebug() << "  m_simpleTextPanel:" << (void*)m_simpleTextPanel;
    
    clearLayout();
    m_currentPrimitive = nullptr;
    
    // Hide text tool panel
    if (m_simpleTextPanel) {
        m_simpleTextPanel->hide();
    }
    
    // Safety check before adding widgets
    if (!m_contentLayout) {
        qDebug() << "PropertyPanel::clearProperties - m_contentLayout is null, cannot add empty label!";
        return;
    }
    
    QLabel* emptyLabel = new QLabel("No selection");
    emptyLabel->setStyleSheet(R"(
        QLabel {
            color: rgba(231,234,240,0.35);
            font-size: 11px;
            padding: 32px 8px;
            background: transparent;
        }
    )");
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentLayout->addWidget(emptyLabel);
    m_contentLayout->addStretch();

    qDebug() << "PropertyPanel::clearProperties completed";
}

void PropertyPanel::clearLayout()
{
    // Safety check - ensure layout exists
    if (!m_contentLayout) {
        qDebug() << "PropertyPanel::clearLayout - m_contentLayout is null!";
        return;
    }
    
    // Clear existing layout
    QLayoutItem* child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        QWidget* widget = child->widget();
        
        // Don't delete the text tool panel directly - just remove it from layout
        if (widget && widget != m_simpleTextPanel) {
            delete widget;
        } else if (widget == m_simpleTextPanel && m_simpleTextPanel) {
            m_simpleTextPanel->hide();
        }
        delete child;
    }
    m_propertyEditors.clear();
}

void PropertyPanel::addGroup(const QString& title)
{
    // Subtle top separator so consecutive groups read as distinct sections.
    QFrame* topSep = new QFrame();
    topSep->setFrameShape(QFrame::HLine);
    topSep->setFrameShadow(QFrame::Plain);
    topSep->setStyleSheet(R"(
        QFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 transparent, stop:0.5 rgba(255,255,255,0.09), stop:1 transparent);
            max-height: 1px;
            margin: 8px 0 0 0;
        }
    )");
    m_contentLayout->addWidget(topSep);

    // Wrapper with a subtle accent strip on the left and spacing above
    QWidget* header = new QWidget();
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(R"(
        QWidget {
            background: transparent;
            margin-top: 10px;
        }
    )");

    QHBoxLayout* lay = new QHBoxLayout(header);
    lay->setContentsMargins(0, 6, 0, 4);
    lay->setSpacing(8);

    QWidget* accent = new QWidget();
    accent->setFixedSize(3, 11);
    accent->setStyleSheet(R"(
        QWidget {
            background: rgba(138, 180, 255, 0.8);
            border-radius: 1.5px;
        }
    )");

    QLabel* groupLabel = new QLabel(title.toUpper());
    groupLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 700;
            font-size: 10px;
            color: rgba(231, 234, 240, 0.72);
            letter-spacing: 0.9px;
            background: transparent;
        }
    )");

    lay->addWidget(accent);
    lay->addWidget(groupLabel);
    lay->addStretch();

    m_contentLayout->addWidget(header);
}

void PropertyPanel::addSeparator()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(R"(
        QFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 transparent, stop:0.5 rgba(255,255,255,0.10), stop:1 transparent);
            height: 1px;
            margin: 10px 0;
        }
    )");
    m_contentLayout->addWidget(line);
}

void PropertyPanel::addPropertyEditor(const QString& propertyName,
                                     const QString& displayName,
                                     QMetaType::Type type, const QVariant& value)
{
    QWidget* editor = createEditor(propertyName, type, value);
    if (!editor) return;

    QWidget* row = new QWidget();
    row->setObjectName("propRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setStyleSheet(R"(
        QWidget#propRow {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QWidget#propRow:hover {
            background: rgba(255,255,255,0.035);
        }
    )");

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(10);

    QLabel* label = new QLabel(displayName);
    label->setFixedWidth(86);
    label->setStyleSheet(R"(
        QLabel {
            color: rgba(231, 234, 240, 0.62);
            font-weight: 500;
            font-size: 11px;
            background: transparent;
        }
    )");
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addWidget(label);
    layout->addWidget(editor, 1);

    m_contentLayout->addWidget(row);

    m_propertyEditors[propertyName] = editor;
}

QWidget* PropertyPanel::createEditor(const QString& propertyName, QMetaType::Type type, const QVariant& value)
{
    // Check if this is a readonly/calculated property
    bool isReadonly = (propertyName == "type");

    // Special case: line style editor
    if (propertyName == "lineStyle") {
        QComboBox* combo = new QComboBox();
        struct Item { const char* name; Qt::PenStyle style; };
        const Item items[] = {
            {"Solid", Qt::SolidLine},
            {"Dash", Qt::DashLine},
            {"Dot", Qt::DotLine},
            {"Dash Dot", Qt::DashDotLine},
            {"Dash Dot Dot", Qt::DashDotDotLine}
        };
        int currentIdx = 0;
        for (int i = 0; i < (int)(sizeof(items)/sizeof(items[0])); ++i) {
            combo->addItem(items[i].name, static_cast<int>(items[i].style));
        }
        Qt::PenStyle current = static_cast<Qt::PenStyle>(value.toInt());
        for (int i = 0; i < combo->count(); ++i) {
            if (static_cast<Qt::PenStyle>(combo->itemData(i).toInt()) == current) {
                currentIdx = i;
                break;
            }
        }
        combo->setCurrentIndex(currentIdx);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PropertyPanel::onPropertyValueChanged);
        return combo;
    }

    // Special case for paper format dropdown
    if (propertyName == "paperFormat") {
            QComboBox* combo = new QComboBox();
            combo->addItem("A4 (210×297mm)", static_cast<int>(DrawingCanvas::PaperFormat::A4));
            combo->addItem("A3 (297×420mm)", static_cast<int>(DrawingCanvas::PaperFormat::A3));
            combo->addItem("A2 (420×594mm)", static_cast<int>(DrawingCanvas::PaperFormat::A2));
            combo->addItem("A1 (594×841mm)", static_cast<int>(DrawingCanvas::PaperFormat::A1));
            combo->addItem("A0 (841×1189mm)", static_cast<int>(DrawingCanvas::PaperFormat::A0));
            combo->addItem("Letter (8.5×11in)", static_cast<int>(DrawingCanvas::PaperFormat::Letter));
            combo->addItem("Legal (8.5×14in)", static_cast<int>(DrawingCanvas::PaperFormat::Legal));
            combo->addItem("Tabloid (11×17in)", static_cast<int>(DrawingCanvas::PaperFormat::Tabloid));
            combo->addItem("Custom", static_cast<int>(DrawingCanvas::PaperFormat::Custom));
            
            // Set current value based on display name
            QString currentName = value.toString();
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemText(i) == currentName) {
                    combo->setCurrentIndex(i);
                    break;
                }
            }
            
            // Use panel-global dark styling
            combo->setStyleSheet(QString());
            
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                    this, &PropertyPanel::onPropertyValueChanged);
            return combo;
        }
    
    switch (type) {
        case QMetaType::QString: {
            // Special case: Use QTextEdit for "text" property (multiline text input)
            if (propertyName == "text") {
                QTextEdit* textEdit = new QTextEdit();
                textEdit->setPlainText(value.toString());
                textEdit->setMinimumHeight(100); // Larger text area
                textEdit->setMaximumHeight(200);
                textEdit->setStyleSheet(R"(
                    QTextEdit {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                        font-family: 'Segoe UI', Arial, sans-serif;
                    }
                    QTextEdit:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QTextEdit:hover {
                        border: 1px solid #6495ed;
                    }
                )");
                connect(textEdit, &QTextEdit::textChanged, this, &PropertyPanel::onPropertyValueChanged);
                return textEdit;
            }
            
            // Special case: Use QComboBox for "fontFamily" property
            if (propertyName == "fontFamily") {
                QComboBox* fontCombo = new QComboBox();
                fontCombo->setEditable(false);
                fontCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                fontCombo->setMaximumWidth(250); // Prevent stretching beyond panel
                fontCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                
                // Add common fonts
                QStringList commonFonts = {
                    "Arial", "Helvetica", "Times New Roman", "Georgia", "Courier New",
                    "Verdana", "Trebuchet MS", "Comic Sans MS", "Impact", "Palatino",
                    "Garamond", "Bookman", "Avant Garde", "Futura", "Optima"
                };
                
                // Add all system fonts
                QStringList allFonts = QFontDatabase::families();
                
                // Combine: common fonts first, then separator, then all fonts
                fontCombo->addItems(commonFonts);
                fontCombo->insertSeparator(commonFonts.size());
                
                // Add remaining fonts (excluding duplicates)
                for (const QString& font : allFonts) {
                    if (!commonFonts.contains(font)) {
                        fontCombo->addItem(font);
                    }
                }
                
                // Set current font
                QString currentFont = value.toString();
                int index = fontCombo->findText(currentFont);
                if (index >= 0) {
                    fontCombo->setCurrentIndex(index);
                }
                
                fontCombo->setStyleSheet(R"(
                    QComboBox {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                    }
                    QComboBox:hover {
                        border: 1px solid #6495ed;
                    }
                    QComboBox:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QComboBox::drop-down {
                        border: none;
                        width: 20px;
                    }
                    QComboBox::down-arrow {
                        image: none;
                        border-left: 5px solid transparent;
                        border-right: 5px solid transparent;
                        border-top: 5px solid #e8e8e8;
                        margin-right: 5px;
                    }
                    QComboBox QAbstractItemView {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        selection-background-color: #6495ed;
                        selection-color: white;
                    }
                )");
                
                connect(fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
                return fontCombo;
            }
            
            // Default: Use QLineEdit for other string properties
            QLineEdit* edit = new QLineEdit();
            edit->setText(value.toString());
            if (isReadonly) {
                edit->setReadOnly(true);
                edit->setStyleSheet(R"(
                    QLineEdit {
                        background-color: #2a2a2a;
                        color: #a0a0a0;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-style: italic;
                    }
                )");
            } else {
                edit->setStyleSheet(R"(
                    QLineEdit {
                        background-color: #2a2a2a;
                        color: #e8e8e8;
                        border: 1px solid #3d3d3d;
                        border-radius: 6px;
                        padding: 8px 12px;
                        font-size: 11px;
                    }
                    QLineEdit:focus {
                        border: 2px solid #6495ed;
                        background-color: #1e1e1e;
                    }
                    QLineEdit:hover {
                        border: 1px solid #6495ed;
                    }
                )");
                connect(edit, &QLineEdit::textChanged, this, &PropertyPanel::onPropertyValueChanged);
            }
            return edit;
        }
        
        case QMetaType::Int: {
            QSpinBox* spin = new QSpinBox();
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);

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
                // Keep readonly fields visually distinct but still readable on dark UI
                spin->setStyleSheet(R"(
                    QSpinBox {
                        background-color: rgba(255,255,255,0.04);
                        color: rgba(231,234,240,0.60);
                        border: 1px solid rgba(255,255,255,0.10);
                        border-radius: 6px;
                        padding: 6px 26px 6px 8px;
                        font-style: italic;
                    }
                )");
            } else {
                // Use panel-global dark stylesheet
                connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Double: {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);

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
                // Keep readonly fields visually distinct but still readable on dark UI
                spin->setStyleSheet(R"(
                    QDoubleSpinBox {
                        background-color: rgba(255,255,255,0.04);
                        color: rgba(231,234,240,0.60);
                        border: 1px solid rgba(255,255,255,0.10);
                        border-radius: 6px;
                        padding: 6px 26px 6px 8px;
                        font-style: italic;
                    }
                )");
            } else {
                // Use panel-global dark stylesheet
                connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, &PropertyPanel::onPropertyValueChanged);
            }
            return spin;
        }
        
        case QMetaType::Bool: {
            QCheckBox* check = new QCheckBox();
            check->setChecked(value.toBool());
            
            // Use panel-global dark stylesheet
            
            if (!isReadonly) {
                connect(check, &QCheckBox::toggled, this, &PropertyPanel::onPropertyValueChanged);
            } else {
                check->setEnabled(false);
            }
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
    if (m_currentPrimitive) {
        // Re-show primitive properties to refresh values
        DrawingPrimitive* primitive = m_currentPrimitive;
        m_currentPrimitive = nullptr; // Reset to force refresh
        showPrimitiveProperties(primitive);
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
    
    qDebug() << "PropertyPanel::onPropertyValueChanged() called - sender:" << sender;
    qDebug() << "Current object name:" << m_currentObjectName;
    
    // Find the property name
    for (auto it = m_propertyEditors.begin(); it != m_propertyEditors.end(); ++it) {
        if (it.value() == sender) {
            propertyName = it.key();
            break;
        }
    }
    
    if (propertyName.isEmpty()) {
        qDebug() << "WARNING: PropertyPanel - property name not found for sender" << sender;
        return;
    }
    
    // Get the value based on widget type
    if (auto textEdit = qobject_cast<QTextEdit*>(sender)) {
        value = textEdit->toPlainText();
    }
    else if (auto edit = qobject_cast<QLineEdit*>(sender)) {
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
    else if (auto combo = qobject_cast<QComboBox*>(sender)) {
        if (propertyName == "paperFormat") {
            // For paper format, return the enum value
            value = combo->currentData().toInt();
        } else if (propertyName == "lineStyle") {
            // For line style, return enum value
            value = combo->currentData().toInt();
        } else {
            value = combo->currentText();
        }
    }
    
    qDebug() << "About to emit propertyChanged signal - Object:" << m_currentObjectName << ", Property:" << propertyName << ", Value:" << value;
    emit propertyChanged(m_currentObjectName, propertyName, value);
    
    qDebug() << "Property changed signal emitted successfully";
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
