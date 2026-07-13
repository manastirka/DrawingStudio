#include "LineMoveOps.h"

#include "DrawingPrimitive.h"

#include <cmath>

namespace LineMoveOps {

bool isCopyAlongModifier(Qt::KeyboardModifiers mods)
{
    return mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
}

QVector2D resolveConstraint(
    const LinePrimitive *line,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById)
{
    if (!line) {
        return {};
    }
    if (findById) {
        if (auto *prev =
                dynamic_cast<LinePrimitive *>(findById(line->connectedLineId()))) {
            QVector2D dir = prev->endPoint() - prev->startPoint();
            if (dir.lengthSquared() > 1e-8f) {
                dir.normalize();
                return dir;
            }
        }
    }
    QVector2D dir = line->moveConstraintDirection();
    if (dir.lengthSquared() > 1e-8f) {
        dir.normalize();
        return dir;
    }
    return {};
}

std::vector<LinePrimitive *> collectConstrainedLines(
    const std::vector<DrawingPrimitive *> &selected,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById)
{
    std::vector<LinePrimitive *> sources;
    for (auto *obj : selected) {
        auto *line = dynamic_cast<LinePrimitive *>(obj);
        if (line &&
            resolveConstraint(line, findById).lengthSquared() > 1e-8f) {
            sources.push_back(line);
        }
    }
    return sources;
}

QVector2D constrainDelta(
    const QVector2D &delta, Qt::KeyboardModifiers mods,
    const std::vector<DrawingPrimitive *> &selected,
    const std::function<DrawingPrimitive *(const QUuid &)> &findById)
{
    if (mods.testFlag(Qt::AltModifier)) {
        return delta;
    }

    QVector2D constraint;
    bool found = false;
    bool mixed = false;
    for (auto *obj : selected) {
        auto *line = dynamic_cast<LinePrimitive *>(obj);
        if (!line) {
            continue;
        }
        const QVector2D dir = resolveConstraint(line, findById);
        if (dir.lengthSquared() < 1e-8f) {
            continue;
        }
        if (!found) {
            constraint = dir;
            found = true;
        } else if (std::abs(QVector2D::dotProduct(constraint, dir)) < 0.98f) {
            mixed = true;
            break;
        }
    }
    if (!found || mixed) {
        return delta;
    }

    const float t = QVector2D::dotProduct(delta, constraint);
    return constraint * t;
}

} // namespace LineMoveOps
