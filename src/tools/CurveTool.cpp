#include "tools/CurveTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>

namespace {
constexpr float kCloseTol = 20.0f;

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

void finishOpen(ToolHost &host)
{
    if (host.currentPrimitive && *host.currentPrimitive &&
        host.commitPrimitive) {
        (*host.currentPrimitive)->setColor(host.defaultColor);
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
    }
    if (host.isDrawing) {
        *host.isDrawing = false;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
} // namespace

void CurveTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.currentPrimitive) {
        return;
    }

    if (event->button() == Qt::RightButton && *host.isDrawing) {
        finishOpen(host);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QVector2D pos = snappedPos(host, event->pos());
    if (host.drawCurrent) {
        *host.drawCurrent = pos;
    }

    if (!*host.isDrawing) {
        auto curve = std::make_unique<CurvePrimitive>();
        curve->addControlPoint(pos);
        curve->setColor(host.defaultColor);
        curve->setLineStyle(host.defaultLineStyle);
        curve->setLineWidth(host.defaultLineWidth);
        *host.currentPrimitive = std::move(curve);
        *host.isDrawing = true;
        if (host.drawStart) {
            *host.drawStart = pos;
        }
        qDebug() << "CurveTool: start at" << pos;
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    auto *curve =
        dynamic_cast<CurvePrimitive *>(host.currentPrimitive->get());
    if (!curve) {
        return;
    }

    const auto &cps = curve->controlPoints();
    if (cps.size() >= 3) {
        const float dist = (pos - cps[0]).length();
        if (dist <= kCloseTol) {
            qDebug() << "CurveTool: close near start";
            curve->setClosed(true);
            finishOpen(host);
            return;
        }
    }

    qDebug() << "CurveTool: add point" << pos;
    curve->addControlPoint(pos);
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void CurveTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.drawCurrent) {
        return;
    }
    QVector2D world =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawCurrent =
        host.snapToGrid ? host.snapToGrid(world) : world;
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void CurveTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Path tools finalize on press (close) or right-click — not on release.
}
