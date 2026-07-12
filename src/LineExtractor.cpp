#include "LineExtractor.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LineExtractor::LineExtractor(QObject *parent)
    : QObject(parent)
{
}

QImage LineExtractor::toGrayscale(const QImage &image)
{
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QImage LineExtractor::gaussianBlur(const QImage &gray)
{
    QImage result = gray.copy();
    int w = gray.width();
    int h = gray.height();

    float kernel[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };
    float kernelSum = 16.0f;

    for (int y = 1; y < h - 1; ++y) {
        const uchar *prevRow = gray.constScanLine(y - 1);
        const uchar *currRow = gray.constScanLine(y);
        const uchar *nextRow = gray.constScanLine(y + 1);
        uchar *outRow = result.scanLine(y);

        for (int x = 1; x < w - 1; ++x) {
            float sum = 0;
            sum += prevRow[x - 1] * kernel[0][0];
            sum += prevRow[x]     * kernel[0][1];
            sum += prevRow[x + 1] * kernel[0][2];
            sum += currRow[x - 1] * kernel[1][0];
            sum += currRow[x]     * kernel[1][1];
            sum += currRow[x + 1] * kernel[1][2];
            sum += nextRow[x - 1] * kernel[2][0];
            sum += nextRow[x]     * kernel[2][1];
            sum += nextRow[x + 1] * kernel[2][2];
            outRow[x] = static_cast<uchar>(std::min(255.0f, sum / kernelSum));
        }
    }
    return result;
}

QImage LineExtractor::sobelEdgeDetection(const QImage &gray)
{
    int w = gray.width();
    int h = gray.height();
    QImage result(w, h, QImage::Format_Grayscale8);
    result.fill(0);

    for (int y = 1; y < h - 1; ++y) {
        const uchar *prevRow = gray.constScanLine(y - 1);
        const uchar *currRow = gray.constScanLine(y);
        const uchar *nextRow = gray.constScanLine(y + 1);
        uchar *outRow = result.scanLine(y);

        for (int x = 1; x < w - 1; ++x) {
            int gx = -prevRow[x-1] + prevRow[x+1]
                   - 2*currRow[x-1] + 2*currRow[x+1]
                   - nextRow[x-1] + nextRow[x+1];

            int gy = -prevRow[x-1] - 2*prevRow[x] - prevRow[x+1]
                   + nextRow[x-1] + 2*nextRow[x] + nextRow[x+1];

            int mag = static_cast<int>(std::sqrt(gx * gx + gy * gy));
            outRow[x] = static_cast<uchar>(std::min(255, mag));
        }
    }
    return result;
}

QImage LineExtractor::thresholdImage(const QImage &edges, int threshold)
{
    int w = edges.width();
    int h = edges.height();
    QImage binary(w, h, QImage::Format_Grayscale8);
    binary.fill(0);

    for (int y = 0; y < h; ++y) {
        const uchar *srcRow = edges.constScanLine(y);
        uchar *dstRow = binary.scanLine(y);
        for (int x = 0; x < w; ++x) {
            dstRow[x] = (srcRow[x] >= threshold) ? 255 : 0;
        }
    }
    return binary;
}

QImage LineExtractor::getEdgePreview(const QImage &image, int threshold)
{
    QImage gray = toGrayscale(image);
    QImage blurred = gaussianBlur(gray);
    QImage edges = sobelEdgeDetection(blurred);
    return thresholdImage(edges, threshold);
}

std::vector<LineExtractor::ExtractedLine>
LineExtractor::houghLinesP(const QImage &binary,
                           int houghThreshold,
                           double minLineLength,
                           double maxLineGap)
{
    std::vector<ExtractedLine> result;
    int w = binary.width();
    int h = binary.height();

    // Collect all edge pixels
    std::vector<std::pair<int, int>> edgePixels;
    edgePixels.reserve(w * h / 4);
    for (int y = 0; y < h; ++y) {
        const uchar *row = binary.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x] > 0) {
                edgePixels.emplace_back(x, y);
            }
        }
    }

    if (edgePixels.empty()) return result;

    // Precompute sin/cos tables
    const int numAngles = 180;
    std::vector<double> cosTable(numAngles);
    std::vector<double> sinTable(numAngles);
    for (int t = 0; t < numAngles; ++t) {
        double theta = t * M_PI / 180.0;
        cosTable[t] = std::cos(theta);
        sinTable[t] = std::sin(theta);
    }

    double maxDist = std::sqrt(w * w + h * h);
    int rhoMax = static_cast<int>(std::ceil(maxDist)) + 1;
    int rhoSize = 2 * rhoMax + 1;

    // Accumulator
    std::vector<int> accumulator(rhoSize * numAngles, 0);

    // Track which pixels are used
    std::vector<std::vector<bool>> used(h, std::vector<bool>(w, false));

    // Shuffle edge pixels for probabilistic approach
    std::mt19937 rng(42);
    std::shuffle(edgePixels.begin(), edgePixels.end(), rng);

    emit progressChanged(10);

    // Vote
    for (size_t i = 0; i < edgePixels.size(); ++i) {
        int x = edgePixels[i].first;
        int y = edgePixels[i].second;
        if (used[y][x]) continue;

        for (int t = 0; t < numAngles; ++t) {
            double rho = x * cosTable[t] + y * sinTable[t];
            int rhoIdx = static_cast<int>(std::round(rho)) + rhoMax;
            if (rhoIdx >= 0 && rhoIdx < rhoSize) {
                accumulator[rhoIdx * numAngles + t]++;
            }
        }

        if (i % 5000 == 0) {
            emit progressChanged(10 + static_cast<int>(30.0 * i / edgePixels.size()));
        }
    }

    emit progressChanged(40);

    // Find peaks and extract line segments
    struct Peak {
        int rhoIdx;
        int thetaIdx;
        int votes;
    };
    std::vector<Peak> peaks;

    for (int r = 0; r < rhoSize; ++r) {
        for (int t = 0; t < numAngles; ++t) {
            int votes = accumulator[r * numAngles + t];
            if (votes >= houghThreshold) {
                peaks.push_back({r, t, votes});
            }
        }
    }

    // Sort peaks by votes descending
    std::sort(peaks.begin(), peaks.end(), [](const Peak &a, const Peak &b) {
        return a.votes > b.votes;
    });

    emit progressChanged(50);

    // For each peak, walk the line to find actual segments
    for (size_t pi = 0; pi < peaks.size() && pi < 2000; ++pi) {
        double rho = peaks[pi].rhoIdx - rhoMax;
        int thetaIdx = peaks[pi].thetaIdx;
        double cosT = cosTable[thetaIdx];
        double sinT = sinTable[thetaIdx];
        int votes = peaks[pi].votes;

        // Determine line direction: walk perpendicular to (cosT, sinT)
        // The line equation is: x*cosT + y*sinT = rho
        // Direction along line: (-sinT, cosT)
        double dx = -sinT;
        double dy = cosT;

        // Find all edge pixels on or near this line
        struct PixelOnLine {
            int x, y;
            double t; // parameter along line
        };
        std::vector<PixelOnLine> linePixels;

        // Point on line closest to origin
        double x0 = rho * cosT;
        double y0 = rho * sinT;

        double maxT = std::sqrt(static_cast<double>(w * w + h * h));

        // Walk along line in both directions, collecting nearby edge pixels
        for (double t = -maxT; t <= maxT; t += 1.0) {
            int px = static_cast<int>(std::round(x0 + t * dx));
            int py = static_cast<int>(std::round(y0 + t * dy));
            if (px < 0 || px >= w || py < 0 || py >= h) continue;

            const uchar *row = binary.constScanLine(py);
            if (row[px] > 0 && !used[py][px]) {
                linePixels.push_back({px, py, t});
            }
        }

        if (linePixels.empty()) continue;

        // Sort by parameter along line
        std::sort(linePixels.begin(), linePixels.end(),
                  [](const PixelOnLine &a, const PixelOnLine &b) { return a.t < b.t; });

        // Extract segments: split at gaps > maxLineGap
        size_t segStart = 0;
        for (size_t j = 1; j <= linePixels.size(); ++j) {
            bool isGap = (j == linePixels.size()) ||
                         (linePixels[j].t - linePixels[j-1].t > maxLineGap);
            if (isGap) {
                // Segment from segStart to j-1
                double segLen = linePixels[j-1].t - linePixels[segStart].t;
                if (segLen >= minLineLength) {
                    ExtractedLine line;
                    line.start = QPointF(linePixels[segStart].x, linePixels[segStart].y);
                    line.end = QPointF(linePixels[j-1].x, linePixels[j-1].y);

                    double ddx = line.end.x() - line.start.x();
                    double ddy = line.end.y() - line.start.y();
                    line.length = static_cast<float>(std::sqrt(ddx * ddx + ddy * ddy));
                    line.angle = static_cast<float>(std::atan2(ddy, ddx) * 180.0 / M_PI);
                    line.votes = votes;

                    if (line.length >= minLineLength) {
                        result.push_back(line);

                        // Mark pixels as used
                        for (size_t k = segStart; k < j; ++k) {
                            used[linePixels[k].y][linePixels[k].x] = true;
                        }
                    }
                }
                segStart = j;
            }
        }

        if (pi % 100 == 0) {
            emit progressChanged(50 + static_cast<int>(40.0 * pi / std::min(peaks.size(), size_t(2000))));
        }
    }

    emit progressChanged(90);
    return result;
}

void LineExtractor::mergeCollinearSegments(std::vector<ExtractedLine> &lines, double maxGap)
{
    if (lines.size() < 2) return;

    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i < lines.size() && !merged; ++i) {
            for (size_t j = i + 1; j < lines.size() && !merged; ++j) {
                // Check if lines are roughly collinear
                float angleDiff = std::abs(lines[i].angle - lines[j].angle);
                if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;
                // Allow parallel lines (0 deg) or anti-parallel (180 deg)
                bool collinear = (angleDiff < 5.0f) || (std::abs(angleDiff - 180.0f) < 5.0f);
                if (!collinear) continue;

                // Check if endpoints are close enough
                auto dist = [](QPointF a, QPointF b) {
                    double dx = a.x() - b.x();
                    double dy = a.y() - b.y();
                    return std::sqrt(dx * dx + dy * dy);
                };

                // Check distance from any endpoint of j to the line defined by i
                QPointF d(lines[i].end.x() - lines[i].start.x(),
                          lines[i].end.y() - lines[i].start.y());
                double len = std::sqrt(d.x() * d.x() + d.y() * d.y());
                if (len < 1e-6) continue;

                QPointF n(-d.y() / len, d.x() / len); // normal

                auto distToLine = [&](QPointF p) {
                    double dx = p.x() - lines[i].start.x();
                    double dy = p.y() - lines[i].start.y();
                    return std::abs(dx * n.x() + dy * n.y());
                };

                double d1 = distToLine(lines[j].start);
                double d2 = distToLine(lines[j].end);

                if (d1 > maxGap || d2 > maxGap) continue;

                // Check endpoint gap
                double minEndpointDist = std::min({
                    dist(lines[i].start, lines[j].start),
                    dist(lines[i].start, lines[j].end),
                    dist(lines[i].end, lines[j].start),
                    dist(lines[i].end, lines[j].end)
                });

                if (minEndpointDist > maxGap * 3) continue;

                // Merge: project all 4 points onto the line direction, pick extremes
                QPointF dir(d.x() / len, d.y() / len);
                QPointF origin = lines[i].start;

                auto project = [&](QPointF p) {
                    return (p.x() - origin.x()) * dir.x() + (p.y() - origin.y()) * dir.y();
                };

                double t0 = project(lines[i].start);
                double t1 = project(lines[i].end);
                double t2 = project(lines[j].start);
                double t3 = project(lines[j].end);

                double tMin = std::min({t0, t1, t2, t3});
                double tMax = std::max({t0, t1, t2, t3});

                lines[i].start = QPointF(origin.x() + tMin * dir.x(),
                                         origin.y() + tMin * dir.y());
                lines[i].end = QPointF(origin.x() + tMax * dir.x(),
                                       origin.y() + tMax * dir.y());

                double ddx = lines[i].end.x() - lines[i].start.x();
                double ddy = lines[i].end.y() - lines[i].start.y();
                lines[i].length = static_cast<float>(std::sqrt(ddx * ddx + ddy * ddy));
                lines[i].angle = static_cast<float>(std::atan2(ddy, ddx) * 180.0 / M_PI);
                lines[i].votes = std::max(lines[i].votes, lines[j].votes);

                lines.erase(lines.begin() + static_cast<long>(j));
                merged = true;
            }
        }
    }
}

void LineExtractor::snapToAxisAligned(std::vector<ExtractedLine> &lines, double angleTolerance)
{
    for (auto &line : lines) {
        double angle = std::atan2(line.end.y() - line.start.y(),
                                  line.end.x() - line.start.x()) * 180.0 / M_PI;

        // Snap to horizontal (0 or 180)
        if (std::abs(angle) < angleTolerance || std::abs(angle - 180.0) < angleTolerance ||
            std::abs(angle + 180.0) < angleTolerance) {
            double midY = (line.start.y() + line.end.y()) / 2.0;
            line.start.setY(midY);
            line.end.setY(midY);
            line.angle = (line.end.x() >= line.start.x()) ? 0.0f : 180.0f;
        }
        // Snap to vertical (90 or -90)
        else if (std::abs(angle - 90.0) < angleTolerance || std::abs(angle + 90.0) < angleTolerance) {
            double midX = (line.start.x() + line.end.x()) / 2.0;
            line.start.setX(midX);
            line.end.setX(midX);
            line.angle = (line.end.y() >= line.start.y()) ? 90.0f : -90.0f;
        }
    }
}

void LineExtractor::removeDuplicates(std::vector<ExtractedLine> &lines, double distThreshold)
{
    auto dist = [](QPointF a, QPointF b) {
        double dx = a.x() - b.x();
        double dy = a.y() - b.y();
        return std::sqrt(dx * dx + dy * dy);
    };

    std::vector<bool> toRemove(lines.size(), false);

    for (size_t i = 0; i < lines.size(); ++i) {
        if (toRemove[i]) continue;
        for (size_t j = i + 1; j < lines.size(); ++j) {
            if (toRemove[j]) continue;

            // Check if lines are essentially the same
            bool sameForward = (dist(lines[i].start, lines[j].start) < distThreshold &&
                                dist(lines[i].end, lines[j].end) < distThreshold);
            bool sameReversed = (dist(lines[i].start, lines[j].end) < distThreshold &&
                                 dist(lines[i].end, lines[j].start) < distThreshold);

            if (sameForward || sameReversed) {
                // Keep the one with more votes
                if (lines[j].votes > lines[i].votes) {
                    toRemove[i] = true;
                    break;
                } else {
                    toRemove[j] = true;
                }
            }
        }
    }

    std::vector<ExtractedLine> filtered;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!toRemove[i]) {
            filtered.push_back(lines[i]);
        }
    }
    lines = std::move(filtered);
}

std::vector<LineExtractor::ExtractedLine>
LineExtractor::extractLines(const QImage &image, const Parameters &params)
{
    emit progressChanged(0);

    // Step 1: Convert to grayscale
    QImage gray = toGrayscale(image);
    emit progressChanged(5);

    // Step 2: Gaussian blur
    QImage blurred = gaussianBlur(gray);
    emit progressChanged(7);

    // Step 3: Sobel edge detection
    QImage edges = sobelEdgeDetection(blurred);
    emit progressChanged(9);

    // Step 4: Binary threshold
    QImage binary = thresholdImage(edges, params.edgeThreshold);
    emit progressChanged(10);

    // Step 5: Probabilistic Hough Line Transform
    auto lines = houghLinesP(binary, params.houghThreshold,
                             params.minLineLength, params.maxLineGap);

    // Step 6: Post-processing
    if (params.mergeCollinear) {
        mergeCollinearSegments(lines, params.maxLineGap);
    }

    snapToAxisAligned(lines, params.angleSnapDegrees);
    removeDuplicates(lines);

    emit progressChanged(100);

    qDebug() << "LineExtractor: Detected" << lines.size() << "lines";
    return lines;
}
