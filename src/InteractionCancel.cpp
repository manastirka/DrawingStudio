#include "InteractionCancel.h"

namespace InteractionCancel {

void applyEscape(const Targets &t)
{
    if (t.isDrawing) {
        *t.isDrawing = false;
    }
    if (t.setIsSelecting) {
        t.setIsSelecting(false);
    }
    if (t.isPanning) {
        *t.isPanning = false;
    }
    if (t.setArrowCursor) {
        t.setArrowCursor();
    }
    if (t.clearSelection) {
        t.clearSelection();
    }
    if (t.requestUpdate) {
        t.requestUpdate();
    }
}

void endSpacePan(bool *isPanning, const std::function<void()> &setArrowCursor)
{
    if (!isPanning || !*isPanning) {
        return;
    }
    *isPanning = false;
    if (setArrowCursor) {
        setArrowCursor();
    }
}

} // namespace InteractionCancel
