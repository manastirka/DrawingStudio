#include "SelectionGestureController.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QLineF>
#include <algorithm>
#include <cmath>

namespace SelectionGestureController {
namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void onResizeDrag(SelectionGestureHost &host, const QVector2D &worldPos,
                  Qt::KeyboardModifiers mods)
{
    if (!host.isResizing || !*host.isResizing || !host.resizingObject ||
        !*host.resizingObject || !host.resizeOrigBounds ||
        !host.resizeHandleIndex || !host.computeResizedBounds ||
        !host.toObjectLocal) {
        return;
    }

    DrawingPrimitive *obj = *host.resizingObject;
    Qt::KeyboardModifiers m = mods;
    if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
        if (img->maintainAspectRatio()) {
            m |= Qt::ShiftModifier;
        }
    }

    const QRectF orig = *host.resizeOrigBounds;
    const QVector2D localPos = host.toObjectLocal(obj, worldPos);
    const QRectF nr = host.computeResizedBounds(
        orig, *host.resizeHandleIndex, localPos, m);
    if (nr.width() <= 1.0f || nr.height() <= 1.0f) {
        return;
    }

    if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
        img->setPosition(QVector2D(static_cast<float>(nr.x()),
                                   static_cast<float>(nr.y())));
        img->setSize(QVector2D(static_cast<float>(nr.width()),
                               static_cast<float>(nr.height())));
    } else if (auto *text = dynamic_cast<TextPrimitive *>(obj)) {
        text->setPosition(QVector2D(static_cast<float>(nr.left()),
                                    static_cast<float>(nr.bottom())));
        const float scaleSafe = std::max(0.001f, text->scale());
        text->setTextBoxWidth(static_cast<float>(nr.width()) / scaleSafe);
        text->setTextBoxHeight(static_cast<float>(nr.height()) / scaleSafe);
    } else {
        const float sx = static_cast<float>(nr.width() / orig.width());
        const float sy = static_cast<float>(nr.height() / orig.height());
        const auto &baseCps =
            (host.resizeOrigControlPoints &&
             !host.resizeOrigControlPoints->empty())
                ? *host.resizeOrigControlPoints
                : obj->getControlPoints();
        for (int i = 0; i < static_cast<int>(baseCps.size()); ++i) {
            const float nx =
                static_cast<float>(nr.x()) +
                (baseCps[static_cast<size_t>(i)].x() -
                 static_cast<float>(orig.x())) *
                    sx;
            const float ny =
                static_cast<float>(nr.y()) +
                (baseCps[static_cast<size_t>(i)].y() -
                 static_cast<float>(orig.y())) *
                    sy;
            obj->setControlPointPosition(i, QVector2D(nx, ny));
        }
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onRotateDrag(SelectionGestureHost &host, const QVector2D &worldPos,
                  Qt::KeyboardModifiers mods)
{
    if (!host.isRotating || !*host.isRotating || !host.rotatingObject ||
        !*host.rotatingObject || !host.rotationPivot ||
        !host.initialRotation || !host.rotationStartAngle ||
        !host.applyObjectRotation) {
        return;
    }

    const QVector2D pivot(*host.rotationPivot);
    const QVector2D delta = worldPos - pivot;
    if (delta.length() <= 0.001f) {
        return;
    }
    float angle =
        std::atan2(delta.y(), delta.x()) * 180.0f / kPi;
    float newRot = *host.initialRotation + (angle - *host.rotationStartAngle);
    if (mods.testFlag(Qt::ShiftModifier)) {
        newRot = std::round(newRot / 15.0f) * 15.0f;
    }
    host.applyObjectRotation(*host.rotatingObject, newRot);
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onResizeFinish(SelectionGestureHost &host)
{
    if (!host.isResizing || !*host.isResizing) {
        return;
    }
    if (host.resizingObject && *host.resizingObject && host.resizeOrigState &&
        !host.resizeOrigState->isEmpty() && host.emitTransformCommand) {
        const QJsonObject newState = (*host.resizingObject)->toJson();
        if (newState != *host.resizeOrigState) {
            host.emitTransformCommand(*host.resizingObject, *host.resizeOrigState,
                                      newState, QStringLiteral("Resize"),
                                      /*isRotate=*/false);
        }
    }
    *host.isResizing = false;
    if (host.resizingObject) {
        *host.resizingObject = nullptr;
    }
    if (host.resizeHandleIndex) {
        *host.resizeHandleIndex = -1;
    }
    if (host.resizeOrigControlPoints) {
        host.resizeOrigControlPoints->clear();
    }
    if (host.resizeOrigState) {
        *host.resizeOrigState = QJsonObject();
    }
    if (host.setCursor) {
        host.setCursor(Qt::ArrowCursor);
    }
    if (host.emitSelectionChanged) {
        host.emitSelectionChanged();
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onRotateFinish(SelectionGestureHost &host)
{
    if (!host.isRotating || !*host.isRotating) {
        return;
    }
    if (host.rotatingObject && *host.rotatingObject && host.rotateOrigState &&
        !host.rotateOrigState->isEmpty() && host.emitTransformCommand) {
        const QJsonObject newState = (*host.rotatingObject)->toJson();
        if (newState != *host.rotateOrigState) {
            host.emitTransformCommand(*host.rotatingObject, *host.rotateOrigState,
                                      newState, QStringLiteral("Rotate"),
                                      /*isRotate=*/true);
        }
    }
    *host.isRotating = false;
    if (host.rotatingObject) {
        *host.rotatingObject = nullptr;
    }
    if (host.rotateOrigState) {
        *host.rotateOrigState = QJsonObject();
    }
    if (host.setCursor) {
        host.setCursor(Qt::ArrowCursor);
    }
    if (host.emitSelectionChanged) {
        host.emitSelectionChanged();
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onMarqueeUpdate(SelectionGestureHost &host, const QVector2D &worldPos)
{
    if (!host.isSelecting || !host.isSelecting()) {
        return;
    }
    const int mode = host.selectionMode ? host.selectionMode() : 0;
    if (mode == 1 && host.lassoPoints) {
        const float zoom = host.zoomLevel ? host.zoomLevel() : 1.0f;
        if (host.lassoPoints->isEmpty() ||
            QLineF(host.lassoPoints->last(), worldPos.toPointF()).length() >
                (2.0f / zoom)) {
            *host.lassoPoints << worldPos.toPointF();
        }
    } else if (host.drawStartPos && host.setSelectionRect) {
        host.setSelectionRect(
            QRectF(host.drawStartPos->toPointF(), worldPos.toPointF())
                .normalized());
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onMarqueeFinish(SelectionGestureHost &host)
{
    if (!host.isSelecting || !host.isSelecting()) {
        return;
    }
    const auto op =
        host.selectionOperation
            ? *host.selectionOperation
            : SelectionManager::SelectionOperation::Replace;
    const int mode = host.selectionMode ? host.selectionMode() : 0;
    if (mode == 1 && host.lassoPoints && host.selectInLasso) {
        host.selectInLasso(*host.lassoPoints, op);
        host.lassoPoints->clear();
    } else if (host.selectionRect && host.selectInRect) {
        host.selectInRect(host.selectionRect(), op);
    }
    if (host.setIsSelecting) {
        host.setIsSelecting(false);
    }
    if (host.selectionOperation) {
        *host.selectionOperation =
            SelectionManager::SelectionOperation::Replace;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void onSelectHover(SelectionGestureHost &host, const QVector2D &worldPos)
{
    if (!host.selectedObjects || !host.toObjectLocal || !host.hitTestHandle ||
        !host.setCursor) {
        return;
    }
    const float zoom = host.zoomLevel ? host.zoomLevel() : 1.0f;
    bool onHandle = false;
    for (auto *obj : host.selectedObjects()) {
        if (!obj) {
            continue;
        }
        const QRectF br = obj->boundingRect();
        const bool canRot =
            host.supportsRotation && host.supportsRotation(obj);
        const int hit = host.hitTestHandle(
            br, host.toObjectLocal(obj, worldPos), canRot);
        if (hit >= 0) {
            if (host.cursorForHandle) {
                host.setCursor(host.cursorForHandle(hit));
            }
            onHandle = true;
            break;
        }
    }
    if (onHandle) {
        return;
    }
    bool onSelected = false;
    for (auto *obj : host.selectedObjects()) {
        if (obj &&
            obj->containsPoint(host.toObjectLocal(obj, worldPos),
                               5.0f / zoom)) {
            host.setCursor(Qt::SizeAllCursor);
            onSelected = true;
            break;
        }
    }
    if (!onSelected) {
        host.setCursor(Qt::ArrowCursor);
    }
}

} // namespace SelectionGestureController
