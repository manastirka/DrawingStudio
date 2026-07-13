#include "InteractionPhase.h"

namespace InteractionPhase {

Move classifyMove(const MoveContext &c)
{
    if (c.editingControlPoints) {
        return Move::ControlPointEdit;
    }
    if (c.brushing || c.blurring) {
        return Move::BrushOrBlur;
    }
    if (c.panning) {
        return Move::Pan;
    }
    if (c.selecting) {
        return Move::Marquee;
    }
    if (c.resizingObject) {
        return Move::ResizeObject;
    }
    if (c.rotatingObject) {
        return Move::RotateObject;
    }
    if (c.moving) {
        return Move::MoveSelection;
    }
    if (c.isTextTool || c.isDrawing) {
        return Move::ActiveTool;
    }
    if (c.isSelectTool && !c.moving && !c.resizingObject && !c.rotatingObject &&
        !c.editingControlPoints && !c.selecting) {
        return Move::SelectHover;
    }
    return Move::Idle;
}

Release classifyRelease(const ReleaseContext &c)
{
    if (c.leftButton && (c.brushing || c.blurring)) {
        return Release::BrushOrBlur;
    }
    if (c.leftButton && c.editingControlPoints) {
        return Release::ControlPointEdit;
    }
    if (c.leftButton && c.resizingObject) {
        return Release::ResizeObject;
    }
    if (c.leftButton && c.rotatingObject) {
        return Release::RotateObject;
    }
    if (c.leftButton && c.rotatingText) {
        return Release::RotateText;
    }
    if (c.leftButton && c.resizingText) {
        return Release::ResizeText;
    }
    if (c.leftButton && c.moving) {
        return Release::MoveSelection;
    }
    if (c.leftButton && c.isTextTool) {
        return Release::TextTool;
    }
    if (c.middleButton && c.panning) {
        return Release::Pan;
    }
    if (c.leftButton && c.selecting) {
        return Release::Marquee;
    }
    if (c.leftButton && c.isDrawing) {
        return Release::DrawingSession;
    }
    return Release::None;
}

} // namespace InteractionPhase
