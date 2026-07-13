#include "ControlPointEditController.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

namespace ControlPointEditController {

void start(ControlPointEditHost &host, DrawingPrimitive *primitive,
           int controlPointIndex)
{
    if (!host.isEditing || !host.selectedIndex || !host.editingPrimitive ||
        !host.originalPosition || !host.origState) {
        return;
    }

    *host.editingPrimitive = primitive;
    *host.selectedIndex = controlPointIndex;
    *host.isEditing = true;
    *host.origState = QJsonObject();

    if (!primitive) {
        return;
    }
    *host.origState = primitive->toJson();

    if (auto *img = dynamic_cast<ImagePrimitive *>(primitive);
        img && img->isEditMode()) {
        const auto contour = img->getEditableContour();
        if (controlPointIndex >= 0 &&
            controlPointIndex < static_cast<int>(contour.size())) {
            const QPointF maskPoint = contour[static_cast<size_t>(controlPointIndex)];
            *host.originalPosition =
                QVector2D(static_cast<float>(maskPoint.x()),
                          static_cast<float>(maskPoint.y()));
        }
    } else {
        const auto cps = primitive->getControlPoints();
        if (controlPointIndex >= 0 &&
            controlPointIndex < static_cast<int>(cps.size())) {
            *host.originalPosition = cps[static_cast<size_t>(controlPointIndex)];
        }
    }
    qDebug() << "ControlPointEditController::start" << controlPointIndex;
}

void update(ControlPointEditHost &host, const QVector2D &newPos,
            Qt::KeyboardModifiers mods)
{
    if (!host.isEditing || !*host.isEditing || !host.editingPrimitive ||
        !*host.editingPrimitive || !host.selectedIndex ||
        *host.selectedIndex < 0) {
        return;
    }

    DrawingPrimitive *prim = *host.editingPrimitive;
    const int idx = *host.selectedIndex;

    if (auto *img = dynamic_cast<ImagePrimitive *>(prim)) {
        if (img->isEditMode()) {
            img->moveControlPoint(idx, newPos);
            if (host.requestUpdate) {
                host.requestUpdate();
            }
            return;
        }
    }

    auto cps = prim->getControlPoints();
    if (idx >= static_cast<int>(cps.size())) {
        return;
    }

    QVector2D snappedPos =
        host.snapToGrid ? host.snapToGrid(newPos) : newPos;

    if (auto *line = dynamic_cast<LinePrimitive *>(prim)) {
        if (idx == 0) {
            QVector2D constrained = snappedPos;
            if (host.findPrimitiveById && host.projectOntoSegment) {
                if (auto *prev = dynamic_cast<LinePrimitive *>(
                        host.findPrimitiveById(line->connectedLineId()))) {
                    constrained = host.projectOntoSegment(
                        snappedPos, prev->startPoint(), prev->endPoint());
                } else if (host.resolveLineMoveConstraint &&
                           host.originalPosition) {
                    const QVector2D dir =
                        host.resolveLineMoveConstraint(line);
                    if (dir.lengthSquared() > 1e-8f) {
                        const QVector2D origin = *host.originalPosition;
                        const float t =
                            QVector2D::dotProduct(snappedPos - origin, dir);
                        constrained = origin + dir * t;
                    }
                }
            }
            const QVector2D delta = constrained - line->startPoint();
            line->setStartPoint(constrained);
            line->setEndPoint(line->endPoint() + delta);
            if (host.requestUpdate) {
                host.requestUpdate();
            }
            return;
        }
        if (idx == 1 && !mods.testFlag(Qt::AltModifier)) {
            QVector2D axis = line->endPoint() - line->startPoint();
            if (axis.lengthSquared() < 1e-8f) {
                axis = QVector2D(1.0f, 0.0f);
            } else {
                axis.normalize();
            }
            const float t =
                QVector2D::dotProduct(snappedPos - line->startPoint(), axis);
            snappedPos = line->startPoint() + axis * std::max(0.0f, t);
        }
    }

    prim->setControlPointPosition(idx, snappedPos);
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void finish(ControlPointEditHost &host)
{
    if (!host.isEditing) {
        return;
    }

    if (host.editingPrimitive && *host.editingPrimitive &&
        host.selectedIndex && *host.selectedIndex >= 0 &&
        host.originalPosition) {
        DrawingPrimitive *prim = *host.editingPrimitive;
        const int idx = *host.selectedIndex;

        if (auto *img = dynamic_cast<ImagePrimitive *>(prim);
            img && img->isEditMode()) {
            const auto contour = img->getEditableContour();
            if (idx < static_cast<int>(contour.size()) &&
                host.emitMaskCpCommand) {
                const QPointF current = contour[static_cast<size_t>(idx)];
                const QPointF original(host.originalPosition->x(),
                                       host.originalPosition->y());
                const QVector2D diff(
                    static_cast<float>(current.x() - original.x()),
                    static_cast<float>(current.y() - original.y()));
                if (diff.length() > 0.1f) {
                    host.emitMaskCpCommand(prim, idx, original, current);
                }
            }
        } else {
            const auto cps = prim->getControlPoints();
            if (idx < static_cast<int>(cps.size())) {
                const QVector2D current = cps[static_cast<size_t>(idx)];
                const bool moved =
                    (current - *host.originalPosition).length() > 0.1f;
                auto *line = dynamic_cast<LinePrimitive *>(prim);
                const bool slideAlongPrev =
                    line && idx == 0 &&
                    (!line->connectedLineId().isNull() ||
                     line->moveConstraintDirection().lengthSquared() > 1e-8f);

                if (moved && slideAlongPrev && host.origState &&
                    !host.origState->isEmpty() && host.emitTransformCommand) {
                    host.emitTransformCommand(
                        prim, *host.origState, prim->toJson(),
                        QStringLiteral("Slide along line"));
                } else if (moved && host.emitCpCommand) {
                    host.emitCpCommand(prim, idx, *host.originalPosition,
                                       current);
                }
            }
        }
    }

    *host.isEditing = false;
    if (host.selectedIndex) {
        *host.selectedIndex = -1;
    }
    if (host.editingPrimitive) {
        *host.editingPrimitive = nullptr;
    }
    if (host.origState) {
        *host.origState = QJsonObject();
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

} // namespace ControlPointEditController
