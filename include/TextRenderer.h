#pragma once

#include <QFont>
#include <QPoint>
#include <QUuid>
#include <QVector2D>
#include <functional>
#include <memory>
#include <vector>

class QPainter;
class TextPrimitive;
class SplinePrimitive;
class DimensionPrimitive;
class DrawingPrimitive;
class LayerManager;

/**
 * Screen-space text drawing helpers (refactor D3 / E7).
 * Low-level draw* + layer traversal for paint-pass orchestration.
 */
namespace TextRenderer {

using PrimitiveList = std::vector<std::unique_ptr<DrawingPrimitive>>;
using WorldToScreenFn = std::function<QPoint(const QVector2D &)>;
using FindSplineFn = std::function<SplinePrimitive *(const QUuid &)>;

void drawFormatted(QPainter *painter, const TextPrimitive *textPrim,
                   const QPoint &pos, const QFont &font, float zoomLevel,
                   bool showBox);

void drawOnSpline(QPainter &painter, const TextPrimitive *textPrim,
                  const SplinePrimitive *spline, float zoomLevel,
                  const WorldToScreenFn &worldToScreen);

void drawDimensionLabel(QPainter *painter, DimensionPrimitive *dimension,
                        const WorldToScreenFn &worldToScreen);

/**
 * Draw one text primitive in screen space (rotation / spline / plain).
 * Selection chrome is intentionally not drawn (renderSelection owns that).
 */
void drawTextPrimitive(QPainter &painter, TextPrimitive *textPrim,
                       float zoomLevel, const WorldToScreenFn &worldToScreen,
                       const FindSplineFn &findSpline);

/**
 * Traverse layers/legacy and draw all visible TextPrimitives (E7).
 * Caller should resetTransform + enable TextAntialiasing first.
 */
void renderAllTexts(QPainter &painter, LayerManager *layers,
                    const PrimitiveList &legacy, float zoomLevel,
                    const WorldToScreenFn &worldToScreen,
                    const FindSplineFn &findSpline);

/**
 * Traverse layers/legacy (+ optional draft) and draw Dimension labels (E7).
 * Caller should resetTransform and set font/pen first if desired.
 */
void renderAllDimensions(QPainter &painter, LayerManager *layers,
                         const PrimitiveList &legacy,
                         DrawingPrimitive *currentPrimitive,
                         const WorldToScreenFn &worldToScreen);

} // namespace TextRenderer
