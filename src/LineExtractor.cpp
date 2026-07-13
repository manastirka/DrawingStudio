#include "LineExtractor.h"

#include <QDebug>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LineExtractor core pipeline + hough (refactor E25).

// filterStairTreads lives in LineExtractorGeometry.cpp
void filterStairTreads(std::vector<LineExtractor::ExtractedLine> &lines);

// --- LineExtractor ---
LineExtractor::LineExtractor(QObject *parent)
    : QObject(parent)
{
}


// --- makeLine ---
LineExtractor::ExtractedLine LineExtractor::makeLine(QPointF a, QPointF b, int votes)
{
    ExtractedLine line;
    line.start = a;
    line.end = b;
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    line.length = static_cast<float>(std::hypot(dx, dy));
    line.angle = static_cast<float>(std::atan2(dy, dx) * 180.0 / M_PI);
    line.votes = votes;
    return line;
}


// --- suggestScaleDefaults ---
void LineExtractor::suggestScaleDefaults(int width, int height,
                                         double *minLineLengthOut,
                                         double *maxLineGapOut)
{
    const double side = static_cast<double>(std::min(width, height));
    if (minLineLengthOut)
        *minLineLengthOut = std::max(20.0, 0.015 * side);
    if (maxLineGapOut)
        *maxLineGapOut = std::max(8.0, 0.01 * side);
}


// --- suggestInkThreshold ---
int LineExtractor::suggestInkThreshold(const QImage &image, bool invert)
{
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (invert) {
        for (int y = 0; y < gray.height(); ++y) {
            uchar *row = gray.scanLine(y);
            for (int x = 0; x < gray.width(); ++x)
                row[x] = static_cast<uchar>(255 - row[x]);
        }
    }
    int hist[256] = {};
    qint64 total = 0;
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *row = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            ++hist[row[x]];
            ++total;
        }
    }
    if (total <= 0)
        return 128;
    double sum = 0;
    for (int i = 0; i < 256; ++i)
        sum += i * hist[i];
    double sumB = 0;
    qint64 wB = 0;
    double maxVar = -1;
    int best = 128;
    for (int t = 1; t < 255; ++t) {
        wB += hist[t];
        if (wB == 0)
            continue;
        const qint64 wF = total - wB;
        if (wF == 0)
            break;
        sumB += t * hist[t];
        const double mB = sumB / wB;
        const double mF = (sum - sumB) / wF;
        const double var = static_cast<double>(wB) * wF * (mB - mF) * (mB - mF);
        if (var > maxVar) {
            maxVar = var;
            best = t;
        }
    }
    return std::clamp(best, 40, 220);
}


// --- suggestEdgeThreshold ---
int LineExtractor::suggestEdgeThreshold(const QImage &image, bool invert)
{
    // For UI: in floor-plan mode this maps to ink threshold
    return suggestInkThreshold(image, invert);
}


// --- getEdgePreview ---
QImage LineExtractor::getEdgePreview(const QImage &image, int threshold, bool invert,
                                     bool morphCleanup, bool floorPlanMode)
{
    QImage gray = toGrayscale(image);
    if (invert)
        gray = invertGray(gray);
    if (!floorPlanMode)
        gray = gaussianBlur(gray);

    if (floorPlanMode) {
        QImage ink;
        if (threshold <= 0)
            ink = otsuBinarizeInk(gray);
        else
            ink = thresholdImage(gray, threshold, true);
        // Preview wall mask from raw ink (same as extract path)
        const QImage inkRaw = ink;
        if (morphCleanup)
            ink = morphologicalOpen(ink);
        Q_UNUSED(ink);
        const int side = std::min(image.width(), image.height());
        const int kern = std::clamp(static_cast<int>(side * 0.018), 9, 51) | 1;
        return extractWallMask(inkRaw, kern, 1);
    }

    QImage edges = sobelEdgeDetection(gray);
    QImage binary = thresholdImage(edges, threshold <= 0 ? 50 : threshold);
    if (morphCleanup)
        binary = morphologicalOpen(binary);
    return binary;
}


// --- houghLinesP ---
std::vector<LineExtractor::ExtractedLine>
LineExtractor::houghLinesP(const QImage &binary, int houghThreshold,
                           double minLineLength, double maxLineGap)
{
    std::vector<ExtractedLine> result;
    const int w = binary.width();
    const int h = binary.height();

    std::vector<std::pair<int, int>> edgePixels;
    edgePixels.reserve(w * h / 8);
    for (int y = 0; y < h; ++y) {
        const uchar *row = binary.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x] > 0)
                edgePixels.emplace_back(x, y);
        }
    }
    if (edgePixels.empty())
        return result;

    constexpr int numAngles = 180;
    std::vector<double> cosTable(numAngles), sinTable(numAngles);
    for (int t = 0; t < numAngles; ++t) {
        const double theta = t * M_PI / 180.0;
        cosTable[t] = std::cos(theta);
        sinTable[t] = std::sin(theta);
    }
    const double maxDist = std::hypot(w, h);
    const int rhoMax = static_cast<int>(std::ceil(maxDist)) + 1;
    const int rhoSize = 2 * rhoMax + 1;
    std::vector<int> accumulator(rhoSize * numAngles, 0);
    std::vector<std::vector<bool>> used(h, std::vector<bool>(w, false));

    std::mt19937 rng(42);
    std::shuffle(edgePixels.begin(), edgePixels.end(), rng);

    for (size_t i = 0; i < edgePixels.size(); ++i) {
        const int x = edgePixels[i].first;
        const int y = edgePixels[i].second;
        if (used[y][x])
            continue;
        for (int t = 0; t < numAngles; ++t) {
            const int rhoIdx =
                static_cast<int>(std::round(x * cosTable[t] + y * sinTable[t])) +
                rhoMax;
            if (rhoIdx >= 0 && rhoIdx < rhoSize)
                ++accumulator[rhoIdx * numAngles + t];
        }
    }

    struct Peak {
        int rhoIdx, thetaIdx, votes;
    };
    std::vector<Peak> peaks;
    for (int r = 0; r < rhoSize; ++r) {
        for (int t = 0; t < numAngles; ++t) {
            const int votes = accumulator[r * numAngles + t];
            if (votes >= houghThreshold)
                peaks.push_back({r, t, votes});
        }
    }
    std::sort(peaks.begin(), peaks.end(),
              [](const Peak &a, const Peak &b) { return a.votes > b.votes; });

    for (size_t pi = 0; pi < peaks.size() && pi < 1500; ++pi) {
        const double rho = peaks[pi].rhoIdx - rhoMax;
        const int thetaIdx = peaks[pi].thetaIdx;
        const double cosT = cosTable[thetaIdx];
        const double sinT = sinTable[thetaIdx];
        const double dx = -sinT;
        const double dy = cosT;
        const double x0 = rho * cosT;
        const double y0 = rho * sinT;
        const double maxT = maxDist;

        struct PixelOnLine {
            int x, y;
            double t;
        };
        std::vector<PixelOnLine> linePixels;
        for (double t = -maxT; t <= maxT; t += 1.0) {
            const int px = static_cast<int>(std::round(x0 + t * dx));
            const int py = static_cast<int>(std::round(y0 + t * dy));
            if (px < 0 || px >= w || py < 0 || py >= h)
                continue;
            if (binary.constScanLine(py)[px] > 0 && !used[py][px])
                linePixels.push_back({px, py, t});
        }
        if (linePixels.empty())
            continue;
        std::sort(linePixels.begin(), linePixels.end(),
                  [](const PixelOnLine &a, const PixelOnLine &b) { return a.t < b.t; });

        size_t segStart = 0;
        for (size_t j = 1; j <= linePixels.size(); ++j) {
            const bool isGap =
                (j == linePixels.size()) ||
                (linePixels[j].t - linePixels[j - 1].t > maxLineGap);
            if (!isGap)
                continue;
            const double segLen = linePixels[j - 1].t - linePixels[segStart].t;
            if (segLen >= minLineLength) {
                auto line =
                    makeLine(QPointF(linePixels[segStart].x, linePixels[segStart].y),
                             QPointF(linePixels[j - 1].x, linePixels[j - 1].y),
                             peaks[pi].votes);
                if (line.length >= minLineLength) {
                    result.push_back(line);
                    for (size_t k = segStart; k < j; ++k)
                        used[linePixels[k].y][linePixels[k].x] = true;
                }
            }
            segStart = j;
        }
    }
    return result;
}


// --- extractLines ---
std::vector<LineExtractor::ExtractedLine>
LineExtractor::extractLines(const QImage &image, const Parameters &params)
{
    emit progressChanged(0);
    Parameters p = params;

    double sugMin = p.minLineLength;
    double sugGap = p.maxLineGap;
    if (p.scaleAwareDefaults) {
        suggestScaleDefaults(image.width(), image.height(), &sugMin, &sugGap);
        p.minLineLength = std::max(p.minLineLength, sugMin);
        p.maxLineGap = std::max(p.maxLineGap, sugGap);
    }

    QImage gray = toGrayscale(image);
    if (p.invert)
        gray = invertGray(gray);
    emit progressChanged(5);

    if (p.deskew) {
        double skew = 0;
        gray = deskewImage(gray, &skew);
        if (std::abs(skew) >= 1.5)
            qDebug() << "LineExtractor: deskewed by" << skew << "deg";
        else if (std::abs(skew) >= 0.4)
            qDebug() << "LineExtractor: skew" << skew << "deg ignored (below 1.5)";
    }
    // Blur destroys 1px CAD walls — only use it for generic Sobel/Hough
    if (!p.floorPlanMode)
        gray = gaussianBlur(gray);
    emit progressChanged(15);

    std::vector<ExtractedLine> lines;
    QImage inkRaw; // floor-plan ink mask for validation

    if (p.floorPlanMode) {
        // --- Architectural pipeline ---
        QImage ink;
        if (p.inkThreshold < 0 && p.edgeThreshold > 0 && p.edgeThreshold != 50) {
            ink = thresholdImage(gray, p.edgeThreshold, true);
        } else if (p.inkThreshold >= 0) {
            ink = thresholdImage(gray, p.inkThreshold, true);
        } else {
            ink = otsuBinarizeInk(gray);
        }
        inkRaw = ink;
        if (p.morphCleanup)
            ink = morphologicalOpen(ink);
        emit progressChanged(30);

        const int side = std::min(image.width(), image.height());
        int kern = p.wallKernelLength;
        if (kern <= 0)
            kern = std::clamp(static_cast<int>(side * 0.018), 9, 51);
        kern |= 1;
        const int thick = std::max(1, (p.wallKernelThickness <= 0 ? 1 : p.wallKernelThickness));
        const int thickOdd = (thick % 2 == 0) ? thick + 1 : thick;

        QImage walls = extractWallMask(inkRaw, kern, thickOdd);
        emit progressChanged(45);

        lines = extractAxisAlignedRuns(walls, p.minLineLength, p.maxLineGap);
        filterByInkSupport(lines, inkRaw, 0.68);

        collapseParallelWalls(lines, std::max(2.0, static_cast<double>(thickOdd)));
        filterStairTreads(lines);
        recoverPartitionWalls(inkRaw, lines, p.minLineLength);
        filterStairTreads(lines);
        filterByInkSupport(lines, inkRaw, 0.68);
        removeParallelOffsetDuplicates(lines, 4.0);
        emit progressChanged(80);
    } else {
        QImage edges = sobelEdgeDetection(gray);
        QImage binary = thresholdImage(edges, p.edgeThreshold);
        if (p.morphCleanup)
            binary = morphologicalOpen(binary);
        emit progressChanged(40);
        lines = houghLinesP(binary, p.houghThreshold, p.minLineLength, p.maxLineGap);
        emit progressChanged(70);
    }

    if (p.mergeCollinear) {
        // Bridge small door-gap breaks, but don't weld distant rooms
        mergeCollinearSegments(lines, std::max(p.maxLineGap, 6.0), 5.0);
        mergeCollinearSegments(lines, std::max(p.maxLineGap * 1.5, 10.0), 6.0);
    }
    snapToAxisAligned(lines, p.angleSnapDegrees);
    quantizeAxes(lines, 2.0);
    const double extendBudget = std::max(p.junctionSnap > 0 ? p.junctionSnap : 6.0, 6.0);
    extendToMeetJunctions(lines, extendBudget);
    if (p.junctionSnap > 0)
        snapJunctions(lines, p.junctionSnap);
    if (p.mergeCollinear)
        mergeCollinearSegments(lines, std::max(p.maxLineGap, 6.0), 4.0);

    removeDuplicates(lines, p.duplicateDist);
    removeShortRemnants(lines, p.floorPlanMode ? p.minLineLength * 0.85
                                               : p.minLineLength * 0.55);

    if (p.floorPlanMode && !inkRaw.isNull()) {
        trimAllToInk(lines, inkRaw, 0.82);
        filterByInkSupport(lines, inkRaw, 0.70);
        removeParallelOffsetDuplicates(lines, 4.0);
    }

    snapToAxisAligned(lines, p.angleSnapDegrees);
    quantizeAxes(lines, 2.0);
    collapseParallelWalls(lines, 2.0);
    removeDuplicates(lines, p.duplicateDist);
    roundToPixels(lines);

    if (p.floorPlanMode && !inkRaw.isNull())
        filterByInkSupport(lines, inkRaw, 0.68);

    emit progressChanged(100);
    qDebug() << "LineExtractor: Detected" << lines.size() << "lines"
             << (p.floorPlanMode ? "(floor-plan mode)" : "(generic)");
    return lines;
}

