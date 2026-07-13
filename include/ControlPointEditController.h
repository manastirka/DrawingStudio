#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QUuid>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <memory>
#include <vector>

class DrawingPrimitive;
class LinePrimitive;

/**
 * Control-point edit session (C4): start / drag / finish with undo commands.
 */
struct ControlPointEditHost {
    std::function<QVector2D(const QVector2D &)> snapToGrid;
    std::function<DrawingPrimitive *(const QUuid &)> findPrimitiveById;
    std::function<QVector2D(const LinePrimitive *)> resolveLineMoveConstraint;
    std::function<QVector2D(const QVector2D &, const QVector2D &,
                            const QVector2D &)>
        projectOntoSegment;
    std::function<void()> requestUpdate;

    // Undo emitters
    std::function<void(DrawingPrimitive *, int index, const QPointF &from,
                       const QPointF &to)>
        emitMaskCpCommand;
    std::function<void(DrawingPrimitive *, int index, const QVector2D &from,
                       const QVector2D &to)>
        emitCpCommand;
    std::function<void(DrawingPrimitive *, const QJsonObject &oldState,
                       const QJsonObject &newState, const QString &label)>
        emitTransformCommand;

    // Session
    bool *isEditing = nullptr;
    int *selectedIndex = nullptr;
    DrawingPrimitive **editingPrimitive = nullptr;
    QVector2D *originalPosition = nullptr;
    QJsonObject *origState = nullptr;
};

namespace ControlPointEditController {

void start(ControlPointEditHost &host, DrawingPrimitive *primitive,
           int controlPointIndex);
void update(ControlPointEditHost &host, const QVector2D &newPos,
            Qt::KeyboardModifiers mods);
void finish(ControlPointEditHost &host);

} // namespace ControlPointEditController
