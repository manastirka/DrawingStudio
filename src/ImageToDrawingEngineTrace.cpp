#include "ImageToDrawingEngine.h"

#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

// auto_trace action (refactor E26).

// --- autoTrace ---
QJsonObject ImageToDrawingEngine::autoTrace(const QJsonObject &params)
{
    if (!m_ctx.canvas)
        return QJsonObject();

    int imageIndex = params["imageIndex"].toInt(0);
    int threshold = params["threshold"].toInt(50);
    double simplifyEpsilon = getDouble(params, "simplify", 2.0);
    int minLength = params["minLength"].toInt(20);
    int maxContours = params["maxContours"].toInt(200);
    bool render = getBool(params, "render", true);
    double lineWidth = getDouble(params, "lineWidth", 1.0);
    QString colorParam = params["color"].toString("auto");
    bool colorMatch = getBool(params, "colorMatch", true);
    bool variableWidth = getBool(params, "variableWidth", true);
    bool gaussianBlur = getBool(params, "gaussianBlur", true);
    QColor lineColor = (colorParam != "auto") ? parseColor(params, "color", QColor("#000000")) : QColor("#000000");
    bool useColorMatch = (colorParam == "auto" && colorMatch);
    double opacity = getDouble(params, "opacity", 0.8);

    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "auto_trace: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();
        int w = img.width();
        int h = img.height();

        // Step 1: Convert to grayscale
        QImage gray = img.convertToFormat(QImage::Format_Grayscale8);

        // Step 1b: Gaussian blur pre-filter (3x3 kernel)
        QImage blurred = gray;
        if (gaussianBlur && w > 4 && h > 4) {
            blurred = QImage(w, h, QImage::Format_Grayscale8);
            // Kernel: [1,2,1; 2,4,2; 1,2,1] / 16
            for (int y = 1; y < h - 1; ++y) {
                const uchar* rA = gray.constScanLine(y - 1);
                const uchar* rC = gray.constScanLine(y);
                const uchar* rB = gray.constScanLine(y + 1);
                uchar* dst = blurred.scanLine(y);
                for (int x = 1; x < w - 1; ++x) {
                    int val = 1 * rA[x-1] + 2 * rA[x] + 1 * rA[x+1]
                            + 2 * rC[x-1] + 4 * rC[x] + 2 * rC[x+1]
                            + 1 * rB[x-1] + 2 * rB[x] + 1 * rB[x+1];
                    dst[x] = static_cast<uchar>(val / 16);
                }
            }
            // Copy borders
            memcpy(blurred.scanLine(0), gray.constScanLine(0), w);
            memcpy(blurred.scanLine(h - 1), gray.constScanLine(h - 1), w);
            for (int y = 0; y < h; ++y) {
                blurred.scanLine(y)[0] = gray.constScanLine(y)[0];
                blurred.scanLine(y)[w - 1] = gray.constScanLine(y)[w - 1];
            }
        }

        // Step 2: Sobel edge detection with gradient direction
        std::vector<float> magnitude(w * h, 0.0f);
        std::vector<float> gradX(w * h, 0.0f);
        std::vector<float> gradY(w * h, 0.0f);
        for (int y = 1; y < h - 1; ++y) {
            const uchar* rowAbove = blurred.constScanLine(y - 1);
            const uchar* rowCurr  = blurred.constScanLine(y);
            const uchar* rowBelow = blurred.constScanLine(y + 1);
            for (int x = 1; x < w - 1; ++x) {
                float gx = -1.0f * rowAbove[x-1] + 1.0f * rowAbove[x+1]
                          -2.0f * rowCurr[x-1]  + 2.0f * rowCurr[x+1]
                          -1.0f * rowBelow[x-1]  + 1.0f * rowBelow[x+1];
                float gy = -1.0f * rowAbove[x-1] - 2.0f * rowAbove[x] - 1.0f * rowAbove[x+1]
                          +1.0f * rowBelow[x-1] + 2.0f * rowBelow[x] + 1.0f * rowBelow[x+1];
                gradX[y * w + x] = gx;
                gradY[y * w + x] = gy;
                magnitude[y * w + x] = std::sqrt(gx * gx + gy * gy);
            }
        }

        // Step 2b: Non-maximum suppression
        std::vector<float> nms(w * h, 0.0f);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                float mag = magnitude[y * w + x];
                if (mag < 1e-6f) continue;

                // Quantize gradient direction to 4 angles
                float angle = std::atan2(gradY[y * w + x], gradX[y * w + x]);
                // Normalize to [0, pi)
                if (angle < 0) angle += static_cast<float>(M_PI);
                // 0: horizontal (0°), 1: diagonal 45°, 2: vertical (90°), 3: diagonal 135°
                int dir;
                if (angle < M_PI / 8 || angle >= 7 * M_PI / 8) dir = 0;
                else if (angle < 3 * M_PI / 8) dir = 1;
                else if (angle < 5 * M_PI / 8) dir = 2;
                else dir = 3;

                float n1 = 0, n2 = 0;
                switch (dir) {
                    case 0: // horizontal: check left/right
                        n1 = magnitude[y * w + (x - 1)];
                        n2 = magnitude[y * w + (x + 1)];
                        break;
                    case 1: // 45°: check top-right/bottom-left
                        n1 = magnitude[(y - 1) * w + (x + 1)];
                        n2 = magnitude[(y + 1) * w + (x - 1)];
                        break;
                    case 2: // vertical: check above/below
                        n1 = magnitude[(y - 1) * w + x];
                        n2 = magnitude[(y + 1) * w + x];
                        break;
                    case 3: // 135°: check top-left/bottom-right
                        n1 = magnitude[(y - 1) * w + (x - 1)];
                        n2 = magnitude[(y + 1) * w + (x + 1)];
                        break;
                }
                nms[y * w + x] = (mag >= n1 && mag >= n2) ? mag : 0.0f;
            }
        }

        // Find max magnitude for variable width scaling
        float maxMagnitude = 1.0f;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (nms[y * w + x] > maxMagnitude) maxMagnitude = nms[y * w + x];
            }
        }

        // Step 3: Trace edge chains (8-connected) using NMS result
        std::vector<bool> visited(w * h, false);
        const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
        const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};

        struct ChainData {
            std::vector<QPointF> points;
            float avgMagnitude;
            QColor matchedColor;
        };
        std::vector<ChainData> chains;

        for (int y = 1; y < h - 1 && static_cast<int>(chains.size()) < maxContours * 2; ++y) {
            for (int x = 1; x < w - 1 && static_cast<int>(chains.size()) < maxContours * 2; ++x) {
                if (visited[y * w + x] || nms[y * w + x] < threshold)
                    continue;

                std::vector<QPointF> chain;
                float magSum = 0;
                int cx = x, cy = y;
                int lastDir = -1;

                while (true) {
                    visited[cy * w + cx] = true;
                    chain.push_back(QPointF(cx, cy));
                    magSum += nms[cy * w + cx];

                    int bestDir = -1;
                    float bestMag = 0;

                    auto tryDir = [&](int d) {
                        int nx = cx + dx[d];
                        int ny = cy + dy[d];
                        if (nx >= 1 && nx < w - 1 && ny >= 1 && ny < h - 1 &&
                            !visited[ny * w + nx] && nms[ny * w + nx] >= threshold) {
                            float mag = nms[ny * w + nx];
                            if (bestDir == -1 || mag > bestMag) {
                                bestDir = d;
                                bestMag = mag;
                            }
                        }
                    };

                    if (lastDir >= 0) {
                        tryDir(lastDir);
                        tryDir((lastDir + 1) % 8);
                        tryDir((lastDir + 7) % 8);
                        tryDir((lastDir + 2) % 8);
                        tryDir((lastDir + 6) % 8);
                        tryDir((lastDir + 3) % 8);
                        tryDir((lastDir + 5) % 8);
                    } else {
                        for (int d = 0; d < 8; ++d) tryDir(d);
                    }

                    if (bestDir == -1) break;
                    cx += dx[bestDir];
                    cy += dy[bestDir];
                    lastDir = bestDir;
                }

                if (static_cast<int>(chain.size()) >= minLength) {
                    ChainData cd;
                    cd.points = std::move(chain);
                    cd.avgMagnitude = magSum / static_cast<float>(cd.points.size());

                    // Color matching: sample both sides of each edge point, pick darker side
                    if (useColorMatch) {
                        long rSum = 0, gSum = 0, bSum = 0;
                        int count = 0;
                        int sampleStep = qMax(1, static_cast<int>(cd.points.size()) / 20);
                        for (size_t pi = 0; pi < cd.points.size(); pi += sampleStep) {
                            int epx = static_cast<int>(cd.points[pi].x());
                            int epy = static_cast<int>(cd.points[pi].y());
                            // Get gradient direction at this point for perpendicular offset
                            float gxv = gradX[epy * w + epx];
                            float gyv = gradY[epy * w + epx];
                            float gmag = std::sqrt(gxv * gxv + gyv * gyv);
                            if (gmag < 1e-6f) continue;
                            // Perpendicular: rotate 90 degrees
                            float perpX = -gyv / gmag;
                            float perpY = gxv / gmag;
                            int off = 2;
                            // Sample both sides
                            int s1x = qBound(0, epx + static_cast<int>(perpX * off), w - 1);
                            int s1y = qBound(0, epy + static_cast<int>(perpY * off), h - 1);
                            int s2x = qBound(0, epx - static_cast<int>(perpX * off), w - 1);
                            int s2y = qBound(0, epy - static_cast<int>(perpY * off), h - 1);
                            const QRgb* scanLine1 = reinterpret_cast<const QRgb*>(img.constScanLine(s1y));
                            const QRgb* scanLine2 = reinterpret_cast<const QRgb*>(img.constScanLine(s2y));
                            QRgb p1 = scanLine1[s1x];
                            QRgb p2 = scanLine2[s2x];
                            // Pick darker side (lower luminance)
                            int lum1 = qRed(p1) * 299 + qGreen(p1) * 587 + qBlue(p1) * 114;
                            int lum2 = qRed(p2) * 299 + qGreen(p2) * 587 + qBlue(p2) * 114;
                            QRgb chosen = (lum1 < lum2) ? p1 : p2;
                            rSum += qRed(chosen);
                            gSum += qGreen(chosen);
                            bSum += qBlue(chosen);
                            count++;
                        }
                        if (count > 0) {
                            cd.matchedColor = QColor(rSum / count, gSum / count, bSum / count);
                        } else {
                            cd.matchedColor = lineColor;
                        }
                    } else {
                        cd.matchedColor = lineColor;
                    }

                    chains.push_back(std::move(cd));
                }
            }
        }

        // Step 4: Douglas-Peucker simplification
        std::function<void(const std::vector<QPointF>&, int, int, double, std::vector<bool>&)> dpSimplify;
        dpSimplify = [&dpSimplify](const std::vector<QPointF>& pts, int start, int end, double epsilon, std::vector<bool>& keep) {
            if (end <= start + 1) return;
            double maxDist = 0;
            int maxIdx = start;
            QPointF lineStart = pts[start];
            QPointF lineEnd = pts[end];
            double lineLen = std::sqrt(std::pow(lineEnd.x() - lineStart.x(), 2) + std::pow(lineEnd.y() - lineStart.y(), 2));
            for (int i = start + 1; i < end; ++i) {
                double dist;
                if (lineLen < 1e-6) {
                    dist = std::sqrt(std::pow(pts[i].x() - lineStart.x(), 2) + std::pow(pts[i].y() - lineStart.y(), 2));
                } else {
                    double t = ((pts[i].x() - lineStart.x()) * (lineEnd.x() - lineStart.x()) +
                                (pts[i].y() - lineStart.y()) * (lineEnd.y() - lineStart.y())) / (lineLen * lineLen);
                    t = qBound(0.0, t, 1.0);
                    double projX = lineStart.x() + t * (lineEnd.x() - lineStart.x());
                    double projY = lineStart.y() + t * (lineEnd.y() - lineStart.y());
                    dist = std::sqrt(std::pow(pts[i].x() - projX, 2) + std::pow(pts[i].y() - projY, 2));
                }
                if (dist > maxDist) {
                    maxDist = dist;
                    maxIdx = i;
                }
            }
            if (maxDist > epsilon) {
                keep[maxIdx] = true;
                dpSimplify(pts, start, maxIdx, epsilon, keep);
                dpSimplify(pts, maxIdx, end, epsilon, keep);
            }
        };

        // Simplify all chains
        struct SimplifiedChain {
            std::vector<QPointF> points;
            float avgMagnitude;
            QColor matchedColor;
        };
        std::vector<SimplifiedChain> simplified;
        for (auto& cd : chains) {
            int n = static_cast<int>(cd.points.size());
            if (n < 2) continue;
            std::vector<bool> keep(n, false);
            keep[0] = true;
            keep[n - 1] = true;
            dpSimplify(cd.points, 0, n - 1, simplifyEpsilon, keep);
            std::vector<QPointF> result;
            for (int i = 0; i < n; ++i) {
                if (keep[i]) result.push_back(cd.points[i]);
            }
            if (result.size() >= 2) {
                SimplifiedChain sc;
                sc.points = std::move(result);
                sc.avgMagnitude = cd.avgMagnitude;
                sc.matchedColor = cd.matchedColor;
                simplified.push_back(std::move(sc));
            }
        }

        // Sort by length (longest first) and limit to maxContours
        std::sort(simplified.begin(), simplified.end(),
            [](const SimplifiedChain& a, const SimplifiedChain& b) {
                return a.points.size() > b.points.size();
            });
        if (static_cast<int>(simplified.size()) > maxContours) {
            simplified.resize(maxContours);
        }

        // Step 5: Render as open PolygonPrimitives with variable width
        int contoursRendered = 0;
        double scaleX = static_cast<double>(worldSize.x()) / w;
        double scaleY = static_cast<double>(worldSize.y()) / h;

        if (render) {
            if (m_ctx.saveUndo) m_ctx.saveUndo("Auto Trace");
            Layer* layer = m_ctx.canvas->layerManager()->activeLayer();

            for (const auto& sc : simplified) {
                if (variableWidth) {
                    // Split chain into 3 width buckets based on magnitude
                    float relMag = sc.avgMagnitude / maxMagnitude;
                    double w_thin   = lineWidth * 0.5;
                    double w_medium = lineWidth * 0.75;
                    double w_thick  = lineWidth * 1.0;
                    double chosenWidth;
                    if (relMag < 0.33f) chosenWidth = w_thin;
                    else if (relMag < 0.66f) chosenWidth = w_medium;
                    else chosenWidth = w_thick;

                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(false);
                    poly->setFilled(false);
                    poly->setColor(sc.matchedColor);
                    poly->setLineWidth(chosenWidth);
                    poly->setOpacityMultiplier(opacity);

                    for (const auto& pt : sc.points) {
                        double wx = worldPos.x() + pt.x() * scaleX;
                        double wy = worldPos.y() + pt.y() * scaleY;
                        poly->addPoint(QVector2D(wx, wy));
                    }
                    layer->addPrimitive(std::move(poly));
                } else {
                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(false);
                    poly->setFilled(false);
                    poly->setColor(sc.matchedColor);
                    poly->setLineWidth(lineWidth);
                    poly->setOpacityMultiplier(opacity);

                    for (const auto& pt : sc.points) {
                        double wx = worldPos.x() + pt.x() * scaleX;
                        double wy = worldPos.y() + pt.y() * scaleY;
                        poly->addPoint(QVector2D(wx, wy));
                    }
                    layer->addPrimitive(std::move(poly));
                }
                contoursRendered++;
            }
            m_ctx.canvas->update();
        }

        // Build result
        QJsonObject result;
        result["contoursFound"] = static_cast<int>(simplified.size());
        result["contoursRendered"] = contoursRendered;

        QJsonArray contoursArr;
        for (const auto& sc : simplified) {
            QJsonObject contourObj;
            QJsonArray pointsArr;
            for (const auto& pt : sc.points) {
                QJsonObject ptObj;
                ptObj["x"] = worldPos.x() + pt.x() * scaleX;
                ptObj["y"] = worldPos.y() + pt.y() * scaleY;
                pointsArr.append(ptObj);
            }
            contourObj["points"] = pointsArr;
            contourObj["pointCount"] = static_cast<int>(sc.points.size());
            contoursArr.append(contourObj);
        }
        result["contours"] = contoursArr;
        return result;
    }

    return QJsonObject();
}


