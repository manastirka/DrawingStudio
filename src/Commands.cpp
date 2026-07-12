#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"
#include "Layer.h"
#include "BrushStrokePrimitive.h"
#include "ImagePrimitive.h"
#include <QDebug>
#include <QJsonDocument>
#include <algorithm>
#include <map>
#include <set>

// AddPrimitiveCommand Implementation
AddPrimitiveCommand::AddPrimitiveCommand(DrawingCanvas* canvas, std::unique_ptr<DrawingPrimitive> primitive)
    : m_canvas(canvas)
    , m_primitive(std::move(primitive))
    , m_executed(false)
{
    if (m_primitive) {
        m_primitiveData = m_primitive->toJson();
    }
}

void AddPrimitiveCommand::execute()
{
    if (!m_executed && m_canvas) {
        if (m_primitive) {
            // First execution: add the primitive
            qDebug() << "Executing AddPrimitiveCommand (first time)";
            m_canvas->addPrimitive(std::move(m_primitive));
        } else if (!m_primitiveData.isEmpty()) {
            // Redo: recreate from JSON
            qDebug() << "Executing AddPrimitiveCommand (redo)";
            auto recreatedPrimitive = DrawingPrimitive::createFromJson(m_primitiveData);
            if (recreatedPrimitive) {
                m_canvas->addPrimitive(std::move(recreatedPrimitive));
            }
        }
        m_executed = true;
    }
}

void AddPrimitiveCommand::undo()
{
    if (m_executed && m_canvas && !m_primitiveData.isEmpty()) {
        qDebug() << "Undoing AddPrimitiveCommand";
        
        // Find the primitive in the canvas/layers and remove it
        LayerManager* layerManager = m_canvas->layerManager();
        if (layerManager) {
            qDebug() << "Searching through" << layerManager->layers().size() << "layers";
            
            // Find the layer containing this primitive
            for (auto& layer : layerManager->layers()) {
                auto& primitives = layer->primitives();
                qDebug() << "Layer has" << primitives.size() << "primitives";
                
                for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                    // Compare by ID (most reliable) or by JSON data
                    QJsonObject currentData = (*it)->toJson();
                    
                    // Compare key properties to find the matching primitive
                    if (currentData["type"] == m_primitiveData["type"] &&
                        currentData["id"] == m_primitiveData["id"]) {
                        
                        qDebug() << "Found matching primitive, removing it";
                        m_primitive = std::move(*it);
                        primitives.erase(it);
                        m_executed = false;
                        m_canvas->update();
                        return;
                    }
                }
            }
            qDebug() << "WARNING: Could not find primitive to remove!";
        } else {
            qDebug() << "WARNING: No layer manager!";
        }
    }
}

QString AddPrimitiveCommand::description() const
{
    if (m_primitive) {
        switch (m_primitive->type()) {
            case PrimitiveType::Line: return "Add Line";
            case PrimitiveType::Rectangle: return "Add Rectangle";
            case PrimitiveType::Circle: return "Add Circle";
            case PrimitiveType::Ellipse: return "Add Ellipse";
            case PrimitiveType::Curve: return "Add Curve";
            case PrimitiveType::BezierCurve: return "Add Bezier Curve";
            case PrimitiveType::Spline: return "Add Spline";
            case PrimitiveType::Arc: return "Add Arc";
            case PrimitiveType::Polygon: return "Add Polygon";
            case PrimitiveType::Text: return "Add Text";
            case PrimitiveType::Dimension: return "Add Dimension";
            case PrimitiveType::Image: return "Add Image";
            default: return "Add Object";
        }
    }
    return "Add Object";
}

// DeletePrimitivesCommand Implementation
DeletePrimitivesCommand::DeletePrimitivesCommand(DrawingCanvas* canvas, const std::vector<DrawingPrimitive*>& primitives)
    : m_canvas(canvas)
{
    // Store the primitives' IDs and indices for restoration.
    LayerManager* layerManager = canvas ? canvas->layerManager() : nullptr;
    if (layerManager) {
        for (DrawingPrimitive* primitive : primitives) {
            if (!primitive) {
                continue;
            }

            DeletedPrimitiveInfo info;
            info.primitiveId = primitive->id();

            // Find the layer and index for restoration
            Layer* layer = layerManager->findLayerContaining(primitive);
            if (layer) {
                info.layerId = layer->id();
                const auto& layerPrimitives = layer->primitives();
                for (size_t i = 0; i < layerPrimitives.size(); ++i) {
                    if (layerPrimitives[i].get() == primitive) {
                        info.index = static_cast<int>(i);
                        break;
                    }
                }
            } else {
                info.layerId = primitive->layerId();
            }

            m_deletedPrimitives.push_back(std::move(info));
        }
    } else {
        for (DrawingPrimitive* primitive : primitives) {
            if (!primitive) {
                continue;
            }
            DeletedPrimitiveInfo info;
            info.primitiveId = primitive->id();
            info.layerId = primitive->layerId();
            m_deletedPrimitives.push_back(std::move(info));
        }
    }
    
    m_commandDescription = QString("Delete %1 Object(s)").arg(m_deletedPrimitives.size());
}

void DeletePrimitivesCommand::execute()
{
    if (m_executed || !m_canvas) {
        return;
    }

    qDebug() << "Executing DeletePrimitivesCommand";

    LayerManager* layerManager = m_canvas->layerManager();
    if (!layerManager || m_deletedPrimitives.empty()) {
        // Fallback: delete current selection
        m_canvas->deleteSelectedPrimitives();
        m_executed = true;
        return;
    }

    std::map<QString, size_t> idToInfoIndex;
    for (size_t i = 0; i < m_deletedPrimitives.size(); ++i) {
        if (!m_deletedPrimitives[i].primitiveId.isNull()) {
            idToInfoIndex.emplace(m_deletedPrimitives[i].primitiveId.toString(), i);
        }
    }

    if (idToInfoIndex.empty()) {
        m_executed = true;
        return;
    }

    bool changed = false;
    for (const auto& layer : layerManager->layers()) {
        if (!layer || !layer->isVisible() || layer->isLocked()) {
            continue;
        }

        auto& layerPrimitives = layer->primitives();
        for (size_t idx = 0; idx < layerPrimitives.size(); /* increment inside */) {
            const auto& prim = layerPrimitives[idx];
            if (!prim) {
                ++idx;
                continue;
            }

            auto it = idToInfoIndex.find(prim->id().toString());
            if (it == idToInfoIndex.end()) {
                ++idx;
                continue;
            }

            DeletedPrimitiveInfo& info = m_deletedPrimitives[it->second];
            info.layerId = layer->id();
            if (info.index < 0) {
                info.index = static_cast<int>(idx);
            }
            info.primitive = std::move(layerPrimitives[idx]);
            layerPrimitives.erase(layerPrimitives.begin() + static_cast<long>(idx));
            changed = true;
            continue;
        }
    }

    m_canvas->clearSelection();
    if (changed) {
        m_canvas->update();
    }

    m_executed = true;
}

void DeletePrimitivesCommand::undo()
{
    if (!m_executed || !m_canvas || m_deletedPrimitives.empty()) {
        return;
    }

    qDebug() << "Undoing DeletePrimitivesCommand";

    LayerManager* layerManager = m_canvas->layerManager();
    if (!layerManager) {
        return;
    }

    // Restore primitives in their original layers/positions (best-effort).
    // Insert in descending index order per layer to keep indices stable.
    std::vector<size_t> order;
    order.reserve(m_deletedPrimitives.size());
    for (size_t i = 0; i < m_deletedPrimitives.size(); ++i) {
        order.push_back(i);
    }

    std::stable_sort(order.begin(), order.end(),
                     [this](size_t a, size_t b) {
                         const auto& ia = m_deletedPrimitives[a];
                         const auto& ib = m_deletedPrimitives[b];
                         if (ia.layerId == ib.layerId) {
                             return ia.index > ib.index; // descending
                         }
                         return ia.layerId.toString() < ib.layerId.toString();
                     });

    for (size_t idx : order) {
        DeletedPrimitiveInfo& info = m_deletedPrimitives[idx];
        if (!info.primitive) {
            continue;
        }

        Layer* targetLayer = layerManager->getLayer(info.layerId);
        if (!targetLayer) {
            targetLayer = layerManager->activeLayer();
        }
        if (!targetLayer) {
            targetLayer = layerManager->createLayer("Background");
            layerManager->setActiveLayer(targetLayer);
        }

        info.primitive->setLayerId(targetLayer->id());
        auto& primitives = targetLayer->primitives();
        size_t insertIndex = primitives.size();
        if (info.index >= 0) {
            insertIndex = std::min(static_cast<size_t>(info.index), primitives.size());
        }

        primitives.insert(primitives.begin() + static_cast<long>(insertIndex),
                          std::move(info.primitive));
    }

    m_canvas->update();
    m_executed = false;
}

QString DeletePrimitivesCommand::description() const
{
    return m_commandDescription;
}

// ModifyPrimitiveCommand Implementation
ModifyPrimitiveCommand::ModifyPrimitiveCommand(DrawingPrimitive* primitive, const QString& propertyName)
    : m_primitive(primitive)
    , m_propertyName(propertyName)
    , m_executed(false)
{
    if (m_primitive) {
        m_oldState = m_primitive->toJson();
    }
}

void ModifyPrimitiveCommand::execute()
{
    if (!m_executed && m_primitive && !m_newState.isEmpty()) {
        qDebug() << "Executing ModifyPrimitiveCommand for" << m_propertyName;
        if (!m_alreadyApplied) {
            m_primitive->fromJson(m_newState);
        }
        m_executed = true;
        // Only skip application once (initial push). Redo should apply.
        m_alreadyApplied = false;
    }
}

void ModifyPrimitiveCommand::undo()
{
    if (m_executed && m_primitive && !m_oldState.isEmpty()) {
        qDebug() << "Undoing ModifyPrimitiveCommand for" << m_propertyName;
        m_primitive->fromJson(m_oldState);
        m_executed = false;
    }
}

QString ModifyPrimitiveCommand::description() const
{
    return QString("Modify %1").arg(m_propertyName);
}

bool ModifyPrimitiveCommand::canMergeWith(const Command* other) const
{
    const ModifyPrimitiveCommand* otherModify = dynamic_cast<const ModifyPrimitiveCommand*>(other);
    return otherModify && 
           otherModify->m_primitive == m_primitive && 
           otherModify->m_propertyName == m_propertyName;
}

void ModifyPrimitiveCommand::mergeWith(const Command* other)
{
    const ModifyPrimitiveCommand* otherModify = dynamic_cast<const ModifyPrimitiveCommand*>(other);
    if (otherModify) {
        m_newState = otherModify->m_newState;
    }
}

void ModifyPrimitiveCommand::storeColorChange(const QColor& oldColor, const QColor& newColor)
{
    Q_UNUSED(oldColor)
    Q_UNUSED(newColor)
    m_newState = m_primitive->toJson();
}

void ModifyPrimitiveCommand::storeLineWidthChange(float oldWidth, float newWidth)
{
    Q_UNUSED(oldWidth)
    Q_UNUSED(newWidth)
    m_newState = m_primitive->toJson();
}

void ModifyPrimitiveCommand::storePositionChange(const QVector2D& oldPos, const QVector2D& newPos)
{
    Q_UNUSED(oldPos)
    Q_UNUSED(newPos)
    m_newState = m_primitive->toJson();
}

void ModifyPrimitiveCommand::storeControlPointChange(int index, const QVector2D& oldPos, const QVector2D& newPos)
{
    Q_UNUSED(index)
    Q_UNUSED(oldPos)
    Q_UNUSED(newPos)
    m_newState = m_primitive->toJson();
}

void ModifyPrimitiveCommand::storePropertyChange(const QJsonObject& oldState, const QJsonObject& newState)
{
    m_oldState = oldState;
    m_newState = newState;
}

void ModifyPrimitiveCommand::captureNewState()
{
    if (m_primitive) {
        m_newState = m_primitive->toJson();
    }
}

bool ModifyPrimitiveCommand::hasStateChange() const
{
    if (m_oldState.isEmpty() || m_newState.isEmpty()) {
        return false;
    }
    return m_oldState != m_newState;
}

// MovePrimitivesCommand Implementation
MovePrimitivesCommand::MovePrimitivesCommand(const std::vector<DrawingPrimitive*>& primitives, const QVector2D& offset)
    : m_offset(offset)
    , m_totalOffset(offset)
{
    m_primitives.reserve(primitives.size());
    for (auto* p : primitives) m_primitives.emplace_back(p);
}

void MovePrimitivesCommand::execute()
{
    qDebug() << "Executing MovePrimitivesCommand";
    for (const auto& primitive : m_primitives) {
        if (primitive) {
            primitive->translate(m_offset);
        }
    }
}

void MovePrimitivesCommand::undo()
{
    qDebug() << "Undoing MovePrimitivesCommand";
    for (const auto& primitive : m_primitives) {
        if (primitive) {
            primitive->translate(-m_totalOffset);
        }
    }
}

QString MovePrimitivesCommand::description() const
{
    return QString("Move %1 Object(s)").arg(m_primitives.size());
}

bool MovePrimitivesCommand::canMergeWith(const Command* other) const
{
    const MovePrimitivesCommand* otherMove = dynamic_cast<const MovePrimitivesCommand*>(other);
    if (!otherMove || otherMove->m_primitives.size() != m_primitives.size()) return false;
    for (size_t i = 0; i < m_primitives.size(); ++i) {
        if (m_primitives[i].data() != otherMove->m_primitives[i].data()) return false;
    }
    return true;
}

void MovePrimitivesCommand::mergeWith(const Command* other)
{
    const MovePrimitivesCommand* otherMove = dynamic_cast<const MovePrimitivesCommand*>(other);
    if (otherMove) {
        m_totalOffset += otherMove->m_offset;
        m_offset = otherMove->m_offset;
    }
}

// TransformPrimitivesCommand Implementation
TransformPrimitivesCommand::TransformPrimitivesCommand(const std::vector<DrawingPrimitive*>& primitives,
                                                     TransformType type, const QString& description)
    : m_transformType(type)
    , m_commandDescription(description)
    , m_executed(false)
{
    m_primitives.reserve(primitives.size());
    for (auto* p : primitives) {
        m_primitives.emplace_back(p);
        if (p) m_oldStates.push_back(p->toJson());
    }
}

void TransformPrimitivesCommand::execute()
{
    if (!m_executed && !m_newStates.empty()) {
        qDebug() << "Executing TransformPrimitivesCommand";
        for (size_t i = 0; i < m_primitives.size() && i < m_newStates.size(); ++i) {
            if (m_primitives[i]) {
                m_primitives[i]->fromJson(m_newStates[i]);
            }
        }
        m_executed = true;
    }
}

void TransformPrimitivesCommand::undo()
{
    if (m_executed && !m_oldStates.empty()) {
        qDebug() << "Undoing TransformPrimitivesCommand";
        for (size_t i = 0; i < m_primitives.size() && i < m_oldStates.size(); ++i) {
            if (m_primitives[i]) {
                m_primitives[i]->fromJson(m_oldStates[i]);
            }
        }
        m_executed = false;
    }
}

QString TransformPrimitivesCommand::description() const
{
    return m_commandDescription;
}

void TransformPrimitivesCommand::storeTransformation(const std::vector<QJsonObject>& oldStates, 
                                                   const std::vector<QJsonObject>& newStates)
{
    m_oldStates = oldStates;
    m_newStates = newStates;
    // Transformation already applied on canvas — undo must restore oldStates
    m_executed = true;
}

// ModifyControlPointCommand Implementation
ModifyControlPointCommand::ModifyControlPointCommand(DrawingPrimitive* primitive, int controlPointIndex,
                                                   const QVector2D& oldPosition, const QVector2D& newPosition)
    : m_primitive(primitive)
    , m_controlPointIndex(controlPointIndex)
    , m_oldPosition(oldPosition)
    , m_newPosition(newPosition)
{
}

void ModifyControlPointCommand::execute()
{
    if (m_primitive) {
        qDebug() << "Executing ModifyControlPointCommand";
        m_primitive->setControlPointPosition(m_controlPointIndex, m_newPosition);
    }
}

void ModifyControlPointCommand::undo()
{
    if (m_primitive) {
        qDebug() << "Undoing ModifyControlPointCommand";
        m_primitive->setControlPointPosition(m_controlPointIndex, m_oldPosition);
    }
}

QString ModifyControlPointCommand::description() const
{
    return QString("Modify Control Point %1").arg(m_controlPointIndex);
}

bool ModifyControlPointCommand::canMergeWith(const Command* other) const
{
    const ModifyControlPointCommand* otherModify = dynamic_cast<const ModifyControlPointCommand*>(other);
    return otherModify && 
           otherModify->m_primitive == m_primitive && 
           otherModify->m_controlPointIndex == m_controlPointIndex;
}

void ModifyControlPointCommand::mergeWith(const Command* other)
{
    const ModifyControlPointCommand* otherModify = dynamic_cast<const ModifyControlPointCommand*>(other);
    if (otherModify) {
        m_newPosition = otherModify->m_newPosition;
    }
}

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

// BrushStrokeCommand Implementation
BrushStrokeCommand::BrushStrokeCommand(DrawingCanvas* canvas, std::unique_ptr<DrawingPrimitive> brushStroke)
    : m_canvas(canvas)
    , m_brushStroke(std::move(brushStroke))
    , m_executed(false)
{
    if (m_brushStroke) {
        m_strokeData = m_brushStroke->toJson();
    }
}

void BrushStrokeCommand::execute()
{
    if (!m_executed && m_brushStroke && m_canvas) {
        qDebug() << "Executing BrushStrokeCommand";
        m_canvas->addPrimitive(std::move(m_brushStroke));
        m_executed = true;
    }
}

void BrushStrokeCommand::undo()
{
    if (m_executed && m_canvas && !m_strokeData.isEmpty()) {
        qDebug() << "Undoing BrushStrokeCommand";
        
        // Find and remove the brush stroke
        LayerManager* layerManager = m_canvas->layerManager();
        if (layerManager) {
            auto recreatedStroke = DrawingPrimitive::createFromJson(m_strokeData);
            if (recreatedStroke) {
                // Find the stroke in layers and remove it
                for (auto& layer : layerManager->layers()) {
                    auto& primitives = layer->primitives();
                    for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                        if ((*it)->type() == recreatedStroke->type()) {
                            QJsonObject currentData = (*it)->toJson();
                            if (currentData == m_strokeData) {
                                m_brushStroke = std::move(*it);
                                primitives.erase(it);
                                m_executed = false;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

QString BrushStrokeCommand::description() const
{
    return "Brush Stroke";
}

bool BrushStrokeCommand::canMergeWith(const Command* other) const
{
    // Brush strokes can be merged if they're consecutive
    const BrushStrokeCommand* otherBrush = dynamic_cast<const BrushStrokeCommand*>(other);
    return otherBrush && otherBrush->m_canvas == m_canvas;
}

void BrushStrokeCommand::mergeWith(const Command* other)
{
    const BrushStrokeCommand* otherBrush = dynamic_cast<const BrushStrokeCommand*>(other);
    if (otherBrush && otherBrush->m_brushStroke) {
        // Merge brush stroke data (combine points)
        BrushStrokePrimitive* thisBrush = dynamic_cast<BrushStrokePrimitive*>(m_brushStroke.get());
        BrushStrokePrimitive* otherBrushPrimitive = dynamic_cast<BrushStrokePrimitive*>(otherBrush->m_brushStroke.get());
        
        if (thisBrush && otherBrushPrimitive) {
            auto thisPoints = thisBrush->points();
            auto otherPoints = otherBrushPrimitive->points();
            thisPoints.insert(thisPoints.end(), otherPoints.begin(), otherPoints.end());
            thisBrush->setPoints(thisPoints);
            m_strokeData = m_brushStroke->toJson();
        }
    }
}

void BrushStrokeCommand::addPoint(const QVector2D& point)
{
    BrushStrokePrimitive* brushPrimitive = dynamic_cast<BrushStrokePrimitive*>(m_brushStroke.get());
    if (brushPrimitive) {
        auto points = brushPrimitive->points();
        points.push_back(point);
        brushPrimitive->setPoints(points);
        m_strokeData = m_brushStroke->toJson();
    }
}

// CompoundCommand Implementation
// ============================================================================
// ExtractSubjectCommand
// ============================================================================

ExtractSubjectCommand::ExtractSubjectCommand(DrawingCanvas* canvas, ImagePrimitive* sourceImage)
    : m_canvas(canvas)
    , m_sourceImage(sourceImage)
    , m_executed(false)
{
}

void ExtractSubjectCommand::execute()
{
    if (m_executed || !m_canvas) {
        return;
    }

    qDebug() << "Executing ExtractSubjectCommand";

    // First execution: extract; Redo: re-add the stored cutout.
    if (!m_extractedImage && m_sourceImage) {
        m_extractedImage = m_sourceImage->extractDetectedSubject();
    }

    if (!m_extractedImage) {
        qDebug() << "Failed to extract subject";
        return;
    }

    // Add to canvas
    ImagePrimitive* extractedPtr = m_extractedImage.get();
    m_extractedImageId = extractedPtr->id();
    m_canvas->addPrimitive(std::move(m_extractedImage));

    // Clear selection and select the extracted cutout
    m_canvas->clearSelection();
    m_canvas->addToSelection(extractedPtr);

    // Auto-activate Select tool — supports both move AND resize handles for the cutout
    m_canvas->setCurrentTool(DrawingTool::Select);

    m_executed = true;
    qDebug() << "Subject extracted successfully";
}

void ExtractSubjectCommand::undo()
{
    if (!m_executed || !m_canvas) {
        return;
    }

    qDebug() << "Undoing ExtractSubjectCommand";

    LayerManager* layerManager = m_canvas->layerManager();
    if (layerManager && !m_extractedImageId.isNull()) {
        for (const auto& layer : layerManager->layers()) {
            if (!layer) {
                continue;
            }
            auto& primitives = layer->primitives();
            for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                if (!(*it) || (*it)->id() != m_extractedImageId) {
                    continue;
                }

                std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                primitives.erase(it);
                m_extractedImage.reset(dynamic_cast<ImagePrimitive*>(removed.release()));

                m_canvas->clearSelection();
                m_canvas->update();
                m_executed = false;
                qDebug() << "Extracted subject removed from canvas";
                return;
            }
        }
    } else {
        // Legacy mode fallback: search in flat primitives list
        auto& primitives = const_cast<std::vector<std::unique_ptr<DrawingPrimitive>>&>(m_canvas->primitives());
        for (auto it = primitives.begin(); it != primitives.end(); ++it) {
            if (!(*it) || (*it)->id() != m_extractedImageId) {
                continue;
            }

            std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
            primitives.erase(it);
            m_extractedImage.reset(dynamic_cast<ImagePrimitive*>(removed.release()));
            m_canvas->clearSelection();
            m_canvas->update();
            m_executed = false;
            qDebug() << "Extracted subject removed from canvas";
            return;
        }
    }

    qDebug() << "Warning: Could not find extracted image to remove";
}

QString ExtractSubjectCommand::description() const
{
    return "Extract Subject";
}

// ============================================================================
// EditTextCommand
// ============================================================================

EditTextCommand::EditTextCommand(TextPrimitive* textPrimitive, const QString& description)
    : m_textPrimitive(textPrimitive)
    , m_description(description)
    , m_executed(false)
{
}

void EditTextCommand::storeOldState(const QJsonObject& oldState)
{
    m_oldState = oldState;
}

void EditTextCommand::storeNewState(const QJsonObject& newState)
{
    m_newState = newState;
}

void EditTextCommand::execute()
{
    if (!m_executed && m_textPrimitive) {
        qDebug() << "Executing EditTextCommand";
        m_textPrimitive->fromJson(m_newState);
        m_executed = true;
    }
}

void EditTextCommand::undo()
{
    if (m_executed && m_textPrimitive) {
        qDebug() << "Undoing EditTextCommand";
        m_textPrimitive->fromJson(m_oldState);
        m_executed = false;
    }
}

QString EditTextCommand::description() const
{
    return m_description;
}

// ============================================================================
// ImportImageCommand
// ============================================================================

ImportImageCommand::ImportImageCommand(DrawingCanvas* canvas, std::unique_ptr<ImagePrimitive> image)
    : m_canvas(canvas)
    , m_image(std::move(image))
    , m_executed(false)
{
}

void ImportImageCommand::execute()
{
    if (!m_executed && m_image && m_canvas) {
        qDebug() << "Executing ImportImageCommand";
        m_imageId = m_image->id();
        m_canvas->addPrimitive(std::move(m_image));
        m_executed = true;
    }
}

void ImportImageCommand::undo()
{
    if (m_executed && m_canvas) {
        qDebug() << "Undoing ImportImageCommand";

        LayerManager* layerManager = m_canvas->layerManager();
        if (layerManager && !m_imageId.isNull()) {
            for (const auto& layer : layerManager->layers()) {
                if (!layer) {
                    continue;
                }
                auto& primitives = layer->primitives();
                for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                    if (!(*it) || (*it)->id() != m_imageId) {
                        continue;
                    }

                    std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                    primitives.erase(it);
                    m_image.reset(dynamic_cast<ImagePrimitive*>(removed.release()));

                    m_canvas->clearSelection();
                    m_canvas->update();
                    m_executed = false;
                    qDebug() << "Imported image removed from canvas";
                    return;
                }
            }
        } else {
            // Legacy mode fallback
            auto& primitives = const_cast<std::vector<std::unique_ptr<DrawingPrimitive>>&>(m_canvas->primitives());
            for (auto it = primitives.begin(); it != primitives.end(); ++it) {
                if (!(*it) || (*it)->id() != m_imageId) {
                    continue;
                }

                std::unique_ptr<DrawingPrimitive> removed = std::move(*it);
                primitives.erase(it);
                m_image.reset(dynamic_cast<ImagePrimitive*>(removed.release()));
                m_canvas->clearSelection();
                m_canvas->update();
                m_executed = false;
                qDebug() << "Imported image removed from canvas";
                return;
            }
        }

        qDebug() << "Warning: Could not find imported image to remove";
    }
}

QString ImportImageCommand::description() const
{
    return "Import Image";
}

// ============================================================================
// SetContourSmoothnessCommand
// ============================================================================

SetContourSmoothnessCommand::SetContourSmoothnessCommand(ImagePrimitive* imagePrimitive, int oldSmoothness, int newSmoothness)
    : m_imagePrimitive(imagePrimitive)
    , m_oldSmoothness(oldSmoothness)
    , m_newSmoothness(newSmoothness)
{
}

void SetContourSmoothnessCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing SetContourSmoothnessCommand:" << m_newSmoothness;
        m_imagePrimitive->setContourSmoothness(m_newSmoothness);
    }
}

void SetContourSmoothnessCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing SetContourSmoothnessCommand:" << m_oldSmoothness;
        m_imagePrimitive->setContourSmoothness(m_oldSmoothness);
    }
}

QString SetContourSmoothnessCommand::description() const
{
    return "Set Contour Smoothness";
}

// ============================================================================
// ResizeTextCommand
// ============================================================================

ResizeTextCommand::ResizeTextCommand(TextPrimitive* textPrimitive)
    : m_textPrimitive(textPrimitive)
    , m_oldWidth(0)
    , m_oldHeight(0)
    , m_oldFontSize(24.0f)
    , m_newWidth(0)
    , m_newHeight(0)
    , m_newFontSize(24.0f)
    , m_executed(false)
{
}

void ResizeTextCommand::storeOldState(const QVector2D& position, float width, float height, float fontSize)
{
    m_oldPosition = position;
    m_oldWidth = width;
    m_oldHeight = height;
    m_oldFontSize = fontSize;
}

void ResizeTextCommand::storeNewState(const QVector2D& position, float width, float height, float fontSize)
{
    m_newPosition = position;
    m_newWidth = width;
    m_newHeight = height;
    m_newFontSize = fontSize;
}

void ResizeTextCommand::execute()
{
    if (!m_executed && m_textPrimitive) {
        qDebug() << "Executing ResizeTextCommand";
        m_textPrimitive->setPosition(m_newPosition);
        m_textPrimitive->setTextBoxWidth(m_newWidth);
        m_textPrimitive->setTextBoxHeight(m_newHeight);
        m_textPrimitive->setFontSize(m_newFontSize);
        m_executed = true;
    }
}

void ResizeTextCommand::undo()
{
    if (m_executed && m_textPrimitive) {
        qDebug() << "Undoing ResizeTextCommand";
        m_textPrimitive->setPosition(m_oldPosition);
        m_textPrimitive->setTextBoxWidth(m_oldWidth);
        m_textPrimitive->setTextBoxHeight(m_oldHeight);
        m_textPrimitive->setFontSize(m_oldFontSize);
        m_executed = false;
    }
}

QString ResizeTextCommand::description() const
{
    return "Resize Text";
}

// ============================================================================
// ============================================================================
// CompoundCommand
// ============================================================================

// SnapshotCommand Implementation
SnapshotCommand::SnapshotCommand(LayerManager* layerManager, const QString& description)
    : m_layerManager(layerManager)
    , m_description(description)
    , m_before(description)
    , m_after(description)
{
    if (m_layerManager) {
        m_before.saveState(m_layerManager);
    }
}

void SnapshotCommand::captureAfterState()
{
    if (m_layerManager) {
        m_after.saveState(m_layerManager);
        m_afterCaptured = true;
    }
}

void SnapshotCommand::execute()
{
    if (m_layerManager && m_afterCaptured) {
        m_after.restoreState(m_layerManager);
    }
}

void SnapshotCommand::undo()
{
    if (m_layerManager) {
        m_before.restoreState(m_layerManager);
    }
}

CompoundCommand::CompoundCommand(const QString& description)
    : m_description(description)
    , m_executed(false)
{
}

CompoundCommand::~CompoundCommand()
{
    // Commands will be automatically cleaned up by unique_ptr
}

void CompoundCommand::execute()
{
    if (!m_executed) {
        qDebug() << "Executing CompoundCommand:" << m_description;
        for (auto& command : m_commands) {
            if (command) {
                command->execute();
            }
        }
        m_executed = true;
    }
}

void CompoundCommand::undo()
{
    if (m_executed) {
        qDebug() << "Undoing CompoundCommand:" << m_description;
        // Undo commands in reverse order
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            if (*it) {
                (*it)->undo();
            }
        }
        m_executed = false;
    }
}

QString CompoundCommand::description() const
{
    return m_description;
}

void CompoundCommand::addCommand(std::unique_ptr<Command> command)
{
    if (command) {
        m_commands.push_back(std::move(command));
    }
}

// ModifyMaskControlPointCommand Implementation
ModifyMaskControlPointCommand::ModifyMaskControlPointCommand(ImagePrimitive* imagePrimitive, int controlPointIndex,
                                                           const QPointF& oldPosition, const QPointF& newPosition)
    : m_imagePrimitive(imagePrimitive)
    , m_controlPointIndex(controlPointIndex)
    , m_oldPosition(oldPosition)
    , m_newPosition(newPosition)
{
}

void ModifyMaskControlPointCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing ModifyMaskControlPointCommand";
        
        // Convert from image coordinates to world coordinates
        // The m_newPosition is in image coordinates, but moveControlPoint expects world coordinates
        float scaleX = m_imagePrimitive->size().x() / m_imagePrimitive->image().width();
        float scaleY = m_imagePrimitive->size().y() / m_imagePrimitive->image().height();
        
        // Convert to world coordinates: world = imagePos + (imageCoord * scale)
        // For Y: Apply Y-flip (image Y=0 is top, world Y increases upward)
        float worldX = m_imagePrimitive->position().x() + (m_newPosition.x() * scaleX);
        float worldY = m_imagePrimitive->position().y() + (m_imagePrimitive->size().y() - (m_newPosition.y() * scaleY));
        
        QVector2D worldPos(worldX, worldY);
        qDebug() << "Converting image coords" << m_newPosition.x() << "," << m_newPosition.y() 
                 << "to world coords" << worldX << "," << worldY;
        
        m_imagePrimitive->moveControlPoint(m_controlPointIndex, worldPos);
    }
}

void ModifyMaskControlPointCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing ModifyMaskControlPointCommand";
        
        // Convert from image coordinates to world coordinates
        // The m_oldPosition is in image coordinates, but moveControlPoint expects world coordinates
        float scaleX = m_imagePrimitive->size().x() / m_imagePrimitive->image().width();
        float scaleY = m_imagePrimitive->size().y() / m_imagePrimitive->image().height();
        
        // Convert to world coordinates: world = imagePos + (imageCoord * scale)
        // For Y: Apply Y-flip (image Y=0 is top, world Y increases upward)
        float worldX = m_imagePrimitive->position().x() + (m_oldPosition.x() * scaleX);
        float worldY = m_imagePrimitive->position().y() + (m_imagePrimitive->size().y() - (m_oldPosition.y() * scaleY));
        
        QVector2D worldPos(worldX, worldY);
        qDebug() << "Converting image coords" << m_oldPosition.x() << "," << m_oldPosition.y() 
                 << "to world coords" << worldX << "," << worldY;
        
        m_imagePrimitive->moveControlPoint(m_controlPointIndex, worldPos);
    }
}

QString ModifyMaskControlPointCommand::description() const
{
    return QString("Modify Mask Point %1").arg(m_controlPointIndex);
}

bool ModifyMaskControlPointCommand::canMergeWith(const Command* other) const
{
    const ModifyMaskControlPointCommand* otherModify = dynamic_cast<const ModifyMaskControlPointCommand*>(other);
    return otherModify && 
           otherModify->m_imagePrimitive == m_imagePrimitive && 
           otherModify->m_controlPointIndex == m_controlPointIndex;
}

void ModifyMaskControlPointCommand::mergeWith(const Command* other)
{
    const ModifyMaskControlPointCommand* otherModify = dynamic_cast<const ModifyMaskControlPointCommand*>(other);
    if (otherModify) {
        m_newPosition = otherModify->m_newPosition;
    }
}

// InsertMaskControlPointCommand Implementation
InsertMaskControlPointCommand::InsertMaskControlPointCommand(ImagePrimitive* imagePrimitive, int afterIndex,
                                                           const QPointF& position)
    : m_imagePrimitive(imagePrimitive)
    , m_afterIndex(afterIndex)
    , m_position(position)
    , m_executed(false)
{
}

void InsertMaskControlPointCommand::execute()
{
    if (!m_executed && m_imagePrimitive) {
        qDebug() << "Executing InsertMaskControlPointCommand";
        m_imagePrimitive->insertControlPoint(m_afterIndex, QVector2D(m_position.x(), m_position.y()));
        m_executed = true;
    }
}

void InsertMaskControlPointCommand::undo()
{
    if (m_executed && m_imagePrimitive) {
        qDebug() << "Undoing InsertMaskControlPointCommand";
        // The inserted point will be at index (m_afterIndex + 1)
        m_imagePrimitive->deleteControlPoint(m_afterIndex + 1);
        m_executed = false;
    }
}

QString InsertMaskControlPointCommand::description() const
{
    return "Insert Mask Point";
}

// DeleteMaskControlPointCommand Implementation
DeleteMaskControlPointCommand::DeleteMaskControlPointCommand(ImagePrimitive* imagePrimitive, int controlPointIndex)
    : m_imagePrimitive(imagePrimitive)
    , m_controlPointIndex(controlPointIndex)
    , m_executed(false)
{
    // Store the position of the point we're about to delete
    if (m_imagePrimitive) {
        auto contour = m_imagePrimitive->getEditableContour();
        if (controlPointIndex >= 0 && controlPointIndex < static_cast<int>(contour.size())) {
            m_deletedPosition = contour[controlPointIndex];
        }
    }
}

void DeleteMaskControlPointCommand::execute()
{
    if (!m_executed && m_imagePrimitive) {
        qDebug() << "Executing DeleteMaskControlPointCommand";
        m_imagePrimitive->deleteControlPoint(m_controlPointIndex);
        m_executed = true;
    }
}

void DeleteMaskControlPointCommand::undo()
{
    if (m_executed && m_imagePrimitive) {
        qDebug() << "Undoing DeleteMaskControlPointCommand";
        // m_deletedPosition is in image space; insertControlPoint expects world
        // coordinates, so convert using the primitive's image->world transform.
        // Insert after (index-1) so the point lands back at its original index.
        int afterIndex = m_controlPointIndex - 1;
        m_imagePrimitive->insertControlPoint(
            afterIndex, m_imagePrimitive->imageToWorld(m_deletedPosition));
        m_executed = false;
    }
}

QString DeleteMaskControlPointCommand::description() const
{
    return QString("Delete Mask Point %1").arg(m_controlPointIndex);
}

// SelectMaskCandidateCommand Implementation
SelectMaskCandidateCommand::SelectMaskCandidateCommand(ImagePrimitive* imagePrimitive, int oldIndex, int newIndex)
    : m_imagePrimitive(imagePrimitive)
    , m_oldIndex(oldIndex)
    , m_newIndex(newIndex)
{
}

void SelectMaskCandidateCommand::execute()
{
    if (m_imagePrimitive) {
        qDebug() << "Executing SelectMaskCandidateCommand";
        m_imagePrimitive->selectMaskCandidate(m_newIndex);
    }
}

void SelectMaskCandidateCommand::undo()
{
    if (m_imagePrimitive) {
        qDebug() << "Undoing SelectMaskCandidateCommand";
        m_imagePrimitive->selectMaskCandidate(m_oldIndex);
    }
}

QString SelectMaskCandidateCommand::description() const
{
    return "Select Mask Candidate";
}
