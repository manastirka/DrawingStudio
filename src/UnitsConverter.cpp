#include "UnitsConverter.h"

QString UnitsConverter::unitsString() const
{
    switch (m_units) {
    case Units::Millimeters:
        return QStringLiteral("mm");
    case Units::Centimeters:
        return QStringLiteral("cm");
    case Units::Inches:
        return QStringLiteral("in");
    }
    return QStringLiteral("mm");
}

float UnitsConverter::worldToUnits(float worldDistance) const
{
    // World coordinates are in pixels → convert via mm
    const float mm = worldDistance / m_pixelsPerMM;
    switch (m_units) {
    case Units::Millimeters:
        return mm;
    case Units::Centimeters:
        return mm / 10.0f;
    case Units::Inches:
        return mm / 25.4f;
    }
    return mm;
}

float UnitsConverter::unitsToWorld(float unitDistance) const
{
    float mm = unitDistance;
    switch (m_units) {
    case Units::Millimeters:
        mm = unitDistance;
        break;
    case Units::Centimeters:
        mm = unitDistance * 10.0f;
        break;
    case Units::Inches:
        mm = unitDistance * 25.4f;
        break;
    }
    return mm * m_pixelsPerMM;
}

double UnitsConverter::pixelsPerUnit() const
{
    switch (m_units) {
    case Units::Millimeters:
        return m_pixelsPerMM;
    case Units::Centimeters:
        return m_pixelsPerMM * 10.0;
    case Units::Inches:
        return m_pixelsPerMM * 25.4;
    }
    return m_pixelsPerMM;
}
