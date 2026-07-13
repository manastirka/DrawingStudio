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
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidgetItem>
#include <QUuid>
#include <QIcon>
#include <QSize>

// Layer row widget (expand, opacity, lock) (refactor E18).

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

