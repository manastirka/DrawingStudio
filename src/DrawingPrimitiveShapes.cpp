#include "DrawingPrimitive.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QPolygonF>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QDebug>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <vector>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
QPolygonF toQPolygonF(const std::vector<QVector2D>& pts) {
    QPolygonF poly;
    poly.reserve(static_cast<int>(pts.size()));
    for (const auto& p : pts)
        poly << QPointF(p.x(), p.y());
    return poly;
}
} // namespace
// Basic shape primitives (refactor E21).

// --- LinePrimitive ---

LinePrimitive::LinePrimitive(const QVector2D &start, const QVector2D &end)
    : DrawingPrimitive(PrimitiveType::Line)
    , m_start(start)
    , m_end(end)
{
}

void LinePrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(15, (int)(m_shadowBlur * 0.5f) + 3));
        float baseAlpha = m_shadowColor.alphaF() * m_opacityMultiplier;

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor;
            sc.setAlphaF(layerAlpha);
            QPen pen(sc);
            pen.setWidthF((m_lineWidth + expansion) * (m_selected ? 2.0f : 1.0f));
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(QPointF(m_start.x(), m_start.y()), QPointF(m_end.x(), m_end.y()));
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    strokeColor.setAlphaF(m_opacityMultiplier);
    QPen pen(strokeColor);
    pen.setWidthF(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(m_start.x(), m_start.y()), QPointF(m_end.x(), m_end.y()));
}

QRectF LinePrimitive::boundingRect() const
{
    float minX = std::min(m_start.x(), m_end.x());
    float minY = std::min(m_start.y(), m_end.y());
    float maxX = std::max(m_start.x(), m_end.x());
    float maxY = std::max(m_start.y(), m_end.y());
    
    // Add some padding for line width
    float padding = m_lineWidth * 2.0f;
    return QRectF(minX - padding, minY - padding, 
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool LinePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    // Distance from point to line segment
    QVector2D lineVec = m_end - m_start;
    QVector2D pointVec = point - m_start;
    
    float lineLength = lineVec.length();
    if (lineLength < 0.001f) {
        return (point - m_start).length() <= tolerance;
    }
    
    float t = QVector2D::dotProduct(pointVec, lineVec) / (lineLength * lineLength);
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to line segment
    
    QVector2D closestPoint = m_start + t * lineVec;
    float distance = (point - closestPoint).length();
    
    return distance <= tolerance;
}

std::unique_ptr<DrawingPrimitive> LinePrimitive::clone() const
{
    auto cloned = std::make_unique<LinePrimitive>(m_start, m_end);
    applyCommonPropertiesTo(cloned.get());
    cloned->setConnectedLineId(m_connectedLineId);
    cloned->setMoveConstraintDirection(m_moveConstraintDirection);
    return cloned;
}

void LinePrimitive::translate(const QVector2D& offset)
{
    m_start += offset;
    m_end += offset;
}

QJsonObject LinePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["startX"] = m_start.x();
    json["startY"] = m_start.y();
    json["endX"] = m_end.x();
    json["endY"] = m_end.y();
    if (!m_connectedLineId.isNull())
        json["connectedLineId"] = m_connectedLineId.toString();
    if (m_moveConstraintDirection.lengthSquared() > 1e-8f) {
        json["constraintDirX"] = m_moveConstraintDirection.x();
        json["constraintDirY"] = m_moveConstraintDirection.y();
    }
    return json;
}

void LinePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_start = QVector2D(json["startX"].toDouble(), json["startY"].toDouble());
    m_end = QVector2D(json["endX"].toDouble(), json["endY"].toDouble());
    if (json.contains(QStringLiteral("connectedLineId")))
        m_connectedLineId = QUuid(json["connectedLineId"].toString());
    else
        m_connectedLineId = QUuid();
    if (json.contains(QStringLiteral("constraintDirX"))) {
        m_moveConstraintDirection = QVector2D(
            json["constraintDirX"].toDouble(),
            json["constraintDirY"].toDouble());
    } else {
        m_moveConstraintDirection = QVector2D();
    }
}

std::vector<QVector2D> LinePrimitive::getControlPoints() const
{
    return {m_start, m_end};
}

void LinePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index == 0) {
        m_start = position;
    } else if (index == 1) {
        m_end = position;
    }
}

// --- RectanglePrimitive ---

RectanglePrimitive::RectanglePrimitive(const QVector2D &topLeft, const QVector2D &bottomRight)
    : DrawingPrimitive(PrimitiveType::Rectangle)
    , m_topLeft(topLeft)
    , m_bottomRight(bottomRight)
    , m_filled(false)
{
}

void RectanglePrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    float left = m_topLeft.x();
    float top = m_topLeft.y();
    float right = m_bottomRight.x();
    float bottom = m_bottomRight.y();
    QRectF rect(left, top, right - left, bottom - top);

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(25, (int)(m_shadowBlur * 0.8f) + 3));
        float baseAlpha = m_shadowColor.alphaF() * m_opacityMultiplier;

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float expansion = t * m_shadowBlur;
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;

            QColor sc = m_shadowColor;
            sc.setAlphaF(layerAlpha);
            painter->setPen(Qt::NoPen);
            painter->setBrush(sc);
            QRectF shadowRect(left - expansion, top - expansion,
                              (right - left) + 2 * expansion, (bottom - top) + 2 * expansion);
            painter->drawRect(shadowRect);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    const QColor fillColorValue = m_hasFillColor ? m_fillColor : strokeColor;
    QColor sc = strokeColor; sc.setAlphaF(m_opacityMultiplier);
    QColor fc = fillColorValue; fc.setAlphaF(m_opacityMultiplier);

    QPen pen(sc);
    pen.setWidthF(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    QBrush fillBrush = (m_gradientFillType != GradientFillType::None)
        ? createGradientBrush(rect) : QBrush(fc);

    if (m_cornerRadius > 0.0f) {
        if (m_filled) {
            painter->setPen(m_lineWidth > 0.0f ? pen : QPen(Qt::NoPen));
            painter->setBrush(fillBrush);
        } else {
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
        }
        renderRoundedRectangle(painter, left, top, right, bottom, m_cornerRadius, m_filled);
        painter->setBrush(Qt::NoBrush);
        return;
    }

    if (m_filled) {
        painter->setPen(m_lineWidth > 0.0f ? pen : QPen(Qt::NoPen));
        painter->setBrush(fillBrush);
        painter->drawRect(rect);
    } else {
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);
    }
    painter->setBrush(Qt::NoBrush);
}

void RectanglePrimitive::renderRoundedRectangle(QPainter* painter, float left, float top, float right, float bottom, float radius, bool filled) const
{
    float width = right - left;
    float height = bottom - top;
    float maxRadius = std::min(width, height) * 0.5f;
    radius = std::min(radius, maxRadius);
    QRectF rect(left, top, width, height);

    if (radius < 0.1f) {
        painter->drawRect(rect);
        return;
    }

    painter->drawRoundedRect(rect, radius, radius);
}

std::unique_ptr<DrawingPrimitive> RectanglePrimitive::clone() const
{
    auto cloned = std::make_unique<RectanglePrimitive>(m_topLeft, m_bottomRight);
    applyCommonPropertiesTo(cloned.get());
    cloned->setFilled(m_filled);
    cloned->setCornerRadius(m_cornerRadius);
    cloned->setMaintainAspectRatio(m_maintainAspectRatio);
    cloned->setCenterOnResize(m_centerOnResize);
    return cloned;
}

void RectanglePrimitive::translate(const QVector2D& offset)
{
    m_topLeft += offset;
    m_bottomRight += offset;
}

QJsonObject RectanglePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["topLeftX"] = m_topLeft.x();
    json["topLeftY"] = m_topLeft.y();
    json["bottomRightX"] = m_bottomRight.x();
    json["bottomRightY"] = m_bottomRight.y();
    json["filled"] = m_filled;
    json["cornerRadius"] = static_cast<double>(m_cornerRadius);
    json["maintainAspectRatio"] = m_maintainAspectRatio;
    json["centerOnResize"] = m_centerOnResize;
    return json;
}

void RectanglePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_topLeft = QVector2D(json["topLeftX"].toDouble(), json["topLeftY"].toDouble());
    m_bottomRight = QVector2D(json["bottomRightX"].toDouble(), json["bottomRightY"].toDouble());
    m_filled = json["filled"].toBool();
    m_cornerRadius = static_cast<float>(json["cornerRadius"].toDouble(m_cornerRadius));
    m_maintainAspectRatio = json["maintainAspectRatio"].toBool(m_maintainAspectRatio);
    m_centerOnResize = json["centerOnResize"].toBool(m_centerOnResize);
}

QRectF RectanglePrimitive::boundingRect() const
{
    float minX = std::min(m_topLeft.x(), m_bottomRight.x());
    float minY = std::min(m_topLeft.y(), m_bottomRight.y());
    float maxX = std::max(m_topLeft.x(), m_bottomRight.x());
    float maxY = std::max(m_topLeft.y(), m_bottomRight.y());

    float padding = m_lineWidth * 2.0f;
    return QRectF(minX - padding, minY - padding,
                  (maxX - minX) + 2 * padding, (maxY - minY) + 2 * padding);
}

bool RectanglePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    float minX = std::min(m_topLeft.x(), m_bottomRight.x());
    float minY = std::min(m_topLeft.y(), m_bottomRight.y());
    float maxX = std::max(m_topLeft.x(), m_bottomRight.x());
    float maxY = std::max(m_topLeft.y(), m_bottomRight.y());

    if (m_filled) {
        return point.x() >= minX && point.x() <= maxX &&
               point.y() >= minY && point.y() <= maxY;
    }

    bool nearLeft = std::abs(point.x() - minX) <= tolerance && point.y() >= minY && point.y() <= maxY;
    bool nearRight = std::abs(point.x() - maxX) <= tolerance && point.y() >= minY && point.y() <= maxY;
    bool nearTop = std::abs(point.y() - minY) <= tolerance && point.x() >= minX && point.x() <= maxX;
    bool nearBottom = std::abs(point.y() - maxY) <= tolerance && point.x() >= minX && point.x() <= maxX;

    return nearLeft || nearRight || nearTop || nearBottom;
}

std::vector<QVector2D> RectanglePrimitive::getControlPoints() const
{
    QVector2D topRight(m_bottomRight.x(), m_topLeft.y());
    QVector2D bottomLeft(m_topLeft.x(), m_bottomRight.y());
    return {m_topLeft, topRight, m_bottomRight, bottomLeft};
}

void RectanglePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    switch (index) {
        case 0: // Top-left
            m_topLeft = position;
            break;
        case 1: // Top-right
            m_topLeft.setY(position.y());
            m_bottomRight.setX(position.x());
            break;
        case 2: // Bottom-right
            m_bottomRight = position;
            break;
        case 3: // Bottom-left
            m_topLeft.setX(position.x());
            m_bottomRight.setY(position.y());
            break;
    }
}

// --- EllipsePrimitive ---

EllipsePrimitive::EllipsePrimitive(const QVector2D &center, float radiusX, float radiusY)
    : DrawingPrimitive(PrimitiveType::Ellipse)
    , m_center(center)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
    , m_filled(false)
{
}

void EllipsePrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    QRectF ellipseRect(m_center.x() - m_radiusX, m_center.y() - m_radiusY,
                       m_radiusX * 2, m_radiusY * 2);

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(25, (int)(m_shadowBlur * 0.8f) + 3));
        float baseAlpha = m_shadowColor.alphaF() * m_opacityMultiplier;

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor;
            sc.setAlphaF(layerAlpha);
            painter->setPen(Qt::NoPen);
            painter->setBrush(sc);
            QRectF shadowRect(m_center.x() - m_radiusX - expansion,
                              m_center.y() - m_radiusY - expansion,
                              (m_radiusX + expansion) * 2, (m_radiusY + expansion) * 2);
            painter->drawEllipse(shadowRect);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    const QColor fillColorValue = m_hasFillColor ? m_fillColor : strokeColor;

    QPen pen(strokeColor);
    pen.setWidthF(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    QBrush eFillBrush = (m_gradientFillType != GradientFillType::None)
        ? createGradientBrush(ellipseRect) : QBrush(fillColorValue);

    if (m_filled) {
        painter->setPen(m_lineWidth > 0.0f ? pen : QPen(Qt::NoPen));
        painter->setBrush(eFillBrush);
        painter->drawEllipse(ellipseRect);
    } else {
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(ellipseRect);
    }

    // Draw axes if enabled
    if (m_showAxes) {
        QPen axisPen(QColor(179, 179, 179), 1);
        painter->setPen(axisPen);
        painter->drawLine(QPointF(m_center.x() - m_radiusX, m_center.y()),
                          QPointF(m_center.x() + m_radiusX, m_center.y()));
        painter->drawLine(QPointF(m_center.x(), m_center.y() - m_radiusY),
                          QPointF(m_center.x(), m_center.y() + m_radiusY));

        // Center point
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(128, 128, 128));
        painter->drawEllipse(QPointF(m_center.x(), m_center.y()), 2.0, 2.0);
    }
    painter->setBrush(Qt::NoBrush);
}

QRectF EllipsePrimitive::boundingRect() const
{
    float padding = m_lineWidth * 2.0f;
    return QRectF(m_center.x() - m_radiusX - padding, 
                  m_center.y() - m_radiusY - padding,
                  2 * (m_radiusX + padding), 
                  2 * (m_radiusY + padding));
}

bool EllipsePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    // Normalize point to unit circle
    float dx = (point.x() - m_center.x()) / m_radiusX;
    float dy = (point.y() - m_center.y()) / m_radiusY;
    float distanceFromCenter = sqrt(dx*dx + dy*dy);
    
    if (m_filled) {
        return distanceFromCenter <= 1.0f;
    } else {
        // More generous tolerance for ellipse border
        float normalizedTolerance = tolerance / std::max(m_radiusX, m_radiusY);
        return fabs(distanceFromCenter - 1.0f) <= std::max(0.1f, normalizedTolerance);
    }
}

std::unique_ptr<DrawingPrimitive> EllipsePrimitive::clone() const
{
    auto cloned = std::make_unique<EllipsePrimitive>(m_center, m_radiusX, m_radiusY);
    applyCommonPropertiesTo(cloned.get());
    cloned->setFilled(m_filled);
    cloned->setSubdivisions(m_subdivisions);
    cloned->setShowAxes(m_showAxes);
    cloned->setLockAspectRatio(m_lockAspectRatio);
    return cloned;
}

void EllipsePrimitive::translate(const QVector2D& offset)
{
    m_center += offset;
}

QJsonObject EllipsePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["centerX"] = m_center.x();
    json["centerY"] = m_center.y();
    json["radiusX"] = m_radiusX;
    json["radiusY"] = m_radiusY;
    json["filled"] = m_filled;
    json["subdivisions"] = m_subdivisions;
    json["showAxes"] = m_showAxes;
    json["lockAspectRatio"] = m_lockAspectRatio;
    return json;
}

void EllipsePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_center = QVector2D(json["centerX"].toDouble(), json["centerY"].toDouble());
    m_radiusX = json["radiusX"].toDouble();
    m_radiusY = json["radiusY"].toDouble();
    m_filled = json["filled"].toBool();
    m_subdivisions = json["subdivisions"].toInt(m_subdivisions);
    m_showAxes = json["showAxes"].toBool(m_showAxes);
    m_lockAspectRatio = json["lockAspectRatio"].toBool(m_lockAspectRatio);
}

std::vector<QVector2D> EllipsePrimitive::getControlPoints() const
{
    return {
        m_center,                                           // Center
        QVector2D(m_center.x() - m_radiusX, m_center.y()), // Left
        QVector2D(m_center.x() + m_radiusX, m_center.y()), // Right
        QVector2D(m_center.x(), m_center.y() - m_radiusY), // Top
        QVector2D(m_center.x(), m_center.y() + m_radiusY)  // Bottom
    };
}

void EllipsePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    switch (index) {
        case 0: // Center
            m_center = position;
            break;
        case 1: // Left
            m_radiusX = std::abs(m_center.x() - position.x());
            break;
        case 2: // Right
            m_radiusX = std::abs(position.x() - m_center.x());
            break;
        case 3: // Top
            m_radiusY = std::abs(m_center.y() - position.y());
            break;
        case 4: // Bottom
            m_radiusY = std::abs(position.y() - m_center.y());
            break;
    }
}

// --- CirclePrimitive ---

std::vector<QVector2D> CirclePrimitive::getControlPoints() const
{
    return {
        m_center,
        QVector2D(m_center.x() + m_radius, m_center.y()), // Right edge
        QVector2D(m_center.x(), m_center.y() - m_radius), // Top edge
        QVector2D(m_center.x() - m_radius, m_center.y()), // Left edge
        QVector2D(m_center.x(), m_center.y() + m_radius)  // Bottom edge
    };
}

void CirclePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index == 0) {
        m_center = position;
    } else {
        // Any edge point adjusts the radius
        m_radius = (position - m_center).length();
    }
}

CirclePrimitive::CirclePrimitive(const QVector2D &center, float radius)
    : DrawingPrimitive(PrimitiveType::Circle), m_center(center), m_radius(radius), m_filled(false)
{
}

void CirclePrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    QRectF circleRect(m_center.x() - m_radius, m_center.y() - m_radius,
                      m_radius * 2, m_radius * 2);

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(25, (int)(m_shadowBlur * 0.8f) + 3));
        float baseAlpha = m_shadowColor.alphaF();

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor; sc.setAlphaF(layerAlpha);
            painter->setPen(Qt::NoPen);
            painter->setBrush(sc);
            QRectF shadowRect(m_center.x() - m_radius - expansion,
                              m_center.y() - m_radius - expansion,
                              (m_radius + expansion) * 2, (m_radius + expansion) * 2);
            painter->drawEllipse(shadowRect);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    QColor fillColor = strokeColor;
    if (!m_selected && m_filled && m_hasFillColor) {
        fillColor = m_fillColor;
    }

    QPen pen(strokeColor);
    pen.setWidthF(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    QBrush cFillBrush = (m_gradientFillType != GradientFillType::None)
        ? createGradientBrush(circleRect) : QBrush(fillColor);

    if (m_filled) {
        painter->setPen(m_lineWidth > 0.0f ? pen : QPen(Qt::NoPen));
        painter->setBrush(cFillBrush);
        painter->drawEllipse(circleRect);
    } else {
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(circleRect);
    }

    if (m_showCenterPoint) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(128, 128, 128));
        painter->drawEllipse(QPointF(m_center.x(), m_center.y()), 3.0, 3.0);
    }

    if (m_showQuadrants) {
        QPen quadPen(QColor(179, 179, 179), 1);
        painter->setPen(quadPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(m_center.x() - m_radius, m_center.y()),
                          QPointF(m_center.x() + m_radius, m_center.y()));
        painter->drawLine(QPointF(m_center.x(), m_center.y() - m_radius),
                          QPointF(m_center.x(), m_center.y() + m_radius));
    }
    painter->setBrush(Qt::NoBrush);
}

QRectF CirclePrimitive::boundingRect() const
{
    float padding = m_lineWidth * 2.0f;
    float extent = m_radius + padding;
    return QRectF(m_center.x() - extent,
                  m_center.y() - extent,
                  extent * 2.0f,
                  extent * 2.0f);
}

bool CirclePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    float distance = (point - m_center).length();
    if (m_filled) {
        return distance <= m_radius + tolerance;
    }
    return std::abs(distance - m_radius) <= tolerance;
}

std::unique_ptr<DrawingPrimitive> CirclePrimitive::clone() const
{
    auto cloned = std::make_unique<CirclePrimitive>(m_center, m_radius);
    applyCommonPropertiesTo(cloned.get());
    cloned->setFilled(m_filled);
    cloned->setShowCenterPoint(m_showCenterPoint);
    cloned->setShowQuadrants(m_showQuadrants);
    return cloned;
}

void CirclePrimitive::translate(const QVector2D& offset)
{
    m_center += offset;
}

QJsonObject CirclePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["centerX"] = m_center.x();
    json["centerY"] = m_center.y();
    json["radius"] = m_radius;
    json["filled"] = m_filled;
    json["showCenterPoint"] = m_showCenterPoint;
    json["showQuadrants"] = m_showQuadrants;
    return json;
}

void CirclePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_center = QVector2D(json["centerX"].toDouble(), json["centerY"].toDouble());
    m_radius = json["radius"].toDouble();
    m_filled = json["filled"].toBool();
    m_showCenterPoint = json["showCenterPoint"].toBool();
    m_showQuadrants = json["showQuadrants"].toBool();
}

// --- ArcPrimitive ---

std::vector<QVector2D> ArcPrimitive::getControlPoints() const
{
    return {
        m_center,
        startPoint(),
        endPoint()
    };
}

void ArcPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    switch (index) {
        case 0: // Center
            m_center = position;
            break;
        case 1: // Start point
        {
            QVector2D fromCenter = position - m_center;
            m_radius = fromCenter.length();
            m_startAngle = atan2(fromCenter.y(), fromCenter.x()) * 180.0f / M_PI;
            break;
        }
        case 2: // End point
        {
            QVector2D fromCenter = position - m_center;
            m_radius = fromCenter.length();
            m_endAngle = atan2(fromCenter.y(), fromCenter.x()) * 180.0f / M_PI;
            break;
        }
    }
}

ArcPrimitive::ArcPrimitive(const QVector2D &center, float radius, float startAngle, float endAngle)
    : DrawingPrimitive(PrimitiveType::Arc), m_center(center), m_radius(radius),
      m_startAngle(startAngle), m_endAngle(endAngle)
{
}

QVector2D ArcPrimitive::startPoint() const
{
    float radians = m_startAngle * M_PI / 180.0f;
    return m_center + QVector2D(m_radius * std::cos(radians), m_radius * std::sin(radians));
}

QVector2D ArcPrimitive::endPoint() const
{
    float radians = m_endAngle * M_PI / 180.0f;
    return m_center + QVector2D(m_radius * std::cos(radians), m_radius * std::sin(radians));
}

void ArcPrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    QPen pen(strokeColor);
    pen.setWidthF(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // QPainter drawArc uses 1/16th degree units and measures from 3 o'clock CCW
    QRectF arcRect(m_center.x() - m_radius, m_center.y() - m_radius,
                   m_radius * 2, m_radius * 2);
    int startAngle16 = static_cast<int>(m_startAngle * 16);
    int spanAngle16 = static_cast<int>((m_endAngle - m_startAngle) * 16);
    painter->drawArc(arcRect, startAngle16, spanAngle16);
}

QRectF ArcPrimitive::boundingRect() const
{
    // For simplicity, use the full circle bounds
    // A more precise implementation would calculate the actual arc bounds
    return QRectF(m_center.x() - m_radius - 5, m_center.y() - m_radius - 5,
                  2 * m_radius + 10, 2 * m_radius + 10);
}

bool ArcPrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    float distance = (point - m_center).length();
    if (std::abs(distance - m_radius) > tolerance) return false;
    
    // Check if point is within the arc angle range
    QVector2D fromCenter = point - m_center;
    float angle = std::atan2(fromCenter.y(), fromCenter.x()) * 180.0f / M_PI;
    
    // Normalize angle to 0-360 range
    if (angle < 0) angle += 360;
    
    float start = m_startAngle;
    float end = m_endAngle;
    if (start < 0) start += 360;
    if (end < 0) end += 360;
    
    if (start <= end) {
        return angle >= start && angle <= end;
    } else {
        return angle >= start || angle <= end;
    }
}

std::unique_ptr<DrawingPrimitive> ArcPrimitive::clone() const
{
    auto cloned = std::make_unique<ArcPrimitive>(m_center, m_radius, m_startAngle, m_endAngle);
    cloned->setColor(m_color);
    if (m_hasFillColor) {
        cloned->setFillColor(m_fillColor);
    } else {
        cloned->clearFillColor();
    }
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
}

void ArcPrimitive::translate(const QVector2D& offset)
{
    m_center += offset;
}

QJsonObject ArcPrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["centerX"] = m_center.x();
    json["centerY"] = m_center.y();
    json["radius"] = m_radius;
    json["startAngle"] = m_startAngle;
    json["endAngle"] = m_endAngle;
    return json;
}

void ArcPrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_center = QVector2D(json["centerX"].toDouble(), json["centerY"].toDouble());
    m_radius = json["radius"].toDouble();
    m_startAngle = json["startAngle"].toDouble();
    m_endAngle = json["endAngle"].toDouble();
}

// --- PolygonPrimitive ---

std::vector<QVector2D> PolygonPrimitive::getControlPoints() const
{
    return m_points;
}

void PolygonPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points[index] = position;
    }
}

PolygonPrimitive::PolygonPrimitive()
    : DrawingPrimitive(PrimitiveType::Polygon)
    , m_closed(true)
    , m_filled(false)
{
}

void PolygonPrimitive::render(QPainter* painter) const
{
    if (!m_visible || m_points.size() < 2 || !painter) return;

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    QColor fillColor = strokeColor;
    if (!m_selected && m_filled && m_hasFillColor) {
        fillColor = m_fillColor;
    }

    QPen pen(strokeColor);
    pen.setWidthF(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    QPolygonF poly = toQPolygonF(m_points);

    if (m_filled) {
        QBrush pFillBrush = (m_gradientFillType != GradientFillType::None)
            ? createGradientBrush(poly.boundingRect()) : QBrush(fillColor);
        painter->setPen(m_lineWidth > 0.0f ? pen : QPen(Qt::NoPen));
        painter->setBrush(pFillBrush);
        painter->drawPolygon(poly);
    }

    if (!m_filled || m_lineWidth > 0.0f) {
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        if (m_closed) {
            painter->drawPolygon(poly);
        } else {
            painter->drawPolyline(poly);
        }
    }
    painter->setBrush(Qt::NoBrush);
}

QRectF PolygonPrimitive::boundingRect() const
{
    if (m_points.empty()) return QRectF();
    
    float minX = m_points[0].x();
    float minY = m_points[0].y();
    float maxX = m_points[0].x();
    float maxY = m_points[0].y();
    
    for (const auto& point : m_points) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }
    
    // Add some padding for line width
    float padding = m_lineWidth * 2.0f;
    return QRectF(minX - padding, minY - padding, 
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool PolygonPrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    if (m_points.size() < 2) return false;

    if (m_filled && m_closed) {
        QPolygonF polygon;
        polygon.reserve(static_cast<int>(m_points.size()));
        for (const auto& p : m_points) {
            polygon << QPointF(p.x(), p.y());
        }
        if (polygon.containsPoint(QPointF(point.x(), point.y()), Qt::OddEvenFill)) {
            return true;
        }
    }

    // Check if point is close to any edge (outline hit)
    for (size_t i = 0; i < m_points.size(); ++i) {
        size_t nextI = (i + 1) % m_points.size();
        if (!m_closed && nextI == 0) break; // Don't check last-to-first edge if not closed

        QVector2D lineVec = m_points[nextI] - m_points[i];
        QVector2D pointVec = point - m_points[i];

        float lineLength = lineVec.length();
        if (lineLength < 0.001f) {
            if ((point - m_points[i]).length() <= tolerance) return true;
            continue;
        }

        float t = QVector2D::dotProduct(pointVec, lineVec) / (lineLength * lineLength);
        t = std::max(0.0f, std::min(1.0f, t));

        QVector2D closestPoint = m_points[i] + t * lineVec;
        float distance = (point - closestPoint).length();

        if (distance <= tolerance) return true;
    }

    return false;
}

std::unique_ptr<DrawingPrimitive> PolygonPrimitive::clone() const
{
    auto cloned = std::make_unique<PolygonPrimitive>();
    cloned->m_points = m_points;
    cloned->m_closed = m_closed;
    cloned->m_filled = m_filled;
    applyCommonPropertiesTo(cloned.get());
    return cloned;
}

void PolygonPrimitive::translate(const QVector2D& offset)
{
    for (auto& point : m_points) {
        point += offset;
    }
}

void PolygonPrimitive::addPoint(const QVector2D &point)
{
    m_points.push_back(point);
}

void PolygonPrimitive::setPoint(int index, const QVector2D &point)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points[index] = point;
    }
}

void PolygonPrimitive::removePoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points.erase(m_points.begin() + index);
    }
}

QJsonObject PolygonPrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    QJsonArray pointsArray;
    for (const auto& point : m_points) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pointsArray.append(pointObj);
    }
    json["points"] = pointsArray;
    json["closed"] = m_closed;
    json["filled"] = m_filled;
    return json;
}

void PolygonPrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_points.clear();
    if (json.contains("points")) {
        QJsonArray pointsArray = json["points"].toArray();
        for (const QJsonValue& pointValue : pointsArray) {
            QJsonObject pointObj = pointValue.toObject();
            m_points.push_back(QVector2D(
                pointObj["x"].toDouble(),
                pointObj["y"].toDouble()
            ));
        }
    }
    m_closed = json["closed"].toBool(true);
    m_filled = json["filled"].toBool(false);
}

