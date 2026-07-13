#include "tools/PolygonTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kCloseTol = 20.0f;
const QColor kPreviewStroke(0, 255, 255);

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

bool isTrivialBounds(const std::vector<QVector2D> &pts)
{
    if (pts.size() < 3) {
        return true;
    }
    float minX = pts[0].x(), maxX = pts[0].x();
    float minY = pts[0].y(), maxY = pts[0].y();
    for (const auto &p : pts) {
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }
    return (maxX - minX) < 2.0f || (maxY - minY) < 2.0f;
}

void applyFill(PolygonPrimitive *poly, const ToolHost &host)
{
    if (!poly) {
        return;
    }
    poly->setFilled(host.defaultFillEnabled);
    if (host.defaultFillEnabled) {
        poly->setFillColor(host.defaultFillColor);
    } else {
        poly->clearFillColor();
    }
}

void discard(ToolHost &host)
{
    if (host.currentPrimitive) {
        host.currentPrimitive->reset();
    }
    if (host.isDrawing) {
        *host.isDrawing = false;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void commitPolygon(ToolHost &host, PolygonPrimitive *poly, bool closed)
{
    if (!poly || !host.currentPrimitive) {
        return;
    }
    const auto pts = poly->getControlPoints();
    if (isTrivialBounds(pts)) {
        qDebug() << "PolygonTool: discard trivial polygon";
        discard(host);
        return;
    }
    if (closed) {
        poly->setClosed(true);
    }
    poly->setColor(host.defaultColor);
    poly->setSelected(true);
    applyFill(poly, host);
    if (host.commitPrimitive) {
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

void PolygonTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.currentPrimitive) {
        return;
    }

    if (event->button() == Qt::RightButton && *host.isDrawing) {
        auto *poly =
            dynamic_cast<PolygonPrimitive *>(host.currentPrimitive->get());
        if (!poly) {
            discard(host);
            return;
        }
        const auto pts = poly->getControlPoints();
        qDebug() << "PolygonTool: finalize with" << pts.size() << "points";
        if (pts.size() < 3) {
            discard(host);
            return;
        }
        commitPolygon(host, poly, /*closed=*/false);
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
        auto poly = std::make_unique<PolygonPrimitive>();
        poly->addPoint(pos);
        poly->setColor(kPreviewStroke);
        poly->setLineStyle(host.defaultLineStyle);
        poly->setLineWidth(host.defaultLineWidth);
        *host.currentPrimitive = std::move(poly);
        *host.isDrawing = true;
        if (host.drawStart) {
            *host.drawStart = pos;
        }
        qDebug() << "PolygonTool: start at" << pos;
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    auto *poly =
        dynamic_cast<PolygonPrimitive *>(host.currentPrimitive->get());
    if (!poly) {
        return;
    }

    const auto &points = poly->getControlPoints();
    if (points.size() >= 3) {
        const float dist = (pos - points[0]).length();
        if (dist <= kCloseTol) {
            qDebug() << "PolygonTool: close near start";
            commitPolygon(host, poly, /*closed=*/true);
            return;
        }
    }

    qDebug() << "PolygonTool: add point" << pos;
    poly->addPoint(pos);
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void PolygonTool::onMove(ToolHost &host, QMouseEvent *event)
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

void PolygonTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Path tools finalize on press (close) or right-click — not on release.
}
