#pragma once

#include <QRectF>
#include <QVector2D>
#include <vector>

class DrawingPrimitive;

/**
 * Pure layout operations on primitive sets (refactor D4).
 * Align / distribute selected objects relative to each other or a page rect.
 *
 * Align values match DrawingCanvas::AlignmentType integer order so they may
 * be cast safely at the canvas boundary.
 */
namespace ObjectLayoutOps {

enum class Align : int {
    None = 0,
    Left,
    Right,
    CenterHorizontal,
    Top,
    Bottom,
    CenterVertical,
    PageLeft,
    PageRight,
    PageCenterHorizontal,
    PageTop,
    PageBottom,
    PageCenterVertical
};

/**
 * Translate @p objects so edges/centers match @p type.
 * @param pageRect  world-space paper rect (for Page* modes); ignored otherwise
 * @return number of objects translated
 */
int alignObjects(const std::vector<DrawingPrimitive *> &objects, Align type,
                 const QRectF &pageRect);

/**
 * Evenly distribute gaps between ≥3 objects along X (horizontal) or Y.
 * @return number of objects translated, or 0 if skipped
 */
int distributeObjects(const std::vector<DrawingPrimitive *> &objects,
                      bool horizontal);

} // namespace ObjectLayoutOps
