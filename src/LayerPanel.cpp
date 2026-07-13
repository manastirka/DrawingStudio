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

// LayerPanel orchestrator (refactor E18).

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

