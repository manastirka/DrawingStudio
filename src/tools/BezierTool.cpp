#include "tools/BezierTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <cmath>

namespace {
const QColor kPreviewStroke(100, 255, 100); // green during creation

QVector2D snappedPos(const ToolHost &host, const QPoint &screen)
{
    QVector2D world =
        host.screenToWorld ? host.screenToWorld(screen) : QVector2D();
    if (host.snapToGrid) {
        world = host.snapToGrid(world);
    }
    if (host.snapToEndpoint) {
        world = host.snapToEndpoint(world);
    }
    return world;
}

int closestControlPoint(const BezierCurvePrimitive *bezier, const QVector2D &pos,
                        float tolerance = 15.0f)
{
    if (!bezier) {
        return -1;
    }
    const auto &points = bezier->controlPoints();
    int closest = -1;
    float minDist = tolerance;
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        const float d = (points[static_cast<size_t>(i)] - pos).length();
        if (d < minDist) {
            minDist = d;
            closest = i;
        }
    }
    return closest;
}

void setHint(ToolHost &host, const QString &hint)
{
    if (host.lastSmartHint) {
        *host.lastSmartHint = hint;
    }
    if (host.setSmartHint) {
        host.setSmartHint(hint);
    }
}
} // namespace

void BezierTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.bezierCreationStage || !host.drawStart ||
        !host.drawCurrent || !host.currentPrimitive) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (*host.isDrawing) {
            host.currentPrimitive->reset();
            *host.isDrawing = false;
            *host.bezierCreationStage = 0;
            setHint(host, QString());
            if (host.requestUpdate) {
                host.requestUpdate();
            }
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QVector2D pos = snappedPos(host, event->pos());
    const bool isShift = event->modifiers() & Qt::ShiftModifier;
    const int stage = *host.bezierCreationStage;

    if (stage == 0) {
        QVector2D startPoint = pos;
        // Continue chain if click is near previous end and canvas has geometry
        const bool canChain =
            (*host.drawStart - pos).length() < 10.0f &&
            (!host.hasPrimitives || host.hasPrimitives());
        if (canChain) {
            startPoint = *host.drawStart;
            qDebug() << "BezierTool: chain from" << startPoint << "to" << pos;
        } else {
            qDebug() << "BezierTool: start at" << pos;
        }

        auto bezier = std::make_unique<BezierCurvePrimitive>();
        const std::vector<QVector2D> points = {startPoint, startPoint, pos,
                                               pos};
        bezier->setControlPoints(points);
        bezier->setColor(kPreviewStroke);
        bezier->setLineStyle(host.defaultLineStyle);
        bezier->setLineWidth(host.defaultLineWidth);
        bezier->setSelected(true); // show handles during creation

        *host.currentPrimitive = std::move(bezier);
        *host.isDrawing = true;
        *host.bezierCreationStage = 1;
        *host.drawStart = startPoint;
        *host.drawCurrent = pos;
        setHint(host, QStringLiteral(
                          "Bezier: drag end point · Shift+click handle · "
                          "release to place · Right-click cancels"));
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    if (stage == 1 && isShift && *host.currentPrimitive) {
        auto *bezier =
            dynamic_cast<BezierCurvePrimitive *>(host.currentPrimitive->get());
        const int cp = closestControlPoint(bezier, pos);
        if (cp >= 0 && host.beginControlPointEdit) {
            qDebug() << "BezierTool: Shift+click control point" << cp;
            host.beginControlPointEdit(host.currentPrimitive->get(), cp);
        }
    }
}

void BezierTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.bezierCreationStage ||
        *host.bezierCreationStage != 1 || !host.drawCurrent ||
        !host.drawStart || !host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawCurrent =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;

    QString smartHint;
    if (host.smartConstraints) {
        *host.drawCurrent = host.smartConstraints(
            *host.drawCurrent, event->modifiers(), &smartHint);
    }

    if (auto *bezier =
            dynamic_cast<BezierCurvePrimitive *>(host.currentPrimitive->get())) {
        QVector2D snappedEnd = *host.drawCurrent;
        if (host.snapToEndpoint) {
            snappedEnd = host.snapToEndpoint(snappedEnd);
        }
        bezier->setEndPoint(snappedEnd);
        const QVector2D dir = snappedEnd - *host.drawStart;
        bezier->setControlPoint1(*host.drawStart + dir * 0.33f);
        bezier->setControlPoint2(*host.drawStart + dir * 0.67f);
        smartHint = QStringLiteral("Bezier length %1 · release to place")
                        .arg(dir.length(), 0, 'f', 1);
    }

    if (!smartHint.isEmpty() && host.lastSmartHint && host.setSmartHint &&
        smartHint != *host.lastSmartHint) {
        *host.lastSmartHint = smartHint;
        host.setSmartHint(smartHint);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void BezierTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.bezierCreationStage ||
        !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (*host.bezierCreationStage != 1 || !*host.currentPrimitive) {
        return;
    }

    auto *b =
        dynamic_cast<BezierCurvePrimitive *>(host.currentPrimitive->get());
    if (!b) {
        return;
    }

    const auto &cps = b->controlPoints();
    const float endDist =
        (cps.size() >= 4) ? (cps.front() - cps.back()).length() : 0.0f;
    constexpr float kMinLen = 2.0f;

    if (endDist >= kMinLen && host.commitPrimitive) {
        *host.bezierCreationStage = 0;
        if (host.drawStart) {
            *host.drawStart = cps.back(); // chain next segment from end
        }
        (*host.currentPrimitive)->setColor(host.defaultColor);
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
        *host.isDrawing = false;
        setHint(host, QStringLiteral(
                          "Bezier placed — click near end to chain, or "
                          "elsewhere for a new curve"));
    } else {
        host.currentPrimitive->reset();
        *host.isDrawing = false;
        *host.bezierCreationStage = 0;
        setHint(host, QString());
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
