#pragma once

#include <QPoint>
#include <QVector2D>

class QPainter;

/**
 * Pure screen ↔ world coordinate math (refactor E7 / E12).
 * Origin at widget center, Y-up in world space — matches DrawingCanvas.
 */
namespace CanvasCoords {

/**
 * Screen pixel → world.
 * @param pixelSnap  round world x/y to integers when true
 */
QVector2D screenToWorld(const QPoint &screenPos, int widgetWidth,
                        int widgetHeight, float zoomLevel,
                        const QVector2D &viewCenter, bool pixelSnap = false);

/** World → screen pixel (integer). */
QPoint worldToScreen(const QVector2D &worldPos, int widgetWidth,
                     int widgetHeight, float zoomLevel,
                     const QVector2D &viewCenter);

/**
 * Mouse position as offset from widget center in y-up screen space
 * (used by zoom-to-cursor).
 */
QVector2D mouseOffsetFromCenter(const QPoint &screenPos, int widgetWidth,
                                int widgetHeight);

/**
 * Apply canvas world transform to @p painter (center, zoom, Y-flip, pan).
 * Does not save/restore painter state.
 */
void applyWorldTransform(QPainter &painter, int widgetWidth, int widgetHeight,
                         float zoomLevel, const QVector2D &viewCenter);

} // namespace CanvasCoords
