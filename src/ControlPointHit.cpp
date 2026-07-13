#include "ControlPointHit.h"

namespace ControlPointHit {

int firstWithin(const std::vector<QVector2D> &points, const QVector2D &pos,
                float tolerance)
{
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        if ((points[static_cast<size_t>(i)] - pos).length() <= tolerance) {
            return i;
        }
    }
    return -1;
}

int closestWithin(const std::vector<QVector2D> &points, const QVector2D &pos,
                  float maxDistance)
{
    int closestIndex = -1;
    float minDistance = maxDistance;
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        const float distance =
            (points[static_cast<size_t>(i)] - pos).length();
        if (distance < minDistance) {
            minDistance = distance;
            closestIndex = i;
        }
    }
    return closestIndex;
}

} // namespace ControlPointHit
