#pragma once

#include "AlignmentGuides.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QVector2D>
#include <functional>
#include <vector>

class QPainter;

/**
 * Pure overlay drawing helpers (screen or world space).
 * Extracted from DrawingCanvas (refactor B5 / E6).
 */
class OverlayRenderer {
public:
    struct GuideLine {
        enum class Axis { Vertical, Horizontal };
        Axis axis = Axis::Vertical;
        float position = 0.f; // x for Vertical, y for Horizontal (world)
        QColor color = QColor(204, 204, 204, 153);
    };

    /** Control point disc in world space (caller sets world transform). */
    static void drawControlPoint(QPainter &painter, const QVector2D &point,
                                 float zoomLevel, bool highlighted);

    /** Magnetic snap crosshair in screen space (caller resets transform). */
    static void drawSnapIndicator(QPainter &painter, const QPoint &screenCenter);

    /** Brush/eraser soft ring in screen space. */
    static void drawCursorPreviewRing(QPainter &painter, const QPoint &screenCenter,
                                      float radiusScreenPx);

    /** Lasso polygon in screen space. */
    static void drawLassoPolygon(QPainter &painter, const QPolygonF &screenPoly);

    /** Alignment guide lines in world space (caller sets world transform). */
    static void drawAlignmentGuides(QPainter &painter,
                                    const std::vector<GuideLine> &guides);

    /**
     * Convert pure AlignmentGuides::Guide → overlay lines (object-snap cyan).
     * Inactive guides are skipped.
     */
    static std::vector<GuideLine>
    guideLinesFrom(const std::vector<AlignmentGuides::Guide> &guides,
                   const QColor &objectColor = QColor(0, 204, 255, 204));

    /**
     * Map world-space lasso vertices to a screen polygon (E10).
     * @param worldToScreen  world → screen pixel converter
     */
    static QPolygonF lassoScreenPolygon(
        const QPolygonF &worldPoints,
        const std::function<QPoint(const QVector2D &)> &worldToScreen);
};
