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

// render_photo_copy action (refactor E26).

// --- renderPhotoCopy ---
QJsonObject ImageToDrawingEngine::renderPhotoCopy(const QJsonObject &params)
{
    if (!m_ctx.canvas)
        return QJsonObject();

    int imageIndex = params["imageIndex"].toInt(0);
    QString quality = params["quality"].toString("high");
    bool removeImage = getBool(params, "removeImage", true);
    QString style = params["style"].toString("photorealistic");

    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "render_photo_copy: image not found at index" << imageIndex;
    } else if (style == "sketch") {
        // ===================== PENCIL SKETCH STYLE =====================
        // Creates hatching/cross-hatching based on luminance — looks like a real pencil drawing
        QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();
        int imgW = srcImg.width();
        int imgH = srcImg.height();
        double scaleX = static_cast<double>(worldSize.x()) / imgW;
        double scaleY = static_cast<double>(worldSize.y()) / imgH;

        // Build luminance map
        std::vector<uchar> lum(imgW * imgH);
        for (int y = 0; y < imgH; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(srcImg.constScanLine(y));
            for (int x = 0; x < imgW; ++x) {
                QRgb p = row[x];
                lum[y * imgW + x] = static_cast<uchar>((qRed(p) * 299 + qGreen(p) * 587 + qBlue(p) * 114) / 1000);
            }
        }

        if (m_ctx.saveUndo) m_ctx.saveUndo("Render Sketch");
        Layer* layer = m_ctx.canvas->layerManager()->activeLayer();
        int primCount = 0;

        // Paper background
        auto bg = std::make_unique<RectanglePrimitive>(
            QVector2D(worldPos.x(), worldPos.y()),
            QVector2D(worldPos.x() + worldSize.x(), worldPos.y() + worldSize.y()));
        bg->setFilled(true);
        bg->setFillColor(QColor(252, 250, 245));
        bg->setLineWidth(0);
        layer->addPrimitive(std::move(bg));
        primCount++;

        // Hatching spacing based on quality
        int sp = 4;
        if (quality == "low") sp = 7;
        else if (quality == "medium") sp = 5;
        else if (quality == "high") sp = 3;
        else if (quality == "ultra") sp = 2;

        QColor pencil(35, 30, 25);

        // Lambda: 45° hatching (lines where x - y = k)
        auto hatch45 = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
            for (int k = -(imgH - 1) + offset; k < imgW; k += spacing) {
                int xS = qMax(0, k);
                int xE = qMin(imgW - 1, k + imgH - 1);
                bool inSeg = false;
                int sx0 = 0;
                for (int x = xS; x <= xE; ++x) {
                    int y = x - k;
                    if (lum[y * imgW + x] < lumThresh) {
                        if (!inSeg) { sx0 = x; inSeg = true; }
                    } else {
                        if (inSeg && (x - sx0) >= 3) {
                            auto poly = std::make_unique<PolygonPrimitive>();
                            poly->setClosed(false); poly->setFilled(false);
                            poly->setColor(pencil); poly->setLineWidth(lw);
                            poly->setOpacityMultiplier(opa);
                            poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (sx0 - k) * scaleY));
                            poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + (x - 1 - k) * scaleY));
                            layer->addPrimitive(std::move(poly));
                            primCount++;
                        }
                        inSeg = false;
                    }
                }
                if (inSeg && (xE - sx0) >= 3) {
                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(false); poly->setFilled(false);
                    poly->setColor(pencil); poly->setLineWidth(lw);
                    poly->setOpacityMultiplier(opa);
                    poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (sx0 - k) * scaleY));
                    poly->addPoint(QVector2D(worldPos.x() + xE * scaleX, worldPos.y() + (xE - k) * scaleY));
                    layer->addPrimitive(std::move(poly));
                    primCount++;
                }
            }
        };

        // Lambda: 135° hatching (lines where x + y = k)
        auto hatch135 = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
            for (int k = offset; k < imgW + imgH - 1; k += spacing) {
                int xS = qMax(0, k - imgH + 1);
                int xE = qMin(imgW - 1, k);
                bool inSeg = false;
                int sx0 = 0;
                for (int x = xS; x <= xE; ++x) {
                    int y = k - x;
                    if (lum[y * imgW + x] < lumThresh) {
                        if (!inSeg) { sx0 = x; inSeg = true; }
                    } else {
                        if (inSeg && (x - sx0) >= 3) {
                            auto poly = std::make_unique<PolygonPrimitive>();
                            poly->setClosed(false); poly->setFilled(false);
                            poly->setColor(pencil); poly->setLineWidth(lw);
                            poly->setOpacityMultiplier(opa);
                            poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (k - sx0) * scaleY));
                            poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + (k - x + 1) * scaleY));
                            layer->addPrimitive(std::move(poly));
                            primCount++;
                        }
                        inSeg = false;
                    }
                }
                if (inSeg && (xE - sx0) >= 3) {
                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(false); poly->setFilled(false);
                    poly->setColor(pencil); poly->setLineWidth(lw);
                    poly->setOpacityMultiplier(opa);
                    poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (k - sx0) * scaleY));
                    poly->addPoint(QVector2D(worldPos.x() + xE * scaleX, worldPos.y() + (k - xE) * scaleY));
                    layer->addPrimitive(std::move(poly));
                    primCount++;
                }
            }
        };

        // Lambda: horizontal hatching (y = const)
        auto hatchH = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
            for (int y = offset; y < imgH; y += spacing) {
                bool inSeg = false;
                int sx0 = 0;
                for (int x = 0; x < imgW; ++x) {
                    if (lum[y * imgW + x] < lumThresh) {
                        if (!inSeg) { sx0 = x; inSeg = true; }
                    } else {
                        if (inSeg && (x - sx0) >= 3) {
                            auto poly = std::make_unique<PolygonPrimitive>();
                            poly->setClosed(false); poly->setFilled(false);
                            poly->setColor(pencil); poly->setLineWidth(lw);
                            poly->setOpacityMultiplier(opa);
                            poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + y * scaleY));
                            poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + y * scaleY));
                            layer->addPrimitive(std::move(poly));
                            primCount++;
                        }
                        inSeg = false;
                    }
                }
                if (inSeg && (imgW - 1 - sx0) >= 3) {
                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(false); poly->setFilled(false);
                    poly->setColor(pencil); poly->setLineWidth(lw);
                    poly->setOpacityMultiplier(opa);
                    poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + y * scaleY));
                    poly->addPoint(QVector2D(worldPos.x() + (imgW - 1) * scaleX, worldPos.y() + y * scaleY));
                    layer->addPrimitive(std::move(poly));
                    primCount++;
                }
            }
        };

        // Hatching passes — graduated tone through multiple threshold layers
        // Pass 1: light hatching 45° (shadows begin)
        hatch45(sp, 210, 0.35, 0.45f);
        // Pass 2: medium hatching 45° (mid-tones)
        hatch45(sp, 160, 0.45, 0.55f, sp / 2);
        // Pass 3: cross-hatching 135° (darker areas)
        hatch135(sp, 140, 0.35, 0.5f);
        // Pass 4: dense cross-hatching 135° (deep shadows)
        hatch135(sp, 90, 0.45, 0.6f, sp / 2);
        // Pass 5: horizontal for extra density in very dark
        hatchH(sp, 70, 0.3, 0.4f);
        // Pass 6: extra 45° fill for near-black
        hatch45(qMax(1, sp - 1), 50, 0.5, 0.7f, sp / 3);

        // Edge contours for structural definition
        QJsonObject traceParams;
        traceParams["imageIndex"] = imageIndex;
        traceParams["threshold"] = 30;
        traceParams["simplify"] = 1.5;
        traceParams["minLength"] = 8;
        traceParams["render"] = true;
        traceParams["lineWidth"] = 0.7;
        traceParams["color"] = "#28231E";
        traceParams["colorMatch"] = false;
        traceParams["variableWidth"] = true;
        traceParams["gaussianBlur"] = true;
        traceParams["opacity"] = 0.75;
        traceParams["maxContours"] = 1500;
                    QJsonObject traceResult = autoTrace(traceParams);

        bool imageRemoved2 = false;
        if (removeImage) {
            for (const auto& ly : m_ctx.canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
            imageRemoved2 = true;
            m_ctx.canvas->update();
        }

        QJsonObject result;
        result["style"] = QString("sketch");
        result["hatchPrimitives"] = primCount;
        result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
        result["quality"] = quality;
        result["imageRemoved"] = imageRemoved2;
        return result;

    } else if (style == "painterly") {
        // ===================== PAINTERLY / IMPRESSIONIST STYLE =====================
        // Visible oriented brush strokes that follow the image's color contours
        QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();
        int imgW = srcImg.width();
        int imgH = srcImg.height();
        double scaleX = static_cast<double>(worldSize.x()) / imgW;
        double scaleY = static_cast<double>(worldSize.y()) / imgH;

        // Build grayscale for gradient direction
        QImage gray = srcImg.convertToFormat(QImage::Format_Grayscale8);
        // Compute Sobel gradient for stroke orientation
        std::vector<float> gxArr(imgW * imgH, 0.0f);
        std::vector<float> gyArr(imgW * imgH, 0.0f);
        for (int y = 1; y < imgH - 1; ++y) {
            const uchar* rA = gray.constScanLine(y - 1);
            const uchar* rC = gray.constScanLine(y);
            const uchar* rB = gray.constScanLine(y + 1);
            for (int x = 1; x < imgW - 1; ++x) {
                gxArr[y * imgW + x] = -1.0f * rA[x-1] + rA[x+1] - 2.0f * rC[x-1] + 2.0f * rC[x+1] - rB[x-1] + rB[x+1];
                gyArr[y * imgW + x] = -1.0f * rA[x-1] - 2.0f * rA[x] - rA[x+1] + rB[x-1] + 2.0f * rB[x] + rB[x+1];
            }
        }

        if (m_ctx.saveUndo) m_ctx.saveUndo("Render Painterly");
        Layer* layer = m_ctx.canvas->layerManager()->activeLayer();
        int strokeCount = 0;

        // Canvas background — warm off-white
        auto bg = std::make_unique<RectanglePrimitive>(
            QVector2D(worldPos.x(), worldPos.y()),
            QVector2D(worldPos.x() + worldSize.x(), worldPos.y() + worldSize.y()));
        bg->setFilled(true);
        bg->setFillColor(QColor(245, 242, 235));
        bg->setLineWidth(0);
        layer->addPrimitive(std::move(bg));

        // Multi-pass brush strokes: large → medium → small
        struct StrokePass {
            int step;       // pixel sampling step
            float major;    // major axis scale (world units factor of step*scaleX)
            float minor;    // minor axis scale
            float opacity;
        };

        std::vector<StrokePass> passes;
        if (quality == "low") {
            passes = {{12, 1.4f, 0.5f, 0.85f}, {6, 1.0f, 0.4f, 0.7f}};
        } else if (quality == "medium") {
            passes = {{8, 1.4f, 0.5f, 0.85f}, {4, 1.0f, 0.4f, 0.7f}, {2, 0.7f, 0.3f, 0.6f}};
        } else if (quality == "high") {
            passes = {{6, 1.3f, 0.45f, 0.85f}, {3, 1.0f, 0.35f, 0.75f}, {2, 0.7f, 0.25f, 0.6f}};
        } else { // ultra
            passes = {{8, 1.5f, 0.5f, 0.9f}, {4, 1.1f, 0.4f, 0.8f}, {2, 0.75f, 0.3f, 0.65f}};
        }

        for (const auto& pass : passes) {
            for (int iy = pass.step / 2; iy < imgH; iy += pass.step) {
                const QRgb* scanLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(iy));
                for (int ix = pass.step / 2; ix < imgW; ix += pass.step) {
                    QRgb pixel = scanLine[ix];
                    QColor color(qRed(pixel), qGreen(pixel), qBlue(pixel));

                    // Gradient direction → stroke perpendicular to edge (along the form)
                    float gx = gxArr[iy * imgW + ix];
                    float gy = gyArr[iy * imgW + ix];
                    float gmag = std::sqrt(gx * gx + gy * gy);
                    float angle;
                    if (gmag > 5.0f) {
                        angle = std::atan2(gy, gx) + static_cast<float>(M_PI) / 2.0f; // perpendicular
                    } else {
                        // In flat areas, use a pseudo-random angle based on position
                        angle = static_cast<float>((ix * 7 + iy * 13) % 628) / 100.0f;
                    }

                    float cos_a = std::cos(angle);
                    float sin_a = std::sin(angle);
                    float mx = pass.major * pass.step * static_cast<float>(scaleX);
                    float my = pass.minor * pass.step * static_cast<float>(scaleY);

                    double cx = worldPos.x() + ix * scaleX;
                    double cy = worldPos.y() + iy * scaleY;

                    // Rotated rectangle as 4-point filled polygon (brush stroke)
                    QVector2D p1(cx + mx * cos_a - my * sin_a, cy + mx * sin_a + my * cos_a);
                    QVector2D p2(cx - mx * cos_a - my * sin_a, cy - mx * sin_a + my * cos_a);
                    QVector2D p3(cx - mx * cos_a + my * sin_a, cy - mx * sin_a - my * cos_a);
                    QVector2D p4(cx + mx * cos_a + my * sin_a, cy + mx * sin_a - my * cos_a);

                    auto poly = std::make_unique<PolygonPrimitive>();
                    poly->setClosed(true);
                    poly->setFilled(true);
                    poly->setFillColor(color);
                    poly->setLineWidth(0);
                    poly->setOpacityMultiplier(pass.opacity);
                    poly->addPoint(p1);
                    poly->addPoint(p2);
                    poly->addPoint(p3);
                    poly->addPoint(p4);
                    layer->addPrimitive(std::move(poly));
                    strokeCount++;
                }
            }
        }

        // Subtle edge contours
        QJsonObject traceParams;
        traceParams["imageIndex"] = imageIndex;
        traceParams["threshold"] = 35;
        traceParams["simplify"] = 2.0;
        traceParams["minLength"] = 12;
        traceParams["render"] = true;
        traceParams["lineWidth"] = 0.6;
        traceParams["color"] = "auto";
        traceParams["colorMatch"] = true;
        traceParams["variableWidth"] = true;
        traceParams["gaussianBlur"] = true;
        traceParams["opacity"] = 0.4;
        traceParams["maxContours"] = 1000;
                    QJsonObject traceResult = autoTrace(traceParams);

        bool imageRemoved2 = false;
        if (removeImage) {
            for (const auto& ly : m_ctx.canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
            imageRemoved2 = true;
            m_ctx.canvas->update();
        }

        QJsonObject result;
        result["style"] = QString("painterly");
        result["strokeCount"] = strokeCount;
        result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
        result["quality"] = quality;
        result["imageRemoved"] = imageRemoved2;
        return result;

    } else {
        // ===================== PHOTOREALISTIC STYLE (original) =====================
        bool traceEdges = getBool(params, "traceEdges", true);
        bool detailOverlay = getBool(params, "detailOverlay", true);
        QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();

        QJsonObject mosaicParams;
        mosaicParams["imageIndex"] = imageIndex;
        mosaicParams["quality"] = quality;
        mosaicParams["mode"] = "adaptive";
                    QJsonObject mosaicResult = renderMosaic(mosaicParams);

        QJsonObject traceResult;
        if (traceEdges) {
            QJsonObject traceParams;
            traceParams["imageIndex"] = imageIndex;
            traceParams["threshold"] = 25;
            traceParams["simplify"] = 1.5;
            traceParams["minLength"] = 10;
            traceParams["render"] = true;
            traceParams["lineWidth"] = 0.8;
            traceParams["color"] = "auto";
            traceParams["colorMatch"] = true;
            traceParams["variableWidth"] = true;
            traceParams["gaussianBlur"] = true;
            traceParams["opacity"] = 0.6;
            traceParams["maxContours"] = 2000;
                            traceResult = autoTrace(traceParams);
        }

        int overlayCount = 0;
        if (detailOverlay) {
            Layer* layer = m_ctx.canvas->layerManager()->activeLayer();
            int imgW = srcImg.width();
            int imgH = srcImg.height();
            double scaleX = static_cast<double>(worldSize.x()) / imgW;
            double scaleY = static_cast<double>(worldSize.y()) / imgH;
            int sampleStep = 4;
            float strokeOpacity = 0.5f;
            if (quality == "low")         { sampleStep = 8;  strokeOpacity = 0.4f; }
            else if (quality == "medium")  { sampleStep = 4;  strokeOpacity = 0.5f; }
            else if (quality == "high")    { sampleStep = 3;  strokeOpacity = 0.55f; }
            else if (quality == "ultra")   { sampleStep = 2;  strokeOpacity = 0.6f; }
            float rx = static_cast<float>(sampleStep * scaleX * 0.7);
            float ry = static_cast<float>(sampleStep * scaleY * 0.7);
            for (int iy = sampleStep / 2; iy < imgH; iy += sampleStep) {
                const QRgb* scanLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(iy));
                for (int ix = sampleStep / 2; ix < imgW; ix += sampleStep) {
                    QRgb pixel = scanLine[ix];
                    QColor color(qRed(pixel), qGreen(pixel), qBlue(pixel));
                    double wx = worldPos.x() + ix * scaleX;
                    double wy = worldPos.y() + iy * scaleY;
                    auto ellipse = std::make_unique<EllipsePrimitive>(QVector2D(wx, wy), rx, ry);
                    ellipse->setFilled(true);
                    ellipse->setFillColor(color);
                    ellipse->setLineWidth(0);
                    ellipse->setOpacityMultiplier(strokeOpacity);
                    layer->addPrimitive(std::move(ellipse));
                    overlayCount++;
                }
            }
        }

        bool imageRemoved2 = false;
        if (removeImage) {
            for (const auto& ly : m_ctx.canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
            imageRemoved2 = true;
            m_ctx.canvas->update();
        }

        QJsonObject result;
        result["style"] = QString("photorealistic");
        result["mosaicPrimitives"] = mosaicResult["primitivesCreated"].toInt(0);
        result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
        result["detailOverlayCount"] = overlayCount;
        result["quality"] = quality;
        result["imageRemoved"] = imageRemoved2;
        return result;
    }

    return QJsonObject();
}


