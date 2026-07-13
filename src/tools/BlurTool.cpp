#include "tools/BlurTool.h"

#include "BrushStrokeOps.h"
#include "DrawingPrimitive.h"

#include <QDebug>

namespace {
void finishStroke(ToolHost &host)
{
    if (host.stopAirbrushTimer) {
        host.stopAirbrushTimer();
    }

    if (host.brushStroke && host.commitPrimitive) {
        static const std::vector<float> kEmpty;
        if (auto stroke = BrushStrokeOps::create(
                *host.brushStroke, kEmpty, kEmpty, host.brushSize,
                host.brushHardness, host.defaultColor, /*isBlur=*/true,
                /*useParticles=*/false)) {
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

void BlurTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.isBlurring || !host.brushStroke || !host.screenToWorld) {
        return;
    }

    qDebug() << "BlurTool::onPress";
    *host.isBlurring = true;
    if (host.isBrushing) {
        *host.isBrushing = false;
    }
    host.brushStroke->clear();
    if (host.brushParticleScale) {
        host.brushParticleScale->clear();
    }
    if (host.brushParticleAlpha) {
        host.brushParticleAlpha->clear();
    }
    host.brushStroke->push_back(host.screenToWorld(event->pos()));
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void BlurTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isBlurring || !*host.isBlurring || !host.brushStroke ||
        !host.screenToWorld) {
        return;
    }
    host.brushStroke->push_back(host.screenToWorld(event->pos()));
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void BlurTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.isBlurring || !*host.isBlurring) {
        return;
    }
    finishStroke(host);
}
