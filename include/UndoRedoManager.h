#pragma once

#include <QObject>
#include <vector>
#include <memory>
#include "CanvasSnapshot.h"

class LayerManager;

/**
 * Simple snapshot-based undo/redo manager
 * Saves entire canvas state before each action
 */
class UndoRedoManager : public QObject {
    Q_OBJECT
    
public:
    explicit UndoRedoManager(LayerManager* layerManager, QObject* parent = nullptr);
    
    // Save current state before an action
    void saveState(const QString& description = "Action");
    
    // Undo last action
    void undo();
    
    // Redo last undone action
    void redo();
    
    // Check if undo/redo is available
    bool canUndo() const;
    bool canRedo() const;
    
    // Get descriptions
    QString undoText() const;
    QString redoText() const;
    
    // Clear history
    void clear();
    
    // Set maximum undo levels
    void setMaxUndoLevels(int max) { m_maxUndoLevels = max; }
    
signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void stateChanged();
    
private:
    LayerManager* m_layerManager;
    std::vector<std::unique_ptr<CanvasSnapshot>> m_undoStack;
    std::vector<std::unique_ptr<CanvasSnapshot>> m_redoStack;
    int m_maxUndoLevels = 50;
    
    void updateSignals();
};
