#include "PrimitiveJsonOps.h"

#include "DrawingPrimitive.h"

namespace PrimitiveJsonOps {

std::vector<QJsonObject>
snapshotStates(const std::vector<DrawingPrimitive *> &objects)
{
    std::vector<QJsonObject> states;
    states.reserve(objects.size());
    for (DrawingPrimitive *obj : objects) {
        if (obj) {
            states.push_back(obj->toJson());
        }
    }
    return states;
}

} // namespace PrimitiveJsonOps
