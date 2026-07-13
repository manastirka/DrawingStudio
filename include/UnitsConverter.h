#pragma once

#include <QString>

/**
 * World (pixel) ↔ real-world units conversion.
 * Extracted from DrawingCanvas (refactor B2).
 *
 * World coordinates are stored in pixels at a fixed pixels-per-mm scale
 * (default ~96 DPI). Display unit (mm/cm/in) is a view of that scale.
 */
class UnitsConverter {
public:
    enum class Units { Millimeters, Centimeters, Inches };

    static constexpr float DEFAULT_PIXELS_PER_MM = 3.77953f; // 96 DPI

    UnitsConverter() = default;

    void setUnits(Units units) { m_units = units; }
    Units units() const { return m_units; }

    void setPixelsPerMM(float pixelsPerMM) { m_pixelsPerMM = pixelsPerMM; }
    float pixelsPerMM() const { return m_pixelsPerMM; }

    QString unitsString() const;
    float worldToUnits(float worldDistance) const;
    float unitsToWorld(float unitDistance) const;
    double pixelsPerUnit() const;

private:
    Units m_units = Units::Millimeters;
    float m_pixelsPerMM = DEFAULT_PIXELS_PER_MM;
};
