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

// render_mosaic action (refactor E26).

// --- renderMosaic ---
QJsonObject ImageToDrawingEngine::renderMosaic(const QJsonObject &params)
{
    if (!m_ctx.canvas)
        return QJsonObject();

    int imageIndex = params["imageIndex"].toInt(0);
    QString quality = params["quality"].toString("medium");
    QString mode = params["mode"].toString("adaptive");

    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "render_mosaic: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();

        // Save undo state once before bulk insert
        if (m_ctx.saveUndo) m_ctx.saveUndo("Render Mosaic");
        Layer* layer = m_ctx.canvas->layerManager()->activeLayer();
        int primitivesCreated = 0;

        // Helper: average color of a small region using fast scanline access
        auto sampleArea = [&](int px, int py, int regionW, int regionH) -> QColor {
            long rSum = 0, gSum = 0, bSum = 0;
            int count = 0;
            int x0 = qMax(0, px);
            int y0 = qMax(0, py);
            int x1 = qMin(img.width(), px + regionW);
            int y1 = qMin(img.height(), py + regionH);
            // For small cells (<16px), sample every pixel
            int minDim = qMin(x1 - x0, y1 - y0);
            int step = (minDim < 16) ? 1 : qMax(1, minDim / 4);
            for (int sy = y0; sy < y1; sy += step) {
                const QRgb* scanLine = reinterpret_cast<const QRgb*>(img.constScanLine(sy));
                for (int sx = x0; sx < x1; sx += step) {
                    QRgb pixel = scanLine[sx];
                    rSum += qRed(pixel);
                    gSum += qGreen(pixel);
                    bSum += qBlue(pixel);
                    count++;
                }
            }
            if (count == 0) return QColor(128, 128, 128);
            return QColor(rSum / count, gSum / count, bSum / count);
        };

        // 3x3 averaged corner sample for better accuracy
        auto sampleCorner = [&](int cx, int cy) -> QColor {
            long rSum = 0, gSum = 0, bSum = 0;
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int sx = qBound(0, cx + dx, img.width() - 1);
                    int sy = qBound(0, cy + dy, img.height() - 1);
                    QRgb pixel = reinterpret_cast<const QRgb*>(img.constScanLine(sy))[sx];
                    rSum += qRed(pixel);
                    gSum += qGreen(pixel);
                    bSum += qBlue(pixel);
                    count++;
                }
            }
            return QColor(rSum / count, gSum / count, bSum / count);
        };

        // Color distance (Euclidean in RGB)
        auto colorDist = [](const QColor& a, const QColor& b) -> float {
            float dr = a.red() - b.red();
            float dg = a.green() - b.green();
            float db = a.blue() - b.blue();
            return std::sqrt(dr * dr + dg * dg + db * db);
        };

        // Average two colors
        auto avgColor = [](const QColor& a, const QColor& b) -> QColor {
            return QColor((a.red() + b.red()) / 2, (a.green() + b.green()) / 2, (a.blue() + b.blue()) / 2);
        };

        // Lambda to create a gradient-filled rect from pixel and world coords
        auto createGradientRect = [&](int px, int py, int cw, int ch,
                                      double wx1, double wy1, double wx2, double wy2)
            -> std::unique_ptr<RectanglePrimitive> {
            auto rect = std::make_unique<RectanglePrimitive>(
                QVector2D(wx1, wy1), QVector2D(wx2, wy2));
            rect->setFilled(true);
            rect->setLineWidth(0);

            // 3x3 averaged corner samples
            int cornerW = qMax(1, cw / 4);
            int cornerH = qMax(1, ch / 4);
            QColor tl = sampleCorner(px + cornerW / 2, py + cornerH / 2);
            QColor tr = sampleCorner(px + cw - cornerW / 2, py + cornerH / 2);
            QColor bl = sampleCorner(px + cornerW / 2, py + ch - cornerH / 2);
            QColor br = sampleCorner(px + cw - cornerW / 2, py + ch - cornerH / 2);

            float distH = colorDist(avgColor(tl, bl), avgColor(tr, br));
            float distV = colorDist(avgColor(tl, tr), avgColor(bl, br));
            float distD1 = colorDist(tl, br);
            float distD2 = colorDist(tr, bl);
            float maxDist = qMax(qMax(distH, distV), qMax(distD1, distD2));

            if (maxDist < 15.0f) {
                QColor center = sampleArea(px + cw / 4, py + ch / 4, cw / 2, ch / 2);
                rect->setFillColor(center);
            } else {
                rect->setGradientFillType(DrawingPrimitive::GradientFillType::Linear);
                if (maxDist == distH) {
                    rect->setGradientAngle(0);
                    rect->setGradientStartColor(avgColor(tl, bl));
                    rect->setGradientEndColor(avgColor(tr, br));
                } else if (maxDist == distV) {
                    rect->setGradientAngle(90);
                    rect->setGradientStartColor(avgColor(bl, br));
                    rect->setGradientEndColor(avgColor(tl, tr));
                } else if (maxDist == distD1) {
                    rect->setGradientAngle(135);
                    rect->setGradientStartColor(br);
                    rect->setGradientEndColor(tl);
                } else {
                    rect->setGradientAngle(45);
                    rect->setGradientStartColor(bl);
                    rect->setGradientEndColor(tr);
                }
            }
            return rect;
        };

        if (mode == "adaptive") {
            // --- Adaptive quadtree subdivision ---
            int adaptiveBase = 80; // medium default
            int maxDepth = 5;
            float varianceThreshold = 20.0f;
            bool blending = getBool(params, "blending", true);

            if (quality == "low")        { adaptiveBase = 50;  maxDepth = 4; varianceThreshold = 40.0f; }
            else if (quality == "medium") { adaptiveBase = 80;  maxDepth = 5; varianceThreshold = 20.0f; }
            else if (quality == "high")   { adaptiveBase = 150; maxDepth = 6; varianceThreshold = 12.0f; }
            else if (quality == "ultra")  { adaptiveBase = 250; maxDepth = 6; varianceThreshold = 8.0f; }

            // Allow explicit overrides
            if (params.contains("maxDepth")) maxDepth = params["maxDepth"].toInt(maxDepth);
            if (params.contains("varianceThreshold")) varianceThreshold = static_cast<float>(getDouble(params, "varianceThreshold", varianceThreshold));

            // Compute base grid from aspect ratio
            float aspect = static_cast<float>(img.width()) / static_cast<float>(img.height());
            int gridX, gridY;
            if (aspect > 1.0f) {
                gridX = adaptiveBase;
                gridY = qMax(1, static_cast<int>(adaptiveBase / aspect));
            } else {
                gridY = adaptiveBase;
                gridX = qMax(1, static_cast<int>(adaptiveBase * aspect));
            }

            // Quadtree cell struct
            struct QuadCell {
                int px, py, cw, ch; // pixel coords in image
                double wx, wy, ww, wh; // world coords
                int depth;
            };

            std::queue<QuadCell> cellQueue;
            // Leaf cells stored for blending pass
            struct LeafCell {
                int px, py, cw, ch;
                double wx, wy, ww, wh;
                float variance;
            };
            std::vector<LeafCell> leafCells;

            double baseCellW = static_cast<double>(img.width()) / gridX;
            double baseCellH = static_cast<double>(img.height()) / gridY;
            double worldBaseCellW = static_cast<double>(worldSize.x()) / gridX;
            double worldBaseCellH = static_cast<double>(worldSize.y()) / gridY;

            // Seed the queue with base grid cells
            for (int row = 0; row < gridY; ++row) {
                for (int col = 0; col < gridX; ++col) {
                    QuadCell cell;
                    cell.px = static_cast<int>(col * baseCellW);
                    cell.py = static_cast<int>(row * baseCellH);
                    cell.cw = static_cast<int>(baseCellW);
                    cell.ch = static_cast<int>(baseCellH);
                    cell.wx = worldPos.x() + col * worldBaseCellW;
                    cell.wy = worldPos.y() + row * worldBaseCellH;
                    cell.ww = worldBaseCellW;
                    cell.wh = worldBaseCellH;
                    cell.depth = 0;
                    cellQueue.push(cell);
                }
            }

            // Process queue iteratively
            while (!cellQueue.empty()) {
                QuadCell cell = cellQueue.front();
                cellQueue.pop();

                // Compute variance: sample 3x3 grid inside cell, max pairwise distance
                QColor samples[9];
                int si = 0;
                for (int gy = 0; gy < 3; ++gy) {
                    for (int gx = 0; gx < 3; ++gx) {
                        int sx = cell.px + (cell.cw * (gx + 1)) / 4;
                        int sy = cell.py + (cell.ch * (gy + 1)) / 4;
                        sx = qBound(0, sx, img.width() - 1);
                        sy = qBound(0, sy, img.height() - 1);
                        samples[si++] = sampleCorner(sx, sy);
                    }
                }
                float maxVar = 0;
                for (int i = 0; i < 9; ++i) {
                    for (int j = i + 1; j < 9; ++j) {
                        float d = colorDist(samples[i], samples[j]);
                        if (d > maxVar) maxVar = d;
                    }
                }

                bool shouldSubdivide = (maxVar > varianceThreshold)
                                       && (cell.depth < maxDepth)
                                       && (cell.cw > 4) && (cell.ch > 4);

                if (shouldSubdivide) {
                    // Subdivide into 4 sub-cells
                    int halfW = cell.cw / 2;
                    int halfH = cell.ch / 2;
                    double wHalfW = cell.ww / 2.0;
                    double wHalfH = cell.wh / 2.0;

                    QuadCell tl = { cell.px, cell.py, halfW, halfH,
                                    cell.wx, cell.wy, wHalfW, wHalfH, cell.depth + 1 };
                    QuadCell tr = { cell.px + halfW, cell.py, cell.cw - halfW, halfH,
                                    cell.wx + wHalfW, cell.wy, cell.ww - wHalfW, wHalfH, cell.depth + 1 };
                    QuadCell bl = { cell.px, cell.py + halfH, halfW, cell.ch - halfH,
                                    cell.wx, cell.wy + wHalfH, wHalfW, cell.wh - wHalfH, cell.depth + 1 };
                    QuadCell br = { cell.px + halfW, cell.py + halfH, cell.cw - halfW, cell.ch - halfH,
                                    cell.wx + wHalfW, cell.wy + wHalfH, cell.ww - wHalfW, cell.wh - wHalfH, cell.depth + 1 };
                    cellQueue.push(tl);
                    cellQueue.push(tr);
                    cellQueue.push(bl);
                    cellQueue.push(br);
                } else {
                    // Leaf cell: render gradient rect
                    auto rect = createGradientRect(cell.px, cell.py, cell.cw, cell.ch,
                                                   cell.wx, cell.wy, cell.wx + cell.ww, cell.wy + cell.wh);
                    layer->addPrimitive(std::move(rect));
                    primitivesCreated++;

                    LeafCell lc;
                    lc.px = cell.px; lc.py = cell.py; lc.cw = cell.cw; lc.ch = cell.ch;
                    lc.wx = cell.wx; lc.wy = cell.wy; lc.ww = cell.ww; lc.wh = cell.wh;
                    lc.variance = maxVar;
                    leafCells.push_back(lc);
                }
            }

            // Blending pass: add thin overlap rectangles at adjacent cell boundaries
            if (blending && leafCells.size() > 1) {
                // Build a spatial index: map from (approx right edge, approx top) to leaf index
                // Simple approach: for each pair of adjacent leaves, create blend strip
                // Use a grid-based spatial lookup for efficiency
                double minCellW = worldBaseCellW;
                double minCellH = worldBaseCellH;
                for (const auto& lc : leafCells) {
                    if (lc.ww < minCellW) minCellW = lc.ww;
                    if (lc.wh < minCellH) minCellH = lc.wh;
                }
                // Check all pairs (for moderate cell counts this is fine)
                // Limit blending to avoid excessive primitives
                int blendCount = 0;
                int maxBlends = static_cast<int>(leafCells.size()) * 2;
                for (size_t i = 0; i < leafCells.size() && blendCount < maxBlends; ++i) {
                    const auto& a = leafCells[i];
                    for (size_t j = i + 1; j < leafCells.size() && blendCount < maxBlends; ++j) {
                        const auto& b = leafCells[j];
                        double smallerW = qMin(a.ww, b.ww);
                        double smallerH = qMin(a.wh, b.wh);

                        // Check horizontal adjacency (a's right edge == b's left edge)
                        bool hAdj = (std::abs((a.wx + a.ww) - b.wx) < smallerW * 0.1)
                                    && (std::abs(a.wy - b.wy) < smallerH * 0.5)
                                    && (a.wy + a.wh > b.wy + smallerH * 0.1)
                                    && (b.wy + b.wh > a.wy + smallerH * 0.1);
                        // Check vertical adjacency (a's bottom edge == b's top edge)
                        bool vAdj = (std::abs((a.wy + a.wh) - b.wy) < smallerH * 0.1)
                                    && (std::abs(a.wx - b.wx) < smallerW * 0.5)
                                    && (a.wx + a.ww > b.wx + smallerW * 0.1)
                                    && (b.wx + b.ww > a.wx + smallerW * 0.1);

                        if (hAdj) {
                            double overlapW = smallerW * 0.25;
                            double overlapTop = qMax(a.wy, b.wy);
                            double overlapBot = qMin(a.wy + a.wh, b.wy + b.wh);
                            double edgeX = a.wx + a.ww;
                            // Sample colors at boundary
                            int aPx = qBound(0, a.px + a.cw - 1, img.width() - 1);
                            int bPx = qBound(0, b.px, img.width() - 1);
                            int midPy = qBound(0, (a.py + a.py + a.ch) / 2, img.height() - 1);
                            QColor aColor = sampleCorner(aPx, midPy);
                            QColor bColor = sampleCorner(bPx, midPy);
                            QColor blendColor = avgColor(aColor, bColor);

                            auto blendRect = std::make_unique<RectanglePrimitive>(
                                QVector2D(edgeX - overlapW / 2, overlapTop),
                                QVector2D(edgeX + overlapW / 2, overlapBot));
                            blendRect->setFilled(true);
                            blendRect->setLineWidth(0);
                            blendRect->setFillColor(blendColor);
                            blendRect->setOpacityMultiplier(0.4f);
                            layer->addPrimitive(std::move(blendRect));
                            primitivesCreated++;
                            blendCount++;
                        }
                        if (vAdj) {
                            double overlapH = smallerH * 0.25;
                            double overlapLeft = qMax(a.wx, b.wx);
                            double overlapRight = qMin(a.wx + a.ww, b.wx + b.ww);
                            double edgeY = a.wy + a.wh;
                            int aPy = qBound(0, a.py + a.ch - 1, img.height() - 1);
                            int bPy = qBound(0, b.py, img.height() - 1);
                            int midPx = qBound(0, (a.px + a.px + a.cw) / 2, img.width() - 1);
                            QColor aColor = sampleCorner(midPx, aPy);
                            QColor bColor = sampleCorner(midPx, bPy);
                            QColor blendColor = avgColor(aColor, bColor);

                            auto blendRect = std::make_unique<RectanglePrimitive>(
                                QVector2D(overlapLeft, edgeY - overlapH / 2),
                                QVector2D(overlapRight, edgeY + overlapH / 2));
                            blendRect->setFilled(true);
                            blendRect->setLineWidth(0);
                            blendRect->setFillColor(blendColor);
                            blendRect->setOpacityMultiplier(0.4f);
                            layer->addPrimitive(std::move(blendRect));
                            primitivesCreated++;
                            blendCount++;
                        }
                    }
                }
            }

            m_ctx.canvas->update();
            QJsonObject result;
            result["primitivesCreated"] = primitivesCreated;
            result["leafCells"] = static_cast<int>(leafCells.size());
            result["mode"] = mode;
            // Store leaf cell data in m_adaptiveLeafCells for use by render_photo_copy detail overlay
            m_adaptiveLeafCells = QJsonArray();
            for (const auto& lc : leafCells) {
                QJsonObject lcObj;
                lcObj["px"] = lc.px; lcObj["py"] = lc.py;
                lcObj["cw"] = lc.cw; lcObj["ch"] = lc.ch;
                lcObj["wx"] = lc.wx; lcObj["wy"] = lc.wy;
                lcObj["ww"] = lc.ww; lcObj["wh"] = lc.wh;
                lcObj["variance"] = static_cast<double>(lc.variance);
                m_adaptiveLeafCells.append(lcObj);
            }
            return result;
        } else {
            // --- Legacy flat/gradient uniform grid mode ---
            int baseGrid = 40;
            if (quality == "low") baseGrid = 20;
            else if (quality == "high") baseGrid = 60;
            else if (quality == "ultra") baseGrid = 100;

            int gridX = params.contains("gridX") ? params["gridX"].toInt(baseGrid) : baseGrid;
            int gridY = params.contains("gridY") ? params["gridY"].toInt(baseGrid) : baseGrid;

            if (!params.contains("gridX") && !params.contains("gridY")) {
                float aspect = static_cast<float>(img.width()) / static_cast<float>(img.height());
                if (aspect > 1.0f) {
                    gridX = baseGrid;
                    gridY = qMax(1, static_cast<int>(baseGrid / aspect));
                } else {
                    gridY = baseGrid;
                    gridX = qMax(1, static_cast<int>(baseGrid * aspect));
                }
            }

            double cellW = static_cast<double>(img.width()) / gridX;
            double cellH = static_cast<double>(img.height()) / gridY;
            double worldCellW = static_cast<double>(worldSize.x()) / gridX;
            double worldCellH = static_cast<double>(worldSize.y()) / gridY;
            bool useGradient = (mode == "gradient");

            for (int row = 0; row < gridY; ++row) {
                for (int col = 0; col < gridX; ++col) {
                    int px = static_cast<int>(col * cellW);
                    int py = static_cast<int>(row * cellH);
                    int cw = static_cast<int>(cellW);
                    int ch = static_cast<int>(cellH);
                    double wx1 = worldPos.x() + col * worldCellW;
                    double wy1 = worldPos.y() + row * worldCellH;
                    double wx2 = wx1 + worldCellW;
                    double wy2 = wy1 + worldCellH;

                    if (useGradient) {
                        auto rect = createGradientRect(px, py, cw, ch, wx1, wy1, wx2, wy2);
                        layer->addPrimitive(std::move(rect));
                    } else {
                        auto rect = std::make_unique<RectanglePrimitive>(
                            QVector2D(wx1, wy1), QVector2D(wx2, wy2));
                        rect->setFilled(true);
                        rect->setLineWidth(0);
                        QColor center = sampleArea(px + cw / 4, py + ch / 4, cw / 2, ch / 2);
                        rect->setFillColor(center);
                        layer->addPrimitive(std::move(rect));
                    }
                    primitivesCreated++;
                }
            }

            m_ctx.canvas->update();
            QJsonObject result;
            result["primitivesCreated"] = primitivesCreated;
            result["gridX"] = gridX;
            result["gridY"] = gridY;
            result["mode"] = mode;
            return result;
        }
    }

    return QJsonObject();
}


