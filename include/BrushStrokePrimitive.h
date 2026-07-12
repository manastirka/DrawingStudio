#pragma once

#include "DrawingPrimitive.h"
#include <vector>
#include <QVector2D>

class BrushStrokePrimitive : public DrawingPrimitive
{
public:
    enum class StrokeType {
        Brush,
        Blur
    };
    
    BrushStrokePrimitive();
    ~BrushStrokePrimitive() override = default;
    
    void render(QPainter* painter) const override;
    bool containsPoint(const QVector2D& point, float tolerance = 5.0f) const override;
    QRectF boundingRect() const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    
    void setPoints(const std::vector<QVector2D>& points) {
        m_points = points;
        m_particleScale.clear();
        m_particleAlpha.clear();
    }
    // Airbrush particles (optional per-particle scale/alpha for natural look)
    void setParticles(const std::vector<QVector2D>& points,
                      const std::vector<float>& scale,
                      const std::vector<float>& alpha) {
        m_points = points;
        m_particleScale = scale;
        m_particleAlpha = alpha;
    }
    const std::vector<QVector2D>& points() const { return m_points; }
    
    void setBrushSize(float size) { m_brushSize = size; }
    float brushSize() const { return m_brushSize; }
    
    void setHardness(float hardness) { m_hardness = hardness; }
    float hardness() const { return m_hardness; }
    
    void setStrokeType(StrokeType type) { m_strokeType = type; }
    StrokeType strokeType() const { return m_strokeType; }
    
private:
    std::vector<QVector2D> m_points;
    // Optional: for Brush (airbrush) strokes; empty => uniform particles.
    std::vector<float> m_particleScale;
    std::vector<float> m_particleAlpha;

    float m_brushSize;
    float m_hardness;
    StrokeType m_strokeType;
};
