#include "CommandManager.h"
#include <QDebug>

CommandManager::CommandManager(QObject* parent)
    : QObject(parent)
{
}

void CommandManager::executeCommand(CommandPtr command)
{
    if (!command) {
        return;
    }
    
    // Execute the command
    command->execute();
    
    // Add to undo stack
    m_undoStack.push_back(std::move(command));
    
    // Clear redo stack (can't redo after new action)
    m_redoStack.clear();
    
    // Limit undo stack size
    if (m_undoStack.size() > static_cast<size_t>(m_maxUndoLevels)) {
        m_undoStack.erase(m_undoStack.begin());
        if (m_cleanUndoSize > 0)
            --m_cleanUndoSize;
        else
            m_cleanValid = false;
    }
    
    updateSignals();
    
    qDebug() << "Command executed. Undo stack size:" << m_undoStack.size();
}

void CommandManager::addCommandWithoutExecuting(CommandPtr command)
{
    if (!command) {
        return;
    }
    
    // Add to undo stack WITHOUT executing (for commands that already happened)
    m_undoStack.push_back(std::move(command));
    
    // Clear redo stack (can't redo after new action)
    m_redoStack.clear();
    
    // Limit undo stack size
    if (m_undoStack.size() > static_cast<size_t>(m_maxUndoLevels)) {
        m_undoStack.erase(m_undoStack.begin());
        if (m_cleanUndoSize > 0)
            --m_cleanUndoSize;
        else
            m_cleanValid = false;
    }
    
    updateSignals();
    
    qDebug() << "Command added (not executed). Undo stack size:" << m_undoStack.size();
}

void CommandManager::undo()
{
    if (!canUndo()) {
        return;
    }
    
    // Get last command
    CommandPtr command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    
    // Undo it
    command->undo();
    
    // Move to redo stack
    m_redoStack.push_back(std::move(command));
    
    updateSignals();
    
    qDebug() << "Undo executed. Undo stack size:" << m_undoStack.size();
}

void CommandManager::redo()
{
    if (!canRedo()) {
        return;
    }
    
    // Get last undone command
    CommandPtr command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    
    // Execute it again
    command->execute();
    
    // Move back to undo stack
    m_undoStack.push_back(std::move(command));
    
    updateSignals();
    
    qDebug() << "Redo executed. Redo stack size:" << m_redoStack.size();
}

bool CommandManager::canUndo() const
{
    return !m_undoStack.empty();
}

bool CommandManager::canRedo() const
{
    return !m_redoStack.empty();
}

QString CommandManager::undoText() const
{
    if (canUndo()) {
        return "Undo " + m_undoStack.back()->description();
    }
    return "Undo";
}

QString CommandManager::redoText() const
{
    if (canRedo()) {
        return "Redo " + m_redoStack.back()->description();
    }
    return "Redo";
}

QStringList CommandManager::undoHistory() const
{
    QStringList list;
    for (auto it = m_undoStack.rbegin(); it != m_undoStack.rend(); ++it) {
        list << (*it)->description();
    }
    return list;
}

QStringList CommandManager::redoHistory() const
{
    QStringList list;
    for (auto it = m_redoStack.rbegin(); it != m_redoStack.rend(); ++it) {
        list << (*it)->description();
    }
    return list;
}

void CommandManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    m_cleanUndoSize = 0;
    m_cleanValid = true;
    updateSignals();
    
    qDebug() << "Command history cleared";
}

void CommandManager::markClean()
{
    m_cleanUndoSize = m_undoStack.size();
    m_cleanValid = true;
    emit cleanChanged(true);
}

void CommandManager::invalidateClean()
{
    if (!m_cleanValid)
        return;
    m_cleanValid = false;
    emit cleanChanged(false);
}

bool CommandManager::isClean() const
{
    return m_cleanValid && m_undoStack.size() == m_cleanUndoSize;
}

void CommandManager::updateSignals()
{
    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
    emit undoTextChanged(undoText());
    emit redoTextChanged(redoText());
    emit historyChanged();
    emit cleanChanged(isClean());
}
