#include "DrawingProject.h"
#include "DrawingPrimitive.h"
#include <QJsonArray>
#include <algorithm>

void DrawingProject::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive)
{
    if (primitive) {
        m_primitives.push_back(std::move(primitive));
    }
}

void DrawingProject::removePrimitive(DrawingPrimitive* primitive)
{
    if (!primitive) return;
    
    // Remove from selection if selected
    auto it = std::find(m_selectedPrimitives.begin(), m_selectedPrimitives.end(), primitive);
    if (it != m_selectedPrimitives.end()) {
        m_selectedPrimitives.erase(it);
    }
    
    // Remove from primitives
    m_primitives.erase(
        std::remove_if(m_primitives.begin(), m_primitives.end(),
            [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
                return p.get() == primitive;
            }),
        m_primitives.end()
    );
}

void DrawingProject::clearPrimitives()
{
    m_selectedPrimitives.clear();
    m_primitives.clear();
}

DrawingPrimitive* DrawingProject::findPrimitiveAt(const QVector2D &position, float tolerance) const
{
    // Search from back to front for proper hit testing
    for (auto it = m_primitives.rbegin(); it != m_primitives.rend(); ++it) {
        if ((*it)->containsPoint(position, tolerance)) {
            return it->get();
        }
    }
    return nullptr;
}

void DrawingProject::clearSelection()
{
    for (auto* primitive : m_selectedPrimitives) {
        primitive->setSelected(false);
    }
    m_selectedPrimitives.clear();
}

void DrawingProject::selectPrimitive(DrawingPrimitive* primitive)
{
    if (!primitive) return;
    
    auto it = std::find(m_selectedPrimitives.begin(), m_selectedPrimitives.end(), primitive);
    if (it == m_selectedPrimitives.end()) {
        primitive->setSelected(true);
        m_selectedPrimitives.push_back(primitive);
    }
}

void DrawingProject::deselectPrimitive(DrawingPrimitive* primitive)
{
    if (!primitive) return;
    
    auto it = std::find(m_selectedPrimitives.begin(), m_selectedPrimitives.end(), primitive);
    if (it != m_selectedPrimitives.end()) {
        primitive->setSelected(false);
        m_selectedPrimitives.erase(it);
    }
}

void DrawingProject::selectAll()
{
    clearSelection();
    for (const auto& primitive : m_primitives) {
        selectPrimitive(primitive.get());
    }
}

void DrawingProject::addLayer(const QString &name, const QColor &color)
{
    m_layers.emplace_back(name, color);
}

void DrawingProject::removeLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < static_cast<int>(m_layers.size())) {
        m_layers.erase(m_layers.begin() + layerIndex);
        
        // Adjust current layer if necessary
        if (m_currentLayer >= static_cast<int>(m_layers.size())) {
            m_currentLayer = std::max(0, static_cast<int>(m_layers.size()) - 1);
        }
    }
}

void DrawingProject::setCurrentLayer(int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < static_cast<int>(m_layers.size())) {
        m_currentLayer = layerIndex;
    }
}

QJsonObject DrawingProject::serialize() const
{
    QJsonObject json;
    
    json["name"] = m_name;
    json["units"] = static_cast<int>(m_units);
    json["gridSize"] = static_cast<int>(m_gridSize);
    json["gridVisible"] = m_gridVisible;
    json["snapToGrid"] = m_snapToGrid;
    json["canvasWidth"] = static_cast<double>(m_canvasSize.x());
    json["canvasHeight"] = static_cast<double>(m_canvasSize.y());
    json["backgroundColor"] = m_backgroundColor.name();
    json["currentLayer"] = m_currentLayer;
    
    // Serialize layers
    QJsonArray layersArray;
    for (const auto& layer : m_layers) {
        QJsonObject layerObj;
        layerObj["name"] = layer.first;
        layerObj["color"] = layer.second.name();
        layersArray.append(layerObj);
    }
    json["layers"] = layersArray;
    
    // Serialize primitives
    QJsonArray primitivesArray;
    for (const auto& primitive : m_primitives) {
        // TODO: Implement primitive serialization
        // primitivesArray.append(primitive->serialize());
    }
    json["primitives"] = primitivesArray;
    
    return json;
}

void DrawingProject::deserialize(const QJsonObject &json)
{
    m_name = json["name"].toString("Untitled Drawing");
    m_units = static_cast<DrawingUnit>(json["units"].toInt(0));
    m_gridSize = static_cast<GridSize>(json["gridSize"].toInt(1));
    m_gridVisible = json["gridVisible"].toBool(true);
    m_snapToGrid = json["snapToGrid"].toBool(true);
    m_canvasSize = QVector2D(
        static_cast<float>(json["canvasWidth"].toDouble(800.0)),
        static_cast<float>(json["canvasHeight"].toDouble(600.0))
    );
    m_backgroundColor = QColor(json["backgroundColor"].toString("#ffffff"));
    m_currentLayer = json["currentLayer"].toInt(0);
    
    // Deserialize layers
    m_layers.clear();
    QJsonArray layersArray = json["layers"].toArray();
    for (const auto& value : layersArray) {
        QJsonObject layerObj = value.toObject();
        QString name = layerObj["name"].toString();
        QColor color(layerObj["color"].toString());
        m_layers.emplace_back(name, color);
    }
    
    // If no layers exist, create a default one
    if (m_layers.empty()) {
        addLayer("Layer 1", Qt::black);
    }
    
    // Clear existing primitives and selection
    clearPrimitives();
    
    // Deserialize primitives (this would need to be implemented in DrawingPrimitive)
    QJsonArray primitivesArray = json["primitives"].toArray();
    for (const auto& value : primitivesArray) {
        // TODO: Implement primitive deserialization
        // This would require a factory pattern or similar to create the correct primitive types
    }
}