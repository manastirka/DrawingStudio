#pragma once

#include <QCursor>
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <Qt>

class QPainter;

/**
 * Pure selection-box handle geometry, hit-test, resize math, and drawing.
 * Extracted from DrawingCanvas (refactor B6).
 * Gesture state (isResizing/isRotating) stays on the canvas.
 */
namespace SelectionTransform {

/** Index of the rotation handle above the top edge. Resize corners/edges are 0–7. */
constexpr int kHandleRotate = 8;

float handleHalfSize(float zoomLevel);
float handleHitRadius(float zoomLevel);

/** Fills out[0..8] with world positions (0–7 box handles, 8 = rotate). */
void handlePositions(const QRectF &boundingRect, float zoomLevel, QPointF out[9]);

int hitTest(const QRectF &boundingRect, const QVector2D &worldPos, float zoomLevel,
            bool includeRotate);

Qt::CursorShape cursorForHandle(int handleIndex);

/**
 * Resize bounding rect from handle drag.
 * Shift = keep aspect; Alt = resize from center.
 */
QRectF computeResizedBounds(const QRectF &original, int handleIndex,
                            const QVector2D &worldPos,
                            Qt::KeyboardModifiers mods);

/** Draws dashed frame + handles (+ optional rotation stem). World transform assumed. */
void drawHandles(QPainter &painter, const QRectF &boundingRect, float zoomLevel,
                 bool showRotate);

/**
 * Draw handles for one selected object, applying external rotation transform
 * when @p usesExternalRotation is true (refactor E8).
 */
void drawHandlesForObject(QPainter &painter, const QRectF &boundingRect,
                          float zoomLevel, bool showRotate,
                          bool usesExternalRotation, float rotationDegrees);

} // namespace SelectionTransform
