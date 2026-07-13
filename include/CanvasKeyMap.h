#pragma once

#include <Qt>

/**
 * Pure keyboard → canvas action mapping (refactor D5).
 * Side effects stay on DrawingCanvas.
 */
namespace CanvasKeyMap {

enum class Action {
    None,
    SpacePanBegin,
    SpacePanEnd,
    EscapeCancel,
    DeleteSelection,
    ZoomIn,
    ZoomOut,
    ZoomFit,
    MaskPrev,
    MaskNext,
    MaskInvert,
    PassThrough // let QWidget handle
};

Action mapPress(int key, Qt::KeyboardModifiers mods);
Action mapRelease(int key);

} // namespace CanvasKeyMap
