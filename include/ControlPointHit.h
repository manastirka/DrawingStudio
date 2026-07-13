#pragma once

#include <QVector2D>
#include <vector>

/**
 * Pure control-point hit tests (refactor E8).
 */
namespace ControlPointHit {

/**
 * First point within @p tolerance of @p pos, or -1.
 * Matches legacy findControlPointAt (order-first, not nearest).
 */
int firstWithin(const std::vector<QVector2D> &points, const QVector2D &pos,
                float tolerance);

/**
 * Nearest point within @p maxDistance of @p pos, or -1.
 * Matches legacy findClosestControlPoint.
 */
int closestWithin(const std::vector<QVector2D> &points, const QVector2D &pos,
                  float maxDistance);

} // namespace ControlPointHit
