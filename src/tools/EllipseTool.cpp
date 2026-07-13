#include "tools/EllipseTool.h"

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

void EllipseTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }
    if (*host.isDrawing || event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "EllipseTool::onPress";

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawStart =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;
    *host.drawCurrent = *host.drawStart;

    auto ellipse =
        std::make_unique<EllipsePrimitive>(*host.drawStart, 10.0f, 10.0f);
    ellipse->setColor(kPreviewStroke);
    ellipse->setLineStyle(host.defaultLineStyle);
    ellipse->setLineWidth(host.defaultLineWidth);
    ellipse->setFilled(host.defaultFillEnabled);
    if (host.defaultFillEnabled) {
        ellipse->setFillColor(host.defaultFillColor);
    }
    *host.currentPrimitive = std::move(ellipse);
    *host.isDrawing = true;

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void EllipseTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }
    if (!updateSmartPos(host, event)) {
        return;
    }

    if (auto *ellipse =
            dynamic_cast<EllipsePrimitive *>(host.currentPrimitive->get())) {
        const QVector2D center =
            (*host.drawStart + *host.drawCurrent) * 0.5f;
        const float rx =
            std::abs(host.drawCurrent->x() - host.drawStart->x()) * 0.5f;
        const float ry =
            std::abs(host.drawCurrent->y() - host.drawStart->y()) * 0.5f;
        ellipse->setCenter(center);
        ellipse->setRadiusX(rx);
        ellipse->setRadiusY(ry);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void EllipseTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    bool shouldAdd = false;
    if (auto *e =
            dynamic_cast<EllipsePrimitive *>(host.currentPrimitive->get())) {
        const auto cps = e->getControlPoints();
        if (cps.size() >= 5) {
            const float rx = std::abs(cps[2].x() - cps[1].x()) * 0.5f;
            const float ry = std::abs(cps[4].y() - cps[3].y()) * 0.5f;
            shouldAdd = (rx >= 1.0f && ry >= 1.0f);
        }
    }

    finishSession(host, shouldAdd);
}
