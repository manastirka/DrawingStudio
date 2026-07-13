#include "tools/LineTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>

void LineTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }

    if (*host.isDrawing) {
        return; // drag already in progress
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    qDebug() << "LineTool::onPress";

    QVector2D worldPos = host.screenToWorld
                             ? host.screenToWorld(event->pos())
                             : QVector2D();
    QVector2D snappedPos =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;
    *host.drawStart =
        host.snapToEndpoint ? host.snapToEndpoint(snappedPos) : snappedPos;
    *host.drawCurrent = *host.drawStart;

    auto line = std::make_unique<LinePrimitive>(*host.drawStart, *host.drawCurrent);
    line->setColor(host.defaultColor);
    line->setLineStyle(host.defaultLineStyle);
    line->setLineWidth(host.defaultLineWidth);
    *host.currentPrimitive = std::move(line);
    *host.isDrawing = true;

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void LineTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.drawCurrent ||
        !host.currentPrimitive || !*host.currentPrimitive) {
        return;
    }

    QVector2D worldPos = host.screenToWorld
                             ? host.screenToWorld(event->pos())
                             : QVector2D();
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

    if (auto *line = dynamic_cast<LinePrimitive *>(host.currentPrimitive->get())) {
        QVector2D snappedEnd = host.snapToEndpoint
                                   ? host.snapToEndpoint(*host.drawCurrent)
                                   : *host.drawCurrent;
        line->setEndPoint(snappedEnd);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void LineTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.currentPrimitive) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    bool shouldAdd = false;
    if (auto *line = dynamic_cast<LinePrimitive *>(host.currentPrimitive->get())) {
        const float len = (line->endPoint() - line->startPoint()).length();
        shouldAdd = len >= 2.0f;
    }

    if (shouldAdd && host.commitPrimitive) {
        (*host.currentPrimitive)->setColor(host.defaultColor);
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
    } else {
        host.currentPrimitive->reset();
    }

    *host.isDrawing = false;

    if (host.lastSmartHint && !host.lastSmartHint->isEmpty() &&
        host.setSmartHint) {
        host.lastSmartHint->clear();
        host.setSmartHint(QString());
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
