#pragma once

#include <QVector2D>
#include <vector>

/**
 * Endpoint magnetic snap (connection) state + pure nearest-point search.
 * Extracted from DrawingCanvas (refactor B3).
 */
class MagneticSnap {
public:
    static constexpr float DEFAULT_TOLERANCE = 10.0f;

    MagneticSnap() = default;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setTolerance(float tolerance) { m_tolerance = tolerance; }
    float tolerance() const { return m_tolerance; }

    /**
     * If enabled, snap `pos` to the nearest candidate within tolerance.
     * Otherwise returns `pos` unchanged.
     */
    QVector2D snap(const QVector2D &pos,
                   const std::vector<QVector2D> &candidates) const;

    /** Pure: nearest candidate within `tolerance`, else `pos`. */
    static QVector2D nearestWithin(const QVector2D &pos, float tolerance,
                                   const std::vector<QVector2D> &candidates);

private:
    bool m_enabled = false;
    float m_tolerance = DEFAULT_TOLERANCE;
};
