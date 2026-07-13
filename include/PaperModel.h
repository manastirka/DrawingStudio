#pragma once

#include <QColor>
#include <QRectF>
#include <QSizeF>
#include <QString>

class CanvasRenderer;

/**
 * Paper format + size (mm) and world-space paper rectangle.
 * Extracted from DrawingCanvas (refactor B4).
 */
class PaperModel {
public:
    enum class Format {
        Custom,
        A4,
        A3,
        A2,
        A1,
        A0,
        Letter,
        Legal,
        Tabloid
    };

    PaperModel();

    void setFormat(Format format);
    Format format() const { return m_format; }

    /** Size in millimeters. */
    void setSizeMm(const QSizeF &sizeMm) { m_sizeMm = sizeMm; }
    QSizeF sizeMm() const { return m_sizeMm; }

    void setColor(const QColor &color) { m_color = color; }
    QColor color() const { return m_color; }

    QString formatName() const;

    /** Paper bounds in world pixels, centered at origin. */
    QRectF worldRect(float pixelsPerMM) const;

    /** Draw paper fill, border, and soft shadow via CanvasRenderer batches. */
    void render(CanvasRenderer *renderer, float pixelsPerMM) const;

    static QSizeF sizeForFormat(Format format);

private:
    Format m_format = Format::A4;
    QSizeF m_sizeMm = QSizeF(210.0f, 297.0f); // A4
    QColor m_color = QColor(255, 255, 255);
};
