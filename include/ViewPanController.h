#pragma once

#include <QPoint>
#include <QVector2D>
#include <functional>

/**
 * Middle-button / space-style view panning (C5).
 */
struct ViewPanHost {
    float *zoomLevel = nullptr;
    QVector2D *viewCenter = nullptr;
    QPoint *lastMousePos = nullptr;
    bool *isPanning = nullptr;
    std::function<void()> requestUpdate;
    std::function<void()> setPanCursor;
    std::function<void()> setArrowCursor;
};

namespace ViewPanController {

void begin(ViewPanHost &host, const QPoint &screenPos);
void update(ViewPanHost &host, const QPoint &screenPos);
void end(ViewPanHost &host);

} // namespace ViewPanController
