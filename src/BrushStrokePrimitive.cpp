#include "BrushStrokePrimitive.h"
#include <QRadialGradient>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <cstddef>

BrushStrokePrimitive::BrushStrokePrimitive()
    : DrawingPrimitive()
    , m_brushSize(10.0f)
    , m_hardness(0.5f)
    , m_strokeType(StrokeType::Brush)
{
}

void BrushStrokePrimitive::render(QPainter* painter) const
{
    if (m_points.empty() || !m_visible) return;

    if (m_strokeType == StrokeType::Blur) {
        int blurLayers = 5;
        for (int layer = 0; layer < blurLayers; layer++) {
            float layerAlpha = 0.15f * (1.0f - layer / (float)blurLayers) * (1.0f - m_hardness) * m_opacityMultiplier;
            float offset = layer * 0.5f;
            QColor blurColor(255, 255, 255, qBound(0, (int)(layerAlpha * 255), 255));
            painter->setPen(Qt::NoPen);
            painter->setBrush(blurColor);
            float radius = (m_brushSize * m_lineWidth * 0.5f) + offset;
            for (const auto& point : m_points) {
                painter->drawEllipse(QPointF(point.x(), point.y()), radius, radius);
            }
        }
        // Subtle gray overlay line
        QColor overlayColor(230, 230, 242, 51);
        float strokeWidth = m_brushSize * m_lineWidth * 0.8f;
        QPen overlayPen(overlayColor, strokeWidth);
        overlayPen.setCosmetic(false);
        overlayPen.setCapStyle(Qt::RoundCap);
        overlayPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(overlayPen);
        painter->setBrush(Qt::NoBrush);
        if (m_points.size() >= 2) {
            QPolygonF polyline;
            for (const auto& p : m_points) polyline << QPointF(p.x(), p.y());
            painter->drawPolyline(polyline);
        }
    } else {
        // Brush effect: Airbrush / spray paint.
        // The stroke stores many particle points (not a path). Accumulation is
        // achieved by dense particle stamping while the mouse is held.
        QColor renderColor = m_color;

        // Particle size/alpha tuned for "spray" feel.
        // Softer airbrush: larger, lower-alpha particles that build up smoothly.
        float baseRadius = std::max(1.2f, (m_brushSize * m_lineWidth) * 0.22f);
        float baseAlpha = std::clamp(0.028f + (1.0f - m_hardness) * 0.020f,
                                     0.018f, 0.060f) *
                          m_opacityMultiplier;

        bool hasPerParticle =
            (!m_particleScale.empty() && m_particleScale.size() == m_points.size() &&
             !m_particleAlpha.empty() && m_particleAlpha.size() == m_points.size());

        painter->setPen(Qt::NoPen);
        for (size_t idx = 0; idx < m_points.size(); ++idx) {
            const auto& point = m_points[idx];
            float s = hasPerParticle ? m_particleScale[idx] : 1.0f;
            float a = hasPerParticle ? m_particleAlpha[idx] : 1.0f;
            float r = baseRadius * s;
            float centerA = renderColor.alphaF() * baseAlpha * a;

            QRadialGradient gradient(QPointF(point.x(), point.y()), r);
            QColor centerColor = renderColor;
            centerColor.setAlphaF(centerA);
            QColor edgeColor = renderColor;
            edgeColor.setAlphaF(0.0f);
            gradient.setColorAt(0.0, centerColor);
            gradient.setColorAt(1.0, edgeColor);

            painter->setBrush(gradient);
            painter->drawEllipse(QPointF(point.x(), point.y()), r, r);
        }
    }
}

bool BrushStrokePrimitive::containsPoint(const QVector2D& point, float tolerance) const
{
    if (m_points.empty()) return false;

    float checkRadius = m_brushSize * 0.5f + tolerance;

    // Check if point is near any stroke point
    for (const auto& strokePoint : m_points) {
        if ((strokePoint - point).length() <= checkRadius) {
            return true;
        }
    }

    return false;
}

QRectF BrushStrokePrimitive::boundingRect() const
{
    if (m_points.empty()) return QRectF();

    float minX = m_points[0].x();
    float maxX = m_points[0].x();
    float minY = m_points[0].y();
    float maxY = m_points[0].y();

    for (const auto& point : m_points) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    float maxScale = 1.0f;
    if (!m_particleScale.empty()) {
        for (float s : m_particleScale) maxScale = std::max(maxScale, s);
    }

    float margin = m_brushSize * 0.5f * maxScale;
    return QRectF(minX - margin, minY - margin,
                  maxX - minX + 2 * margin, maxY - minY + 2 * margin);
}

std::unique_ptr<DrawingPrimitive> BrushStrokePrimitive::clone() const
{
    auto cloned = std::make_unique<BrushStrokePrimitive>();
    if (!m_particleScale.empty() && m_particleScale.size() == m_points.size() &&
        !m_particleAlpha.empty() && m_particleAlpha.size() == m_points.size()) {
        cloned->setParticles(m_points, m_particleScale, m_particleAlpha);
    } else {
        cloned->setPoints(m_points);
    }
    cloned->setBrushSize(m_brushSize);
    cloned->setHardness(m_hardness);
    cloned->setStrokeType(m_strokeType);
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
}

void BrushStrokePrimitive::translate(const QVector2D& offset)
{
    for (auto& point : m_points) {
        point += offset;
    }
}
