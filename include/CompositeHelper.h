#pragma once

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

/**
 * Local pre-composite: paint ARGB subject cutout(s) onto a background plate.
 * Keeps subject pixels intact (optional subtle lighting tint only).
 * Subjects must have a real alpha matte — transparent outside the silhouette.
 */
class CompositeHelper
{
public:
    struct Placement {
        float heightFraction; // subject height vs plate height
        float centerX;        // 0..1 horizontal center
        float bottomY;        // 0..1 vertical: subject bottom relative to plate

        static Placement defaults()
        {
            return Placement{0.55f, 0.5f, 0.92f};
        }
    };

    /**
     * One cutout for the plate. When sourceNormRect is valid (w,h > 0),
     * the subject is placed using that rect in the source photo (0..1),
     * so multiple subjects keep their original relative layout without
     * being glued into one rectangular crop of the original.
     */
    struct SubjectSpec {
        QImage image;
        QRectF sourceNormRect; // empty / null → auto-pack
    };

    /** Map provider imageSize + aspect to a reasonable pixel size. */
    static QSize platePixelSize(const QString &imageSize,
                                const QString &aspectRatio);

    /**
     * Build an opaque RGB plate: background scaled to cover, then subject
     * cutouts drawn with alpha (no opaque bounding rectangle).
     * Layout-aware when SubjectSpec::sourceNormRect is set.
     */
    static QImage buildPlate(const QImage &background,
                             const QVector<SubjectSpec> &subjects,
                             const QSize &plateSize,
                             bool matchLighting);

    /** Legacy: images only → auto side-by-side / centered packing. */
    static QImage buildPlate(const QImage &background,
                             const QVector<QImage> &subjectsArgb,
                             const QSize &plateSize,
                             bool matchLighting);

    /** Convenience for a single subject with default placement. */
    static QImage buildPlate(const QImage &background,
                             const QImage &subjectArgb,
                             const QSize &plateSize,
                             const Placement &placement,
                             bool matchLighting);
};
