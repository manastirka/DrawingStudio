#pragma once

#include <QUuid>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <vector>

class DrawingPrimitive;
class LinePrimitive;

/**
 * Connected-line move / copy-along helpers (refactor E7).
 * Pure constraint math; canvas still owns clone + command commit.
 */
namespace LineMoveOps {

/** Ctrl or Meta (macOS Cmd) enables copy-along drag. */
bool isCopyAlongModifier(Qt::KeyboardModifiers mods);

/**
 * Unit direction a line must move along: previous connected segment,
 * else line's own moveConstraintDirection.
 */
QVector2D resolveConstraint(
    const LinePrimitive *line,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById);

/**
 * Selected lines that have a non-zero constraint (copy-along sources).
 */
std::vector<LinePrimitive *> collectConstrainedLines(
    const std::vector<DrawingPrimitive *> &selected,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById);

/**
 * Project @p delta onto a shared constraint axis for the selection.
 * Alt = free move; mixed axes among selection → free.
 */
QVector2D constrainDelta(
    const QVector2D &delta, Qt::KeyboardModifiers mods,
    const std::vector<DrawingPrimitive *> &selected,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById);

} // namespace LineMoveOps
