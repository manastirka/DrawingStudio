#include "GroupOps.h"

#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

namespace GroupOps {

bool canGroup(const std::vector<DrawingPrimitive *> &selected)
{
    int count = 0;
    for (auto *obj : selected) {
        if (obj) {
            ++count;
            if (count >= 2) {
                return true;
            }
        }
    }
    return false;
}

bool anyGrouped(const std::vector<DrawingPrimitive *> &selected)
{
    for (auto *obj : selected) {
        if (obj && !obj->groupId().isNull()) {
            return true;
        }
    }
    return false;
}

QUuid assignNewGroup(const std::vector<DrawingPrimitive *> &selected)
{
    if (!canGroup(selected)) {
        return {};
    }
    const QUuid gid = QUuid::createUuid();
    for (auto *obj : selected) {
        if (obj) {
            obj->setGroupId(gid);
        }
    }
    return gid;
}

void clearGroups(const std::vector<DrawingPrimitive *> &selected)
{
    for (auto *obj : selected) {
        if (obj) {
            obj->setGroupId(QUuid());
        }
    }
}

std::vector<DrawingPrimitive *>
collectMembers(LayerManager *layers, const PrimitiveList &legacy,
               const QUuid &groupId)
{
    std::vector<DrawingPrimitive *> members;
    if (groupId.isNull()) {
        return members;
    }

    auto tryAdd = [&](DrawingPrimitive *p) {
        if (p && p->isVisible() && p->groupId() == groupId) {
            members.push_back(p);
        }
    };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible() || layer->isLocked()) {
                continue;
            }
            for (const auto &prim : layer->primitives()) {
                tryAdd(prim.get());
            }
        }
    } else {
        for (const auto &prim : legacy) {
            tryAdd(prim.get());
        }
    }
    return members;
}

} // namespace GroupOps
