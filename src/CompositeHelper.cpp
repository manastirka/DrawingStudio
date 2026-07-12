#include "CompositeHelper.h"

#include <QPainter>
#include <QColor>
#include <QtMath>
#include <algorithm>

QSize CompositeHelper::platePixelSize(const QString &imageSize,
                                      const QString &aspectRatio)
{
    int longSide = 1024;
    const QString sz = imageSize.trimmed().toUpper();
    if (sz == QLatin1String("512"))
        longSide = 512;
    else if (sz == QLatin1String("2K"))
        longSide = 2048;
    else if (sz == QLatin1String("4K"))
        longSide = 4096;
    else
        longSide = 1024;

    double aw = 1.0;
    double ah = 1.0;
    const QStringList parts = aspectRatio.split(QLatin1Char(':'));
    if (parts.size() == 2) {
        aw = std::max(0.01, parts[0].toDouble());
        ah = std::max(0.01, parts[1].toDouble());
    }

    int w = longSide;
    int h = longSide;
    if (aw >= ah) {
        w = longSide;
        h = qMax(1, static_cast<int>(qRound(longSide * (ah / aw))));
    } else {
        h = longSide;
        w = qMax(1, static_cast<int>(qRound(longSide * (aw / ah))));
    }
    w -= w % 2;
    h -= h % 2;
    return QSize(qMax(2, w), qMax(2, h));
}

static QColor sampleAmbient(const QImage &plate, int cx, int bottom, int pad)
{
    qint64 rSum = 0, gSum = 0, bSum = 0, n = 0;
    const int sampleY = qBound(0, bottom - 2, plate.height() - 1);
    const int half = qMax(8, pad);
    for (int dx = -half; dx <= half; dx += 4) {
        const int x = qBound(0, cx + dx, plate.width() - 1);
        const QRgb px = plate.pixel(x, sampleY);
        rSum += qRed(px);
        gSum += qGreen(px);
        bSum += qBlue(px);
        ++n;
    }
    if (n == 0)
        return QColor(128, 128, 128);
    return QColor(int(rSum / n), int(gSum / n), int(bSum / n));
}

/** Force fully transparent pixels to zero RGB so no opaque rectangle leaks. */
static QImage ensureTrueCutout(const QImage &subjectArgb)
{
    QImage out = subjectArgb.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            if (qAlpha(line[x]) < 8)
                line[x] = qRgba(0, 0, 0, 0);
        }
    }
    return out;
}

static QImage tintSubjectForLighting(const QImage &subjectArgb,
                                     const QColor &ambient)
{
    QImage out = ensureTrueCutout(subjectArgb);
    const float strength = 0.18f;
    const float ar = ambient.redF();
    const float ag = ambient.greenF();
    const float ab = ambient.blueF();

    for (int y = 0; y < out.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const int a = qAlpha(line[x]);
            if (a < 8)
                continue;
            const float r = qRed(line[x]) / 255.0f;
            const float g = qGreen(line[x]) / 255.0f;
            const float b = qBlue(line[x]) / 255.0f;
            const float nr = r * (1.0f - strength) + r * ar * strength;
            const float ng = g * (1.0f - strength) + g * ag * strength;
            const float nb = b * (1.0f - strength) + b * ab * strength;
            line[x] = qRgba(qBound(0, int(nr * 255.0f), 255),
                            qBound(0, int(ng * 255.0f), 255),
                            qBound(0, int(nb * 255.0f), 255), a);
        }
    }
    return out;
}

static void drawSubjectAtRect(QImage &plate, const QImage &subjectArgb,
                              const QRect &dest, bool matchLighting)
{
    if (subjectArgb.isNull() || plate.isNull() || !dest.isValid())
        return;

    QImage sub = ensureTrueCutout(subjectArgb);
    if (matchLighting) {
        const QColor ambient =
            sampleAmbient(plate, dest.center().x(), dest.bottom(),
                          dest.width() / 2 + 16);
        sub = tintSubjectForLighting(sub, ambient);
    }

    QPainter painter(&plate);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(dest, sub);
}

static void drawSubjectCutout(QImage &plate, const QImage &subjectArgb,
                              const CompositeHelper::Placement &placement,
                              bool matchLighting)
{
    if (subjectArgb.isNull() || plate.isNull())
        return;

    const QSize plateSize = plate.size();
    const int targetH = qMax(
        8, static_cast<int>(qRound(plateSize.height() *
                                   qBound(0.12, placement.heightFraction, 0.9))));
    QImage sub = ensureTrueCutout(subjectArgb);
    const float aspect =
        sub.height() > 0
            ? static_cast<float>(sub.width()) / static_cast<float>(sub.height())
            : 1.0f;
    const int targetW = qMax(8, static_cast<int>(qRound(targetH * aspect)));

    const int cx = static_cast<int>(
        qRound(plateSize.width() * qBound(0.0, placement.centerX, 1.0)));
    const int bottom = static_cast<int>(
        qRound(plateSize.height() * qBound(0.1, placement.bottomY, 1.0)));
    const int left = cx - targetW / 2;
    const int top = bottom - targetH;

    drawSubjectAtRect(plate, sub, QRect(left, top, targetW, targetH),
                      matchLighting);
}

static bool hasLayout(const CompositeHelper::SubjectSpec &s)
{
    return s.sourceNormRect.isValid() && s.sourceNormRect.width() > 1e-6 &&
           s.sourceNormRect.height() > 1e-6;
}

QImage CompositeHelper::buildPlate(const QImage &background,
                                   const QVector<SubjectSpec> &subjects,
                                   const QSize &plateSize,
                                   bool matchLighting)
{
    if (background.isNull() || subjects.isEmpty() || !plateSize.isValid())
        return {};

    QImage plate(plateSize, QImage::Format_ARGB32_Premultiplied);
    plate.fill(Qt::white);

    {
        QPainter painter(&plate);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QImage bg = background.convertToFormat(QImage::Format_ARGB32);
        const QSize bgScaled =
            bg.size().scaled(plateSize, Qt::KeepAspectRatioByExpanding);
        const int bgX = (plateSize.width() - bgScaled.width()) / 2;
        const int bgY = (plateSize.height() - bgScaled.height()) / 2;
        painter.drawImage(QRect(bgX, bgY, bgScaled.width(), bgScaled.height()), bg);
    }

    // Prefer layout-preserving placement when any subject has source coords
    int layoutCount = 0;
    for (const auto &s : subjects) {
        if (!s.image.isNull() && hasLayout(s))
            ++layoutCount;
    }

    if (layoutCount >= 1) {
        QRectF unionNorm;
        bool first = true;
        for (const auto &s : subjects) {
            if (s.image.isNull() || !hasLayout(s))
                continue;
            if (first) {
                unionNorm = s.sourceNormRect;
                first = false;
            } else {
                unionNorm = unionNorm.united(s.sourceNormRect);
            }
        }
        if (unionNorm.width() < 1e-6 || unionNorm.height() < 1e-6)
            return plate.convertToFormat(QImage::Format_RGB888);

        // Fit the union into the plate with margins (keep aspect of the group)
        const double margin = 0.08;
        const QRectF fitArea(margin, margin, 1.0 - 2.0 * margin,
                             1.0 - 2.0 * margin);
        const double scale = std::min(fitArea.width() / unionNorm.width(),
                                      fitArea.height() / unionNorm.height());
        const double usedW = unionNorm.width() * scale;
        const double usedH = unionNorm.height() * scale;
        // Bias group slightly toward the bottom of the plate
        const double originX =
            fitArea.x() + (fitArea.width() - usedW) * 0.5;
        const double originY =
            fitArea.y() + (fitArea.height() - usedH) * 0.72;

        for (const auto &s : subjects) {
            if (s.image.isNull() || !hasLayout(s))
                continue;
            const double nx =
                (s.sourceNormRect.x() - unionNorm.x()) / unionNorm.width();
            const double ny =
                (s.sourceNormRect.y() - unionNorm.y()) / unionNorm.height();
            const double nw = s.sourceNormRect.width() / unionNorm.width();
            const double nh = s.sourceNormRect.height() / unionNorm.height();

            const int left =
                static_cast<int>(qRound((originX + nx * usedW) * plateSize.width()));
            const int top =
                static_cast<int>(qRound((originY + ny * usedH) * plateSize.height()));
            const int w = qMax(
                8, static_cast<int>(qRound(nw * usedW * plateSize.width())));
            const int h = qMax(
                8, static_cast<int>(qRound(nh * usedH * plateSize.height())));
            drawSubjectAtRect(plate, s.image, QRect(left, top, w, h),
                              matchLighting);
        }
        return plate.convertToFormat(QImage::Format_RGB888);
    }

    // No source layout → pack side-by-side / centered
    const int n = subjects.size();
    const float heightFrac =
        n <= 1 ? 0.55f
               : qBound(0.28f, 0.72f / static_cast<float>(n), 0.50f);

    for (int i = 0; i < n; ++i) {
        if (subjects[i].image.isNull())
            continue;
        Placement p;
        p.heightFraction = heightFrac;
        p.centerX = (i + 1.0f) / (n + 1.0f);
        p.bottomY = 0.92f;
        drawSubjectCutout(plate, subjects[i].image, p, matchLighting);
    }

    return plate.convertToFormat(QImage::Format_RGB888);
}

QImage CompositeHelper::buildPlate(const QImage &background,
                                   const QVector<QImage> &subjectsArgb,
                                   const QSize &plateSize,
                                   bool matchLighting)
{
    QVector<SubjectSpec> specs;
    specs.reserve(subjectsArgb.size());
    for (const QImage &im : subjectsArgb) {
        SubjectSpec s;
        s.image = im;
        specs.append(s);
    }
    return buildPlate(background, specs, plateSize, matchLighting);
}

QImage CompositeHelper::buildPlate(const QImage &background,
                                   const QImage &subjectArgb,
                                   const QSize &plateSize,
                                   const Placement &placement,
                                   bool matchLighting)
{
    Q_UNUSED(placement);
    QVector<SubjectSpec> subjects;
    if (!subjectArgb.isNull()) {
        SubjectSpec s;
        s.image = subjectArgb;
        subjects.append(s);
    }
    return buildPlate(background, subjects, plateSize, matchLighting);
}
