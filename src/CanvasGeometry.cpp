#include "CanvasGeometry.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace CanvasGeometry {

QVector2D projectPointOntoLineSegment(const QVector2D &point, const QVector2D &a,
                                      const QVector2D &b)
{
    QVector2D ab = b - a;
    const float len2 = ab.lengthSquared();
    if (len2 < 1e-8f)
        return a;
    float t = QVector2D::dotProduct(point - a, ab) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    return a + ab * t;
}

bool computeCircleThroughPoints(const QVector2D &p1, const QVector2D &p2,
                                const QVector2D &p3, QVector2D &centerOut,
                                float &radiusOut)
{
    // Perpendicular bisectors intersection
    float a = p2.x() - p1.x();
    float b = p2.y() - p1.y();
    float c = p3.x() - p1.x();
    float d = p3.y() - p1.y();
    float e = a * (p1.x() + p2.x()) + b * (p1.y() + p2.y());
    float f = c * (p1.x() + p3.x()) + d * (p1.y() + p3.y());
    float g = 2.0f * (a * (p3.y() - p2.y()) - b * (p3.x() - p2.x()));
    if (std::abs(g) < 1e-5f) {
        return false; // collinear or too close
    }
    float cx = (d * e - b * f) / g;
    float cy = (a * f - c * e) / g;
    centerOut = QVector2D(cx, cy);
    radiusOut = (centerOut - p1).length();
    return true;
}

bool pointInPolygon(const QVector2D &point,
                    const std::vector<QVector2D> &polygon)
{
    if (polygon.size() < 3) {
        return false;
    }

    int crossings = 0;
    const int n = static_cast<int>(polygon.size());

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;

        QVector2D p1 = polygon[static_cast<size_t>(i)];
        QVector2D p2 = polygon[static_cast<size_t>(j)];

        if (((p1.y() <= point.y()) && (point.y() < p2.y())) ||
            ((p2.y() <= point.y()) && (point.y() < p1.y()))) {
            float xIntersect =
                p1.x() +
                (point.y() - p1.y()) * (p2.x() - p1.x()) / (p2.y() - p1.y());

            if (point.x() < xIntersect) {
                crossings++;
            }
        }
    }

    return (crossings % 2) == 1;
}

QVector2D snapAngleLineEndpoint(const QVector2D &origin,
                                const QVector2D &rawEnd,
                                const QVector2D &baselineStart,
                                const QVector2D &baselineEnd,
                                Qt::KeyboardModifiers mods)
{
    QVector2D delta = rawEnd - origin;
    const float len = delta.length();
    if (len < 0.001f)
        return origin;

    // Alt: free angle (no snap to baseline)
    if (mods.testFlag(Qt::AltModifier))
        return rawEnd;

    QVector2D base = baselineEnd - baselineStart;
    if (base.length() < 0.001f)
        return rawEnd;

    const float baseAng = std::atan2(base.y(), base.x());
    const float ang = std::atan2(delta.y(), delta.x());
    float rel = ang - baseAng;
    while (rel > static_cast<float>(M_PI))
        rel -= 2.0f * static_cast<float>(M_PI);
    while (rel < -static_cast<float>(M_PI))
        rel += 2.0f * static_cast<float>(M_PI);

    // Default 15° steps relative to baseline; Shift = finer 5°
    const float stepDeg = mods.testFlag(Qt::ShiftModifier) ? 5.0f : 15.0f;
    const float step = stepDeg * static_cast<float>(M_PI) / 180.0f;
    const float snappedRel = std::round(rel / step) * step;
    const float finalAng = baseAng + snappedRel;

    return origin + QVector2D(std::cos(finalAng), std::sin(finalAng)) * len;
}

} // namespace CanvasGeometry
