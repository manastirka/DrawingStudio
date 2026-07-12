#include "UndoRedoManager.h"
#include "CanvasSnapshot.h"
#include "LayerManager.h"
#include <QDebug>
#include <memory>

UndoRedoManager::UndoRedoManager(LayerManager* layerManager, QObject* parent)
    : QObject(parent)
    , m_layerManager(layerManager)
{
}

void UndoRedoManager::saveState(const QString& description)
{
    if (!m_layerManager) {
        qWarning() << "Cannot save state: null layer manager";
        return;
    }
    
    // Create snapshot
    auto snapshot = std::make_unique<CanvasSnapshot>(description);
    snapshot->saveState(m_layerManager);
    
    // Add to undo stack
    m_undoStack.push_back(std::move(snapshot));
    
    // Clear redo stack (can't redo after new action)
    m_redoStack.clear();
    
    // Limit stack size
    if (m_undoStack.size() > static_cast<size_t>(m_maxUndoLevels)) {
        m_undoStack.erase(m_undoStack.begin());
    }
    
    updateSignals();
    
    qDebug() << "State saved:" << description << "- Undo stack size:" << m_undoStack.size();
}

void UndoRedoManager::undo()
{
    if (!canUndo()) {
        qDebug() << "❌ Cannot undo: stack empty";
        return;
    }
    
    qDebug() << "🔄 UNDO: Starting undo operation...";
    qDebug() << "   Undo stack size:" << m_undoStack.size();
    
    // Get current state (to move to redo stack)
    auto currentSnapshot = std::make_unique<CanvasSnapshot>("Current State");
    currentSnapshot->saveState(m_layerManager);
    qDebug() << "   ✓ Current state saved";
    
    // Get last snapshot
    auto snapshot = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    qDebug() << "   ✓ Retrieved snapshot:" << snapshot->description();
    
    // Restore it
    snapshot->restoreState(m_layerManager);
    qDebug() << "   ✓ State restored";
    
    // Move current state to redo stack
    m_redoStack.push_back(std::move(currentSnapshot));
    
    updateSignals();
    emit stateChanged();
    
    qDebug() << "✅ UNDO COMPLETE:" << snapshot->description();
}

void UndoRedoManager::redo()
{
    if (!canRedo()) {
        qDebug() << "Cannot redo: stack empty";
        return;
    }
    
    // Get current state (to move back to undo stack)
    auto currentSnapshot = std::make_unique<CanvasSnapshot>("Current State");
    currentSnapshot->saveState(m_layerManager);
    
    // Get last undone snapshot
    auto snapshot = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    
    // Restore it
    snapshot->restoreState(m_layerManager);
    
    // Move current state back to undo stack
    m_undoStack.push_back(std::move(currentSnapshot));
    
    updateSignals();
    emit stateChanged();
    
    qDebug() << "Redo:" << snapshot->description();
}

bool UndoRedoManager::canUndo() const
{
    return !m_undoStack.empty();
}

bool UndoRedoManager::canRedo() const
{
    return !m_redoStack.empty();
}

QString UndoRedoManager::undoText() const
{
    if (canUndo()) {
        return "Undo " + m_undoStack.back()->description();
    }
    return "Undo";
}

QString UndoRedoManager::redoText() const
{
    if (canRedo()) {
        return "Redo " + m_redoStack.back()->description();
    }
    return "Redo";
}

void UndoRedoManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    updateSignals();
    qDebug() << "Undo/Redo history cleared";
}

void UndoRedoManager::updateSignals()
{
    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
}
