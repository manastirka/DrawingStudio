#pragma once

#include <QVector2D>
#include <QString>
#include <QJsonObject>
#include <QColor>
#include <memory>
#include <vector>

class DrawingPrimitive;

enum class DrawingUnit {
    Millimeters,
    Centimeters,
    Inches,
    Points
};

enum class GridSize {
    Fine,      // 1mm
    Medium,    // 5mm
    Coarse     // 10mm
};

class DrawingProject
{
public:
    DrawingProject() = default;
    ~DrawingProject() = default;
    
    // Primitive management
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void removePrimitive(DrawingPrimitive* primitive);
    const std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() const { return m_primitives; }
    void clearPrimitives();
    DrawingPrimitive* findPrimitiveAt(const QVector2D &position, float tolerance = 10.0f) const;
    
    // Selection management
    void clearSelection();
    void selectPrimitive(DrawingPrimitive* primitive);
    void deselectPrimitive(DrawingPrimitive* primitive);
    void selectAll();
    const std::vector<DrawingPrimitive*>& selectedPrimitives() const { return m_selectedPrimitives; }
    
    // Layer management
    void addLayer(const QString &name, const QColor &color = Qt::black);
    void removeLayer(int layerIndex);
    void setCurrentLayer(int layerIndex);
    int currentLayer() const { return m_currentLayer; }
    const std::vector<std::pair<QString, QColor>>& layers() const { return m_layers; }
    
    // Project properties
    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }
    
    void setUnits(DrawingUnit units) { m_units = units; }
    DrawingUnit units() const { return m_units; }
    
    void setGridSize(GridSize gridSize) { m_gridSize = gridSize; }
    GridSize gridSize() const { return m_gridSize; }
    
    void setGridVisible(bool visible) { m_gridVisible = visible; }
    bool isGridVisible() const { return m_gridVisible; }
    
    void setSnapToGrid(bool snap) { m_snapToGrid = snap; }
    bool isSnapToGrid() const { return m_snapToGrid; }
    
    void setCanvasSize(const QVector2D &size) { m_canvasSize = size; }
    QVector2D canvasSize() const { return m_canvasSize; }
    
    void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }
    QColor backgroundColor() const { return m_backgroundColor; }
    
    // Serialization
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    
private:
    QString m_name = "Untitled Drawing";
    DrawingUnit m_units = DrawingUnit::Millimeters;
    GridSize m_gridSize = GridSize::Medium;
    bool m_gridVisible = true;
    bool m_snapToGrid = true;
    QVector2D m_canvasSize = QVector2D(800.0f, 600.0f);
    QColor m_backgroundColor = Qt::white;
    
    int m_currentLayer = 0;
    std::vector<std::pair<QString, QColor>> m_layers;
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
    std::vector<DrawingPrimitive*> m_selectedPrimitives;
};