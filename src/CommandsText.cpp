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

// Text commands (refactor E20).

// ============================================================================
// EditTextCommand
// ============================================================================

EditTextCommand::EditTextCommand(TextPrimitive* textPrimitive, const QString& description)
    : m_textPrimitive(textPrimitive)
    , m_description(description)
    , m_executed(false)
{
}

void EditTextCommand::storeOldState(const QJsonObject& oldState)
{
    m_oldState = oldState;
}

void EditTextCommand::storeNewState(const QJsonObject& newState)
{
    m_newState = newState;
}

void EditTextCommand::execute()
{
    if (!m_executed && m_textPrimitive) {
        qDebug() << "Executing EditTextCommand";
        m_textPrimitive->fromJson(m_newState);
        m_executed = true;
    }
}

void EditTextCommand::undo()
{
    if (m_executed && m_textPrimitive) {
        qDebug() << "Undoing EditTextCommand";
        m_textPrimitive->fromJson(m_oldState);
        m_executed = false;
    }
}

QString EditTextCommand::description() const
{
    return m_description;
}

// ============================================================================
// ResizeTextCommand
// ============================================================================

ResizeTextCommand::ResizeTextCommand(TextPrimitive* textPrimitive)
    : m_textPrimitive(textPrimitive)
    , m_oldWidth(0)
    , m_oldHeight(0)
    , m_oldFontSize(24.0f)
    , m_newWidth(0)
    , m_newHeight(0)
    , m_newFontSize(24.0f)
    , m_executed(false)
{
}

void ResizeTextCommand::storeOldState(const QVector2D& position, float width, float height, float fontSize)
{
    m_oldPosition = position;
    m_oldWidth = width;
    m_oldHeight = height;
    m_oldFontSize = fontSize;
}

void ResizeTextCommand::storeNewState(const QVector2D& position, float width, float height, float fontSize)
{
    m_newPosition = position;
    m_newWidth = width;
    m_newHeight = height;
    m_newFontSize = fontSize;
}

void ResizeTextCommand::execute()
{
    if (!m_executed && m_textPrimitive) {
        qDebug() << "Executing ResizeTextCommand";
        m_textPrimitive->setPosition(m_newPosition);
        m_textPrimitive->setTextBoxWidth(m_newWidth);
        m_textPrimitive->setTextBoxHeight(m_newHeight);
        m_textPrimitive->setFontSize(m_newFontSize);
        m_executed = true;
    }
}

void ResizeTextCommand::undo()
{
    if (m_executed && m_textPrimitive) {
        qDebug() << "Undoing ResizeTextCommand";
        m_textPrimitive->setPosition(m_oldPosition);
        m_textPrimitive->setTextBoxWidth(m_oldWidth);
        m_textPrimitive->setTextBoxHeight(m_oldHeight);
        m_textPrimitive->setFontSize(m_oldFontSize);
        m_executed = false;
    }
}

QString ResizeTextCommand::description() const
{
    return "Resize Text";
}

