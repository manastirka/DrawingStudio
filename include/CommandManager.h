#pragma once

#include "Command.h"
#include <QObject>
#include <QStringList>
#include <vector>
#include <memory>

/**
 * Manages undo/redo stack
 * Keeps track of all commands and allows undo/redo operations
 */
class CommandManager : public QObject {
    Q_OBJECT
    
public:
    explicit CommandManager(QObject* parent = nullptr);
    
    // Execute a command and add it to the stack
    void executeCommand(CommandPtr command);
    
    // Add a command to the stack without executing (for already-completed actions)
    void addCommandWithoutExecuting(CommandPtr command);
    
    // Undo the last command
    void undo();
    
    // Redo the last undone command
    void redo();
    
    // Check if undo/redo is available
    bool canUndo() const;
    bool canRedo() const;
    
    // Get descriptions for UI
    QString undoText() const;
    QString redoText() const;
    QStringList undoHistory() const;
    QStringList redoHistory() const;
    
    // Clear all history
    void clear();
    
    // Set maximum undo levels (default: 50)
    void setMaxUndoLevels(int max) { m_maxUndoLevels = max; }

    // Dirty/clean tracking relative to last save
    void markClean();
    void invalidateClean();
    bool isClean() const;
    size_t undoStackSize() const { return m_undoStack.size(); }
    
signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void undoTextChanged(const QString& text);
    void redoTextChanged(const QString& text);
    void historyChanged();
    void cleanChanged(bool clean);
    
private:
    std::vector<CommandPtr> m_undoStack;
    std::vector<CommandPtr> m_redoStack;
    int m_maxUndoLevels = 50;
    size_t m_cleanUndoSize = 0;
    bool m_cleanValid = true;
    
    void updateSignals();
};
