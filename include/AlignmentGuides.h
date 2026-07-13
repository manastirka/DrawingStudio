#pragma once

#include <QRectF>
#include <QVector2D>
#include <vector>

/**
 * Pure object-edge alignment guide computation during move (refactor D2).
 * Canvas maps Guide::axis into its AlignmentType for rendering.
 */
namespace AlignmentGuides {

enum class Axis { Horizontal, Vertical };

struct Guide {
    QVector2D position; // y used for Horizontal, x for Vertical
    Axis axis = Axis::Horizontal;
    bool isActive = true;
};

struct Result {
    std::vector<Guide> guides;
    bool snapActive = false;
    QVector2D snapPos;
};

/**
 * Compute nearest horizontal/vertical guides from unselected object bounds.
 * @param mousePos   current drag/cursor position in world space
 * @param zoomLevel  for tolerance scaling (10 / zoom)
 * @param bounds     bounding rects of objects to snap against
 */
Result compute(const QVector2D &mousePos, float zoomLevel,
               const std::vector<QRectF> &bounds);

/** True if @p bounds edges/centers lie near any active guide. */
bool objectNearGuides(const QRectF &bounds, const std::vector<Guide> &guides,
                      float tolerance);

} // namespace AlignmentGuides
