#include "DrawingCommandDispatcher.h"

#include "DrawingPrimitive.h"
#include "DXFExporter.h"
#include "EdgeSelectionTool.h"
#include "ImagePrimitive.h"
#include "ImageToDrawingEngine.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QVector2D>

#include <memory>
#include <vector>

// Image sample / mask / analyze commands (refactor E29).

bool DrawingCommandDispatcher::tryExecuteImage(const QString &action, const QJsonObject &params)
{
    if (action == "sample_color") {
    int imageIndex = params["imageIndex"].toInt(0);
    int pixelX = params["pixelX"].toInt(0);
    int pixelY = params["pixelY"].toInt(0);
    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "sample_color: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image();
        if (pixelX < 0 || pixelX >= img.width() || pixelY < 0 || pixelY >= img.height()) {
            qWarning() << "sample_color: pixel out of bounds";
        } else {
            QColor c = img.pixelColor(pixelX, pixelY);
            QJsonObject result;
            result["color"] = c.name(QColor::HexArgb);
            result["r"] = c.red();
            result["g"] = c.green();
            result["b"] = c.blue();
            result["a"] = c.alpha();
            m_lastResult = result;
        }
    }
        return true;
    }

    if (action == "sample_colors_grid") {
    int imageIndex = params["imageIndex"].toInt(0);
    int gridX = params["gridX"].toInt(10);
    int gridY = params["gridY"].toInt(10);
    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "sample_colors_grid: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image();
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();

        double cellW = static_cast<double>(img.width()) / gridX;
        double cellH = static_cast<double>(img.height()) / gridY;
        double worldCellW = worldSize.x() / gridX;
        double worldCellH = worldSize.y() / gridY;

        QJsonArray samples;
        for (int row = 0; row < gridY; ++row) {
            for (int col = 0; col < gridX; ++col) {
                int startX = static_cast<int>(col * cellW);
                int startY = static_cast<int>(row * cellH);
                int endX = static_cast<int>((col + 1) * cellW);
                int endY = static_cast<int>((row + 1) * cellH);
                endX = qMin(endX, img.width());
                endY = qMin(endY, img.height());

                // Average color with subsampling for large cells
                long rSum = 0, gSum = 0, bSum = 0;
                int count = 0;
                int step = qMax(1, qMin((endX - startX), (endY - startY)) / 8);
                for (int py = startY; py < endY; py += step) {
                    for (int px = startX; px < endX; px += step) {
                        QColor c = img.pixelColor(px, py);
                        rSum += c.red();
                        gSum += c.green();
                        bSum += c.blue();
                        count++;
                    }
                }
                QColor avg(count > 0 ? rSum / count : 0,
                           count > 0 ? gSum / count : 0,
                           count > 0 ? bSum / count : 0);

                // World-space rectangle for this cell
                double wx1 = worldPos.x() + col * worldCellW;
                double wy1 = worldPos.y() + row * worldCellH;
                double wx2 = wx1 + worldCellW;
                double wy2 = wy1 + worldCellH;

                QJsonObject sample;
                sample["row"] = row;
                sample["col"] = col;
                sample["avgColor"] = avg.name();
                sample["pixelX"] = (startX + endX) / 2;
                sample["pixelY"] = (startY + endY) / 2;
                sample["x1"] = wx1;
                sample["y1"] = wy1;
                sample["x2"] = wx2;
                sample["y2"] = wy2;
                samples.append(sample);
            }
        }

        QJsonObject result;
        result["samples"] = samples;
        result["gridX"] = gridX;
        result["gridY"] = gridY;
        result["imageWidth"] = img.width();
        result["imageHeight"] = img.height();
        m_lastResult = result;
    }
        return true;
    }

    if (action == "detect_subjects") {
    int imageIndex = params["imageIndex"].toInt(0);
    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "detect_subjects: image not found at index" << imageIndex;
    } else {
        selectImageForMaskUI(imgPrim);
        imgPrim->startSubjectDetection();
        if (m_ctx.setStatusText) m_ctx.setStatusText("Detecting subjects…");
    }
        return true;
    }

    if (action == "get_mask_info") {
    int imageIndex = params["imageIndex"].toInt(0);
    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (imgPrim) {
        QJsonObject result;
        result["candidateCount"] = imgPrim->getMaskCandidateCount();
        result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
        const auto *c = imgPrim->getSelectedCandidate();
        if (c) {
            result["score"] = c->score;
            result["areaPercent"] = c->area_percent;
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "next_mask") {
    if (m_ctx.selectNextMask) m_ctx.selectNextMask();
    ImagePrimitive* imgPrim = selectedImageWithMasks();
    if (imgPrim) {
        QJsonObject result;
        result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
        result["candidateCount"] = imgPrim->getMaskCandidateCount();
        m_lastResult = result;
    }
        return true;
    }

    if (action == "prev_mask") {
    if (m_ctx.selectPreviousMask) m_ctx.selectPreviousMask();
    ImagePrimitive* imgPrim = selectedImageWithMasks();
    if (imgPrim) {
        QJsonObject result;
        result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
        result["candidateCount"] = imgPrim->getMaskCandidateCount();
        m_lastResult = result;
    }
        return true;
    }

    if (action == "invert_mask") {
    if (m_ctx.invertSelectedMask) m_ctx.invertSelectedMask();
    ImagePrimitive* imgPrim = selectedImageWithMasks();
    if (imgPrim) {
        QJsonObject result;
        result["inverted"] = imgPrim->isMaskInverted();
        m_lastResult = result;
    }
        return true;
    }

    if (action == "detect_edges") {
    int imageIndex = params["imageIndex"].toInt(0);
    int seedX = params["seedX"].toInt(0);
    int seedY = params["seedY"].toInt(0);
    float tolerance = static_cast<float>(getDouble(params, "tolerance", 30.0));

    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "detect_edges: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image();
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();

        EdgeSelectionTool edgeTool;
        // tolerance param is 0-255; EdgeSelectionTool uses 0-1 normalized
        edgeTool.setGrowthTolerance(tolerance / 255.0f);
        auto selResult = edgeTool.selectByEdges(img, QPointF(seedX, seedY));

        QJsonObject result;
        result["success"] = selResult.success;
        result["confidence"] = static_cast<double>(selResult.confidence);
        result["pointCount"] = static_cast<int>(selResult.contour.size());

        if (selResult.success && !selResult.contour.empty()) {
            // Scale factors: pixel → world
            double scaleX = worldSize.x() / img.width();
            double scaleY = worldSize.y() / img.height();

            QJsonArray contourPixels;
            QJsonArray contourWorld;
            for (const auto& pt : selResult.contour) {
                QJsonObject pixelPt;
                pixelPt["x"] = pt.x();
                pixelPt["y"] = pt.y();
                contourPixels.append(pixelPt);

                QJsonObject worldPt;
                worldPt["x"] = worldPos.x() + pt.x() * scaleX;
                worldPt["y"] = worldPos.y() + pt.y() * scaleY;
                contourWorld.append(worldPt);
            }
            result["contourPixels"] = contourPixels;
            result["contourWorld"] = contourWorld;
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "analyze_regions") {
    int imageIndex = params["imageIndex"].toInt(0);
    int rows = params["rows"].toInt(10);
    int cols = params["cols"].toInt(10);
    ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
    if (!imgPrim) {
        qWarning() << "analyze_regions: image not found at index" << imageIndex;
    } else {
        QImage img = imgPrim->image();
        QVector2D worldPos = imgPrim->position();
        QVector2D worldSize = imgPrim->size();

        double cellW = static_cast<double>(img.width()) / cols;
        double cellH = static_cast<double>(img.height()) / rows;
        double worldCellW = worldSize.x() / cols;
        double worldCellH = worldSize.y() / rows;

        QJsonArray regions;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                int startX = static_cast<int>(col * cellW);
                int startY = static_cast<int>(row * cellH);
                int endX = static_cast<int>((col + 1) * cellW);
                int endY = static_cast<int>((row + 1) * cellH);
                endX = qMin(endX, img.width());
                endY = qMin(endY, img.height());

                long rSum = 0, gSum = 0, bSum = 0;
                int minBright = 255, maxBright = 0;
                int count = 0;
                int step = qMax(1, qMin((endX - startX), (endY - startY)) / 8);
                for (int py = startY; py < endY; py += step) {
                    for (int px = startX; px < endX; px += step) {
                        QColor c = img.pixelColor(px, py);
                        rSum += c.red();
                        gSum += c.green();
                        bSum += c.blue();
                        int brightness = (c.red() + c.green() + c.blue()) / 3;
                        minBright = qMin(minBright, brightness);
                        maxBright = qMax(maxBright, brightness);
                        count++;
                    }
                }
                QColor avg(count > 0 ? rSum / count : 0,
                           count > 0 ? gSum / count : 0,
                           count > 0 ? bSum / count : 0);
                int contrast = maxBright - minBright;

                double wx1 = worldPos.x() + col * worldCellW;
                double wy1 = worldPos.y() + row * worldCellH;
                double wx2 = wx1 + worldCellW;
                double wy2 = wy1 + worldCellH;

                QJsonObject region;
                region["row"] = row;
                region["col"] = col;
                region["avgColor"] = avg.name();
                region["contrast"] = contrast;
                region["x1"] = wx1;
                region["y1"] = wy1;
                region["x2"] = wx2;
                region["y2"] = wy2;
                regions.append(region);
            }
        }

        QJsonObject result;
        result["regions"] = regions;
        result["rows"] = rows;
        result["cols"] = cols;
        m_lastResult = result;
    }
        return true;
    }

    if (action == "delete_primitive") {
    int index = params["index"].toInt(-1);
    auto allPrims = m_ctx.canvas->layerManager()->getAllPrimitives();
    int totalCount = static_cast<int>(allPrims.size());

    if (totalCount == 0) {
        qWarning() << "delete_primitive: no primitives on canvas";
    } else {
        // Resolve index (-1 = last)
        int resolvedIndex = (index < 0) ? totalCount + index : index;
        if (resolvedIndex < 0 || resolvedIndex >= totalCount) {
            qWarning() << "delete_primitive: index out of range:" << index;
        } else {
            DrawingPrimitive* target = allPrims[resolvedIndex];
            // Search all layers and remove the primitive
            for (const auto& layer : m_ctx.canvas->layerManager()->layers()) {
                layer->removePrimitive(target);
            }
            m_ctx.canvas->update();
        }
    }
        return true;
    }

    return false;
}
