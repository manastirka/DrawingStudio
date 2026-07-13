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

// LineExtractor image preprocessing (refactor E25).

// --- toGrayscale ---
QImage LineExtractor::toGrayscale(const QImage &image)
{
    return image.convertToFormat(QImage::Format_Grayscale8);
}


// --- invertGray ---
QImage LineExtractor::invertGray(const QImage &gray)
{
    QImage out = gray.copy();
    for (int y = 0; y < out.height(); ++y) {
        uchar *row = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x)
            row[x] = static_cast<uchar>(255 - row[x]);
    }
    return out;
}


// --- gaussianBlur ---
QImage LineExtractor::gaussianBlur(const QImage &gray)
{
    QImage result = gray.copy();
    const int w = gray.width();
    const int h = gray.height();
    const float kernel[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    constexpr float kernelSum = 16.0f;

    for (int y = 1; y < h - 1; ++y) {
        const uchar *prevRow = gray.constScanLine(y - 1);
        const uchar *currRow = gray.constScanLine(y);
        const uchar *nextRow = gray.constScanLine(y + 1);
        uchar *outRow = result.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            float sum = 0;
            sum += prevRow[x - 1] * kernel[0][0] + prevRow[x] * kernel[0][1] +
                   prevRow[x + 1] * kernel[0][2];
            sum += currRow[x - 1] * kernel[1][0] + currRow[x] * kernel[1][1] +
                   currRow[x + 1] * kernel[1][2];
            sum += nextRow[x - 1] * kernel[2][0] + nextRow[x] * kernel[2][1] +
                   nextRow[x + 1] * kernel[2][2];
            outRow[x] = static_cast<uchar>(std::min(255.0f, sum / kernelSum));
        }
    }
    return result;
}


// --- sobelEdgeDetection ---
QImage LineExtractor::sobelEdgeDetection(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QImage result(w, h, QImage::Format_Grayscale8);
    result.fill(0);
    for (int y = 1; y < h - 1; ++y) {
        const uchar *prevRow = gray.constScanLine(y - 1);
        const uchar *currRow = gray.constScanLine(y);
        const uchar *nextRow = gray.constScanLine(y + 1);
        uchar *outRow = result.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            const int gx = -prevRow[x - 1] + prevRow[x + 1] - 2 * currRow[x - 1] +
                           2 * currRow[x + 1] - nextRow[x - 1] + nextRow[x + 1];
            const int gy = -prevRow[x - 1] - 2 * prevRow[x] - prevRow[x + 1] +
                           nextRow[x - 1] + 2 * nextRow[x] + nextRow[x + 1];
            outRow[x] = static_cast<uchar>(
                std::min(255, static_cast<int>(std::sqrt(gx * gx + gy * gy))));
        }
    }
    return result;
}


// --- thresholdImage ---
QImage LineExtractor::thresholdImage(const QImage &src, int threshold, bool inv)
{
    const int w = src.width();
    const int h = src.height();
    QImage binary(w, h, QImage::Format_Grayscale8);
    binary.fill(0);
    for (int y = 0; y < h; ++y) {
        const uchar *srcRow = src.constScanLine(y);
        uchar *dstRow = binary.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const bool on = inv ? (srcRow[x] < threshold) : (srcRow[x] >= threshold);
            dstRow[x] = on ? 255 : 0;
        }
    }
    return binary;
}


// --- otsuBinarizeInk ---
QImage LineExtractor::otsuBinarizeInk(const QImage &gray)
{
    // Dark ink on light paper → walls = 255
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
        return thresholdImage(gray, 128, true);

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
    return thresholdImage(gray, best, true);
}


// --- adaptiveBinarizeInk ---
QImage LineExtractor::adaptiveBinarizeInk(const QImage &gray)
{
    // Local mean threshold (approx adaptive): walls where pixel << local mean
    const int w = gray.width();
    const int h = gray.height();
    const int radius = std::clamp(std::min(w, h) / 40, 8, 25);
    QImage blurred = gray;
    // Box blur via repeated small Gaussian is enough for local mean approx
    for (int i = 0; i < 3; ++i)
        blurred = gaussianBlur(blurred);
    // Extra blur iterations for larger radius feel
    Q_UNUSED(radius);

    QImage binary(w, h, QImage::Format_Grayscale8);
    binary.fill(0);
    constexpr int C = 12;
    for (int y = 0; y < h; ++y) {
        const uchar *src = gray.constScanLine(y);
        const uchar *mean = blurred.constScanLine(y);
        uchar *dst = binary.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (static_cast<int>(src[x]) < static_cast<int>(mean[x]) - C)
                dst[x] = 255;
        }
    }
    return binary;
}


// --- morphologicalOpen ---
QImage LineExtractor::morphologicalOpen(const QImage &binary)
{
    return morphOpenRect(binary, 3, 3);
}


// --- morphologicalClose ---
QImage LineExtractor::morphologicalClose(const QImage &binary)
{
    // dilate then erode
    const int w = binary.width();
    const int h = binary.height();
    auto dilate3 = [&](const QImage &src) {
        QImage dst(w, h, QImage::Format_Grayscale8);
        dst.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            uchar *out = dst.scanLine(y);
            for (int x = 1; x < w - 1; ++x) {
                bool any = false;
                for (int dy = -1; dy <= 1 && !any; ++dy) {
                    const uchar *row = src.constScanLine(y + dy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (row[x + dx] > 0) {
                            any = true;
                            break;
                        }
                    }
                }
                out[x] = any ? 255 : 0;
            }
        }
        return dst;
    };
    auto erode3 = [&](const QImage &src) {
        QImage dst(w, h, QImage::Format_Grayscale8);
        dst.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            uchar *out = dst.scanLine(y);
            for (int x = 1; x < w - 1; ++x) {
                bool keep = true;
                for (int dy = -1; dy <= 1 && keep; ++dy) {
                    const uchar *row = src.constScanLine(y + dy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (row[x + dx] == 0) {
                            keep = false;
                            break;
                        }
                    }
                }
                out[x] = keep ? 255 : 0;
            }
        }
        return dst;
    };
    return erode3(dilate3(binary));
}


// --- morphOpenRect ---
QImage LineExtractor::morphOpenRect(const QImage &binary, int kw, int kh)
{
    // Open = erode then dilate with rectangular kernel
    kw = std::max(1, kw | 1); // odd
    kh = std::max(1, kh | 1);
    const int rx = kw / 2;
    const int ry = kh / 2;
    const int w = binary.width();
    const int h = binary.height();

    auto erode = [&](const QImage &src) {
        QImage dst(w, h, QImage::Format_Grayscale8);
        dst.fill(0);
        for (int y = ry; y < h - ry; ++y) {
            uchar *out = dst.scanLine(y);
            for (int x = rx; x < w - rx; ++x) {
                bool keep = true;
                for (int dy = -ry; dy <= ry && keep; ++dy) {
                    const uchar *row = src.constScanLine(y + dy);
                    for (int dx = -rx; dx <= rx; ++dx) {
                        if (row[x + dx] == 0) {
                            keep = false;
                            break;
                        }
                    }
                }
                out[x] = keep ? 255 : 0;
            }
        }
        return dst;
    };
    auto dilate = [&](const QImage &src) {
        QImage dst(w, h, QImage::Format_Grayscale8);
        dst.fill(0);
        for (int y = ry; y < h - ry; ++y) {
            uchar *out = dst.scanLine(y);
            for (int x = rx; x < w - rx; ++x) {
                bool any = false;
                for (int dy = -ry; dy <= ry && !any; ++dy) {
                    const uchar *row = src.constScanLine(y + dy);
                    for (int dx = -rx; dx <= rx; ++dx) {
                        if (row[x + dx] > 0) {
                            any = true;
                            break;
                        }
                    }
                }
                out[x] = any ? 255 : 0;
            }
        }
        return dst;
    };
    return dilate(erode(binary));
}


// --- morphDilate ---
QImage LineExtractor::morphDilate(const QImage &binary, int iters)
{
    QImage cur = binary;
    const int w = binary.width();
    const int h = binary.height();
    for (int i = 0; i < iters; ++i) {
        QImage dst(w, h, QImage::Format_Grayscale8);
        dst.fill(0);
        for (int y = 1; y < h - 1; ++y) {
            uchar *out = dst.scanLine(y);
            for (int x = 1; x < w - 1; ++x) {
                bool any = false;
                for (int dy = -1; dy <= 1 && !any; ++dy) {
                    const uchar *row = cur.constScanLine(y + dy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (row[x + dx] > 0) {
                            any = true;
                            break;
                        }
                    }
                }
                out[x] = any ? 255 : 0;
            }
        }
        cur = dst;
    }
    return cur;
}


// --- extractWallMask ---
QImage LineExtractor::extractWallMask(const QImage &inkBinary, int kernLen, int thickness)
{
    // Long H + V morphological open isolates wall runs; drops stairs/text noise
    kernLen = std::max(5, kernLen | 1);
    thickness = std::max(1, thickness | 1);
    QImage horiz = morphOpenRect(inkBinary, kernLen, thickness);
    QImage vert = morphOpenRect(inkBinary, thickness, kernLen);

    const int w = inkBinary.width();
    const int h = inkBinary.height();
    QImage walls(w, h, QImage::Format_Grayscale8);
    walls.fill(0);
    for (int y = 0; y < h; ++y) {
        const uchar *hr = horiz.constScanLine(y);
        const uchar *vr = vert.constScanLine(y);
        uchar *out = walls.scanLine(y);
        for (int x = 0; x < w; ++x)
            out[x] = (hr[x] | vr[x]) ? 255 : 0;
    }
    // Reconnect fragments at T-junctions
    walls = morphDilate(walls, 1);
    walls = morphologicalClose(walls);
    return walls;
}


// --- deskewImage ---
QImage LineExtractor::deskewImage(const QImage &gray, double *skewDegreesOut)
{
    if (skewDegreesOut)
        *skewDegreesOut = 0.0;

    QImage small = gray;
    if (std::max(gray.width(), gray.height()) > 900)
        small = gray.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage ink = otsuBinarizeInk(gaussianBlur(small));
    const int w = ink.width();
    const int h = ink.height();
    std::vector<double> deviations;
    deviations.reserve(256);

    // Sample long runs as angle cues
    for (int y = 0; y < h; y += 3) {
        const uchar *row = ink.constScanLine(y);
        int run = 0;
        for (int x = 0; x < w; ++x) {
            if (row[x] > 0) {
                ++run;
            } else {
                if (run > w / 10)
                    deviations.push_back(0.0); // horizontal
                run = 0;
            }
        }
    }
    for (int x = 0; x < w; x += 3) {
        int run = 0;
        for (int y = 0; y < h; ++y) {
            if (ink.constScanLine(y)[x] > 0) {
                ++run;
            } else {
                if (run > h / 10)
                    deviations.push_back(0.0); // vertical → 0 skew from axis
                run = 0;
            }
        }
    }

    // Fine skew via Sobel Hough peaks (small angles only)
    QImage edges = sobelEdgeDetection(gaussianBlur(small));
    QImage bin = thresholdImage(edges, 50);
    const int numAngles = 180;
    const double maxDist = std::hypot(w, h);
    const int rhoMax = static_cast<int>(std::ceil(maxDist)) + 1;
    const int rhoSize = 2 * rhoMax + 1;
    std::vector<int> acc(rhoSize * numAngles, 0);
    std::vector<double> cosT(numAngles), sinT(numAngles);
    for (int t = 0; t < numAngles; ++t) {
        const double th = t * M_PI / 180.0;
        cosT[t] = std::cos(th);
        sinT[t] = std::sin(th);
    }
    for (int y = 0; y < h; y += 2) {
        const uchar *row = bin.constScanLine(y);
        for (int x = 0; x < w; x += 2) {
            if (!row[x])
                continue;
            for (int t = 0; t < numAngles; ++t) {
                const int rhoIdx =
                    static_cast<int>(std::round(x * cosT[t] + y * sinT[t])) + rhoMax;
                if (rhoIdx >= 0 && rhoIdx < rhoSize)
                    ++acc[rhoIdx * numAngles + t];
            }
        }
    }
    const int voteMin = std::max(40, (w + h) / 30);
    std::vector<double> skews;
    for (int r = 0; r < rhoSize; ++r) {
        for (int t = 0; t < numAngles; ++t) {
            if (acc[r * numAngles + t] < voteMin)
                continue;
            double lineAngle = std::fmod(t + 90.0, 180.0);
            double deviation = std::fmod(lineAngle, 90.0);
            if (deviation > 45.0)
                deviation -= 90.0;
            skews.push_back(deviation);
        }
    }
    if (skews.empty())
        return gray;

    std::nth_element(skews.begin(), skews.begin() + static_cast<long>(skews.size() / 2),
                     skews.end());
    const double skew = skews[skews.size() / 2];
    if (skewDegreesOut)
        *skewDegreesOut = skew;
    if (std::abs(skew) < 1.5)  // ignore tiny / noisy skew estimates
        return gray;

    QTransform xform;
    xform.translate(gray.width() * 0.5, gray.height() * 0.5);
    xform.rotate(-skew);
    xform.translate(-gray.width() * 0.5, -gray.height() * 0.5);
    return gray.transformed(xform, Qt::SmoothTransformation);
}


