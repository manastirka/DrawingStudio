#include "MoveController.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QApplication>
#include <QDebug>

namespace MoveController {

void onPress(MoveHost &host, QMouseEvent *event)
{
    if (!host.screenToWorld || !host.isMoving || !host.moveStartPos ||
        !host.totalMoveOffset) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "MoveController::onPress";
    const QVector2D worldPos = host.screenToWorld(event->pos());
    const float cpTol =
        host.controlPointTolerance ? host.controlPointTolerance() : 8.0f;
    const float hitTol =
        host.objectHitTolerance ? host.objectHitTolerance() : 5.0f;

    if (host.selectedObjects && host.findControlPointAt &&
        host.startControlPointEdit && host.editingPrimitive) {
        for (auto *selectedObj : host.selectedObjects()) {
            if (!selectedObj || !selectedObj->isSelected()) {
                continue;
            }
            if (dynamic_cast<ImagePrimitive *>(selectedObj)) {
                continue;
            }
            *host.editingPrimitive = selectedObj;
            const int cpIndex = host.findControlPointAt(worldPos, cpTol);
            if (cpIndex >= 0) {
                host.startControlPointEdit(selectedObj, cpIndex);
                return;
            }
        }
        *host.editingPrimitive = nullptr;
    }

    DrawingPrimitive *clickedObject = nullptr;
    if (host.selectedObjects) {
        for (auto *selectedObj : host.selectedObjects()) {
            if (selectedObj &&
                selectedObj->containsPoint(worldPos, hitTol)) {
                clickedObject = selectedObj;
                break;
            }
        }
    }
    if (!clickedObject && host.findPrimitiveAt) {
        clickedObject = host.findPrimitiveAt(worldPos);
    }
    if (!clickedObject) {
        qDebug() << "MoveController: no object under cursor";
        return;
    }

    if (!clickedObject->isSelected()) {
        const bool isImage =
            dynamic_cast<ImagePrimitive *>(clickedObject) != nullptr;
        const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
        const bool additive =
            isImage || mods.testFlag(Qt::ShiftModifier) ||
            mods.testFlag(Qt::ControlModifier) ||
            mods.testFlag(Qt::MetaModifier);
        if (!additive && host.clearSelection) {
            host.clearSelection();
        }
        clickedObject->setSelected(true);
        if (auto *img = dynamic_cast<ImagePrimitive *>(clickedObject)) {
            img->setMaskOverlayVisible(true);
        }
        if (host.addToSelection) {
            host.addToSelection(clickedObject);
        }
        if (host.emitSelectionChanged) {
            host.emitSelectionChanged();
        }
    }

    *host.isMoving = true;
    *host.moveStartPos = worldPos;
    *host.totalMoveOffset = QVector2D(0, 0);
    if (host.isCopyAlongMove) {
        *host.isCopyAlongMove = false;
    }

    if (host.setCursor) {
        if (dynamic_cast<TextPrimitive *>(clickedObject)) {
            host.setCursor(Qt::IBeamCursor);
        } else {
            host.setCursor(Qt::SizeAllCursor);
        }
    }
}

void onDrag(MoveHost &host, const QVector2D &worldPos)
{
    if (!host.isMoving || !*host.isMoving || !host.moveStartPos ||
        !host.totalMoveOffset || !host.selectedObjects) {
        return;
    }

    if (host.updateAlignmentGuides) {
        host.updateAlignmentGuides(worldPos);
    }

    const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    QVector2D currentOffset = worldPos - *host.moveStartPos;
    if (host.constrainDelta) {
        currentOffset = host.constrainDelta(currentOffset, mods);
    }
    const QVector2D deltaOffset = currentOffset - *host.totalMoveOffset;

    const auto selected = host.selectedObjects();
    if (selected.empty()) {
        return;
    }
    for (auto *obj : selected) {
        if (obj) {
            obj->translate(deltaOffset);
        }
    }
    *host.totalMoveOffset = currentOffset;
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onFinish(MoveHost &host)
{
    if (!host.isMoving) {
        return;
    }
    *host.isMoving = false;

    if (host.setCursor) {
        const bool moveTool =
            host.isMoveToolActive && host.isMoveToolActive();
        host.setCursor(moveTool ? Qt::SizeAllCursor : Qt::ArrowCursor);
    }

    const bool copyAlong =
        host.isCopyAlongMove && *host.isCopyAlongMove;
    const float moved =
        host.totalMoveOffset ? host.totalMoveOffset->length() : 0.0f;

    if (moved > 1.0f) {
        if (host.selectedObjects && host.emitMoveCommand) {
            const auto selected = host.selectedObjects();
            if (!selected.empty() && host.totalMoveOffset) {
                host.emitMoveCommand(selected, *host.totalMoveOffset);
            }
        }
        if (copyAlong && host.setSmartHint) {
            host.setSmartHint(
                QStringLiteral("Copied along connected line"));
        }
    } else if (copyAlong) {
        if (host.deleteSelectedWithCommand) {
            host.deleteSelectedWithCommand();
        }
        if (host.setSmartHint) {
            host.setSmartHint(QString());
        }
    }

    if (host.isCopyAlongMove) {
        *host.isCopyAlongMove = false;
    }
}

} // namespace MoveController
