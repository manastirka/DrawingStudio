#include "BrushStrokeOps.h"

#include "BrushStrokePrimitive.h"

namespace BrushStrokeOps {

std::unique_ptr<DrawingPrimitive> create(
    const std::vector<QVector2D> &points,
    const std::vector<float> &particleScale,
    const std::vector<float> &particleAlpha, float brushSize, float hardness,
    const QColor &color, bool isBlur, bool useParticles)
{
    if (points.size() < 2) {
        return nullptr;
    }

    auto stroke = std::make_unique<BrushStrokePrimitive>();
    if (!isBlur && useParticles && !particleScale.empty() &&
        particleScale.size() == points.size() &&
        particleAlpha.size() == points.size()) {
        stroke->setParticles(points, particleScale, particleAlpha);
    } else {
        stroke->setPoints(points);
    }
    stroke->setBrushSize(brushSize);
    stroke->setHardness(hardness);
    stroke->setColor(color);
    stroke->setStrokeType(isBlur ? BrushStrokePrimitive::StrokeType::Blur
                                 : BrushStrokePrimitive::StrokeType::Brush);
    return stroke;
}

void clearBuffers(std::vector<QVector2D> *points,
                  std::vector<float> *particleScale,
                  std::vector<float> *particleAlpha)
{
    if (points) {
        points->clear();
    }
    if (particleScale) {
        particleScale->clear();
    }
    if (particleAlpha) {
        particleAlpha->clear();
    }
}

} // namespace BrushStrokeOps
