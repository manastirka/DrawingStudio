#pragma once

#include <QColor>
#include <QVector2D>
#include <memory>
#include <vector>

class DrawingPrimitive;

/**
 * Shared brush/blur stroke construction (refactor E6).
 * Used by BrushTool, BlurTool, and DrawingCanvas::setCurrentTool cleanup.
 */
namespace BrushStrokeOps {

/**
 * Build a committed stroke primitive from live buffers.
 * @return null if @p points has fewer than 2 samples
 * @param useParticles  when true and scale/alpha match points, setParticles;
 *                      otherwise setPoints (blur always uses points)
 */
std::unique_ptr<DrawingPrimitive> create(
    const std::vector<QVector2D> &points,
    const std::vector<float> &particleScale,
    const std::vector<float> &particleAlpha, float brushSize, float hardness,
    const QColor &color, bool isBlur, bool useParticles);

/** Clear live stroke + particle buffers (null pointers skipped). */
void clearBuffers(std::vector<QVector2D> *points,
                  std::vector<float> *particleScale,
                  std::vector<float> *particleAlpha);

} // namespace BrushStrokeOps
