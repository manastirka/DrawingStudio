#include "LayerPanel.h"
#include "Commands.h"
#include "Layer.h"
#include "LayerManager.h"
#include "DrawingPrimitive.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QDebug>
#include <QSignalBlocker>
#include <QStyle>
#include <QApplication>
#include <QVBoxLayout>
#include <QLayoutItem>

// PrimitiveItemWidget Implementation
PrimitiveItemWidget::PrimitiveItemWidget(DrawingPrimitive* primitive, QWidget* parent)
    : QWidget(parent)
    , m_primitive(primitive)
    , m_isSelected(false)
{
    setupUI();
    updateFromPrimitive();
    setFixedHeight(25); // Smaller than layer items
}

PrimitiveItemWidget::~PrimitiveItemWidget()
{
}

void PrimitiveItemWidget::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(20, 2, 5, 2); // Indent to show hierarchy
    m_layout->setSpacing(5);
    
    // Visibility button
    m_visibilityButton = new QToolButton();
    m_visibilityButton->setFixedSize(16, 16);
    m_visibilityButton->setText("👁");
    m_visibilityButton->setStyleSheet("font-size: 10px;");
    m_visibilityButton->setToolTip("Toggle primitive visibility");
    connect(m_visibilityButton, &QToolButton::clicked, this, &PrimitiveItemWidget::onVisibilityClicked);
    
    // Type label
    m_typeLabel = new QLabel();
    m_typeLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #444;");
    m_typeLabel->setMinimumWidth(60);
    
    // Description label
    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setStyleSheet("font-size: 10px; color: #666;");
    
    m_layout->addWidget(m_visibilityButton);
    m_layout->addWidget(m_typeLabel);
    m_layout->addWidget(m_descriptionLabel, 1); // Take remaining space
}

void PrimitiveItemWidget::updateFromPrimitive()
{
    if (!m_primitive) return;
    
    m_typeLabel->setText(getPrimitiveTypeName());
    m_descriptionLabel->setText(getPrimitiveDescription());
    
    // Update visibility icon
    if (m_primitive->isVisible()) {
        m_visibilityButton->setStyleSheet("font-size: 10px; color: black;");
    } else {
        m_visibilityButton->setStyleSheet("font-size: 10px; color: #ccc;");
    }
    
    update();
}

QString PrimitiveItemWidget::getPrimitiveTypeName() const
{
    if (!m_primitive) return "Unknown";
    
    switch (m_primitive->type()) {
        case PrimitiveType::Line: return "Line";
        case PrimitiveType::Curve: return "Curve";
        case PrimitiveType::BezierCurve: return "Bézier";
        case PrimitiveType::Spline: return "Spline";
        case PrimitiveType::Arc: return "Arc";
        case PrimitiveType::Circle: return "Circle";
        case PrimitiveType::Rectangle: return "Rectangle";
        case PrimitiveType::Ellipse: return "Ellipse";
        case PrimitiveType::Polygon: return "Polygon";
        case PrimitiveType::Text: return "Text";
        case PrimitiveType::Dimension: return "Dimension";
        default: return "Unknown";
    }
}

QString PrimitiveItemWidget::getPrimitiveDescription() const
{
    if (!m_primitive) return "";
    
    // Return a brief description based on primitive type
    switch (m_primitive->type()) {
        case PrimitiveType::Rectangle: {
            auto rect = dynamic_cast<RectanglePrimitive*>(m_primitive);
            if (rect) {
                QVector2D tl = rect->topLeft();
                QVector2D br = rect->bottomRight();
                float width = abs(br.x() - tl.x());
                float height = abs(br.y() - tl.y());
                return QString("%1×%2").arg(QString::number(width, 'f', 1))
                                       .arg(QString::number(height, 'f', 1));
            }
            break;
        }
        case PrimitiveType::Line: {
            auto line = dynamic_cast<LinePrimitive*>(m_primitive);
            if (line) {
                float length = (line->endPoint() - line->startPoint()).length();
                return QString("Length: %1").arg(QString::number(length, 'f', 1));
            }
            break;
        }
        default:
            return QString("Object #%1").arg(reinterpret_cast<quintptr>(m_primitive) & 0xFFFF);
    }
    return "";
}

void PrimitiveItemWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    
    if (m_isSelected) {
        painter.fillRect(rect(), QColor(100, 150, 255, 30)); // Light blue selection
    } else {
        painter.fillRect(rect(), QColor(250, 250, 250)); // Very light gray background
    }
    
    QWidget::paintEvent(event);
}

void PrimitiveItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit primitiveSelected(m_primitive);
    }
    QWidget::mousePressEvent(event);
}

void PrimitiveItemWidget::onVisibilityClicked()
{
    if (m_primitive) {
        bool newVisibility = !m_primitive->isVisible();
        emit primitiveVisibilityToggled(m_primitive, newVisibility);
    }
}

// LayerItemWidget Implementation
LayerItemWidget::LayerItemWidget(Layer* layer, QWidget* parent)
    : QWidget(parent)
    , m_layer(layer)
    , m_isSelected(false)
    , m_isExpanded(false)
{
    if (!layer) {
        qWarning() << "LayerItemWidget created with null layer!";
    }
    setupUI();
    updateFromLayer();
    updatePrimitivesList();
}

LayerItemWidget::~LayerItemWidget()
{
}

void LayerItemWidget::setupUI()
{
    // Set proper size constraints FIRST - wider to fit blend mode
    setMinimumWidth(260);
    setMaximumWidth(300);
    setFixedHeight(60); // Fixed height to prevent it from being huge
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    
    // Simple background - borders drawn in paintEvent
    setStyleSheet("LayerItemWidget { background: transparent; }");
    
    // Main widget layout (horizontal: thumbnail + info) - Photoshop style
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);
    
    // Thumbnail preview (left side)
    QLabel* thumbnailLabel = new QLabel();
    thumbnailLabel->setFixedSize(40, 40);
    thumbnailLabel->setStyleSheet(R"(
        QLabel {
            background: #2a2a2a;
            border: 1px solid #1a1a1a;
            border-radius: 2px;
        }
    )");
    thumbnailLabel->setAlignment(Qt::AlignCenter);
    
    // Create checkerboard pattern for transparency
    QPixmap thumbnail(40, 40);
    thumbnail.fill(Qt::transparent);
    QPainter thumbPainter(&thumbnail);
    thumbPainter.fillRect(0, 0, 40, 40, QColor(60, 60, 60));
    thumbPainter.setPen(Qt::white);
    thumbPainter.setFont(QFont("Arial", 10));
    thumbPainter.drawText(thumbnail.rect(), Qt::AlignCenter, "L");
    thumbnailLabel->setPixmap(thumbnail);
    
    mainLayout->addWidget(thumbnailLabel);
    
    // Info section (right side)
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    
    // Top row: visibility, lock, name
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    topRow->setContentsMargins(0, 0, 0, 0);
    
    // Visibility button
    m_visibilityButton = new QToolButton();
    m_visibilityButton->setFixedSize(18, 18);
    m_visibilityButton->setToolTip("Toggle layer visibility");
    connect(m_visibilityButton, &QToolButton::clicked, this, &LayerItemWidget::onVisibilityClicked);
    
    // Lock button
    m_lockButton = new QToolButton();
    m_lockButton->setFixedSize(18, 18);
    m_lockButton->setToolTip("Toggle layer lock");
    connect(m_lockButton, &QToolButton::clicked, this, &LayerItemWidget::onLockClicked);
    
    // Layer name edit
    m_nameEdit = new QLineEdit();
    m_nameEdit->setStyleSheet(R"(
        QLineEdit {
            border: none;
            background: transparent;
            font-size: 11px;
            color: #e0e0e0;
            padding: 2px 4px;
        }
        QLineEdit:focus {
            border: 1px solid #4a90e2;
            background: #2a2a2a;
            border-radius: 2px;
        }
    )");
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &LayerItemWidget::onNameEditFinished);
    
    topRow->addWidget(m_visibilityButton);
    topRow->addWidget(m_lockButton);
    topRow->addWidget(m_nameEdit, 1);
    
    // Bottom row: opacity and blend mode - compact
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(6);
    bottomRow->setContentsMargins(0, 0, 0, 0);
    
    // Opacity label and value
    QLabel* opacityLabel = new QLabel("Opacity:");
    opacityLabel->setStyleSheet("font-size: 9px; color: #999;");
    
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setFixedWidth(60);
    m_opacitySlider->setFixedHeight(14);
    m_opacitySlider->setToolTip("Layer opacity");
    m_opacitySlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #2a2a2a;
            height: 3px;
            border-radius: 1px;
        }
        QSlider::handle:horizontal {
            background: #e0e0e0;
            width: 8px;
            margin: -3px 0;
            border-radius: 4px;
        }
        QSlider::handle:horizontal:hover {
            background: white;
        }
    )");
    connect(m_opacitySlider, &QSlider::valueChanged, this, &LayerItemWidget::onOpacityChanged);
    
    bottomRow->addWidget(opacityLabel);
    bottomRow->addWidget(m_opacitySlider);
    
    // Blend mode dropdown
    QLabel* blendLabel = new QLabel("Blend:");
    blendLabel->setStyleSheet("font-size: 9px; color: #999;");
    
    m_blendModeCombo = new QComboBox();
    m_blendModeCombo->addItems({"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten"});
    m_blendModeCombo->setFixedWidth(70);
    m_blendModeCombo->setFixedHeight(18);
    m_blendModeCombo->setStyleSheet(R"(
        QComboBox {
            background: #2a2a2a;
            color: #e0e0e0;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 2px 4px;
            padding-right: 15px;
            font-size: 9px;
        }
        QComboBox:hover {
            border: 1px solid #6495ed;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 15px;
            border: none;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 3px solid transparent;
            border-right: 3px solid transparent;
            border-top: 4px solid #e0e0e0;
            margin-right: 3px;
        }
        QComboBox QAbstractItemView {
            background: #2a2a2a;
            color: #e0e0e0;
            border: 1px solid #555;
            selection-background-color: #6495ed;
            selection-color: white;
        }
    )");
    connect(m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LayerItemWidget::onBlendModeChanged);
    
    bottomRow->addWidget(blendLabel);
    bottomRow->addWidget(m_blendModeCombo);
    bottomRow->addStretch();
    
    infoLayout->addLayout(topRow);
    infoLayout->addLayout(bottomRow);
    
    mainLayout->addLayout(infoLayout, 1);
    
    // Primitive count label
    m_primitiveCountLabel = new QLabel();
    m_primitiveCountLabel->setStyleSheet("color: #777; font-size: 8px;");
    m_primitiveCountLabel->setVisible(false); // Hidden by default
    
    // Expand button and color label - not used in Photoshop style
    m_expandButton = new QToolButton();
    m_expandButton->setVisible(false);
    m_colorLabel = new QLabel();
    m_colorLabel->setVisible(false);
    
    // Create primitives container (initially hidden)
    m_primitivesContainer = new QWidget();
    m_primitivesContainer->setStyleSheet("background-color: #2a2a2a; border-top: 1px solid #1a1a1a;");
    m_primitivesLayout = new QVBoxLayout(m_primitivesContainer);
    m_primitivesLayout->setContentsMargins(5, 2, 2, 2);
    m_primitivesLayout->setSpacing(1);
    m_primitivesContainer->setVisible(false);
    
    // Initially collapsed
    m_isExpanded = false;
    
    updateVisibilityIcon();
    updateLockIcon();
}

void LayerItemWidget::updateFromLayer()
{
    if (!m_layer) return;

    const QSignalBlocker nameBlocker(m_nameEdit);
    const QSignalBlocker opacityBlocker(m_opacitySlider);
    const QSignalBlocker blendModeBlocker(m_blendModeCombo);

    m_nameEdit->setText(m_layer->name());
    m_opacitySlider->setValue(static_cast<int>(m_layer->opacity() * 100));
    m_blendModeCombo->setCurrentIndex(static_cast<int>(m_layer->blendMode()));
    m_primitiveCountLabel->setText(QString("%1 objects").arg(m_layer->primitiveCount()));
    
    // Update color indicator
    QPixmap colorPixmap(18, 18);
    colorPixmap.fill(m_layer->color());
    m_colorLabel->setPixmap(colorPixmap);
    
    updateVisibilityIcon();
    updateLockIcon();
    updatePrimitivesList(); // Refresh primitives list
    update(); // Repaint to show selection state
}

void LayerItemWidget::updateVisibilityIcon()
{
    if (!m_layer) return;
    
    QString baseStyle = R"(
        QToolButton {
            background: transparent;
            border: none;
            font-size: 12px;
            border-radius: 2px;
        }
        QToolButton:hover {
            background: #505050;
        }
    )";
    
    if (m_layer->isVisible()) {
        m_visibilityButton->setText("👁"); // Eye icon - visible
        m_visibilityButton->setStyleSheet(baseStyle + "QToolButton { color: #90ee90; }"); // Light green
    } else {
        m_visibilityButton->setText("👁"); // Eye icon always shown
        m_visibilityButton->setStyleSheet(baseStyle + "QToolButton { color: #666; text-decoration: line-through; }"); // Gray with strikethrough
    }
}

void LayerItemWidget::updateLockIcon()
{
    if (!m_layer) return;
    
    QString baseStyle = R"(
        QToolButton {
            background: transparent;
            border: none;
            font-size: 12px;
            border-radius: 2px;
        }
        QToolButton:hover {
            background: #505050;
        }
    )";
    
    if (m_layer->isLocked()) {
        m_lockButton->setText("🔒"); // Locked
        m_lockButton->setStyleSheet(baseStyle + "QToolButton { color: #ff6b6b; }"); // Red
    } else {
        m_lockButton->setText("🔓"); // Unlocked - always shown
        m_lockButton->setStyleSheet(baseStyle + "QToolButton { color: #90ee90; }"); // Light green
    }
}

void LayerItemWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect r = rect().adjusted(3, 3, -3, -3); // Margins
    
    // Draw gradient background with smoother transitions
    QLinearGradient gradient(r.topLeft(), r.bottomLeft());
    if (m_isSelected) {
        gradient.setColorAt(0, QColor(90, 90, 90));     // Lighter top
        gradient.setColorAt(0.3, QColor(75, 75, 75));   // Smooth transition
        gradient.setColorAt(0.7, QColor(65, 65, 65));   // Smooth transition
        gradient.setColorAt(1, QColor(55, 55, 55));     // Darker bottom
    } else {
        gradient.setColorAt(0, QColor(70, 70, 70));     // Lighter top
        gradient.setColorAt(0.3, QColor(58, 58, 58));   // Smooth transition
        gradient.setColorAt(0.7, QColor(50, 50, 50));   // Smooth transition
        gradient.setColorAt(1, QColor(42, 42, 42));     // Darker bottom
    }
    
    // Fill with gradient
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(r, 6, 6);
    
    // Draw subtle inner highlight for more depth
    painter.setPen(QPen(QColor(255, 255, 255, 15), 1));
    painter.drawRoundedRect(r.adjusted(1, 1, -1, -1), 5, 5);
    
    // Draw 3D borders with better colors
    QPen topPen(m_isSelected ? QColor(110, 110, 110) : QColor(90, 90, 90), 1.5);
    QPen bottomPen(m_isSelected ? QColor(40, 40, 40) : QColor(30, 30, 30), 1.5);
    
    // Top highlight
    painter.setPen(topPen);
    painter.drawLine(r.topLeft() + QPoint(6, 1), r.topRight() + QPoint(-6, 1));
    
    // Left border (blue accent if selected)
    if (m_isSelected) {
        painter.setPen(QPen(QColor(100, 149, 237), 3)); // Blue accent
        painter.drawLine(r.topLeft() + QPoint(2, 6), r.bottomLeft() + QPoint(2, -6));
    } else {
        painter.setPen(topPen);
        painter.drawLine(r.topLeft() + QPoint(1, 6), r.bottomLeft() + QPoint(1, -6));
    }
    
    // Bottom shadow
    painter.setPen(bottomPen);
    painter.drawLine(r.bottomLeft() + QPoint(6, -1), r.bottomRight() + QPoint(-6, -1));
    
    // Right shadow
    painter.drawLine(r.topRight() + QPoint(-1, 6), r.bottomRight() + QPoint(-1, -6));
    
    QWidget::paintEvent(event);
}

void LayerItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit layerSelected(m_layer);
    }
    QWidget::mousePressEvent(event);
}

void LayerItemWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu;
    menu.addSeparator();
    menu.addAction("Change Color", [this]() {
        if (m_layer) {
            QColor color = QColorDialog::getColor(m_layer->color(), this, "Select Layer Color");
            if (color.isValid()) {
                m_layer->setColor(color);
                updateFromLayer();
            }
        }
    });
    menu.exec(event->globalPos());
}

void LayerItemWidget::onVisibilityClicked()
{
    if (m_layer) {
        bool newVisibility = !m_layer->isVisible();
        emit visibilityToggled(m_layer, newVisibility);
    }
}

void LayerItemWidget::onLockClicked()
{
    if (m_layer) {
        bool newLockState = !m_layer->isLocked();
        emit lockToggled(m_layer, newLockState);
    }
}

void LayerItemWidget::onOpacityChanged(int value)
{
    if (m_layer) {
        float opacity = value / 100.0f;
        emit opacityChanged(m_layer, opacity);
    }
}

void LayerItemWidget::onNameEditFinished()
{
    if (m_layer) {
        QString newName = m_nameEdit->text().trimmed();
        if (!newName.isEmpty() && newName != m_layer->name()) {
            emit nameChanged(m_layer, newName);
        }
    }
}

void LayerItemWidget::onBlendModeChanged(int index)
{
    if (m_layer) {
        emit blendModeChanged(m_layer, index);
    }
}

void LayerItemWidget::onExpandClicked()
{
    setExpanded(!m_isExpanded);
    emit expandToggled(m_layer, m_isExpanded);
}

void LayerItemWidget::setSelected(bool selected)
{
    m_isSelected = selected;
    update(); // Trigger repaint with new selection state
}

void LayerItemWidget::setExpanded(bool expanded)
{
    m_isExpanded = expanded;
    
    if (m_isExpanded) {
        m_expandButton->setText("▼"); // Down-pointing triangle
        m_primitivesContainer->show();
    } else {
        m_expandButton->setText("▶"); // Right-pointing triangle
        m_primitivesContainer->hide();
    }
    
    updateGeometry();
    update();
}

void LayerItemWidget::updatePrimitivesList()
{
    // Clear existing primitive widgets
    QLayoutItem* child;
    while ((child = m_primitivesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    
    if (!m_layer) return;
    
    // Add primitive widgets
    const auto& primitives = m_layer->primitives();
    for (const auto& primitive : primitives) {
        if (primitive) {
            PrimitiveItemWidget* primitiveWidget = new PrimitiveItemWidget(primitive.get());
            
            // Connect primitive signals
            connect(primitiveWidget, &PrimitiveItemWidget::primitiveSelected,
                    [this](DrawingPrimitive* prim) {
                        // Forward primitive selection signal
                        // This could be connected to canvas selection logic
                    });
            
            connect(primitiveWidget, &PrimitiveItemWidget::primitiveVisibilityToggled,
                    [this, primitiveWidget](DrawingPrimitive* prim, bool visible) {
                        emit primitiveVisibilityToggled(prim, visible);
                        primitiveWidget->updateFromPrimitive();
                    });
            
            m_primitivesLayout->addWidget(primitiveWidget);
        }
    }
    
    // Add stretch to push primitive items to top
    m_primitivesLayout->addStretch();
}

// LayerPanel Implementation
LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
    , m_layerManager(nullptr)
    , m_selectedLayer(nullptr)
{
    setupUI();
    setupToolbar();
}

LayerPanel::~LayerPanel()
{
}

void LayerPanel::setupUI()
{
    setFixedWidth(310); // Wider to fit blend mode
    
    // Photoshop-style dark theme for LayerPanel
    setStyleSheet(R"(
        LayerPanel {
            background: #2a2a2a;
            border-left: 1px solid #1a1a1a;
            padding: 0px;
        }
        QScrollArea {
            border: none;
            background: transparent;
            margin: 0px;
        }
        QScrollBar:vertical {
            background: #1a1a1a;
            width: 12px;
            border-radius: 6px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #505050;
            border-radius: 6px;
            min-height: 24px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #606060;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: none;
            background: none;
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        QToolTip {
            background-color: rgba(30, 30, 30, 0.95);
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
    )");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Title section with Photoshop-style header
    QWidget* titleWidget = new QWidget();
    titleWidget->setStyleSheet(R"(
        QWidget {
            background: #2a2a2a;
            border-bottom: 1px solid #1a1a1a;
        }
    )");
    
    QHBoxLayout* titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel* titleLabel = new QLabel("LAYERS");
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 600;
            font-size: 11px;
            color: #b0b0b0;
            background: transparent;
            letter-spacing: 1px;
        }
    )");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    m_mainLayout->addWidget(titleWidget);
    
    // Scroll area for layer items
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_layerListWidget = new QWidget();
    m_layerListLayout = new QVBoxLayout(m_layerListWidget);
    m_layerListLayout->setContentsMargins(12, 12, 12, 12);
    m_layerListLayout->setSpacing(4);
    m_layerListLayout->addStretch(); // Push items to top
    
    m_scrollArea->setWidget(m_layerListWidget);
    m_mainLayout->addWidget(m_scrollArea, 1); // Take available space
}

void LayerPanel::setupToolbar()
{
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(4);
    
    // Clean, flat modern button styling
    QString modernButtonStyle = R"(
        QPushButton {
            background: #ffffff;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            color: #495057;
            font-weight: 500;
            font-size: 14px;
            padding: 8px;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border: 1px solid #0d6efd;
            color: #0d6efd;
        }
        QPushButton:pressed {
            background: rgba(13, 110, 253, 0.1);
            border: 1px solid #0d6efd;
            color: #0d6efd;
        }
        QPushButton:disabled {
            background: #f8f9fa;
            color: #adb5bd;
            border: 1px solid #e9ecef;
        }
    )";
    
    // Add layer button
    m_addLayerButton = new QPushButton("+");
    m_addLayerButton->setFixedSize(26, 26);
    m_addLayerButton->setToolTip("Add new layer");
    m_addLayerButton->setStyleSheet(modernButtonStyle);
    connect(m_addLayerButton, &QPushButton::clicked, this, &LayerPanel::onAddLayer);

    // Delete layer button
    m_deleteLayerButton = new QPushButton("-");
    m_deleteLayerButton->setFixedSize(26, 26);
    m_deleteLayerButton->setToolTip("Delete selected layer");
    m_deleteLayerButton->setStyleSheet(modernButtonStyle);
    connect(m_deleteLayerButton, &QPushButton::clicked, this, &LayerPanel::onDeleteLayer);

    // Duplicate layer button
    m_duplicateLayerButton = new QPushButton(QStringLiteral("⧉"));
    m_duplicateLayerButton->setFixedSize(26, 26);
    m_duplicateLayerButton->setToolTip("Duplicate selected layer");
    m_duplicateLayerButton->setStyleSheet(modernButtonStyle);
    connect(m_duplicateLayerButton, &QPushButton::clicked, this, &LayerPanel::onDuplicateLayer);

    // Move up / down
    m_moveUpButton = new QPushButton(QStringLiteral("↑"));
    m_moveUpButton->setFixedSize(26, 26);
    m_moveUpButton->setToolTip("Move layer up");
    m_moveUpButton->setStyleSheet(modernButtonStyle);
    connect(m_moveUpButton, &QPushButton::clicked, this, &LayerPanel::onMoveLayerUp);

    m_moveDownButton = new QPushButton(QStringLiteral("↓"));
    m_moveDownButton->setFixedSize(26, 26);
    m_moveDownButton->setToolTip("Move layer down");
    m_moveDownButton->setStyleSheet(modernButtonStyle);
    connect(m_moveDownButton, &QPushButton::clicked, this, &LayerPanel::onMoveLayerDown);

    m_toolbarLayout->addWidget(m_addLayerButton);
    m_toolbarLayout->addWidget(m_deleteLayerButton);
    m_toolbarLayout->addWidget(m_duplicateLayerButton);
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_moveUpButton);
    m_toolbarLayout->addWidget(m_moveDownButton);
    
    // Show/hide all buttons
    m_showAllButton = new QPushButton("Show All");
    m_showAllButton->setToolTip("Show all layers");
    m_showAllButton->setStyleSheet(modernButtonStyle);
    connect(m_showAllButton, &QPushButton::clicked, this, &LayerPanel::onShowAllLayers);
    
    m_hideAllButton = new QPushButton("Hide All");
    m_hideAllButton->setToolTip("Hide all layers");
    m_hideAllButton->setStyleSheet(modernButtonStyle);
    connect(m_hideAllButton, &QPushButton::clicked, this, &LayerPanel::onHideAllLayers);
    
    QHBoxLayout* showHideLayout = new QHBoxLayout();
    showHideLayout->addWidget(m_showAllButton);
    showHideLayout->addWidget(m_hideAllButton);
    
    m_mainLayout->addLayout(m_toolbarLayout);
    m_mainLayout->addLayout(showHideLayout);
}

void LayerPanel::setLayerManager(LayerManager* manager)
{
    if (m_layerManager) {
        // Disconnect old signals
        disconnect(m_layerManager, nullptr, this, nullptr);
    }
    
    m_layerManager = manager;
    
    if (m_layerManager) {
        connectSignals();
        refresh();
    }
}

void LayerPanel::connectSignals()
{
    if (!m_layerManager) return;
    
    connect(m_layerManager, &LayerManager::layerCreated,
            this, &LayerPanel::onLayerCreated);
    connect(m_layerManager, &LayerManager::layerDeleted,
            this, &LayerPanel::onLayerDeleted);
    connect(m_layerManager, &LayerManager::activeLayerChanged,
            this, &LayerPanel::onActiveLayerChanged);
    connect(m_layerManager, &LayerManager::layersReordered,
            this, &LayerPanel::onLayersReordered);
    connect(m_layerManager, &LayerManager::primitiveAdded,
            this, &LayerPanel::onPrimitiveAdded);
    connect(m_layerManager, &LayerManager::layerVisibilityChanged,
            this, [this](Layer* layer, bool /*visible*/) {
                if (LayerItemWidget* widget = findLayerWidget(layer)) {
                    widget->updateFromLayer();
                }
                emit layerPropertyChanged();
            });
    connect(m_layerManager, &LayerManager::layerLockChanged,
            this, [this](Layer* layer, bool /*locked*/) {
                if (LayerItemWidget* widget = findLayerWidget(layer)) {
                    widget->updateFromLayer();
                }
                emit layerPropertyChanged();
            });
    connect(m_layerManager, &LayerManager::layerOpacityChanged,
            this, [this](Layer* layer, float /*opacity*/) {
                if (LayerItemWidget* widget = findLayerWidget(layer)) {
                    widget->updateFromLayer();
                }
                emit layerPropertyChanged();
            });
    connect(m_layerManager, &LayerManager::layerRenamed,
            this, [this](Layer* layer, const QString& /*oldName*/) {
                if (LayerItemWidget* widget = findLayerWidget(layer)) {
                    widget->updateFromLayer();
                }
            });
    connect(m_layerManager, &LayerManager::layerBlendModeChanged,
            this, [this](Layer* layer, int /*blendMode*/) {
                if (LayerItemWidget* widget = findLayerWidget(layer)) {
                    widget->updateFromLayer();
                }
                emit layerPropertyChanged();
            });
}

void LayerPanel::refresh()
{
    rebuildLayerList();
}

void LayerPanel::rebuildLayerList()
{
    // Clear existing widgets - proper cleanup order
    while (QLayoutItem* child = m_layerListLayout->takeAt(0)) {
        if (QWidget* widget = child->widget()) {
            widget->deleteLater();
        }
        delete child;
    }
    
    if (!m_layerManager) {
        m_layerListLayout->addStretch();
        return;
    }
    
    // Add layer widgets (in reverse order since layers are rendered bottom-to-top)
    const auto& layers = m_layerManager->layers();
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        Layer* layer = layers[i].get();
        if (!layer) continue; // Safety check
        
        LayerItemWidget* widget = new LayerItemWidget(layer);
        
        // Connect signals
        connect(widget, &LayerItemWidget::visibilityToggled,
                this, &LayerPanel::onLayerVisibilityToggled);
        connect(widget, &LayerItemWidget::lockToggled,
                this, &LayerPanel::onLayerLockToggled);
        connect(widget, &LayerItemWidget::opacityChanged,
                this, &LayerPanel::onLayerOpacityChanged);
        connect(widget, &LayerItemWidget::nameChanged,
                this, &LayerPanel::onLayerNameChanged);
        connect(widget, &LayerItemWidget::layerSelected,
                this, &LayerPanel::onLayerSelected);
        connect(widget, &LayerItemWidget::blendModeChanged,
                this, &LayerPanel::onLayerBlendModeChanged);
        connect(widget, &LayerItemWidget::primitiveVisibilityToggled,
                this, &LayerPanel::onPrimitiveVisibilityToggled);
        connect(widget, &LayerItemWidget::expandToggled,
                [](Layer* /*layer*/, bool /*expanded*/) {
                    // Optional: store expand state or handle expand/collapse
                });
        
        m_layerListLayout->addWidget(widget);
    }
    
    m_layerListLayout->addStretch(); // Push items to top
    
    // Update selection
    if (m_layerManager->activeLayer()) {
        selectLayer(m_layerManager->activeLayer());
    }
}

LayerItemWidget* LayerPanel::findLayerWidget(Layer* layer) const
{
    for (int i = 0; i < m_layerListLayout->count(); ++i) {
        QLayoutItem* item = m_layerListLayout->itemAt(i);
        if (LayerItemWidget* widget = qobject_cast<LayerItemWidget*>(item->widget())) {
            if (widget->layer() == layer) {
                return widget;
            }
        }
    }
    return nullptr;
}

void LayerPanel::selectLayer(Layer* layer)
{
    // Deselect previous
    if (LayerItemWidget* prevWidget = findLayerWidget(m_selectedLayer)) {
        prevWidget->setSelected(false);
    }
    
    // Select new
    m_selectedLayer = layer;
    if (LayerItemWidget* widget = findLayerWidget(layer)) {
        widget->setSelected(true);
    }
    
    emit layerSelectionChanged(layer);
}

Layer* LayerPanel::getSelectedLayer() const
{
    return m_selectedLayer;
}

// Slot implementations
void LayerPanel::onAddLayer()
{
    qDebug() << "LayerPanel::onAddLayer() called";
    if (m_layerManager) {
        auto command = std::make_unique<CreateLayerCommand>(m_layerManager);
        emit commandRequested(command.release());
    } else {
        qDebug() << "WARNING: LayerManager is null in onAddLayer!";
    }
}

void LayerPanel::onDeleteLayer()
{
    if (m_layerManager && m_selectedLayer) {
        if (m_layerManager->layerCount() <= 1) {
            qDebug() << "LayerPanel: refusing to delete last layer";
            return;
        }
        auto command = std::make_unique<DeleteLayerCommand>(m_layerManager, m_selectedLayer->id());
        emit commandRequested(command.release());
    }
}

void LayerPanel::onDuplicateLayer()
{
    if (!m_layerManager || !m_selectedLayer)
        return;
    if (m_layerManager->duplicateLayer(m_selectedLayer)) {
        emit documentModified();
        emit layerPropertyChanged();
        refresh();
    }
}

void LayerPanel::onMoveLayerUp()
{
    if (m_layerManager && m_selectedLayer) {
        const size_t oldIndex = m_layerManager->getLayerIndex(m_selectedLayer);
        if (oldIndex != SIZE_MAX && oldIndex + 1 < m_layerManager->layerCount()) {
            auto command = std::make_unique<ReorderLayerCommand>(
                m_layerManager, m_selectedLayer->id(),
                static_cast<int>(oldIndex), static_cast<int>(oldIndex + 1),
                "Move Layer Up");
            emit commandRequested(command.release());
        }
    }
}

void LayerPanel::onMoveLayerDown()
{
    if (m_layerManager && m_selectedLayer) {
        const size_t oldIndex = m_layerManager->getLayerIndex(m_selectedLayer);
        if (oldIndex != SIZE_MAX && oldIndex > 0) {
            auto command = std::make_unique<ReorderLayerCommand>(
                m_layerManager, m_selectedLayer->id(),
                static_cast<int>(oldIndex), static_cast<int>(oldIndex - 1),
                "Move Layer Down");
            emit commandRequested(command.release());
        }
    }
}

void LayerPanel::onShowAllLayers()
{
    if (m_layerManager) {
        auto command =
            std::make_unique<SetAllLayersVisibilityCommand>(m_layerManager, true);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onHideAllLayers()
{
    if (m_layerManager) {
        auto command =
            std::make_unique<SetAllLayersVisibilityCommand>(m_layerManager, false);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onPrimitiveVisibilityToggled(DrawingPrimitive* primitive, bool visible)
{
    if (!primitive) {
        return;
    }

    const bool oldVisible = primitive->isVisible();
    if (oldVisible == visible) {
        return;
    }

    auto command =
        std::make_unique<SetPrimitiveVisibilityCommand>(primitive, oldVisible, visible);
    emit commandRequested(command.release());
}

// LayerManager signal handlers
void LayerPanel::onLayerCreated(Layer* layer)
{
    qDebug() << "LayerPanel: Layer created" << layer->name();
    refresh();
}

void LayerPanel::onLayerDeleted(const QUuid& layerId)
{
    qDebug() << "LayerPanel: Layer deleted" << layerId;
    if (m_selectedLayer && m_selectedLayer->id() == layerId) {
        m_selectedLayer = nullptr;
    }
    refresh();
}

void LayerPanel::onActiveLayerChanged(Layer* layer)
{
    selectLayer(layer);
    if (m_layerManager) {
        m_layerManager->setActiveLayer(layer);
    }
}

void LayerPanel::onLayersReordered()
{
    qDebug() << "LayerPanel: Layers reordered";
    refresh();
}

void LayerPanel::onPrimitiveAdded(Layer* layer, DrawingPrimitive* primitive)
{
    qDebug() << "LayerPanel: Primitive added to layer" << layer->name();
    
    // Find and update the specific layer widget
    if (LayerItemWidget* widget = findLayerWidget(layer)) {
        widget->updateFromLayer(); // This will refresh the primitives list
    }
}

// LayerItemWidget signal handlers
void LayerPanel::onLayerVisibilityToggled(Layer* layer, bool visible)
{
    if (m_layerManager && layer) {
        const bool oldVisible = layer->isVisible();
        if (oldVisible == visible) {
            return;
        }

        auto command = std::make_unique<SetLayerVisibilityCommand>(
            m_layerManager, layer->id(), oldVisible, visible);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onLayerLockToggled(Layer* layer, bool locked)
{
    if (m_layerManager && layer) {
        const bool oldLocked = layer->isLocked();
        if (oldLocked == locked) {
            return;
        }

        auto command = std::make_unique<SetLayerLockCommand>(
            m_layerManager, layer->id(), oldLocked, locked);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onLayerOpacityChanged(Layer* layer, float opacity)
{
    if (m_layerManager && layer) {
        const float oldOpacity = layer->opacity();
        if (qFuzzyCompare(oldOpacity, opacity)) {
            return;
        }

        auto command = std::make_unique<SetLayerOpacityCommand>(
            m_layerManager, layer->id(), oldOpacity, opacity);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onLayerNameChanged(Layer* layer, const QString& newName)
{
    if (m_layerManager && layer) {
        const QString oldName = layer->name();
        if (oldName == newName) {
            return;
        }

        auto command = std::make_unique<RenameLayerCommand>(
            m_layerManager, layer->id(), oldName, newName);
        emit commandRequested(command.release());
    }
}

void LayerPanel::onLayerSelected(Layer* layer)
{
    selectLayer(layer);
    if (m_layerManager) {
        m_layerManager->setActiveLayer(layer);
    }
}

void LayerPanel::onLayerBlendModeChanged(Layer* layer, int blendMode)
{
    if (m_layerManager && layer) {
        const int oldBlendMode = static_cast<int>(layer->blendMode());
        if (oldBlendMode == blendMode) {
            return;
        }

        auto command = std::make_unique<SetLayerBlendModeCommand>(
            m_layerManager, layer->id(), oldBlendMode, blendMode);
        emit commandRequested(command.release());
    }
}

void LayerPanel::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_contextMenu) {
        m_contextMenu = new QMenu(this);
        m_contextMenu->addAction("Add Layer", this, &LayerPanel::onAddLayer);
        m_contextMenu->addAction("Duplicate Layer", this, &LayerPanel::onDuplicateLayer);
        m_contextMenu->addAction("Delete Layer", this, &LayerPanel::onDeleteLayer);
        m_contextMenu->addSeparator();
        m_contextMenu->addAction("Show All", this, &LayerPanel::onShowAllLayers);
        m_contextMenu->addAction("Hide All", this, &LayerPanel::onHideAllLayers);
    }
    
    m_contextMenu->exec(event->globalPos());
}
