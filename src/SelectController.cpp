#include "SelectController.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QApplication>
#include <QDebug>
#include <cmath>

namespace SelectController {

void onPress(SelectHost &host, QMouseEvent *event)
{
    if (!host.screenToWorld || !host.selectedObjects) {
        return;
    }

    const QVector2D worldPos = host.screenToWorld(event->pos());
    const float zoom = host.zoomLevel ? host.zoomLevel() : 1.0f;

    // RIGHT-CLICK: Image mask edit context menu
    if (event->button() == Qt::RightButton) {
        constexpr float lineTolerance = 30.0f;
        for (auto *selectedObj : host.selectedObjects()) {
            if (!selectedObj || !selectedObj->isSelected()) {
                continue;
            }
            auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObj);
            if (!imgPrim || !imgPrim->isEditMode()) {
                continue;
            }
            const int cpIndex = imgPrim->getControlPointAt(worldPos, 8.0f);
            const int segmentIndex =
                imgPrim->getNearestContourSegment(worldPos, lineTolerance);
            if (host.showImageEditContextMenu) {
                host.showImageEditContextMenu(event->globalPosition().toPoint(),
                                              imgPrim, cpIndex, segmentIndex);
            }
            return;
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const Qt::KeyboardModifiers mods =
        event->modifiers() | QApplication::keyboardModifiers();
    const bool forceAdd = host.forceAdditive && host.forceAdditive();
    const bool forceSub = host.forceSubtractive && host.forceSubtractive();
    const bool additive =
        forceAdd || mods.testFlag(Qt::ShiftModifier) ||
        mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
    const bool subtractive =
        forceSub || mods.testFlag(Qt::AltModifier);

    // 1. Image mask control points
    for (auto *selectedObj : host.selectedObjects()) {
        if (!selectedObj || !selectedObj->isSelected()) {
            continue;
        }
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObj)) {
            if (imgPrim->isEditMode()) {
                const int cpIndex =
                    imgPrim->getControlPointAt(worldPos, 20.0f);
                if (cpIndex >= 0 && host.startControlPointEdit) {
                    if (host.editingPrimitive) {
                        *host.editingPrimitive = selectedObj;
                    }
                    host.startControlPointEdit(selectedObj, cpIndex);
                    return;
                }
            }
        }
    }

    // 1b. Geometry control points (non-image, non-text)
    if (host.findControlPointAt && host.startControlPointEdit) {
        for (auto *selectedObj : host.selectedObjects()) {
            if (!selectedObj || !selectedObj->isSelected()) {
                continue;
            }
            if (dynamic_cast<ImagePrimitive *>(selectedObj) ||
                dynamic_cast<TextPrimitive *>(selectedObj)) {
                continue;
            }
            if (host.editingPrimitive) {
                *host.editingPrimitive = selectedObj;
            }
            const float cpTol = 8.0f / zoom;
            const int cpIndex = host.findControlPointAt(worldPos, cpTol);
            if (cpIndex >= 0) {
                host.startControlPointEdit(selectedObj, cpIndex);
                return;
            }
        }
    }
    if (host.editingPrimitive) {
        *host.editingPrimitive = nullptr;
    }

    auto imageUnderCursorUnselected = [&]() -> bool {
        if (!host.forEachVisiblePrimitiveTopFirst || !host.toObjectLocal) {
            return false;
        }
        bool found = false;
        host.forEachVisiblePrimitiveTopFirst([&](DrawingPrimitive *p) {
            if (found || !p || !p->isVisible() || p->isSelected()) {
                return;
            }
            if (!dynamic_cast<ImagePrimitive *>(p)) {
                return;
            }
            const float hitTol = std::max(4.0f, 10.0f / zoom);
            if (p->containsPoint(host.toObjectLocal(p, worldPos), hitTol)) {
                found = true;
            }
        });
        return found;
    };

    // 2. Resize / rotate handles
    if (!subtractive && !imageUnderCursorUnselected() &&
        host.hitTestSelectionHandle && host.toObjectLocal) {
        for (auto *obj : host.selectedObjects()) {
            if (!obj) {
                continue;
            }
            const QRectF br = obj->boundingRect();
            const bool canRotate =
                host.supportsRotationHandle && host.supportsRotationHandle(obj);
            const int hit = host.hitTestSelectionHandle(
                br, host.toObjectLocal(obj, worldPos), canRotate);
            if (hit < 0) {
                continue;
            }

            if (hit == kHandleRotate && host.isRotatingObject &&
                host.rotatingObject && host.rotationPivot &&
                host.initialRotation && host.rotationStartAngle &&
                host.rotateOrigState) {
                *host.isRotatingObject = true;
                *host.rotatingObject = obj;
                *host.rotationPivot = br.center();
                *host.initialRotation =
                    host.objectRotationDegrees
                        ? host.objectRotationDegrees(obj)
                        : 0.0f;
                *host.rotateOrigState = obj->toJson();
                const QVector2D delta =
                    worldPos - QVector2D(*host.rotationPivot);
                *host.rotationStartAngle =
                    std::atan2(delta.y(), delta.x()) * 180.0f /
                    static_cast<float>(M_PI);
                if (host.setCursor) {
                    host.setCursor(Qt::PointingHandCursor);
                }
                return;
            }

            if (host.isResizingObject && host.resizingObject &&
                host.resizeHandleIndex && host.resizeOrigBounds &&
                host.resizeOrigControlPoints && host.resizeOrigState) {
                *host.isResizingObject = true;
                *host.resizingObject = obj;
                *host.resizeHandleIndex = hit;
                *host.resizeOrigBounds = br;
                *host.resizeOrigControlPoints = obj->getControlPoints();
                *host.resizeOrigState = obj->toJson();
                if (host.setCursor && host.cursorForSelectionHandle) {
                    host.setCursor(host.cursorForSelectionHandle(hit));
                }
                return;
            }
        }
    }

    // 3. Hits under cursor (top → bottom)
    QVector<DrawingPrimitive *> hitsUnderCursor;
    if (host.forEachVisiblePrimitiveTopFirst && host.toObjectLocal) {
        host.forEachVisiblePrimitiveTopFirst([&](DrawingPrimitive *primitive) {
            if (!primitive || !primitive->isVisible()) {
                return;
            }
            const float hitTol = std::max(4.0f, 10.0f / zoom);
            if (primitive->containsPoint(
                    host.toObjectLocal(primitive, worldPos), hitTol)) {
                hitsUnderCursor.append(primitive);
            }
        });
    }

    DrawingPrimitive *clickedObject = nullptr;
    if (!hitsUnderCursor.isEmpty()) {
        for (DrawingPrimitive *p : hitsUnderCursor) {
            if (dynamic_cast<ImagePrimitive *>(p) && !p->isSelected()) {
                clickedObject = p;
                break;
            }
        }
        if (!clickedObject) {
            for (DrawingPrimitive *p : hitsUnderCursor) {
                if (!p->isSelected()) {
                    clickedObject = p;
                    break;
                }
            }
        }
        if (!clickedObject) {
            clickedObject = hitsUnderCursor.first();
        }
    }

    if (clickedObject) {
        auto *clickedImage = dynamic_cast<ImagePrimitive *>(clickedObject);

        if (clickedImage) {
            if (subtractive) {
                if (clickedImage->isSelected()) {
                    clickedImage->setSelected(false);
                    if (host.removeFromSelection) {
                        host.removeFromSelection(clickedImage);
                    }
                    if (host.emitSelectionChanged) {
                        host.emitSelectionChanged();
                    }
                    if (host.requestUpdate) {
                        host.requestUpdate();
                    }
                }
                return;
            }

            if (clickedImage->getMaskCandidateCount() > 0) {
                const int maskIdx = clickedImage->getMaskIndexAt(
                    worldPos, /*preferUnselected=*/true);
                if (maskIdx != -1) {
                    if (!clickedImage->isSelected()) {
                        clickedImage->setSelected(true);
                        if (host.addToSelection) {
                            host.addToSelection(clickedImage);
                        }
                    }
                    clickedImage->setMaskOverlayVisible(true);
                    clickedImage->addMaskCandidateToSelection(maskIdx);
                    if (host.emitSelectionChanged) {
                        host.emitSelectionChanged();
                    }
                    if (host.setSmartHint) {
                        host.setSmartHint(QStringLiteral(
                            "%1 green subject(s) selected — click another to add")
                                              .arg(static_cast<int>(
                                                  clickedImage
                                                      ->selectedMaskIndices()
                                                      .size())));
                    }
                    if (host.requestUpdate) {
                        host.requestUpdate();
                    }
                    return;
                }
            }

            if (clickedImage->isSelected()) {
                if (host.isMoving && host.moveStartPos &&
                    host.totalMoveOffset) {
                    *host.isMoving = true;
                    if (host.isCopyAlongMove) {
                        *host.isCopyAlongMove = false;
                    }
                    *host.moveStartPos = worldPos;
                    *host.totalMoveOffset = QVector2D(0, 0);
                }
                if (host.setCursor) {
                    host.setCursor(Qt::SizeAllCursor);
                }
                return;
            }

            if (!additive && host.clearSelection) {
                host.clearSelection();
            }
            clickedImage->setSelected(true);
            clickedImage->setMaskOverlayVisible(true);
            if (host.addToSelection) {
                host.addToSelection(clickedImage);
            }
            if (host.emitSelectionChanged) {
                host.emitSelectionChanged();
            }
            if (host.setSmartHint) {
                host.setSmartHint(
                    additive
                        ? QStringLiteral(
                              "%1 image(s) selected — click another to add")
                              .arg(static_cast<int>(
                                  host.selectedObjects().size()))
                        : QStringLiteral("Image selected"));
            }
            if (host.requestUpdate) {
                host.requestUpdate();
            }
            return;
        }

        // Non-image
        bool wasAlreadySelected = false;
        for (auto *obj : host.selectedObjects()) {
            if (obj == clickedObject) {
                wasAlreadySelected = true;
                break;
            }
        }

        if (!additive && !subtractive && wasAlreadySelected) {
            if (host.isMoving && host.moveStartPos && host.totalMoveOffset) {
                *host.isMoving = true;
                if (host.isCopyAlongMove) {
                    *host.isCopyAlongMove = false;
                }
                *host.moveStartPos = worldPos;
                *host.totalMoveOffset = QVector2D(0, 0);
            }
            if (host.setCursor) {
                host.setCursor(Qt::SizeAllCursor);
            }
            const Qt::KeyboardModifiers dragMods =
                event->modifiers() | QApplication::keyboardModifiers();
            if (host.isCopyAlongModifier &&
                host.isCopyAlongModifier(dragMods) &&
                host.beginCopyAlongConnectedLineMove) {
                if (host.beginCopyAlongConnectedLineMove()) {
                    if (host.setSmartHint) {
                        host.setSmartHint(QStringLiteral(
                            "Hold ⌘/Ctrl + drag to place copy along line"));
                    }
                }
            } else if (auto *line =
                           dynamic_cast<LinePrimitive *>(clickedObject)) {
                if (host.resolveLineMoveConstraint &&
                    host.resolveLineMoveConstraint(line).lengthSquared() >
                        1e-8f &&
                    host.setSmartHint) {
                    host.setSmartHint(QStringLiteral(
                        "Slide along line · hold ⌘/Ctrl then drag to copy · "
                        "Alt = free"));
                }
            }
            return;
        }

        if (subtractive) {
            if (wasAlreadySelected) {
                clickedObject->setSelected(false);
                if (host.removeFromSelection) {
                    host.removeFromSelection(clickedObject);
                }
                if (host.emitSelectionChanged) {
                    host.emitSelectionChanged();
                }
                if (host.requestUpdate) {
                    host.requestUpdate();
                }
            }
            return;
        }

        if (additive && wasAlreadySelected) {
            clickedObject->setSelected(false);
            if (host.removeFromSelection) {
                host.removeFromSelection(clickedObject);
            }
            if (host.emitSelectionChanged) {
                host.emitSelectionChanged();
            }
            if (host.requestUpdate) {
                host.requestUpdate();
            }
            return;
        }

        if (!additive && host.clearSelection) {
            host.clearSelection();
        }
        clickedObject->setSelected(true);
        if (host.addToSelection) {
            host.addToSelection(clickedObject);
        }
        if (!additive && !subtractive && host.selectGroupMembers) {
            host.selectGroupMembers(clickedObject);
        }
        if (host.emitSelectionChanged) {
            host.emitSelectionChanged();
        }
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    // Empty space → marquee / lasso
    if (!additive && !subtractive && host.clearSelection) {
        host.clearSelection();
        if (host.requestUpdate) {
            host.requestUpdate();
        }
    }
    if (host.selectionOperation) {
        *host.selectionOperation =
            subtractive ? SelectionManager::SelectionOperation::Subtract
                        : (additive ? SelectionManager::SelectionOperation::Add
                                    : SelectionManager::SelectionOperation::
                                          Replace);
    }
    if (host.setIsSelecting) {
        host.setIsSelecting(true);
    }
    if (host.drawStartPos) {
        *host.drawStartPos = worldPos;
    }
    const int mode = host.selectionMode ? host.selectionMode() : 0;
    if (mode == 1 && host.lassoPoints) {
        host.lassoPoints->clear();
        *host.lassoPoints << worldPos.toPointF();
        if (host.setSelectionRect) {
            host.setSelectionRect(QRectF());
        }
    } else if (host.setSelectionRect) {
        host.setSelectionRect(QRectF(worldPos.toPointF(), QSizeF(0, 0)));
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

} // namespace SelectController
