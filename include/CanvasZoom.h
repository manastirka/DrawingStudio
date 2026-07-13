#pragma once

#include <QPoint>
#include <QVector2D>

/**
 * Pure zoom-to-cursor math (refactor E5).
 * No widget state — DrawingCanvas applies results to m_zoomLevel / m_viewCenter.
 */
namespace CanvasZoom {

/** Default clamp range used by the canvas wheel zoom. */
inline constexpr float kMinZoom = 0.05f;
inline constexpr float kMaxZoom = 20.0f;

/**
 * Resolve vertical scroll from Qt wheel deltas.
 * Prefers pixelDelta (trackpad), then angleDelta/8 (mouse), then raw angle/120.
 * @return 0 when there is no meaningful scroll
 */
float resolveScrollDeltaY(const QPoint &pixelDelta, const QPoint &angleDelta);

/**
 * Zoom in/out by @p zoomFactor based on @p deltaY sign, keeping the world
 * point under the cursor stable.
 *
 * @param zoomLevel              in/out current zoom
 * @param viewCenter             in/out world-space view center
 * @param zoomFactor             multiplicative step (typically m_zoomSensitivity)
 * @param deltaY                 positive = zoom in, negative = zoom out
 * @param mouseOffsetFromCenter  screen offset from widget center, y-up
 *                               (x - w/2, h/2 - y) — matches DrawingCanvas coords
 * @param minZoom / maxZoom      clamps
 * @return true if zoomLevel changed
 */
bool applyZoomToCursor(float &zoomLevel, QVector2D &viewCenter, float zoomFactor,
                       float deltaY, const QVector2D &mouseOffsetFromCenter,
                       float minZoom = kMinZoom, float maxZoom = kMaxZoom);

/** Multiplicative zoom (toolbar + / −). @p factor > 1 zooms in. */
bool zoomByFactor(float &zoomLevel, float factor, float minZoom = kMinZoom,
                  float maxZoom = kMaxZoom);

/** Reset zoom to 1 and view center to origin (Fit / Actual Size). */
void resetView(float &zoomLevel, QVector2D &viewCenter);

/** Default factor for zoomIn / zoomOut buttons. */
inline constexpr float kButtonZoomFactor = 1.2f;

/** Clamp wheel/trackpad sensitivity (DrawingCanvas m_zoomSensitivity). */
inline constexpr float kMinSensitivity = 1.01f;
inline constexpr float kMaxSensitivity = 3.0f;

/** Clamp @p sensitivity into [kMinSensitivity, kMaxSensitivity]. */
float clampSensitivity(float sensitivity);

} // namespace CanvasZoom
