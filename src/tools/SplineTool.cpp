#include "tools/SplineTool.h"

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

void SplineTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.currentPrimitive) {
        return;
    }

    if (event->button() == Qt::RightButton && *host.isDrawing) {
        if (auto *sp =
                dynamic_cast<SplinePrimitive *>(host.currentPrimitive->get())) {
            qDebug() << "SplineTool: finalize with" << sp->points().size()
                     << "points";
        }
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
        auto spline = std::make_unique<SplinePrimitive>();
        spline->addPoint(pos);
        spline->setColor(host.defaultColor);
        spline->setLineStyle(host.defaultLineStyle);
        spline->setLineWidth(host.defaultLineWidth);
        *host.currentPrimitive = std::move(spline);
        *host.isDrawing = true;
        if (host.drawStart) {
            *host.drawStart = pos;
        }
        qDebug() << "SplineTool: start at" << pos;
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    auto *spline =
        dynamic_cast<SplinePrimitive *>(host.currentPrimitive->get());
    if (!spline) {
        return;
    }

    const auto &points = spline->points();
    if (points.size() >= 3) {
        const float dist = (pos - points[0]).length();
        if (dist <= kCloseTol) {
            qDebug() << "SplineTool: close near start";
            spline->setClosed(true);
            finishOpen(host);
            return;
        }
    }

    qDebug() << "SplineTool: add point" << pos;
    spline->addPoint(pos);
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void SplineTool::onMove(ToolHost &host, QMouseEvent *event)
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

void SplineTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Path tools finalize on press (close) or right-click — not on release.
}
