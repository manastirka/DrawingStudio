#include "CanvasZoom.h"

#include <algorithm>
#include <cmath>

namespace CanvasZoom {

float resolveScrollDeltaY(const QPoint &pixelDelta, const QPoint &angleDelta)
{
    // Trackpad: pixel delta is more precise
    if (!pixelDelta.isNull() && pixelDelta.y() != 0) {
        return static_cast<float>(pixelDelta.y());
    }
    // Mouse wheel: angleDelta is typically multiples of 120; Qt docs use /8
    const QPoint numDegrees = angleDelta / 8;
    if (!numDegrees.isNull() && numDegrees.y() != 0) {
        return static_cast<float>(numDegrees.y());
    }
    // Very small movements: raw angle delta scaled
    if (!angleDelta.isNull() && angleDelta.y() != 0) {
        return static_cast<float>(angleDelta.y()) / 120.0f * 15.0f;
    }
    return 0.0f;
}

bool applyZoomToCursor(float &zoomLevel, QVector2D &viewCenter, float zoomFactor,
                       float deltaY, const QVector2D &mouseOffsetFromCenter,
                       float minZoom, float maxZoom)
{
    if (std::abs(deltaY) < 0.1f || zoomFactor <= 1.0f) {
        return false;
    }

    const float oldZoom = zoomLevel;
    if (deltaY > 0) {
        zoomLevel = std::min(maxZoom, zoomLevel * zoomFactor);
    } else {
        zoomLevel = std::max(minZoom, zoomLevel / zoomFactor);
    }

    if (std::abs(zoomLevel - oldZoom) < 1e-9f) {
        return false;
    }

    // Keep the world point under the cursor fixed when zoom changes
    const QVector2D worldDelta =
        mouseOffsetFromCenter * (1.0f / oldZoom - 1.0f / zoomLevel);
    viewCenter += worldDelta;
    return true;
}

bool zoomByFactor(float &zoomLevel, float factor, float minZoom, float maxZoom)
{
    if (factor <= 0.0f) {
        return false;
    }
    const float oldZoom = zoomLevel;
    zoomLevel = std::clamp(zoomLevel * factor, minZoom, maxZoom);
    return std::abs(zoomLevel - oldZoom) > 1e-9f;
}

void resetView(float &zoomLevel, QVector2D &viewCenter)
{
    zoomLevel = 1.0f;
    viewCenter = QVector2D(0.0f, 0.0f);
}

float clampSensitivity(float sensitivity)
{
    return std::clamp(sensitivity, kMinSensitivity, kMaxSensitivity);
}

} // namespace CanvasZoom
