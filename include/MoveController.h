#pragma once

#include <QCursor>
#include <QMouseEvent>
#include <QString>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <vector>

class DrawingPrimitive;
class LinePrimitive;

/**
 * Move interaction (press → drag → finish) extracted from DrawingCanvas (C1).
 * Session state stays on the canvas; host supplies callbacks + pointers.
 */
struct MoveHost {
    std::function<QVector2D(const QPoint &)> screenToWorld;
    std::function<std::vector<DrawingPrimitive *>()> selectedObjects;
    std::function<DrawingPrimitive *(const QVector2D &pos)> findPrimitiveAt;
    /// Hit tolerance already scaled for zoom (e.g. 8/zoom).
    std::function<int(const QVector2D &pos, float tolerance)> findControlPointAt;
    std::function<void(DrawingPrimitive *, int)> startControlPointEdit;
    std::function<void()> clearSelection;
    std::function<void(DrawingPrimitive *)> addToSelection;
    std::function<void()> emitSelectionChanged;
    std::function<void(const QString &)> setSmartHint;
    std::function<void()> requestUpdate;
    std::function<void(Qt::CursorShape)> setCursor;
    std::function<bool()> isMoveToolActive;
    std::function<QVector2D(const QVector2D &delta, Qt::KeyboardModifiers)>
        constrainDelta;
    std::function<void(const QVector2D &)> updateAlignmentGuides;
    std::function<void(const std::vector<DrawingPrimitive *> &,
                       const QVector2D &offset)>
        emitMoveCommand;
    std::function<void()> deleteSelectedWithCommand;
    std::function<float()> controlPointTolerance; // world units
    std::function<float()> objectHitTolerance;

    // Session (owned by canvas)
    bool *isMoving = nullptr;
    bool *isCopyAlongMove = nullptr;
    QVector2D *moveStartPos = nullptr;
    QVector2D *totalMoveOffset = nullptr;
    DrawingPrimitive **editingPrimitive = nullptr;
};

namespace MoveController {

void onPress(MoveHost &host, QMouseEvent *event);
void onDrag(MoveHost &host, const QVector2D &worldPos);
void onFinish(MoveHost &host);

} // namespace MoveController
