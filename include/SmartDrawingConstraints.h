#pragma once

#include <QString>
#include <QVector2D>
#include <Qt>

/**
 * Shift / soft-aspect drawing constraints (axis lock, square, circle).
 * Extracted from DrawingCanvas (refactor B3).
 */
namespace SmartDrawingConstraints {

enum class ToolKind {
    LineOrMeasure,
    Rectangle,
    Ellipse,
    Other
};

/**
 * @param start     Drag origin (m_drawStartPos).
 * @param rawPos    Current pointer in world space.
 * @param mods      Keyboard modifiers (Shift locks, Alt frees soft snap).
 * @param hintOut   Optional coaching string (cleared if unused).
 */
QVector2D apply(ToolKind kind, const QVector2D &start, const QVector2D &rawPos,
                Qt::KeyboardModifiers mods, QString *hintOut);

} // namespace SmartDrawingConstraints
