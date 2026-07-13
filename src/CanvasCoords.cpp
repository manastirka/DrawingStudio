#include "CanvasCoords.h"

#include <QPainter>
#include <cmath>

namespace CanvasCoords {

QVector2D screenToWorld(const QPoint &screenPos, int widgetWidth,
                        int widgetHeight, float zoomLevel,
                        const QVector2D &viewCenter, bool pixelSnap)
{
    const float z = (zoomLevel > 1e-9f) ? zoomLevel : 1.0f;
    float x = (screenPos.x() - widgetWidth / 2.0f) / z + viewCenter.x();
    float y = (widgetHeight / 2.0f - screenPos.y()) / z + viewCenter.y();
    if (pixelSnap) {
        x = std::round(x);
        y = std::round(y);
    }
    return QVector2D(x, y);
}

QPoint worldToScreen(const QVector2D &worldPos, int widgetWidth,
                     int widgetHeight, float zoomLevel,
                     const QVector2D &viewCenter)
{
    const float x =
        (worldPos.x() - viewCenter.x()) * zoomLevel + widgetWidth / 2.0f;
    const float y =
        widgetHeight / 2.0f - (worldPos.y() - viewCenter.y()) * zoomLevel;
    return QPoint(static_cast<int>(x), static_cast<int>(y));
}

QVector2D mouseOffsetFromCenter(const QPoint &screenPos, int widgetWidth,
                                int widgetHeight)
{
    return QVector2D(screenPos.x() - widgetWidth / 2.0f,
                     widgetHeight / 2.0f - screenPos.y());
}

void applyWorldTransform(QPainter &painter, int widgetWidth, int widgetHeight,
                         float zoomLevel, const QVector2D &viewCenter)
{
    painter.translate(widgetWidth / 2.0, widgetHeight / 2.0);
    painter.scale(static_cast<double>(zoomLevel),
                  -static_cast<double>(zoomLevel));
    painter.translate(-viewCenter.x(), -viewCenter.y());
}

} // namespace CanvasCoords
