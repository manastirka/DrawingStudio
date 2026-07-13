#include "tools/RectangleTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <cmath>

namespace {
// Live preview stroke while dragging (matches legacy handleRectangleTool)
const QColor kPreviewStroke(0, 255, 255);

void applyStrokeAndFill(DrawingPrimitive *prim, const ToolHost &host, bool preview)
{
    if (!prim) {
        return;
    }
    prim->setColor(preview ? kPreviewStroke : host.defaultColor);
    prim->setLineStyle(host.defaultLineStyle);
    prim->setLineWidth(host.defaultLineWidth);
}

bool updateSmartPos(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.drawCurrent) {
        return false;
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
    if (host.lastSmartHint && host.setSmartHint &&
        smartHint != *host.lastSmartHint) {
        *host.lastSmartHint = smartHint;
        host.setSmartHint(smartHint);
    }
    return true;
}

void finishSession(ToolHost &host, bool shouldAdd)
{
    if (shouldAdd && host.commitPrimitive && host.currentPrimitive &&
        *host.currentPrimitive) {
        (*host.currentPrimitive)->setColor(host.defaultColor);
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
    } else if (host.currentPrimitive) {
        host.currentPrimitive->reset();
    }

    if (host.isDrawing) {
        *host.isDrawing = false;
    }

    if (host.lastSmartHint && !host.lastSmartHint->isEmpty() &&
        host.setSmartHint) {
        host.lastSmartHint->clear();
        host.setSmartHint(QString());
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
} // namespace

void RectangleTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }
    if (*host.isDrawing || event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "RectangleTool::onPress";

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawStart =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;
    *host.drawCurrent = *host.drawStart;

    auto rect =
        std::make_unique<RectanglePrimitive>(*host.drawStart, *host.drawCurrent);
    applyStrokeAndFill(rect.get(), host, /*preview=*/true);
    rect->setFilled(host.defaultFillEnabled);
    if (host.defaultFillEnabled) {
        rect->setFillColor(host.defaultFillColor);
    }
    *host.currentPrimitive = std::move(rect);
    *host.isDrawing = true;

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void RectangleTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }
    if (!updateSmartPos(host, event)) {
        return;
    }

    if (auto *rect =
            dynamic_cast<RectanglePrimitive *>(host.currentPrimitive->get())) {
        rect->setBottomRight(*host.drawCurrent);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void RectangleTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    bool shouldAdd = false;
    if (auto *r =
            dynamic_cast<RectanglePrimitive *>(host.currentPrimitive->get())) {
        const auto cps = r->getControlPoints();
        if (cps.size() >= 3) {
            const float w = std::abs(cps[1].x() - cps[0].x());
            const float h = std::abs(cps[2].y() - cps[0].y());
            shouldAdd = (w >= 2.0f && h >= 2.0f);
        }
    }

    finishSession(host, shouldAdd);
}
