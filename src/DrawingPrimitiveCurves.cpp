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
// Curve primitives (refactor E21).

// --- CurvePrimitive ---

CurvePrimitive::CurvePrimitive()
    : DrawingPrimitive(PrimitiveType::Curve)
    , m_closed(false)
{
}

void CurvePrimitive::render(QPainter* painter) const
{
    if (!m_visible || m_controlPoints.size() < 2 || !painter) return;

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(15, (int)(m_shadowBlur * 0.5f) + 3));
        float baseAlpha = m_shadowColor.alphaF();

        QPolygonF shadowPoly = toQPolygonF(m_controlPoints);
        if (m_closed) shadowPoly << shadowPoly.first();

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor; sc.setAlphaF(layerAlpha);
            QPen pen(sc); pen.setWidthF((m_lineWidth + expansion) * (m_selected ? 2.0f : 1.0f));
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolyline(shadowPoly);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    const QColor fillColor = m_hasFillColor ? m_fillColor : strokeColor;

    QPen pen(strokeColor);
    pen.setWidthF(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    // If filled and closed, render filled area first
    if (m_filled && m_closed && m_controlPoints.size() >= 3) {
        std::vector<QVector2D> pointsToFill;
        if (m_curveType == 0) {
            pointsToFill = m_controlPoints;
        } else if (m_curveType == 1) {
            pointsToFill = generateQuadraticBezierCurve();
        } else if (m_curveType == 2) {
            pointsToFill = generateCubicBezierCurve();
        } else {
            pointsToFill = generateCatmullRomCurve();
        }

        if (pointsToFill.size() >= 3) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(fillColor);
            painter->drawPolygon(toQPolygonF(pointsToFill));
        }
    }

    // Render main curve outline
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    std::vector<QVector2D> drawPoints;
    if (m_curveType == 0) {
        drawPoints = m_controlPoints;
    } else if (m_curveType == 1) {
        drawPoints = generateQuadraticBezierCurve();
    } else if (m_curveType == 2) {
        drawPoints = generateCubicBezierCurve();
    } else {
        drawPoints = generateCatmullRomCurve();
    }

    if (!drawPoints.empty()) {
        QPolygonF poly = toQPolygonF(drawPoints);
        if (m_closed) {
            painter->drawPolygon(poly);
        } else {
            painter->drawPolyline(poly);
        }
    }

    // Show control polygon if enabled
    if (m_showControlPolygon && m_controlPoints.size() > 1) {
        QPen dotPen(QColor(179, 179, 179), 1, Qt::DotLine);
        painter->setPen(dotPen);
        painter->drawPolyline(toQPolygonF(m_controlPoints));

        // Draw control points as small red squares
        painter->setPen(QPen(Qt::red, 1));
        painter->setBrush(Qt::NoBrush);
        float ps = 3.0f;
        for (const auto& pt : m_controlPoints) {
            painter->drawRect(QRectF(pt.x() - ps, pt.y() - ps, ps * 2, ps * 2));
        }
    }
}

QRectF CurvePrimitive::boundingRect() const
{
    if (m_controlPoints.empty()) {
        return QRectF();
    }
    
    float minX = m_controlPoints[0].x();
    float minY = m_controlPoints[0].y();
    float maxX = minX;
    float maxY = minY;
    
    for (const auto& point : m_controlPoints) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }
    
    float padding = m_lineWidth * 2.0f;
    return QRectF(minX - padding, minY - padding,
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool CurvePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    if (m_controlPoints.size() < 2) return false;
    
    // Check distance to each line segment
    for (size_t i = 0; i < m_controlPoints.size() - 1; ++i) {
        LinePrimitive segment(m_controlPoints[i], m_controlPoints[i + 1]);
        if (segment.containsPoint(point, tolerance)) {
            return true;
        }
    }
    
    // Check closing segment if closed
    if (m_closed && m_controlPoints.size() > 2) {
        LinePrimitive segment(m_controlPoints.back(), m_controlPoints.front());
        return segment.containsPoint(point, tolerance);
    }
    
    return false;
}

std::unique_ptr<DrawingPrimitive> CurvePrimitive::clone() const
{
    auto cloned = std::make_unique<CurvePrimitive>();
    cloned->setColor(m_color);
    if (m_hasFillColor) {
        cloned->setFillColor(m_fillColor);
    } else {
        cloned->clearFillColor();
    }
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setClosed(m_closed);
    cloned->setCurveType(m_curveType);
    cloned->setShowControlPolygon(m_showControlPolygon);
    for (const auto& point : m_controlPoints) {
        cloned->addControlPoint(point);
    }
    return cloned;
}

void CurvePrimitive::translate(const QVector2D& offset)
{
    for (auto& point : m_controlPoints) {
        point += offset;
    }
}

void CurvePrimitive::addControlPoint(const QVector2D &point)
{
    m_controlPoints.push_back(point);
}

void CurvePrimitive::setControlPoint(int index, const QVector2D &point)
{
    if (index >= 0 && index < static_cast<int>(m_controlPoints.size())) {
        m_controlPoints[index] = point;
    }
}

void CurvePrimitive::removeControlPoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_controlPoints.size())) {
        m_controlPoints.erase(m_controlPoints.begin() + index);
    }
}

QVector2D CurvePrimitive::evaluateBezier(const std::vector<QVector2D>& points, float t) const
{
    // Simple Bezier evaluation - could be enhanced
    if (points.size() == 2) {
        return (1.0f - t) * points[0] + t * points[1];
    }
    // For more complex curves, implement De Casteljau's algorithm
    return points[0]; // Placeholder
}

std::vector<QVector2D> CurvePrimitive::generateQuadraticBezierCurve() const
{
    std::vector<QVector2D> result;
    if (m_controlPoints.size() < 3) return result;
    
    const int segments = 30;
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        
        // Quadratic Bezier formula: B(t) = (1-t)^2*P0 + 2*(1-t)*t*P1 + t^2*P2
        float u = 1.0f - t;
        QVector2D point = u*u*m_controlPoints[0] + 2*u*t*m_controlPoints[1] + t*t*m_controlPoints[2];
        result.push_back(point);
    }
    
    return result;
}

std::vector<QVector2D> CurvePrimitive::generateCubicBezierCurve() const
{
    std::vector<QVector2D> result;
    if (m_controlPoints.size() < 4) return result;
    
    const int segments = 30;
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        
        // Cubic Bezier formula: B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
        float u = 1.0f - t;
        float u2 = u * u;
        float u3 = u2 * u;
        float t2 = t * t;
        float t3 = t2 * t;
        
        QVector2D point = u3*m_controlPoints[0] + 3*u2*t*m_controlPoints[1] + 
                         3*u*t2*m_controlPoints[2] + t3*m_controlPoints[3];
        result.push_back(point);
    }
    
    return result;
}

std::vector<QVector2D> CurvePrimitive::generateCatmullRomCurve() const
{
    std::vector<QVector2D> result;
    if (m_controlPoints.size() < 2) return result;
    
    // For Catmull-Rom, we need at least 2 points, but it works better with more
    if (m_controlPoints.size() == 2) {
        // Just a straight line
        const int segments = 10;
        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / segments;
            QVector2D point = (1.0f - t) * m_controlPoints[0] + t * m_controlPoints[1];
            result.push_back(point);
        }
        return result;
    }
    
    // Generate smooth curve using Catmull-Rom splines
    const int segmentsPerCurve = 20;
    
    for (size_t i = 0; i < m_controlPoints.size() - 1; ++i) {
        QVector2D p0, p1, p2, p3;
        
        // Get the four points needed for Catmull-Rom
        p1 = m_controlPoints[i];
        p2 = m_controlPoints[i + 1];
        
        // Handle boundary conditions
        if (i == 0) {
            p0 = p1 - (p2 - p1); // Extrapolate backwards
        } else {
            p0 = m_controlPoints[i - 1];
        }
        
        if (i + 2 < m_controlPoints.size()) {
            p3 = m_controlPoints[i + 2];
        } else {
            p3 = p2 + (p2 - p1); // Extrapolate forwards
        }
        
        // Generate points between p1 and p2
        for (int j = (i == 0 ? 0 : 1); j <= segmentsPerCurve; ++j) {
            float t = static_cast<float>(j) / segmentsPerCurve;
            QVector2D point = catmullRom(p0, p1, p2, p3, t);
            result.push_back(point);
        }
    }
    
    return result;
}

QVector2D CurvePrimitive::catmullRom(const QVector2D& p0, const QVector2D& p1, 
                                   const QVector2D& p2, const QVector2D& p3, float t) const
{
    float t2 = t * t;
    float t3 = t2 * t;
    
    // Catmull-Rom spline formula
    QVector2D result = 0.5f * (
        2.0f * p1 +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
    
    return result;
}

QJsonObject CurvePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    
    // Save all control points
    QJsonArray pointsArray;
    for (const auto& point : m_controlPoints) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pointsArray.append(pointObj);
    }
    json["controlPoints"] = pointsArray;
    json["pointCount"] = static_cast<int>(m_controlPoints.size());
    
    // Save curve properties
    json["closed"] = m_closed;
    json["filled"] = m_filled;
    json["curveType"] = m_curveType;
    json["showControlPolygon"] = m_showControlPolygon;
    
    return json;
}

void CurvePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    
    // Restore control points
    m_controlPoints.clear();
    if (json.contains("controlPoints")) {
        QJsonArray pointsArray = json["controlPoints"].toArray();
        for (const QJsonValue& pointValue : pointsArray) {
            QJsonObject pointObj = pointValue.toObject();
            m_controlPoints.push_back(QVector2D(
                pointObj["x"].toDouble(),
                pointObj["y"].toDouble()
            ));
        }
    }
    
    // Restore curve properties
    m_closed = json["closed"].toBool(false);
    m_filled = json["filled"].toBool(false);
    setCurveType(json["curveType"].toInt(0));
    m_showControlPolygon = json["showControlPolygon"].toBool(false);
}

std::vector<QVector2D> CurvePrimitive::getControlPoints() const
{
    return m_controlPoints;
}

void CurvePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index >= 0 && index < static_cast<int>(m_controlPoints.size())) {
        m_controlPoints[index] = position;
    }
}

// --- BezierCurvePrimitive ---

std::vector<QVector2D> BezierCurvePrimitive::getControlPoints() const
{
    return m_controlPoints;
}

void BezierCurvePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index >= 0 && index < static_cast<int>(m_controlPoints.size())) {
        m_controlPoints[index] = position;
    }
}

BezierCurvePrimitive::BezierCurvePrimitive()
    : DrawingPrimitive(PrimitiveType::BezierCurve)
{
    // Initialize with 4 control points for cubic Bézier
    m_controlPoints.resize(4);
    m_controlPoints[0] = QVector2D(0, 0);     // Start point
    m_controlPoints[1] = QVector2D(50, 50);   // Control point 1
    m_controlPoints[2] = QVector2D(150, 50);  // Control point 2
    m_controlPoints[3] = QVector2D(200, 0);   // End point
}

void BezierCurvePrimitive::setControlPoints(const std::vector<QVector2D> &points)
{
    m_controlPoints = points;
    if (m_controlPoints.size() < 4) {
        m_controlPoints.resize(4, QVector2D(0, 0));
    }
}

QVector2D BezierCurvePrimitive::evaluateAt(float t) const
{
    if (m_controlPoints.size() < 4) return QVector2D();
    return evaluateCubicBezier(m_controlPoints[0], m_controlPoints[1], 
                              m_controlPoints[2], m_controlPoints[3], t);
}

QVector2D BezierCurvePrimitive::evaluateCubicBezier(const QVector2D &p0, const QVector2D &p1,
                                                   const QVector2D &p2, const QVector2D &p3, float t) const
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    
    QVector2D point = uuu * p0;
    point += 3 * uu * t * p1;
    point += 3 * u * tt * p2;
    point += ttt * p3;
    
    return point;
}

void BezierCurvePrimitive::render(QPainter* painter) const
{
    if (!m_visible || m_controlPoints.size() < 4 || !painter) return;

    std::vector<QVector2D> effectivePoints = getEffectiveControlPoints();

    // Helper: generate curve polyline
    auto generateCurvePoly = [&](const std::vector<QVector2D>& pts, int segs) {
        QPolygonF poly;
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / segs;
            QVector2D p = evaluateCubicBezier(pts[0], pts[1], pts[2], pts[3], t);
            poly << QPointF(p.x(), p.y());
        }
        return poly;
    };

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(15, (int)(m_shadowBlur * 0.5f) + 3));
        float baseAlpha = m_shadowColor.alphaF();
        QPolygonF shadowCurve = generateCurvePoly(
            {m_controlPoints[0], m_controlPoints[1], m_controlPoints[2], m_controlPoints[3]}, 50);

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor; sc.setAlphaF(layerAlpha);
            QPen pen(sc); pen.setWidthF((m_lineWidth + expansion) * (m_selected ? 2.0f : 1.0f));
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolyline(shadowCurve);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    QPen pen(strokeColor);
    pen.setWidthF(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    int segments = std::max(10, std::min(200, m_subdivisionLevel));
    QPolygonF curvePoly = generateCurvePoly(effectivePoints, segments);
    painter->drawPolyline(curvePoly);

    // Draw control lines if enabled
    if (m_selected || m_showControlLines) {
        QPen ctrlPen(QColor(153, 153, 153), 1.5);
        painter->setPen(ctrlPen);
        if (effectivePoints.size() >= 4) {
            painter->drawLine(QPointF(effectivePoints[0].x(), effectivePoints[0].y()),
                              QPointF(effectivePoints[1].x(), effectivePoints[1].y()));
            painter->drawLine(QPointF(effectivePoints[2].x(), effectivePoints[2].y()),
                              QPointF(effectivePoints[3].x(), effectivePoints[3].y()));
        }

        // Draw control points (all green)
        float size = 4.0f;
        for (int i = 0; i < static_cast<int>(m_controlPoints.size()); ++i) {
            const auto& pt = m_controlPoints[i];
            QColor ptColor(50, 205, 50);
            painter->setPen(QPen(ptColor, 1));
            painter->setBrush(ptColor);
            QRectF rect(pt.x() - size * 0.4f, pt.y() - size * 0.4f, size * 0.8f, size * 0.8f);
            painter->drawRect(rect);
        }

        // Draw effective control points if different
        if (m_autoTangents || m_symmetricHandles) {
            painter->setPen(QPen(QColor(204, 204, 204), 1));
            painter->setBrush(Qt::NoBrush);
            float es = 3.0f;
            for (int i = 1; i <= 2 && i < static_cast<int>(effectivePoints.size()); ++i) {
                const auto& ep = effectivePoints[i];
                painter->drawRect(QRectF(ep.x() - es, ep.y() - es, es * 2, es * 2));
            }
        }
    }
    painter->setBrush(Qt::NoBrush);
}

QRectF BezierCurvePrimitive::boundingRect() const
{
    if (m_controlPoints.empty()) return QRectF();
    
    float minX = m_controlPoints[0].x(), maxX = m_controlPoints[0].x();
    float minY = m_controlPoints[0].y(), maxY = m_controlPoints[0].y();
    
    // Sample the curve to find actual bounds
    const int samples = 20;
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / samples;
        QVector2D point = evaluateAt(t);
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }
    
    float padding = 5.0f;
    return QRectF(minX - padding, minY - padding,
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool BezierCurvePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    const int samples = 30;
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / samples;
        QVector2D curvePoint = evaluateAt(t);
        if ((curvePoint - point).length() <= tolerance) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<DrawingPrimitive> BezierCurvePrimitive::clone() const
{
    auto cloned = std::make_unique<BezierCurvePrimitive>();
    cloned->setControlPoints(m_controlPoints);
    cloned->setColor(m_color);
    if (m_hasFillColor) {
        cloned->setFillColor(m_fillColor);
    } else {
        cloned->clearFillColor();
    }
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setShowControlLines(m_showControlLines);
    cloned->setSubdivisionLevel(m_subdivisionLevel);
    cloned->setAutoTangents(m_autoTangents);
    cloned->setSymmetricHandles(m_symmetricHandles);
    return cloned;
}

void BezierCurvePrimitive::translate(const QVector2D& offset)
{
    for (auto& point : m_controlPoints) {
        point += offset;
    }
}

std::vector<QVector2D> BezierCurvePrimitive::getEffectiveControlPoints() const
{
    if (m_controlPoints.size() < 4) return m_controlPoints;
    
    std::vector<QVector2D> effective = m_controlPoints;
    
    if (m_autoTangents) {
        // Auto-generate tangent directions based on the start and end points
        QVector2D direction = (effective[3] - effective[0]).normalized();
        float distance = (effective[3] - effective[0]).length() * 0.3f;
        
        effective[1] = effective[0] + direction * distance;
        effective[2] = effective[3] - direction * distance;
    }
    
    if (m_symmetricHandles) {
        // Make control handles symmetric around start and end points
        QVector2D startToControl1 = effective[1] - effective[0];
        QVector2D endToControl2 = effective[2] - effective[3];
        
        // Make them equal length but opposite directions
        float avgLength = (startToControl1.length() + endToControl2.length()) * 0.5f;
        
        QVector2D midPoint = (effective[0] + effective[3]) * 0.5f;
        QVector2D direction = (effective[3] - effective[0]).normalized();
        QVector2D perpendicular(-direction.y(), direction.x());
        
        // Adjust control points to be symmetric
        effective[1] = effective[0] + startToControl1.normalized() * avgLength;
        effective[2] = effective[3] + endToControl2.normalized() * avgLength;
    }
    
    return effective;
}

QJsonObject BezierCurvePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    
    // Save all control points
    QJsonArray pointsArray;
    for (const auto& point : m_controlPoints) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pointsArray.append(pointObj);
    }
    json["controlPoints"] = pointsArray;
    json["pointCount"] = static_cast<int>(m_controlPoints.size());
    
    // Save bezier properties
    json["filled"] = m_filled;
    json["showControlLines"] = m_showControlLines;
    json["subdivisionLevel"] = m_subdivisionLevel;
    json["autoTangents"] = m_autoTangents;
    json["symmetricHandles"] = m_symmetricHandles;
    
    return json;
}

void BezierCurvePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    
    // Restore control points
    m_controlPoints.clear();
    if (json.contains("controlPoints")) {
        QJsonArray pointsArray = json["controlPoints"].toArray();
        for (const QJsonValue& pointValue : pointsArray) {
            QJsonObject pointObj = pointValue.toObject();
            m_controlPoints.push_back(QVector2D(
                pointObj["x"].toDouble(),
                pointObj["y"].toDouble()
            ));
        }
    }
    
    // Restore bezier properties
    m_filled = json["filled"].toBool(false);
    m_showControlLines = json["showControlLines"].toBool(true);
    setSubdivisionLevel(json["subdivisionLevel"].toInt(50));
    m_autoTangents = json["autoTangents"].toBool(false);
    m_symmetricHandles = json["symmetricHandles"].toBool(false);
}

// --- SplinePrimitive ---

std::vector<QVector2D> SplinePrimitive::getControlPoints() const
{
    return m_points;
}

void SplinePrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points[index] = position;
    }
}

SplinePrimitive::SplinePrimitive()
    : DrawingPrimitive(PrimitiveType::Spline)
    , m_closed(false)
    , m_filled(false)
    , m_smoothness(0.5f)
{
}

void SplinePrimitive::render(QPainter* painter) const
{
    if (!m_visible || m_points.size() < 2 || !painter) return;

    std::vector<QVector2D> splinePoints = generateSpline();
    QPolygonF splinePoly = toQPolygonF(splinePoints);

    // Render shadow first if enabled
    if (m_shadowEnabled) {
        painter->save();
        painter->translate(m_shadowOffsetX, m_shadowOffsetY);

        int blurLayers = qMax(3, qMin(15, (int)(m_shadowBlur * 0.5f) + 3));
        float baseAlpha = m_shadowColor.alphaF();

        for (int layer = 0; layer < blurLayers; ++layer) {
            float t = (float)layer / (float)(blurLayers - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float layerAlpha = baseAlpha * gaussianWeight / (float)blurLayers * 2.0f;
            float expansion = t * m_shadowBlur;

            QColor sc = m_shadowColor; sc.setAlphaF(layerAlpha);
            QPen pen(sc); pen.setWidthF((m_lineWidth + expansion) * (m_selected ? 2.0f : 1.0f));
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolyline(splinePoly);
        }
        painter->restore();
    }

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    QColor fillColorValue = m_hasFillColor ? m_fillColor : strokeColor;

    QPen pen(strokeColor);
    pen.setWidthF(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    pen.setCosmetic(true);
    pen.setStyle(m_lineStyle);

    if (m_filled && splinePoints.size() >= 3) {
        QBrush sFillBrush = (m_gradientFillType != GradientFillType::None)
            ? createGradientBrush(splinePoly.boundingRect()) : QBrush(fillColorValue);
        painter->setPen(Qt::NoPen);
        painter->setBrush(sFillBrush);
        painter->drawPolygon(splinePoly);
    }

    // Draw outline
    if (!m_filled || m_lineWidth > 0.0f || m_selected) {
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        if (m_closed) {
            painter->drawPolygon(splinePoly);
        } else {
            painter->drawPolyline(splinePoly);
        }
    }

    // Show control points if enabled
    if (m_showPoints && !m_points.empty()) {
        // Control polygon
        if (m_points.size() > 1) {
            QPen dotPen(QColor(230, 230, 230), 0.5, Qt::DotLine);
            painter->setPen(dotPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolyline(toQPolygonF(m_points));
        }

        // Control points as circles
        float ps = 5.0f;
        for (size_t i = 0; i < m_points.size(); ++i) {
            const auto& pt = m_points[i];
            QColor fillC = (i == 0 || i == m_points.size() - 1)
                ? QColor(0, 255, 0) : QColor(0, 153, 0);
            painter->setPen(QPen(QColor(0, 204, 0), 1));
            painter->setBrush(fillC);
            painter->drawEllipse(QPointF(pt.x(), pt.y()), ps * 0.7, ps * 0.7);
        }
    }
    painter->setBrush(Qt::NoBrush);
}

QRectF SplinePrimitive::boundingRect() const
{
    if (m_points.empty()) return QRectF();
    
    float minX = m_points[0].x();
    float minY = m_points[0].y();
    float maxX = minX;
    float maxY = minY;
    
    for (const auto& point : m_points) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }
    
    float margin = m_lineWidth + 5;
    return QRectF(minX - margin, minY - margin, 
                  maxX - minX + 2*margin, maxY - minY + 2*margin);
}

bool SplinePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    if (m_points.size() < 2) return false;
    
    std::vector<QVector2D> splinePoints = generateSpline();
    
    // Check if point is near the spline curve
    for (size_t i = 0; i < splinePoints.size() - 1; ++i) {
        QVector2D lineStart = splinePoints[i];
        QVector2D lineEnd = splinePoints[i + 1];
        
        // Distance from point to line segment
        QVector2D line = lineEnd - lineStart;
        QVector2D pointToStart = point - lineStart;
        
        float lineLength = line.length();
        if (lineLength < 0.001f) continue;
        
        float projection = QVector2D::dotProduct(pointToStart, line) / lineLength;
        projection = std::max(0.0f, std::min(lineLength, projection));
        
        QVector2D closestPoint = lineStart + (line / lineLength) * projection;
        float distance = (point - closestPoint).length();
        
        if (distance <= tolerance) return true;
    }
    
    return false;
}

std::unique_ptr<DrawingPrimitive> SplinePrimitive::clone() const
{
    auto cloned = std::make_unique<SplinePrimitive>();
    cloned->m_points = m_points;
    cloned->m_closed = m_closed;
    cloned->m_filled = m_filled;
    cloned->m_smoothness = m_smoothness;
    cloned->setInterpolationType(m_interpolationType);
    cloned->setTension(m_tension);
    cloned->setAutoSmooth(m_autoSmooth);
    cloned->setShowPoints(m_showPoints);
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

void SplinePrimitive::translate(const QVector2D& offset)
{
    for (auto& point : m_points) {
        point += offset;
    }
}

void SplinePrimitive::addPoint(const QVector2D &point)
{
    m_points.push_back(point);
}

void SplinePrimitive::setPoint(int index, const QVector2D &point)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points[index] = point;
    }
}

void SplinePrimitive::removePoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_points.size())) {
        m_points.erase(m_points.begin() + index);
    }
}

std::vector<QVector2D> SplinePrimitive::generateSpline() const
{
    std::vector<QVector2D> result;
    
    if (m_points.size() < 2) return result;
    if (m_points.size() == 2) {
        // Just a line
        result = m_points;
        return result;
    }
    
    // For interpolating splines, we want the curve to pass THROUGH the control points
    // Use Catmull-Rom splines which are interpolating (curve passes through p1 and p2)
    const int segmentsPerCurve = 20;
    
    // Always start with the first control point
    result.push_back(m_points[0]);
    
    for (size_t i = 0; i < m_points.size() - 1; ++i) {
        // Get control points for interpolating Catmull-Rom spline
        QVector2D p0, p1, p2, p3;
        
        // p1 and p2 are the points we're interpolating between (curve passes through these)
        p1 = m_points[i];
        p2 = m_points[i + 1];
        
        // p0 and p3 are helper points that influence the tangent
        if (i == 0) {
            // First segment - extrapolate backwards from first two points
            p0 = p1 - (p2 - p1);
        } else {
            p0 = m_points[i - 1];
        }
        
        if (i + 2 < m_points.size()) {
            p3 = m_points[i + 2];
        } else {
            // Last segment - extrapolate forwards from last two points
            p3 = p2 + (p2 - p1);
        }
        
        // Generate interpolated points between p1 and p2
        // Skip t=0 since we already added p1 (except for first point)
        int startJ = (i == 0) ? 0 : 1;
        for (int j = startJ; j <= segmentsPerCurve; ++j) {
            float t = static_cast<float>(j) / segmentsPerCurve;
            
            // Apply smoothness: 0 = linear interpolation, 1 = full Catmull-Rom
            QVector2D point;
            if (m_smoothness < 0.01f) {
                // Linear interpolation
                point = p1 * (1.0f - t) + p2 * t;
            } else {
                // Catmull-Rom interpolation
                point = catmullRom(p0, p1, p2, p3, t);
                
                // Blend with linear for smoothness control
                if (m_smoothness < 1.0f) {
                    QVector2D linear = p1 * (1.0f - t) + p2 * t;
                    point = linear * (1.0f - m_smoothness) + point * m_smoothness;
                }
            }
            result.push_back(point);
        }
    }
    
    return result;
}

QVector2D SplinePrimitive::catmullRom(const QVector2D& p0, const QVector2D& p1, const QVector2D& p2, const QVector2D& p3, float t) const
{
    float t2 = t * t;
    float t3 = t2 * t;
    
    // Standard Catmull-Rom spline formula (interpolates through p1 and p2)
    // The curve passes exactly through p1 at t=0 and p2 at t=1
    QVector2D result = 0.5f * (
        2.0f * p1 +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
    
    // Apply tension parameter to control curve tightness
    if (m_tension != 0.5f) {
        // Blend between tight (linear) and loose (expanded) curves
        QVector2D linear = p1 * (1.0f - t) + p2 * t;
        
        if (m_tension < 0.5f) {
            // Tighter curve - blend toward linear
            float blendFactor = (0.5f - m_tension) * 2.0f;
            result = result * (1.0f - blendFactor) + linear * blendFactor;
        } else {
            // Looser curve - expand the interpolation
            float expansionFactor = (m_tension - 0.5f) * 2.0f;
            QVector2D expanded = result + (result - linear) * expansionFactor;
            result = expanded;
        }
    }
    
    return result;
}

QJsonObject SplinePrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    
    // Save all control points
    QJsonArray pointsArray;
    for (const auto& point : m_points) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pointsArray.append(pointObj);
    }
    json["points"] = pointsArray;
    json["pointCount"] = static_cast<int>(m_points.size());
    
    // Save spline properties
    json["closed"] = m_closed;
    json["filled"] = m_filled;
    json["smoothness"] = static_cast<double>(m_smoothness);
    json["interpolationType"] = m_interpolationType;
    json["tension"] = static_cast<double>(m_tension);
    json["autoSmooth"] = m_autoSmooth;
    json["showPoints"] = m_showPoints;
    
    return json;
}

void SplinePrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    
    // Restore control points
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
    
    // Restore spline properties
    m_closed = json["closed"].toBool(false);
    m_filled = json["filled"].toBool(false);
    setSmoothness(json["smoothness"].toDouble(0.5));
    setInterpolationType(json["interpolationType"].toInt(0));
    setTension(json["tension"].toDouble(0.5));
    m_autoSmooth = json["autoSmooth"].toBool(false);
    m_showPoints = json["showPoints"].toBool(true);
}
