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

// Snapshot / compound commands (refactor E20).

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


