#include "tools/BrushTool.h"

#include "BrushStrokeOps.h"
#include "DrawingPrimitive.h"

#include <QDebug>

namespace {
void finishStroke(ToolHost &host, bool isBlur)
{
    if (host.stopAirbrushTimer) {
        host.stopAirbrushTimer();
    }

    if (host.brushStroke && host.commitPrimitive) {
        static const std::vector<float> kEmpty;
        const auto &scale =
            host.brushParticleScale ? *host.brushParticleScale : kEmpty;
        const auto &alpha =
            host.brushParticleAlpha ? *host.brushParticleAlpha : kEmpty;
        if (auto stroke = BrushStrokeOps::create(
                *host.brushStroke, scale, alpha, host.brushSize,
                host.brushHardness, host.defaultColor, isBlur,
                /*useParticles=*/!isBlur)) {
            host.commitPrimitive(std::move(stroke));
        }
    }

    BrushStrokeOps::clearBuffers(host.brushStroke, host.brushParticleScale,
                                 host.brushParticleAlpha);
    if (host.clearAirbrushDrips) {
        host.clearAirbrushDrips();
    }
    if (host.isBrushing) {
        *host.isBrushing = false;
    }
    if (host.isBlurring) {
        *host.isBlurring = false;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
} // namespace

void BrushTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.isBrushing || !host.brushStroke || !host.screenToWorld) {
        return;
    }

    qDebug() << "BrushTool::onPress";
    *host.isBrushing = true;
    if (host.isBlurring) {
        *host.isBlurring = false;
    }
    host.brushStroke->clear();
    if (host.brushParticleScale) {
        host.brushParticleScale->clear();
    }
    if (host.brushParticleAlpha) {
        host.brushParticleAlpha->clear();
    }
    if (host.clearAirbrushDrips) {
        host.clearAirbrushDrips();
    }
    const QVector2D pos = host.screenToWorld(event->pos());
    if (host.airbrushPos) {
        *host.airbrushPos = pos;
    }
    if (host.startAirbrushTimer) {
        host.startAirbrushTimer();
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void BrushTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isBrushing || !*host.isBrushing || !host.screenToWorld) {
        return;
    }
    const QVector2D pos = host.screenToWorld(event->pos());
    if (host.airbrushPos) {
        *host.airbrushPos = pos;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void BrushTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.isBrushing || !*host.isBrushing) {
        return;
    }
    finishStroke(host, /*isBlur=*/false);
}
