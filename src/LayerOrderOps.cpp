#include "LayerOrderOps.h"

#include "LayerManager.h"

namespace LayerOrderOps {

void apply(LayerManager *layers,
           const std::vector<DrawingPrimitive *> &selected, Op op)
{
    if (!layers) {
        return;
    }
    for (DrawingPrimitive *obj : selected) {
        if (!obj) {
            continue;
        }
        switch (op) {
        case Op::ToFront:
            layers->bringToFront(obj);
            break;
        case Op::ToBack:
            layers->sendToBack(obj);
            break;
        case Op::Forward:
            layers->bringForward(obj);
            break;
        case Op::Backward:
            layers->sendBackward(obj);
            break;
        }
    }
}

} // namespace LayerOrderOps
