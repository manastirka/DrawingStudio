#include "tools/EraserTool.h"

#include <QDebug>
#include <algorithm>

void EraserTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.screenToWorld) {
        return;
    }

    const QVector2D pos = host.screenToWorld(event->pos());
    const float eraserRadius = std::max(2.0f, host.eraserSize * 0.5f);
    qDebug() << "EraserTool: erase at" << pos << "r=" << eraserRadius;

    const QPoint tip = event->globalPosition().toPoint();
    const auto showEmpty = [&]() {
        if (host.showTooltip) {
            host.showTooltip(
                QStringLiteral(
                    "Nothing to erase here. Increase eraser size or move "
                    "over a shape."),
                tip);
        }
    };

    const bool useLayers =
        host.hasLayerManager && host.hasLayerManager() &&
        host.collectHitPrimitives && host.deletePrimitives;

    if (useLayers) {
        const auto hits = host.collectHitPrimitives(pos, eraserRadius);
        if (!hits.empty()) {
            host.deletePrimitives(hits);
        } else {
            showEmpty();
        }
    } else if (host.eraseLegacyDirect) {
        if (!host.eraseLegacyDirect(pos, eraserRadius)) {
            showEmpty();
        }
    } else {
        showEmpty();
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void EraserTool::onMove(ToolHost & /*host*/, QMouseEvent * /*event*/) {}

void EraserTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/) {}
