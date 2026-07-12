#pragma once

#include <QImage>
#include <QString>
#include <memory>
#include <vector>

class DrawingPrimitive;
class Layer;
class LayerManager;

/**
 * Snapshot of canvas state for undo/redo
 * Stores a copy of all layers and primitives
 */
class CanvasSnapshot {
public:
    CanvasSnapshot(const QString& description = "Action");
    ~CanvasSnapshot();
    
    // Save current state from layer manager
    void saveState(LayerManager* layerManager);
    
    // Restore state to layer manager
    void restoreState(LayerManager* layerManager);
    
    QString description() const { return m_description; }
    
private:
    QString m_description;
    
    // Serialized state (JSON or binary)
    QByteArray m_serializedState;
};
