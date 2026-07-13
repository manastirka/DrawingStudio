#pragma once

#include <QColor>
#include <QPainter>
#include <QPoint>
#include <QVector2D>
#include <functional>
#include <memory>
#include <vector>

class DrawingPrimitive;
class LayerManager;
class CanvasRenderer;

/**
 * Paint-pass orchestration helpers (refactor E4).
 * Individual draw routines stay on DrawingCanvas / TextRenderer / OverlayRenderer;
 * this module owns ordered world-object rendering and small HUD bits.
 */
namespace CanvasPaintPipeline {

using PrimitiveList = std::vector<std::unique_ptr<DrawingPrimitive>>;

/** Fill widget background. */
void clearBackground(QPainter &painter, const QRect &widgetRect,
                     const QColor &background);

/**
 * Render all layer (or legacy) primitives in world space.
 * Caller must set world transform on @p painter before calling.
 *
 * @param showsGeometryCPs  whether to draw path control points for selection
 * @param drawControlPoint  (painter, worldPos, highlighted)
 */
void renderWorldObjects(
    QPainter &painter, LayerManager *layers, const PrimitiveList &legacy,
    DrawingPrimitive *currentPrimitive,
    const std::function<std::vector<DrawingPrimitive *>()> &selectedObjects,
    const std::function<bool(const DrawingPrimitive *)> &showsGeometryCPs,
    const std::function<void(QPainter &, const QVector2D &, bool)> &
        drawControlPoint);

/** Screen-space HUD: rotation degrees while rotating text. */
void drawRotationAngleHud(QPainter &painter, const QPoint &screenCenter,
                          float rotationDegrees);

// --- Live tool previews (refactor E5); caller sets world transform first ---

/** In-progress brush / blur stroke (matches final BrushStrokePrimitive look). */
void renderBrushStrokePreview(QPainter &painter,
                              const std::vector<QVector2D> &points,
                              const std::vector<float> &particleScale,
                              const std::vector<float> &particleAlpha,
                              float brushSize, float hardness,
                              const QColor &color, bool isBlur);

/** Angle-line tool baseline (dashed) + endpoint dot. */
void renderAngleBaseline(QPainter &painter, const QVector2D &start,
                         const QVector2D &end, float zoomLevel);

/** Live shape while drawing (color applied then render). */
void renderDrawingPrimitivePreview(QPainter &painter,
                                   DrawingPrimitive *primitive,
                                   const QColor &color);

} // namespace CanvasPaintPipeline
