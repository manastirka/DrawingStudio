#include "tools/CircleTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <cmath>

namespace {
const QColor kPreviewStroke(0, 255, 255);

bool updateSmartPos(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.drawCurrent ||
        !host.drawStart) {
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

void CircleTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }
    if (*host.isDrawing || event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "CircleTool::onPress";

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawStart =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;
    *host.drawCurrent = *host.drawStart;

    auto circle = std::make_unique<CirclePrimitive>(*host.drawStart, 1.0f);
    circle->setColor(kPreviewStroke);
    circle->setLineStyle(host.defaultLineStyle);
    circle->setLineWidth(host.defaultLineWidth);
    circle->setFilled(host.defaultFillEnabled);
    if (host.defaultFillEnabled) {
        circle->setFillColor(host.defaultFillColor);
    }
    *host.currentPrimitive = std::move(circle);
    *host.isDrawing = true;

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void CircleTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }
    if (!updateSmartPos(host, event)) {
        return;
    }

    if (auto *circle =
            dynamic_cast<CirclePrimitive *>(host.currentPrimitive->get())) {
        const float radius = (*host.drawCurrent - *host.drawStart).length();
        circle->setRadius(radius);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void CircleTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    bool shouldAdd = false;
    if (auto *c =
            dynamic_cast<CirclePrimitive *>(host.currentPrimitive->get())) {
        const auto cps = c->getControlPoints();
        if (cps.size() >= 2) {
            const float r = (cps[1] - cps[0]).length();
            shouldAdd = r >= 1.0f;
        }
    }

    finishSession(host, shouldAdd);
}
