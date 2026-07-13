#include "CanvasPaintPipeline.h"

#include "BrushStrokePrimitive.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QFont>
#include <QFontMetrics>
#include <cmath>

namespace CanvasPaintPipeline {
namespace {

void renderOne(QPainter &painter, DrawingPrimitive *primitive, float opacity)
{
    if (!primitive || !primitive->isVisible()) {
        return;
    }
    primitive->setOpacityMultiplier(opacity);

    const bool selfRotates =
        dynamic_cast<ImagePrimitive *>(primitive) != nullptr ||
        dynamic_cast<TextPrimitive *>(primitive) != nullptr;
    const float rot = primitive->rotationDegrees();
    if (!selfRotates && std::fabs(rot) > 0.01f) {
        const QRectF br = primitive->boundingRect();
        const QPointF c = br.center();
        painter.save();
        painter.translate(c);
        painter.rotate(static_cast<double>(rot));
        painter.translate(-c);
        primitive->render(&painter);
        painter.restore();
    } else {
        primitive->render(&painter);
    }
    primitive->setOpacityMultiplier(1.0f);
}

} // namespace

void clearBackground(QPainter &painter, const QRect &widgetRect,
                     const QColor &background)
{
    painter.fillRect(widgetRect, background);
}

void renderWorldObjects(
    QPainter &painter, LayerManager *layers, const PrimitiveList &legacy,
    DrawingPrimitive *currentPrimitive,
    const std::function<std::vector<DrawingPrimitive *>()> &selectedObjects,
    const std::function<bool(const DrawingPrimitive *)> &showsGeometryCPs,
    const std::function<void(QPainter &, const QVector2D &, bool)> &
        drawControlPoint)
{
    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (layer && layer->isVisible()) {
                const float opacity = layer->opacity();
                for (const auto &primitive : layer->primitives()) {
                    renderOne(painter, primitive.get(), opacity);
                }
            }
        }
    } else {
        for (const auto &primitive : legacy) {
            renderOne(painter, primitive.get(), 1.0f);
        }
    }

    if (currentPrimitive) {
        currentPrimitive->render(&painter);
    }

    if (!selectedObjects || !showsGeometryCPs || !drawControlPoint) {
        return;
    }
    for (auto *selectedObj : selectedObjects()) {
        if (!selectedObj || !selectedObj->isSelected()) {
            continue;
        }
        if (!showsGeometryCPs(selectedObj)) {
            continue;
        }
        for (const auto &point : selectedObj->getControlPoints()) {
            drawControlPoint(painter, point, false);
        }
    }
}

void drawRotationAngleHud(QPainter &painter, const QPoint &screenCenter,
                          float rotationDegrees)
{
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QString angleText =
        QStringLiteral("%1°").arg(rotationDegrees, 0, 'f', 1);
    QFont font(QStringLiteral("Arial"), 12, QFont::Bold);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    QRect textRect = metrics.boundingRect(angleText);
    QRect bgRect = textRect;
    bgRect.moveCenter(screenCenter);
    bgRect.adjust(-5, -3, 5, 3);
    painter.fillRect(bgRect, QColor(0, 0, 0, 180));
    painter.setPen(QColor(100, 255, 100));
    painter.drawText(bgRect, Qt::AlignCenter, angleText);
}

void renderBrushStrokePreview(QPainter &painter,
                              const std::vector<QVector2D> &points,
                              const std::vector<float> &particleScale,
                              const std::vector<float> &particleAlpha,
                              float brushSize, float hardness,
                              const QColor &color, bool isBlur)
{
    if (points.empty()) {
        return;
    }

    BrushStrokePrimitive stroke;
    if (!particleScale.empty() && particleScale.size() == points.size() &&
        particleAlpha.size() == points.size()) {
        stroke.setParticles(points, particleScale, particleAlpha);
    } else {
        stroke.setPoints(points);
    }
    stroke.setBrushSize(brushSize);
    stroke.setHardness(hardness);
    stroke.setColor(color);
    stroke.setStrokeType(isBlur ? BrushStrokePrimitive::StrokeType::Blur
                                : BrushStrokePrimitive::StrokeType::Brush);
    stroke.render(&painter);
}

void renderAngleBaseline(QPainter &painter, const QVector2D &start,
                         const QVector2D &end, float zoomLevel)
{
    QPen basePen(QColor(59, 130, 246, 180), 1.25, Qt::DashLine);
    basePen.setCosmetic(true);
    painter.setPen(basePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(start.toPointF(), end.toPointF());
    painter.setBrush(QColor(59, 130, 246));
    painter.setPen(Qt::NoPen);
    const float z = (zoomLevel > 1e-6f) ? zoomLevel : 1.0f;
    const float r = 3.0f / z;
    painter.drawEllipse(end.toPointF(), r, r);
}

void renderDrawingPrimitivePreview(QPainter &painter,
                                   DrawingPrimitive *primitive,
                                   const QColor &color)
{
    if (!primitive) {
        return;
    }
    primitive->setColor(color);
    primitive->render(&painter);
}

} // namespace CanvasPaintPipeline
