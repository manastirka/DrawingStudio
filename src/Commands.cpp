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

// Primitive commands (refactor E20).

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

// ============================================================================
// Brush stroke
// ============================================================================

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


