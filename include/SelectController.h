#pragma once

#include "SelectionManager.h"

#include <QJsonObject>
#include <QMouseEvent>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <vector>

class DrawingPrimitive;
class ImagePrimitive;
class LinePrimitive;

/**
 * Select-tool press interaction extracted from DrawingCanvas (C2).
 * Resize/rotate/move session flags stay on the canvas via SelectHost pointers.
 */
struct SelectHost {
    std::function<QVector2D(const QPoint &)> screenToWorld;
    std::function<std::vector<DrawingPrimitive *>()> selectedObjects;
    std::function<void(const std::function<void(DrawingPrimitive *)> &)>
        forEachVisiblePrimitiveTopFirst;
    std::function<QVector2D(const DrawingPrimitive *, const QVector2D &)>
        toObjectLocal;
    std::function<int(const QRectF &, const QVector2D &, bool canRotate)>
        hitTestSelectionHandle;
    std::function<Qt::CursorShape(int handleIndex)> cursorForSelectionHandle;
    std::function<bool(const DrawingPrimitive *)> supportsRotationHandle;
    std::function<float(const DrawingPrimitive *)> objectRotationDegrees;
    std::function<int(const QVector2D &, float)> findControlPointAt;
    std::function<void(DrawingPrimitive *, int)> startControlPointEdit;
    std::function<void()> clearSelection;
    std::function<void(DrawingPrimitive *)> addToSelection;
    std::function<void(DrawingPrimitive *)> removeFromSelection;
    std::function<void(DrawingPrimitive *)> selectGroupMembers;
    std::function<void()> emitSelectionChanged;
    std::function<void(const QString &)> setSmartHint;
    std::function<void()> requestUpdate;
    std::function<void(Qt::CursorShape)> setCursor;
    std::function<void(ImagePrimitive *, int cpIndex)> deleteMaskControlPoint;
    std::function<void(ImagePrimitive *, int segmentIndex, const QPointF &)>
        insertMaskControlPoint;
    std::function<void(const QPoint &globalPos, ImagePrimitive *, int cpIndex,
                       int segmentIndex)>
        showImageEditContextMenu;
    std::function<bool()> beginCopyAlongConnectedLineMove;
    std::function<bool(Qt::KeyboardModifiers)> isCopyAlongModifier;
    std::function<QVector2D(const LinePrimitive *)> resolveLineMoveConstraint;
    std::function<float()> zoomLevel;
    std::function<bool()> forceAdditive;
    std::function<bool()> forceSubtractive;
    std::function<int()> selectionMode; // 0=rect, 1=lasso

    // Session (owned by canvas)
    bool *isMoving = nullptr;
    bool *isCopyAlongMove = nullptr;
    QVector2D *moveStartPos = nullptr;
    QVector2D *totalMoveOffset = nullptr;
    bool *isResizingObject = nullptr;
    DrawingPrimitive **resizingObject = nullptr;
    int *resizeHandleIndex = nullptr;
    QRectF *resizeOrigBounds = nullptr;
    std::vector<QVector2D> *resizeOrigControlPoints = nullptr;
    QJsonObject *resizeOrigState = nullptr;
    bool *isRotatingObject = nullptr;
    DrawingPrimitive **rotatingObject = nullptr;
    QPointF *rotationPivot = nullptr;
    QJsonObject *rotateOrigState = nullptr;
    float *rotationStartAngle = nullptr;
    float *initialRotation = nullptr;
    DrawingPrimitive **editingPrimitive = nullptr;
    QVector2D *drawStartPos = nullptr;
    QPolygonF *lassoPoints = nullptr;
    SelectionManager::SelectionOperation *selectionOperation = nullptr;
    bool *isSelecting = nullptr; // via SelectionManager setIsSelecting
    std::function<void(bool)> setIsSelecting;
    std::function<void(const QRectF &)> setSelectionRect;
};

namespace SelectController {

constexpr int kHandleRotate = 8;

void onPress(SelectHost &host, QMouseEvent *event);

} // namespace SelectController
