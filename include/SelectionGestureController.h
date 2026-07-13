#pragma once

#include "SelectionManager.h"

#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <vector>

class DrawingPrimitive;

/**
 * Mid-gesture selection interactions (C3):
 * resize/rotate drag + commit, marquee/lasso update + finalize, handle hover cursor.
 * Press-to-begin stays in SelectController; session state on canvas via host.
 */
struct SelectionGestureHost {
    std::function<QVector2D(const DrawingPrimitive *, const QVector2D &)>
        toObjectLocal;
    std::function<QRectF(const QRectF &, int, const QVector2D &,
                         Qt::KeyboardModifiers)>
        computeResizedBounds;
    std::function<void(DrawingPrimitive *, float degrees)> applyObjectRotation;
    std::function<void(DrawingPrimitive *, const QJsonObject &oldState,
                       const QJsonObject &newState, const QString &label,
                       bool isRotate)>
        emitTransformCommand;
    std::function<void()> requestUpdate;
    std::function<void()> emitSelectionChanged;
    std::function<void(Qt::CursorShape)> setCursor;
    std::function<std::vector<DrawingPrimitive *>()> selectedObjects;
    std::function<int(const QRectF &, const QVector2D &, bool)> hitTestHandle;
    std::function<Qt::CursorShape(int)> cursorForHandle;
    std::function<bool(const DrawingPrimitive *)> supportsRotation;
    std::function<float()> zoomLevel;
    std::function<void(const QRectF &, SelectionManager::SelectionOperation)>
        selectInRect;
    std::function<void(const QPolygonF &, SelectionManager::SelectionOperation)>
        selectInLasso;

    // Resize session
    bool *isResizing = nullptr;
    DrawingPrimitive **resizingObject = nullptr;
    int *resizeHandleIndex = nullptr;
    QRectF *resizeOrigBounds = nullptr;
    std::vector<QVector2D> *resizeOrigControlPoints = nullptr;
    QJsonObject *resizeOrigState = nullptr;

    // Rotate session
    bool *isRotating = nullptr;
    DrawingPrimitive **rotatingObject = nullptr;
    QPointF *rotationPivot = nullptr;
    QJsonObject *rotateOrigState = nullptr;
    float *rotationStartAngle = nullptr;
    float *initialRotation = nullptr;

    // Marquee / lasso
    QVector2D *drawStartPos = nullptr;
    QPolygonF *lassoPoints = nullptr;
    SelectionManager::SelectionOperation *selectionOperation = nullptr;
    std::function<bool()> isSelecting;
    std::function<void(bool)> setIsSelecting;
    std::function<QRectF()> selectionRect;
    std::function<void(const QRectF &)> setSelectionRect;
    std::function<int()> selectionMode; // 0 rect, 1 lasso
};

namespace SelectionGestureController {

void onResizeDrag(SelectionGestureHost &host, const QVector2D &worldPos,
                  Qt::KeyboardModifiers mods);
void onRotateDrag(SelectionGestureHost &host, const QVector2D &worldPos,
                  Qt::KeyboardModifiers mods);
void onResizeFinish(SelectionGestureHost &host);
void onRotateFinish(SelectionGestureHost &host);

void onMarqueeUpdate(SelectionGestureHost &host, const QVector2D &worldPos);
void onMarqueeFinish(SelectionGestureHost &host);

/** Select-tool idle hover: handle / move / arrow cursor. */
void onSelectHover(SelectionGestureHost &host, const QVector2D &worldPos);

} // namespace SelectionGestureController
