#include "MagneticSnap.h"

QVector2D MagneticSnap::nearestWithin(const QVector2D &pos, float tolerance,
                                      const std::vector<QVector2D> &candidates)
{
    QVector2D nearest = pos;
    float minDistance = tolerance;

    for (const QVector2D &candidate : candidates) {
        const float d = (candidate - pos).length();
        if (d < minDistance) {
            nearest = candidate;
            minDistance = d;
        }
    }
    return nearest;
}

QVector2D MagneticSnap::snap(const QVector2D &pos,
                             const std::vector<QVector2D> &candidates) const
{
    if (!m_enabled) {
        return pos;
    }
    return nearestWithin(pos, m_tolerance, candidates);
}
