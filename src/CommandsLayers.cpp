#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"
#include "Layer.h"
#include "BrushStrokePrimitive.h"
#include "ImagePrimitive.h"
#include <QDebug>
#include <QJsonDocument>
#include <QColor>
#include <algorithm>
#include <map>
#include <set>

// Layer commands (refactor E20).

// LayerCommand Implementation
LayerCommand::LayerCommand(LayerManager* layerManager, LayerOperation operation, const QString& description)
    : m_layerManager(layerManager)
    , m_operation(operation)
    , m_commandDescription(description)
    , m_executed(false)
{
}

void LayerCommand::execute()
{
    if (!m_executed && m_layerManager) {
        qDebug() << "Executing LayerCommand:" << m_commandDescription;
        
        switch (m_operation) {
            case CreateLayer: {
                Layer* newLayer = m_layerManager->createLayer(m_layerName);
                if (newLayer) {
                    m_layerId = newLayer->id();
                }
                break;
            }
                
            case DeleteLayer:
                // Layer deletion is handled by LayerManager
                break;
                
            case MoveToLayer:
                for (const auto& primitive : m_primitives) {
                    if (!primitive) continue;
                    Layer* targetLayer = m_layerManager->getLayer(m_toLayerId);
                    if (targetLayer) {
                        m_layerManager->movePrimitiveTo(primitive.data(), targetLayer);
                    }
                }
                break;

            case ReorderLayer:
                // Layer reordering would need to be implemented in LayerManager
                break;
        }

        m_executed = true;
    }
}

void LayerCommand::undo()
{
    if (m_executed && m_layerManager) {
        qDebug() << "Undoing LayerCommand:" << m_commandDescription;

        switch (m_operation) {
            case CreateLayer:
                m_layerManager->deleteLayer(m_layerId);
                break;

            case DeleteLayer:
                // Restore deleted layer (would need implementation)
                break;

            case MoveToLayer:
                for (const auto& primitive : m_primitives) {
                    if (!primitive) continue;
                    Layer* sourceLayer = m_layerManager->getLayer(m_fromLayerId);
                    if (sourceLayer) {
                        m_layerManager->movePrimitiveTo(primitive.data(), sourceLayer);
                    }
                }
                break;
                
            case ReorderLayer:
                // Restore layer order
                break;
        }
        
        m_executed = false;
    }
}

QString LayerCommand::description() const
{
    return m_commandDescription;
}

void LayerCommand::setCreateLayerData(const QString& layerName)
{
    m_layerName = layerName;
}

void LayerCommand::setDeleteLayerData(const QUuid& layerId, const QJsonObject& layerData)
{
    m_layerId = layerId;
    m_layerData = layerData;
}

void LayerCommand::setMoveToLayerData(const std::vector<DrawingPrimitive*>& primitives,
                                     const QUuid& fromLayerId, const QUuid& toLayerId)
{
    m_primitives.clear();
    m_primitives.reserve(primitives.size());
    for (auto* p : primitives) m_primitives.emplace_back(p);
    m_fromLayerId = fromLayerId;
    m_toLayerId = toLayerId;
}

void LayerCommand::setReorderLayerData(const QUuid& layerId, int oldIndex, int newIndex)
{
    m_layerId = layerId;
    m_oldIndex = oldIndex;
    m_newIndex = newIndex;
}

// ============================================================================
// Layer UI Commands (LayerPanel)
// ============================================================================

CreateLayerCommand::CreateLayerCommand(LayerManager* layerManager, const QString& name)
    : m_layerManager(layerManager)
    , m_layerName(name)
{
}

void CreateLayerCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    if (Layer* active = m_layerManager->activeLayer()) {
        m_previousActiveLayerId = active->id();
    }

    Layer* createdLayer = nullptr;
    if (m_layer) {
        createdLayer = m_layerManager->insertLayer(std::move(m_layer), m_insertIndex);
    } else {
        createdLayer = m_layerManager->createLayer(m_layerName);
        if (createdLayer) {
            m_layerId = createdLayer->id();
            m_insertIndex = m_layerManager->getLayerIndex(createdLayer);
        }
    }

    if (createdLayer) {
        m_layerManager->setActiveLayer(createdLayer);
    }

    m_executed = true;
}

void CreateLayerCommand::undo()
{
    if (!m_executed || !m_layerManager || m_layerId.isNull()) {
        return;
    }

    size_t removedIndex = 0;
    m_layer = m_layerManager->takeLayer(m_layerId, &removedIndex);
    m_insertIndex = removedIndex;

    if (!m_previousActiveLayerId.isNull() &&
        m_layerManager->getLayer(m_previousActiveLayerId)) {
        m_layerManager->setActiveLayer(m_previousActiveLayerId);
    }

    m_executed = false;
}

QString CreateLayerCommand::description() const
{
    return "Create Layer";
}

DeleteLayerCommand::DeleteLayerCommand(LayerManager* layerManager, const QUuid& layerId)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
{
}

void DeleteLayerCommand::execute()
{
    if (m_executed || !m_layerManager || m_layerId.isNull()) {
        return;
    }

    if (m_layerManager->layerCount() <= 1) {
        qDebug() << "DeleteLayerCommand: refusing to delete last layer";
        return;
    }

    if (Layer* active = m_layerManager->activeLayer()) {
        m_previousActiveLayerId = active->id();
        m_deletedWasActive = (active->id() == m_layerId);
    }

    m_removedLayer = m_layerManager->takeLayer(m_layerId, &m_removedIndex);
    if (!m_removedLayer) {
        return;
    }

    m_executed = true;
}

void DeleteLayerCommand::undo()
{
    if (!m_executed || !m_layerManager || !m_removedLayer) {
        return;
    }

    Layer* restored =
        m_layerManager->insertLayer(std::move(m_removedLayer), m_removedIndex);

    if (m_deletedWasActive) {
        if (restored) {
            m_layerManager->setActiveLayer(restored);
        }
    } else if (!m_previousActiveLayerId.isNull() &&
               m_layerManager->getLayer(m_previousActiveLayerId)) {
        m_layerManager->setActiveLayer(m_previousActiveLayerId);
    }

    m_executed = false;
}

QString DeleteLayerCommand::description() const
{
    return "Delete Layer";
}
ReorderLayerCommand::ReorderLayerCommand(LayerManager* layerManager, const QUuid& layerId,
                                         int oldIndex, int newIndex, const QString& description)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldIndex(oldIndex)
    , m_newIndex(newIndex)
    , m_description(description)
{
}

void ReorderLayerCommand::execute()
{
    if (m_executed || !m_layerManager || m_layerId.isNull()) {
        return;
    }

    const size_t count = m_layerManager->layerCount();
    if (m_newIndex < 0 || static_cast<size_t>(m_newIndex) >= count) {
        return;
    }

    m_layerManager->moveLayerToIndex(m_layerId, static_cast<size_t>(m_newIndex));
    m_executed = true;
}

void ReorderLayerCommand::undo()
{
    if (!m_executed || !m_layerManager || m_layerId.isNull()) {
        return;
    }

    const size_t count = m_layerManager->layerCount();
    if (m_oldIndex < 0 || static_cast<size_t>(m_oldIndex) >= count) {
        return;
    }

    m_layerManager->moveLayerToIndex(m_layerId, static_cast<size_t>(m_oldIndex));
    m_executed = false;
}

QString ReorderLayerCommand::description() const
{
    return m_description;
}

SetLayerVisibilityCommand::SetLayerVisibilityCommand(LayerManager* layerManager, const QUuid& layerId,
                                                     bool oldVisible, bool newVisible)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldVisible(oldVisible)
    , m_newVisible(newVisible)
{
}

void SetLayerVisibilityCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setVisible(m_newVisible);
    emit m_layerManager->layerVisibilityChanged(layer, m_newVisible);
    m_executed = true;
}

void SetLayerVisibilityCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setVisible(m_oldVisible);
    emit m_layerManager->layerVisibilityChanged(layer, m_oldVisible);
    m_executed = false;
}

QString SetLayerVisibilityCommand::description() const
{
    return "Change Layer Visibility";
}

SetLayerLockCommand::SetLayerLockCommand(LayerManager* layerManager, const QUuid& layerId,
                                         bool oldLocked, bool newLocked)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldLocked(oldLocked)
    , m_newLocked(newLocked)
{
}

void SetLayerLockCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setLocked(m_newLocked);
    emit m_layerManager->layerLockChanged(layer, m_newLocked);
    m_executed = true;
}

void SetLayerLockCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setLocked(m_oldLocked);
    emit m_layerManager->layerLockChanged(layer, m_oldLocked);
    m_executed = false;
}

QString SetLayerLockCommand::description() const
{
    return "Change Layer Lock";
}

SetLayerOpacityCommand::SetLayerOpacityCommand(LayerManager* layerManager, const QUuid& layerId,
                                               float oldOpacity, float newOpacity)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldOpacity(oldOpacity)
    , m_newOpacity(newOpacity)
{
}

void SetLayerOpacityCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setOpacity(m_newOpacity);
    emit m_layerManager->layerOpacityChanged(layer, m_newOpacity);
    m_executed = true;
}

void SetLayerOpacityCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setOpacity(m_oldOpacity);
    emit m_layerManager->layerOpacityChanged(layer, m_oldOpacity);
    m_executed = false;
}

QString SetLayerOpacityCommand::description() const
{
    return "Change Layer Opacity";
}

RenameLayerCommand::RenameLayerCommand(LayerManager* layerManager, const QUuid& layerId,
                                       const QString& oldName, const QString& newName)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldName(oldName)
    , m_newName(newName)
{
}

void RenameLayerCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setName(m_newName);
    emit m_layerManager->layerRenamed(layer, m_oldName);
    m_executed = true;
}

void RenameLayerCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setName(m_oldName);
    emit m_layerManager->layerRenamed(layer, m_newName);
    m_executed = false;
}

QString RenameLayerCommand::description() const
{
    return "Rename Layer";
}

SetLayerBlendModeCommand::SetLayerBlendModeCommand(LayerManager* layerManager, const QUuid& layerId,
                                                   int oldBlendMode, int newBlendMode)
    : m_layerManager(layerManager)
    , m_layerId(layerId)
    , m_oldBlendMode(oldBlendMode)
    , m_newBlendMode(newBlendMode)
{
}

void SetLayerBlendModeCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setBlendMode(static_cast<Layer::BlendMode>(m_newBlendMode));
    emit m_layerManager->layerBlendModeChanged(layer, m_newBlendMode);
    m_executed = true;
}

void SetLayerBlendModeCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    Layer* layer = m_layerManager->getLayer(m_layerId);
    if (!layer) {
        return;
    }

    layer->setBlendMode(static_cast<Layer::BlendMode>(m_oldBlendMode));
    emit m_layerManager->layerBlendModeChanged(layer, m_oldBlendMode);
    m_executed = false;
}

QString SetLayerBlendModeCommand::description() const
{
    return "Change Blend Mode";
}

SetAllLayersVisibilityCommand::SetAllLayersVisibilityCommand(LayerManager* layerManager, bool newVisible)
    : m_layerManager(layerManager)
    , m_newVisible(newVisible)
{
}

void SetAllLayersVisibilityCommand::execute()
{
    if (m_executed || !m_layerManager) {
        return;
    }

    m_layerIds.clear();
    m_oldVisibilities.clear();
    for (const auto& layer : m_layerManager->layers()) {
        if (!layer) {
            continue;
        }
        m_layerIds.push_back(layer->id());
        m_oldVisibilities.push_back(layer->isVisible());
    }

    for (size_t i = 0; i < m_layerIds.size(); ++i) {
        Layer* layer = m_layerManager->getLayer(m_layerIds[i]);
        if (!layer) {
            continue;
        }
        layer->setVisible(m_newVisible);
        emit m_layerManager->layerVisibilityChanged(layer, m_newVisible);
    }

    m_executed = true;
}

void SetAllLayersVisibilityCommand::undo()
{
    if (!m_executed || !m_layerManager) {
        return;
    }

    const size_t count = std::min(m_layerIds.size(), m_oldVisibilities.size());
    for (size_t i = 0; i < count; ++i) {
        Layer* layer = m_layerManager->getLayer(m_layerIds[i]);
        if (!layer) {
            continue;
        }
        layer->setVisible(m_oldVisibilities[i]);
        emit m_layerManager->layerVisibilityChanged(layer, m_oldVisibilities[i]);
    }

    m_executed = false;
}

QString SetAllLayersVisibilityCommand::description() const
{
    return m_newVisible ? "Show All Layers" : "Hide All Layers";
}

SetPrimitiveVisibilityCommand::SetPrimitiveVisibilityCommand(DrawingPrimitive* primitive,
                                                             bool oldVisible, bool newVisible)
    : m_primitive(primitive)
    , m_oldVisible(oldVisible)
    , m_newVisible(newVisible)
{
}

void SetPrimitiveVisibilityCommand::execute()
{
    if (m_executed || !m_primitive) {
        return;
    }

    m_primitive->setVisible(m_newVisible);
    m_executed = true;
}

void SetPrimitiveVisibilityCommand::undo()
{
    if (!m_executed || !m_primitive) {
        return;
    }

    m_primitive->setVisible(m_oldVisible);
    m_executed = false;
}

QString SetPrimitiveVisibilityCommand::description() const
{
    return "Change Object Visibility";
}

