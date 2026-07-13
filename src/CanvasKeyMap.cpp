#include "CanvasKeyMap.h"

namespace CanvasKeyMap {

Action mapPress(int key, Qt::KeyboardModifiers mods)
{
    switch (key) {
    case Qt::Key_Space:
        return Action::SpacePanBegin;
    case Qt::Key_Escape:
        return Action::EscapeCancel;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        return Action::DeleteSelection;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        return Action::ZoomIn;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        return Action::ZoomOut;
    case Qt::Key_0:
        if (mods.testFlag(Qt::ControlModifier)) {
            return Action::ZoomFit;
        }
        return Action::PassThrough;
    case Qt::Key_BracketLeft:
        return Action::MaskPrev;
    case Qt::Key_BracketRight:
        return Action::MaskNext;
    case Qt::Key_I:
        if (mods.testFlag(Qt::ControlModifier)) {
            return Action::MaskInvert;
        }
        return Action::PassThrough;
    default:
        return Action::PassThrough;
    }
}

Action mapRelease(int key)
{
    if (key == Qt::Key_Space) {
        return Action::SpacePanEnd;
    }
    return Action::PassThrough;
}

} // namespace CanvasKeyMap
