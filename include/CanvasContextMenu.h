#pragma once

#include <QPoint>
#include <functional>

class QMenu;
class QWidget;

/**
 * Builds the canvas context menu (refactor D5 / E9).
 * Actions are injected so the menu stays free of DrawingCanvas types.
 */
namespace CanvasContextMenu {

struct Actions {
    std::function<void()> deleteSelected;
    std::function<void()> bringToFront;
    std::function<void()> sendToBack;
    std::function<void()> bringForward;
    std::function<void()> sendBackward;
    std::function<void()> zoomFit;
    std::function<void()> zoomActual;
};

/** Creates a styled QMenu; caller owns the pointer (parented to @p parent). */
QMenu *build(QWidget *parent, bool hasSelection, const Actions &actions);

/** Shared dark translucent stylesheet. */
const char *styleSheet();

/** Result of image-mask control-point context menu (E9). */
enum class MaskEditAction {
    None,
    DeleteControlPoint,
    InsertControlPoint
};

/**
 * Modal mask CP edit menu at @p globalPos.
 * @param canDelete  show delete when a CP index is valid
 * @param canInsert  show insert when a segment index is valid
 */
MaskEditAction execMaskEditMenu(QWidget *parent, const QPoint &globalPos,
                                bool canDelete, bool canInsert);

} // namespace CanvasContextMenu
