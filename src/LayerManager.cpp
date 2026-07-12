#include "LayerManager.h"
#include "Layer.h"
#include "DrawingPrimitive.h"
#include <algorithm>
#include <QDebug>

LayerManager::LayerManager(QObject* parent)
    : QObject(parent)
    , m_activeLayer(nullptr)
    , m_nextLayerNumber(1)
{
    qDebug() << "LayerManager::LayerManager() constructor called";
    // Create default layer
    ensureDefaultLayer();
    qDebug() << "LayerManager initialized with" << m_layers.size() << "layers";
}

LayerManager::~LayerManager()
{
    clearLayers();
}

void LayerManager::ensureDefaultLayer()
{
    if (m_layers.empty()) {
        Layer* defaultLayer = createLayer("Background");
        setActiveLayer(defaultLayer);
    }
}

Layer* LayerManager::createLayer(const QString& name)
{
    QString layerName = name.isEmpty() ? generateUniqueLayerName() : name;
    
    auto layer = std::make_unique<Layer>(layerName);
    Layer* layerPtr = layer.get();
    
    // Set z-order based on position (higher index = higher z-order)
    layer->setZOrder(static_cast<int>(m_layers.size()));
    
    m_layers.push_back(std::move(layer));
    
    // If this is the first layer, make it active
    if (m_layers.size() == 1) {
        m_activeLayer = layerPtr;
    }
    
    emit layerCreated(layerPtr);
    qDebug() << "Created layer:" << layerName << "Total layers:" << m_layers.size();
    
    return layerPtr;
}

Layer* LayerManager::duplicateLayer(Layer* source)
{
    if (!source)
        return nullptr;

    const QString copyName = source->name() + QStringLiteral(" copy");
    Layer* layer = createLayer(copyName);
    if (!layer)
        return nullptr;

    layer->setVisible(source->isVisible());
    layer->setLocked(source->isLocked());
    layer->setOpacity(source->opacity());
    layer->setBlendMode(source->blendMode());
    layer->setColor(source->color());

    for (const auto &prim : source->primitives()) {
        if (!prim)
            continue;
        auto cloned = prim->clone();
        if (cloned)
            layer->addPrimitive(std::move(cloned));
    }

    setActiveLayer(layer);
    return layer;
}

Layer* LayerManager::createLayer(const QUuid& id, const QString& name)
{
    QString layerName = name.isEmpty() ? generateUniqueLayerName() : name;

    QUuid layerId = id.isNull() ? QUuid::createUuid() : id;
    if (getLayer(layerId) != nullptr) {
        qWarning() << "LayerManager: Duplicate layer ID detected, generating new ID";
        layerId = QUuid::createUuid();
    }

    auto layer = std::make_unique<Layer>(layerId, layerName);
    Layer* layerPtr = layer.get();

    // Set z-order based on position (higher index = higher z-order)
    layer->setZOrder(static_cast<int>(m_layers.size()));

    m_layers.push_back(std::move(layer));

    // If this is the first layer, make it active
    if (m_layers.size() == 1) {
        m_activeLayer = layerPtr;
    }

    emit layerCreated(layerPtr);
    qDebug() << "Created layer:" << layerName << "ID:" << layerId.toString()
             << "Total layers:" << m_layers.size();

    return layerPtr;
}

void LayerManager::deleteLayer(const QUuid& layerId)
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerId](const std::unique_ptr<Layer>& layer) {
            return layer->id() == layerId;
        });
    
    if (it != m_layers.end()) {
        Layer* layerPtr = it->get();
        
        // If we're deleting the active layer, choose a new one
        if (m_activeLayer == layerPtr) {
            if (m_layers.size() > 1) {
                // Choose next layer, or previous if this is the last
                size_t index = std::distance(m_layers.begin(), it);
                if (index < m_layers.size() - 1) {
                    m_activeLayer = m_layers[index + 1].get();
                } else if (index > 0) {
                    m_activeLayer = m_layers[index - 1].get();
                } else {
                    m_activeLayer = nullptr;
                }
            } else {
                m_activeLayer = nullptr;
            }
        }
        
        emit layerDeleted(layerId);
        m_layers.erase(it);
        
        // Ensure we always have at least one layer
        ensureDefaultLayer();
        
        // Update z-ordering
        updateLayerOrdering();
        
        qDebug() << "Deleted layer with ID:" << layerId.toString() << "Remaining layers:" << m_layers.size();
    }
}

void LayerManager::deleteLayer(Layer* layer)
{
    if (layer) {
        deleteLayer(layer->id());
    }
}

std::unique_ptr<Layer> LayerManager::takeLayer(const QUuid& layerId, size_t* indexOut)
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerId](const std::unique_ptr<Layer>& layer) {
            return layer && layer->id() == layerId;
        });

    if (it == m_layers.end()) {
        return nullptr;
    }

    const size_t index = static_cast<size_t>(std::distance(m_layers.begin(), it));
    if (indexOut) {
        *indexOut = index;
    }

    Layer* layerPtr = it->get();
    const bool wasActive = (m_activeLayer == layerPtr);

    emit layerDeleted(layerId);

    std::unique_ptr<Layer> removed = std::move(*it);
    m_layers.erase(it);

    if (wasActive) {
        Layer* newActive = nullptr;
        if (!m_layers.empty()) {
            if (index < m_layers.size()) {
                newActive = m_layers[index].get();
            } else {
                newActive = m_layers.back().get();
            }
        }

        m_activeLayer = newActive;
        emit activeLayerChanged(newActive);
    }

    // Ensure we always have at least one layer
    ensureDefaultLayer();

    // Update z-ordering
    updateLayerOrdering();

    return removed;
}

Layer* LayerManager::insertLayer(std::unique_ptr<Layer> layer, size_t index)
{
    if (!layer) {
        return nullptr;
    }

    if (index > m_layers.size()) {
        index = m_layers.size();
    }

    if (getLayer(layer->id()) != nullptr) {
        qWarning() << "LayerManager: insertLayer() detected duplicate layer ID:"
                   << layer->id().toString();
    }

    Layer* layerPtr = layer.get();
    m_layers.insert(m_layers.begin() + static_cast<long>(index), std::move(layer));

    // Update z-ordering
    updateLayerOrdering();

    emit layerCreated(layerPtr);
    emit layersReordered();

    return layerPtr;
}

void LayerManager::clearLayers()
{
    emit layersReordered(); // Notify before clearing
    m_layers.clear();
    m_activeLayer = nullptr;
    m_nextLayerNumber = 1;
    ensureDefaultLayer();
}

void LayerManager::clearLayersNoDefault()
{
    emit layersReordered(); // Notify before clearing
    m_layers.clear();
    m_activeLayer = nullptr;
    m_nextLayerNumber = 1;
    // Don't call ensureDefaultLayer() - let caller restore layers
}

Layer* LayerManager::getLayer(const QUuid& layerId) const
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerId](const std::unique_ptr<Layer>& layer) {
            return layer->id() == layerId;
        });
    
    return (it != m_layers.end()) ? it->get() : nullptr;
}

Layer* LayerManager::getLayerByName(const QString& name) const
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&name](const std::unique_ptr<Layer>& layer) {
            return layer->name() == name;
        });
    
    return (it != m_layers.end()) ? it->get() : nullptr;
}

Layer* LayerManager::getLayerAt(size_t index) const
{
    return (index < m_layers.size()) ? m_layers[index].get() : nullptr;
}

size_t LayerManager::getLayerIndex(const QUuid& layerId) const
{
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i]->id() == layerId) {
            return i;
        }
    }
    return SIZE_MAX; // Not found
}

size_t LayerManager::getLayerIndex(Layer* layer) const
{
    return layer ? getLayerIndex(layer->id()) : SIZE_MAX;
}

void LayerManager::setActiveLayer(const QUuid& layerId)
{
    Layer* layer = getLayer(layerId);
    setActiveLayer(layer);
}

void LayerManager::setActiveLayer(Layer* layer)
{
    if (m_activeLayer != layer) {
        m_activeLayer = layer;
        emit activeLayerChanged(layer);
        qDebug() << "Active layer changed to:" << (layer ? layer->name() : "None");
    }
}

void LayerManager::setActiveLayer(size_t index)
{
    Layer* layer = getLayerAt(index);
    setActiveLayer(layer);
}

void LayerManager::moveLayerUp(const QUuid& layerId)
{
    size_t index = getLayerIndex(layerId);
    if (index != SIZE_MAX && index < m_layers.size() - 1) {
        std::swap(m_layers[index], m_layers[index + 1]);
        updateLayerOrdering();
        emit layersReordered();
    }
}

void LayerManager::moveLayerDown(const QUuid& layerId)
{
    size_t index = getLayerIndex(layerId);
    if (index != SIZE_MAX && index > 0) {
        std::swap(m_layers[index], m_layers[index - 1]);
        updateLayerOrdering();
        emit layersReordered();
    }
}

void LayerManager::moveLayerUp(Layer* layer)
{
    if (layer) {
        moveLayerUp(layer->id());
    }
}

void LayerManager::moveLayerDown(Layer* layer)
{
    if (layer) {
        moveLayerDown(layer->id());
    }
}

void LayerManager::moveLayerToIndex(const QUuid& layerId, size_t newIndex)
{
    size_t currentIndex = getLayerIndex(layerId);
    if (currentIndex != SIZE_MAX && newIndex < m_layers.size() && currentIndex != newIndex) {
        auto layer = std::move(m_layers[currentIndex]);
        m_layers.erase(m_layers.begin() + currentIndex);
        
        // Adjust newIndex if necessary
        if (newIndex > currentIndex) {
            newIndex--;
        }
        
        m_layers.insert(m_layers.begin() + newIndex, std::move(layer));
        updateLayerOrdering();
        emit layersReordered();
    }
}


void LayerManager::addPrimitiveToActiveLayer(std::unique_ptr<DrawingPrimitive> primitive)
{
    ensureDefaultLayer();
    if (m_activeLayer && primitive) {
        DrawingPrimitive* primitivePtr = primitive.get();
        m_activeLayer->addPrimitive(std::move(primitive));
        emit primitiveAdded(m_activeLayer, primitivePtr);
    }
}

void LayerManager::addPrimitiveToLayer(const QUuid& layerId, std::unique_ptr<DrawingPrimitive> primitive)
{
    Layer* layer = getLayer(layerId);
    if (layer && primitive) {
        layer->addPrimitive(std::move(primitive));
    }
}

void LayerManager::movePrimitiveTo(DrawingPrimitive* primitive, const QUuid& targetLayerId)
{
    Layer* targetLayer = getLayer(targetLayerId);
    movePrimitiveTo(primitive, targetLayer);
}

void LayerManager::movePrimitiveTo(DrawingPrimitive* primitive, Layer* targetLayer)
{
    if (!primitive || !targetLayer) return;
    
    // Find the source layer
    Layer* sourceLayer = findLayerContaining(primitive);
    if (!sourceLayer || sourceLayer == targetLayer) return;
    
    // Find and move the primitive
    auto& sourcePrimitives = sourceLayer->primitives();
    auto it = std::find_if(sourcePrimitives.begin(), sourcePrimitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != sourcePrimitives.end()) {
        auto primitivePtr = std::move(*it);
        sourcePrimitives.erase(it);
        targetLayer->addPrimitive(std::move(primitivePtr));
        qDebug() << "Moved primitive from" << sourceLayer->name() << "to" << targetLayer->name();
    }
}

Layer* LayerManager::findLayerContaining(DrawingPrimitive* primitive) const
{
    for (const auto& layer : m_layers) {
        const auto& primitives = layer->primitives();
        auto it = std::find_if(primitives.begin(), primitives.end(),
            [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
                return p.get() == primitive;
            });
        
        if (it != primitives.end()) {
            return layer.get();
        }
    }
    return nullptr;
}

void LayerManager::showAllLayers()
{
    for (auto& layer : m_layers) {
        if (!layer->isVisible()) {
            layer->setVisible(true);
            emit layerVisibilityChanged(layer.get(), true);
        }
    }
}

void LayerManager::hideAllLayers()
{
    for (auto& layer : m_layers) {
        if (layer->isVisible()) {
            layer->setVisible(false);
            emit layerVisibilityChanged(layer.get(), false);
        }
    }
}

void LayerManager::showOnlyLayer(const QUuid& layerId)
{
    for (auto& layer : m_layers) {
        bool shouldBeVisible = (layer->id() == layerId);
        if (layer->isVisible() != shouldBeVisible) {
            layer->setVisible(shouldBeVisible);
            emit layerVisibilityChanged(layer.get(), shouldBeVisible);
        }
    }
}

QString LayerManager::generateUniqueLayerName(const QString& baseName) const
{
    QString name = baseName;
    int counter = 1;
    
    while (getLayerByName(name) != nullptr) {
        name = QString("%1 %2").arg(baseName).arg(counter);
        counter++;
    }
    
    return name;
}

void LayerManager::updateLayerOrdering()
{
    for (size_t i = 0; i < m_layers.size(); ++i) {
        m_layers[i]->setZOrder(static_cast<int>(i));
    }
}

void LayerManager::reorderLayers()
{
    updateLayerOrdering();
    emit layersReordered();
}

size_t LayerManager::findLayerIndex(Layer* layer) const
{
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].get() == layer) {
            return i;
        }
    }
    return SIZE_MAX;
}

void LayerManager::onLayerPropertyChanged()
{
    // This can be connected to layer property change signals if needed
    // For now, it's a placeholder for future functionality
}

std::vector<DrawingPrimitive*> LayerManager::getAllPrimitives() const
{
    std::vector<DrawingPrimitive*> allPrimitives;
    
    for (const auto& layer : m_layers) {
        if (layer->isVisible()) {
            const auto& primitives = layer->primitives();
            for (const auto& primitive : primitives) {
                allPrimitives.push_back(primitive.get());
            }
        }
    }
    
    return allPrimitives;
}

void LayerManager::bringToFront(DrawingPrimitive* primitive)
{
    Layer* layer = findLayerContaining(primitive);
    if (!layer) return;
    
    auto& primitives = layer->primitives();
    auto it = std::find_if(primitives.begin(), primitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != primitives.end()) {
        auto primitivePtr = std::move(*it);
        primitives.erase(it);
        primitives.push_back(std::move(primitivePtr));
        qDebug() << "Brought primitive to front in layer:" << layer->name();
    }
}

void LayerManager::sendToBack(DrawingPrimitive* primitive)
{
    Layer* layer = findLayerContaining(primitive);
    if (!layer) return;
    
    auto& primitives = layer->primitives();
    auto it = std::find_if(primitives.begin(), primitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != primitives.end()) {
        auto primitivePtr = std::move(*it);
        primitives.erase(it);
        primitives.insert(primitives.begin(), std::move(primitivePtr));
        qDebug() << "Sent primitive to back in layer:" << layer->name();
    }
}

void LayerManager::bringForward(DrawingPrimitive* primitive)
{
    Layer* layer = findLayerContaining(primitive);
    if (!layer) return;
    
    auto& primitives = layer->primitives();
    auto it = std::find_if(primitives.begin(), primitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != primitives.end() && it + 1 != primitives.end()) {
        std::iter_swap(it, it + 1);
        qDebug() << "Brought primitive forward in layer:" << layer->name();
    }
}

void LayerManager::sendBackward(DrawingPrimitive* primitive)
{
    Layer* layer = findLayerContaining(primitive);
    if (!layer) return;
    
    auto& primitives = layer->primitives();
    auto it = std::find_if(primitives.begin(), primitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != primitives.end() && it != primitives.begin()) {
        std::iter_swap(it, it - 1);
        qDebug() << "Sent primitive backward in layer:" << layer->name();
    }
}

void LayerManager::movePrimitiveToTop(DrawingPrimitive* primitive)
{
    bringToFront(primitive);
}

void LayerManager::movePrimitiveToBottom(DrawingPrimitive* primitive)
{
    sendToBack(primitive);
}
