#include "ViewPanController.h"

namespace ViewPanController {

void begin(ViewPanHost &host, const QPoint &screenPos)
{
    if (!host.isPanning || !host.lastMousePos) {
        return;
    }
    *host.isPanning = true;
    *host.lastMousePos = screenPos;
    if (host.setPanCursor) {
        host.setPanCursor();
    }
}

void update(ViewPanHost &host, const QPoint &screenPos)
{
    if (!host.isPanning || !*host.isPanning || !host.viewCenter ||
        !host.zoomLevel || !host.lastMousePos) {
        return;
    }
    const QPoint delta = screenPos - *host.lastMousePos;
    const float z = *host.zoomLevel;
    const QVector2D worldDelta(-delta.x() / z, delta.y() / z);
    *host.viewCenter += worldDelta;
    *host.lastMousePos = screenPos;
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void end(ViewPanHost &host)
{
    if (!host.isPanning) {
        return;
    }
    *host.isPanning = false;
    if (host.setArrowCursor) {
        host.setArrowCursor();
    }
}

} // namespace ViewPanController
