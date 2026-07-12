#include "CanvasSnapshot.h"
#include "LayerManager.h"
#include "Layer.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

CanvasSnapshot::CanvasSnapshot(const QString& description)
    : m_description(description)
{
}

CanvasSnapshot::~CanvasSnapshot()
{
}

void CanvasSnapshot::saveState(LayerManager* layerManager)
{
    if (!layerManager) {
        qWarning() << "Cannot save snapshot: null layer manager";
        return;
    }
    
    QJsonObject root;
    QJsonArray layersArray;

    if (layerManager->activeLayer()) {
        root["activeLayerId"] = layerManager->activeLayer()->id().toString();
    }
    
    for (const auto& layer : layerManager->layers()) {
        if (!layer) continue;
        
        QJsonObject layerObj;
        layerObj["name"] = layer->name();
        layerObj["visible"] = layer->isVisible();
        layerObj["locked"] = layer->isLocked();
        layerObj["opacity"] = static_cast<double>(layer->opacity());
        layerObj["zOrder"] = layer->zOrder();
        layerObj["color"] = layer->color().name();
        layerObj["blendMode"] = static_cast<int>(layer->blendMode());
        layerObj["id"] = layer->id().toString();
        
        // Serialize all primitives in this layer
        QJsonArray primitivesArray;
        for (const auto& primitive : layer->primitives()) {
            if (primitive) {
                QJsonObject primJson = primitive->toJson();
                primitivesArray.append(primJson);
            }
        }
        
        layerObj["primitives"] = primitivesArray;
        layerObj["primitiveCount"] = primitivesArray.size();
        
        layersArray.append(layerObj);
    }
    
    root["layers"] = layersArray;
    root["layerCount"] = layersArray.size();
    
    QJsonDocument doc(root);
    m_serializedState = doc.toJson(QJsonDocument::Compact);
    
    qDebug() << "Snapshot saved:" << m_description << "- Size:" << m_serializedState.size() << "bytes" 
             << "- Layers:" << layersArray.size();
}

void CanvasSnapshot::restoreState(LayerManager* layerManager)
{
    if (!layerManager) {
        qWarning() << "Cannot restore snapshot: null layer manager";
        return;
    }
    
    if (m_serializedState.isEmpty()) {
        qWarning() << "Cannot restore snapshot: empty state";
        return;
    }
    
    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(m_serializedState);
    if (doc.isNull()) {
        qWarning() << "Cannot restore snapshot: invalid JSON";
        return;
    }
    
    QJsonObject root = doc.object();
    QJsonArray layersArray = root["layers"].toArray();
    QString activeLayerId = root["activeLayerId"].toString();
    
    // Clear all current layers (but don't create default layer)
    // We'll restore the exact layers from the snapshot
    layerManager->clearLayersNoDefault();
    
    // Restore each layer
    for (const QJsonValue& layerValue : layersArray) {
        QJsonObject layerObj = layerValue.toObject();
        
        QString name = layerObj["name"].toString();
        bool visible = layerObj["visible"].toBool();
        bool locked = layerObj["locked"].toBool();
        QUuid id = QUuid(layerObj["id"].toString());
        
        // Create layer
        Layer* layer = id.isNull() ? layerManager->createLayer(name)
                                   : layerManager->createLayer(id, name);
        if (!layer) continue;
        
        layer->setVisible(visible);
        layer->setLocked(locked);
        if (layerObj.contains("opacity")) {
            layer->setOpacity(static_cast<float>(layerObj["opacity"].toDouble(1.0)));
        }
        if (layerObj.contains("zOrder")) {
            layer->setZOrder(layerObj["zOrder"].toInt(layer->zOrder()));
        }
        if (layerObj.contains("color")) {
            layer->setColor(QColor(layerObj["color"].toString()));
        }
        if (layerObj.contains("blendMode")) {
            layer->setBlendMode(static_cast<Layer::BlendMode>(layerObj["blendMode"].toInt(0)));
        }
        
        // Restore primitives
        QJsonArray primitivesArray = layerObj["primitives"].toArray();
        int restoredCount = 0;
        for (const QJsonValue& primValue : primitivesArray) {
            QJsonObject primJson = primValue.toObject();
            auto primitive = DrawingPrimitive::createFromJson(primJson);
            if (primitive) {
                primitive->setLayerId(layer->id());
                layer->addPrimitive(std::move(primitive));
                restoredCount++;
            }
        }
        
        qDebug() << "Restored layer:" << name << "with" << restoredCount << "/" << primitivesArray.size() << "primitives";
    }

    if (!activeLayerId.isEmpty()) {
        QUuid activeId(activeLayerId);
        Layer* activeLayer = layerManager->getLayer(activeId);
        if (activeLayer) {
            layerManager->setActiveLayer(activeLayer);
        }
    }
    
    qDebug() << "Snapshot restored:" << m_description << "- Layers:" << layersArray.size();
}
