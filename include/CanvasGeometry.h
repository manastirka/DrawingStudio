#pragma once

#include <QVector2D>
#include <Qt>
#include <vector>

/**
 * Pure geometry helpers for the drawing canvas.
 * Extracted from DrawingCanvas (refactor B1) — no canvas state.
 */
namespace CanvasGeometry {

/** Closest point on segment AB to `point` (clamped to segment). */
QVector2D projectPointOntoLineSegment(const QVector2D &point, const QVector2D &a,
                                      const QVector2D &b);

/**
 * Circle through three points. Returns false if collinear / degenerate.
 * On success writes center and radius.
 */
bool computeCircleThroughPoints(const QVector2D &p1, const QVector2D &p2,
                                const QVector2D &p3, QVector2D &centerOut,
                                float &radiusOut);

/** Ray-crossing point-in-polygon test. */
bool pointInPolygon(const QVector2D &point,
                    const std::vector<QVector2D> &polygon);

/**
 * Snap endpoint relative to a baseline direction.
 * Alt = free angle; Shift = 5° steps, else 15° steps relative to baseline.
 */
QVector2D snapAngleLineEndpoint(const QVector2D &origin,
                                const QVector2D &rawEnd,
                                const QVector2D &baselineStart,
                                const QVector2D &baselineEnd,
                                Qt::KeyboardModifiers mods);

} // namespace CanvasGeometry
