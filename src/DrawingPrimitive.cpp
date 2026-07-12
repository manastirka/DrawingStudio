#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
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
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <vector>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: build a QPolygonF from a vector of QVector2D
namespace {
    QPolygonF toQPolygonF(const std::vector<QVector2D>& pts) {
        QPolygonF poly;
        poly.reserve(static_cast<int>(pts.size()));
        for (const auto& p : pts)
            poly << QPointF(p.x(), p.y());
        return poly;
    }

    void setupPenStyle(QPainter* painter, Qt::PenStyle style, const QColor& color, float width) {
        QPen pen(color);
        pen.setWidthF(width);
        pen.setStyle(style);
        painter->setPen(pen);
    }
}

// Base DrawingPrimitive implementation
DrawingPrimitive::DrawingPrimitive(PrimitiveType type, QObject* parent)
    : QObject(parent)
    , m_type(type)
    , m_color(Qt::black)
    , m_fillColor(Qt::white)
    , m_lineWidth(1.0f)
    , m_lineStyle(Qt::SolidLine)
    , m_selected(false)
    , m_visible(true)
    , m_hasFillColor(false)
    , m_id(QUuid::createUuid()) // Generate unique ID
    , m_layerId(QUuid()) // Default to null UUID
{
}

QBrush DrawingPrimitive::createGradientBrush(const QRectF& bounds) const {
    if (m_gradientFillType == GradientFillType::Linear) {
        float rad = qDegreesToRadians(m_gradientAngle);
        QPointF center = bounds.center();
        float dx = cos(rad) * bounds.width() / 2;
        float dy = -sin(rad) * bounds.height() / 2;
        QLinearGradient grad(center - QPointF(dx, dy), center + QPointF(dx, dy));
        grad.setColorAt(0, m_gradientStartColor);
        grad.setColorAt(1, m_gradientEndColor);
        return QBrush(grad);
    } else if (m_gradientFillType == GradientFillType::Radial) {
        QRadialGradient grad(bounds.center(), qMax(bounds.width(), bounds.height()) / 2);
        grad.setColorAt(0, m_gradientStartColor);
        grad.setColorAt(1, m_gradientEndColor);
        return QBrush(grad);
    }
    return QBrush();
}

void DrawingPrimitive::renderControlPoints(QPainter* painter) const
{
    if (!m_selected || !painter) return;

    auto controlPoints = getControlPoints();
    if (controlPoints.empty()) return;

    float size = 4.0f;
    // Use green for control points for better visibility
    QPen cpPen(QColor(50, 205, 50), 1.5); // lime green
    cpPen.setCosmetic(true);
    for (const auto& point : controlPoints) {
        QRectF rect(point.x() - size, point.y() - size, size * 2, size * 2);
        painter->setPen(cpPen);
        painter->setBrush(QColor(50, 205, 50, 180));
        painter->drawEllipse(rect);
    }
    painter->setBrush(Qt::NoBrush);
}

std::vector<QVector2D> DrawingPrimitive::getControlPoints() const
{
    // Default implementation returns empty vector
    return std::vector<QVector2D>();
}

void DrawingPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    // Default implementation does nothing
    Q_UNUSED(index);
    Q_UNUSED(position);
}

QJsonObject DrawingPrimitive::toJson() const
{
    QJsonObject json;
    json["type"] = static_cast<int>(m_type);
    json["id"] = m_id.toString();
    json["color"] = m_color.name(QColor::HexArgb);
    json["fillColor"] = m_fillColor.name(QColor::HexArgb);
    json["hasFillColor"] = m_hasFillColor;
    json["lineWidth"] = m_lineWidth;
    json["lineStyle"] = static_cast<int>(m_lineStyle);
    json["selected"] = m_selected;
    json["visible"] = m_visible;
    json["opacityMultiplier"] = static_cast<double>(m_opacityMultiplier);
    json["layerId"] = m_layerId.toString();

    json["shadowEnabled"] = m_shadowEnabled;
    json["shadowOffsetX"] = static_cast<double>(m_shadowOffsetX);
    json["shadowOffsetY"] = static_cast<double>(m_shadowOffsetY);
    json["shadowBlur"] = static_cast<double>(m_shadowBlur);
    json["shadowColor"] = m_shadowColor.name(QColor::HexArgb);
    json["rotationDegrees"] = static_cast<double>(m_rotationDegrees);
    if (!m_groupId.isNull())
        json["groupId"] = m_groupId.toString();
    return json;
}

void DrawingPrimitive::fromJson(const QJsonObject& json)
{
    m_type = static_cast<PrimitiveType>(json["type"].toInt());
    if (json.contains("id")) {
        QUuid loadedId(json["id"].toString());
        if (!loadedId.isNull()) {
            m_id = loadedId;
        }
    }
    m_color = QColor(json["color"].toString(m_color.name(QColor::HexArgb)));
    m_fillColor = QColor(json["fillColor"].toString(m_fillColor.name(QColor::HexArgb)));
    m_hasFillColor = json["hasFillColor"].toBool(m_hasFillColor);
    m_lineWidth = static_cast<float>(json["lineWidth"].toDouble(m_lineWidth));
    m_lineStyle = static_cast<Qt::PenStyle>(json["lineStyle"].toInt(static_cast<int>(m_lineStyle)));
    m_selected = json["selected"].toBool(m_selected);
    m_visible = json["visible"].toBool(m_visible);
    m_opacityMultiplier = static_cast<float>(json["opacityMultiplier"].toDouble(m_opacityMultiplier));
    m_layerId = QUuid(json["layerId"].toString(m_layerId.toString()));

    m_shadowEnabled = json["shadowEnabled"].toBool(m_shadowEnabled);
    m_shadowOffsetX = static_cast<float>(json["shadowOffsetX"].toDouble(m_shadowOffsetX));
    m_shadowOffsetY = static_cast<float>(json["shadowOffsetY"].toDouble(m_shadowOffsetY));
    m_shadowBlur = static_cast<float>(json["shadowBlur"].toDouble(m_shadowBlur));
    m_shadowColor = QColor(json["shadowColor"].toString(m_shadowColor.name(QColor::HexArgb)));
    m_rotationDegrees = static_cast<float>(json["rotationDegrees"].toDouble(m_rotationDegrees));
    if (json.contains(QStringLiteral("groupId")))
        m_groupId = QUuid(json["groupId"].toString());
    else
        m_groupId = QUuid();
}

void DrawingPrimitive::applyCommonPropertiesTo(DrawingPrimitive *dst) const
{
    if (!dst)
        return;
    dst->setColor(m_color);
    if (m_hasFillColor)
        dst->setFillColor(m_fillColor);
    else
        dst->clearFillColor();
    dst->setLineWidth(m_lineWidth);
    dst->setLineStyle(m_lineStyle);
    dst->setVisible(m_visible);
    dst->setRotationDegrees(m_rotationDegrees);
    dst->setGroupId(m_groupId);
    dst->setShadowEnabled(m_shadowEnabled);
    dst->setShadowBlur(m_shadowBlur);
    dst->setShadowOffset(m_shadowOffsetX, m_shadowOffsetY);
    dst->setShadowColor(m_shadowColor);
}

std::unique_ptr<DrawingPrimitive> DrawingPrimitive::createFromJson(const QJsonObject& json)
{
    if (!json.contains("type")) {
        qDebug() << "Error: JSON does not contain primitive type";
        return nullptr;
    }
    
    PrimitiveType type = static_cast<PrimitiveType>(json["type"].toInt());
    std::unique_ptr<DrawingPrimitive> primitive;
    
    // Create appropriate primitive based on type
    switch (type) {
        case PrimitiveType::Line: {
            QVector2D start(json["startX"].toDouble(), json["startY"].toDouble());
            QVector2D end(json["endX"].toDouble(), json["endY"].toDouble());
            primitive = std::make_unique<LinePrimitive>(start, end);
            break;
        }
        case PrimitiveType::Rectangle: {
            QVector2D topLeft(json["topLeftX"].toDouble(), json["topLeftY"].toDouble());
            QVector2D bottomRight(json["bottomRightX"].toDouble(), json["bottomRightY"].toDouble());
            primitive = std::make_unique<RectanglePrimitive>(topLeft, bottomRight);
            break;
        }
        case PrimitiveType::Ellipse: {
            QVector2D center(json["centerX"].toDouble(), json["centerY"].toDouble());
            float radiusX = json["radiusX"].toDouble();
            float radiusY = json["radiusY"].toDouble();
            primitive = std::make_unique<EllipsePrimitive>(center, radiusX, radiusY);
            break;
        }
        case PrimitiveType::Circle: {
            QVector2D center(json["centerX"].toDouble(), json["centerY"].toDouble());
            float radius = json["radius"].toDouble();
            primitive = std::make_unique<CirclePrimitive>(center, radius);
            break;
        }
        case PrimitiveType::Arc: {
            QVector2D center(json["centerX"].toDouble(), json["centerY"].toDouble());
            float radius = json["radius"].toDouble();
            float startAngle = json["startAngle"].toDouble();
            float endAngle = json["endAngle"].toDouble();
            primitive = std::make_unique<ArcPrimitive>(center, radius, startAngle, endAngle);
            break;
        }
        case PrimitiveType::Curve: {
            primitive = std::make_unique<CurvePrimitive>();
            break;
        }
        case PrimitiveType::BezierCurve: {
            primitive = std::make_unique<BezierCurvePrimitive>();
            break;
        }
        case PrimitiveType::Spline: {
            primitive = std::make_unique<SplinePrimitive>();
            break;
        }
        case PrimitiveType::Polygon: {
            primitive = std::make_unique<PolygonPrimitive>();
            break;
        }
        case PrimitiveType::Dimension: {
            QVector2D start(json["startX"].toDouble(), json["startY"].toDouble());
            QVector2D end(json["endX"].toDouble(), json["endY"].toDouble());
            primitive = std::make_unique<DimensionPrimitive>(start, end);
            break;
        }
        case PrimitiveType::Text: {
            QVector2D position(json["positionX"].toDouble(), json["positionY"].toDouble());
            QString text = json["text"].toString();
            primitive = std::make_unique<TextPrimitive>(position, text);
            break;
        }
        case PrimitiveType::Image: {
            // Image primitives need special handling - create empty and let fromJson restore
            auto imagePrim = std::make_unique<ImagePrimitive>();
            primitive = std::move(imagePrim);
            break;
        }
        default:
            qDebug() << "Error: Unknown primitive type:" << static_cast<int>(type);
            return nullptr;
    }
    
    // Restore common properties from JSON
    if (primitive) {
        primitive->fromJson(json);
    }
    
    return primitive;
}

void DrawingPrimitive::renderShadow(QPainter* painter) const
{
    if (!m_shadowEnabled || m_shadowBlur <= 0 || !painter) return;
    // Shadow rendering is handled per-primitive in their render() methods
}

// LinePrimitive implementation
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

// RectanglePrimitive implementation
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

// EllipsePrimitive implementation
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

// CurvePrimitive implementation
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
    m_curveType = json["curveType"].toInt(0);
    m_showControlPolygon = json["showControlPolygon"].toBool(false);
}

// DimensionPrimitive implementation
DimensionPrimitive::DimensionPrimitive(const QVector2D &start, const QVector2D &end)
    : DrawingPrimitive(PrimitiveType::Dimension)
    , m_start(start)
    , m_end(end)
    , m_unitsString("mm")
    , m_measurementValue(0.0f)
    , m_pixelsPerUnit(1.0f)
{
    setColor(QColor(255, 200, 0)); // Yellow/orange for dimensions
}

float DimensionPrimitive::measuredLength() const
{
    const float worldLen = (m_end - m_start).length();
    return worldLen / std::max(0.0001f, m_pixelsPerUnit);
}

void DimensionPrimitive::setPixelsPerUnit(float ppu)
{
    m_pixelsPerUnit = std::max(0.0001f, ppu);
}

void DimensionPrimitive::recalculateMeasurement()
{
    m_measurementValue = measuredLength();
}

void DimensionPrimitive::render(QPainter* painter) const
{
    if (!m_visible || !painter) return;

    QColor strokeColor = m_selected ? QColor(255, 165, 0) : m_color;
    strokeColor.setAlphaF(m_opacityMultiplier);
    QPen pen(strokeColor, 1.5);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    QVector2D delta = m_end - m_start;
    if (delta.length() < 0.5f) {
        return;
    }

    QVector2D direction = delta.normalized();
    QVector2D perpendicular(-direction.y(), direction.x());
    float offset = 20.0f;

    QVector2D dimStart = m_start + perpendicular * offset;
    QVector2D dimEnd = m_end + perpendicular * offset;

    // Extension lines
    painter->drawLine(QPointF(m_start.x(), m_start.y()), QPointF(dimStart.x(), dimStart.y()));
    painter->drawLine(QPointF(m_end.x(), m_end.y()), QPointF(dimEnd.x(), dimEnd.y()));
    // Main dimension line
    painter->drawLine(QPointF(dimStart.x(), dimStart.y()), QPointF(dimEnd.x(), dimEnd.y()));

    renderArrows(painter, dimStart, dimEnd, direction);
    // Label is drawn in screen space by DrawingCanvas::renderDimensionTexts
    // so it stays readable at any zoom.
}

void DimensionPrimitive::renderArrows(QPainter* painter, const QVector2D &start, const QVector2D &end, const QVector2D &direction) const
{
    float arrowSize = 5.0f;
    QVector2D perpendicular(-direction.y(), direction.x());

    // Arrow at start
    painter->drawLine(QPointF(start.x(), start.y()),
                      QPointF(start.x() + direction.x() * arrowSize + perpendicular.x() * arrowSize * 0.5f,
                              start.y() + direction.y() * arrowSize + perpendicular.y() * arrowSize * 0.5f));
    painter->drawLine(QPointF(start.x(), start.y()),
                      QPointF(start.x() + direction.x() * arrowSize - perpendicular.x() * arrowSize * 0.5f,
                              start.y() + direction.y() * arrowSize - perpendicular.y() * arrowSize * 0.5f));

    // Arrow at end
    painter->drawLine(QPointF(end.x(), end.y()),
                      QPointF(end.x() - direction.x() * arrowSize + perpendicular.x() * arrowSize * 0.5f,
                              end.y() - direction.y() * arrowSize + perpendicular.y() * arrowSize * 0.5f));
    painter->drawLine(QPointF(end.x(), end.y()),
                      QPointF(end.x() - direction.x() * arrowSize - perpendicular.x() * arrowSize * 0.5f,
                              end.y() - direction.y() * arrowSize - perpendicular.y() * arrowSize * 0.5f));
}

void DimensionPrimitive::renderText(QPainter* painter, const QVector2D &position, const QString &text) const
{
    if (!painter) return;
    painter->save();
    painter->translate(position.x(), position.y());
    painter->scale(1, -1); // Un-flip Y for text
    QFont font("Arial", 8);
    painter->setFont(font);
    painter->drawText(QPointF(0, 0), text);
    painter->restore();
}

QString DimensionPrimitive::getDisplayText() const
{
    return QString("%1 %2").arg(measuredLength(), 0, 'f', 2).arg(m_unitsString);
}

// Control point implementations for each primitive type

// LinePrimitive control points
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

// RectanglePrimitive control points
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

// EllipsePrimitive control points
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

// CurvePrimitive control points
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

// BezierCurvePrimitive control points
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

// SplinePrimitive control points
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

// ArcPrimitive control points
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

// CirclePrimitive control points
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

// PolygonPrimitive control points
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

// DimensionPrimitive control points
std::vector<QVector2D> DimensionPrimitive::getControlPoints() const
{
    return {m_start, m_end};
}

void DimensionPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index == 0) {
        m_start = position;
    } else if (index == 1) {
        m_end = position;
    }
    recalculateMeasurement();
}

QRectF DimensionPrimitive::boundingRect() const
{
    float minX = std::min(m_start.x(), m_end.x());
    float minY = std::min(m_start.y(), m_end.y());
    float maxX = std::max(m_start.x(), m_end.x());
    float maxY = std::max(m_start.y(), m_end.y());
    
    // Add padding for dimension lines and text
    float padding = 30.0f;
    return QRectF(minX - padding, minY - padding,
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool DimensionPrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    // Check if point is near the dimension line or extension lines
    LinePrimitive mainLine(m_start, m_end);
    return mainLine.containsPoint(point, tolerance * 2.0f); // More generous tolerance
}

std::unique_ptr<DrawingPrimitive> DimensionPrimitive::clone() const
{
    auto cloned = std::make_unique<DimensionPrimitive>(m_start, m_end);
    cloned->setColor(m_color);
    if (m_hasFillColor) {
        cloned->setFillColor(m_fillColor);
    } else {
        cloned->clearFillColor();
    }
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setUnitsString(m_unitsString);
    cloned->setPixelsPerUnit(m_pixelsPerUnit);
    cloned->setMeasurementValue(m_measurementValue);
    return cloned;
}

void DimensionPrimitive::translate(const QVector2D& offset)
{
    m_start += offset;
    m_end += offset;
}

// BezierCurvePrimitive Implementation
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
    m_subdivisionLevel = json["subdivisionLevel"].toInt(50);
    m_autoTangents = json["autoTangents"].toBool(false);
    m_symmetricHandles = json["symmetricHandles"].toBool(false);
}

// ArcPrimitive Implementation
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

// CirclePrimitive Implementation
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

// SplinePrimitive Implementation - For smooth, realistic guitar curves
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
    m_smoothness = json["smoothness"].toDouble(0.5);
    m_interpolationType = json["interpolationType"].toInt(0);
    m_tension = json["tension"].toDouble(0.5);
    m_autoSmooth = json["autoSmooth"].toBool(false);
    m_showPoints = json["showPoints"].toBool(true);
}

// PolygonPrimitive implementation
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

// TextPrimitive implementation
TextPrimitive::TextPrimitive(const QVector2D &position, const QString &text)
    : DrawingPrimitive(PrimitiveType::Text)
    , m_position(position)
    , m_text(text)
    , m_fontFamily("Arial")
    , m_fontSize(20.0f)
    , m_bold(false)
    , m_italic(false)
    , m_underline(false)
    , m_baselineShift(BaselineShift::Normal)
    , m_rotation(0.0f)
    , m_scale(1.0f)
    , m_followsSpline(false)
    , m_splineId(QUuid())
    , m_pathOffset(0.0f)
    , m_alignment(TextAlignment::Left)
    , m_letterSpacing(0.0f)
    , m_lineSpacing(1.0f)
    , m_textBoxWidth(0.0f)
    , m_textBoxHeight(0.0f)
{
}

void TextPrimitive::render(QPainter* painter) const
{
    if (!m_visible) {
        return;
    }
    // All visual rendering is handled by DrawingCanvas::renderFormattedText()
    Q_UNUSED(painter);
}

QRectF TextPrimitive::boundingRect() const
{
    // The canvas world coordinate system has Y pointing UP (math convention)
    // but text renders DOWNWARD on screen from m_position.
    // So in world space the bounding rect extends in the -Y direction.

    float w, h;
    if (m_textBoxWidth > 0 && m_textBoxHeight > 0) {
        w = m_textBoxWidth * m_scale;
        h = m_textBoxHeight * m_scale;
    } else {
        // No explicit box — measure from font metrics
        QFont baseFont(m_fontFamily, m_fontSize);
        baseFont.setBold(m_bold);
        baseFont.setItalic(m_italic);
        baseFont.setUnderline(m_underline);

        QFontMetrics metrics(baseFont);

        const QStringList lines = m_text.split('\n');
        float maxLineWidth = 0.0f;

        for (const QString& line : lines) {
            qreal lineWidth = metrics.horizontalAdvance(line);
            if (!line.isEmpty()) {
                lineWidth += qMax(0, line.size() - 1) * m_letterSpacing;
            }
            maxLineWidth = std::max(maxLineWidth, static_cast<float>(lineWidth));
        }

        if (lines.isEmpty()) {
            maxLineWidth = metrics.horizontalAdvance(" ");
        }

        float lineHeight = metrics.height() * m_lineSpacing;
        const qsizetype lineCount = std::max<qsizetype>(qsizetype(1), lines.size());
        float contentHeight = lineHeight * static_cast<float>(lineCount);

        w = maxLineWidth * m_scale;
        h = contentHeight * m_scale;
    }

    // Rect extends downward in screen = negative Y in world space
    return QRectF(m_position.x(), m_position.y() - h, w, h);
}

bool TextPrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    QRectF bounds = boundingRect();
    bounds.adjust(-tolerance, -tolerance, tolerance, tolerance);
    return bounds.contains(point.x(), point.y());
}

std::unique_ptr<DrawingPrimitive> TextPrimitive::clone() const
{
    auto cloned = std::make_unique<TextPrimitive>(m_position, m_text);
    cloned->setFontFamily(m_fontFamily);
    cloned->setFontSize(m_fontSize);
    cloned->setBold(m_bold);
    cloned->setItalic(m_italic);
    cloned->setUnderline(m_underline);
    cloned->setBaselineShift(m_baselineShift);
    cloned->setRotation(m_rotation);
    cloned->setScale(m_scale);
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setAlignment(m_alignment);
    cloned->setLetterSpacing(m_letterSpacing);
    cloned->setLineSpacing(m_lineSpacing);
    cloned->setTextBoxWidth(m_textBoxWidth);
    cloned->setTextBoxHeight(m_textBoxHeight);
    return cloned;
}

void TextPrimitive::translate(const QVector2D& offset)
{
    m_position += offset;
}

std::vector<QVector2D> TextPrimitive::getControlPoints() const
{
    return {m_position};
}

void TextPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (index == 0) {
        m_position = position;
    }
}

void TextPrimitive::renderSelectionHandles(QPainter* painter) const
{
    // Selection visuals are handled by QPainter path rendering in DrawingCanvas
    Q_UNUSED(painter);
}

bool TextPrimitive::isPointOnHandle(const QVector2D& point, int* handleIndex) const
{
    const QRectF bounds = boundingRect();
    const float handleSize = 12.0f;

    const QVector2D handleCenters[8] = {
        QVector2D(bounds.left(), bounds.top()),
        QVector2D(bounds.center().x(), bounds.top()),
        QVector2D(bounds.right(), bounds.top()),
        QVector2D(bounds.right(), bounds.center().y()),
        QVector2D(bounds.right(), bounds.bottom()),
        QVector2D(bounds.center().x(), bounds.bottom()),
        QVector2D(bounds.left(), bounds.bottom()),
        QVector2D(bounds.left(), bounds.center().y())
    };

    for (int i = 0; i < 8; ++i) {
        QRectF handleRect(handleCenters[i].x() - handleSize * 0.5f,
                          handleCenters[i].y() - handleSize * 0.5f,
                          handleSize,
                          handleSize);
        if (handleRect.contains(point.x(), point.y())) {
            if (handleIndex) {
                *handleIndex = i;
            }
            return true;
        }
    }

    return false;
}

QRectF TextPrimitive::getHandleRect(int handleIndex) const
{
    const float handleSize = 6.0f;
    QRectF bounds = boundingRect();
    
    QVector2D handles[8] = {
        QVector2D(bounds.left(), bounds.top()),
        QVector2D(bounds.center().x(), bounds.top()),
        QVector2D(bounds.right(), bounds.top()),
        QVector2D(bounds.right(), bounds.center().y()),
        QVector2D(bounds.right(), bounds.bottom()),
        QVector2D(bounds.center().x(), bounds.bottom()),
        QVector2D(bounds.left(), bounds.bottom()),
        QVector2D(bounds.left(), bounds.center().y())
    };
    
    if (handleIndex >= 0 && handleIndex < 8) {
        QVector2D handle = handles[handleIndex];
        return QRectF(handle.x() - handleSize/2, handle.y() - handleSize/2, 
                     handleSize, handleSize);
    }
    
    return QRectF();
}

void TextPrimitive::resizeFromHandle(int handleIndex, const QVector2D& newPosition)
{
    QRectF bounds = boundingRect();

    float left = bounds.left();
    float right = bounds.right();
    float top = bounds.top();
    float bottom = bounds.bottom();

    switch (handleIndex) {
        case 0: // Top-left
            left = newPosition.x();
            top = newPosition.y();
            break;
        case 1: // Top-center
            top = newPosition.y();
            break;
        case 2: // Top-right
            right = newPosition.x();
            top = newPosition.y();
            break;
        case 3: // Right-center
            right = newPosition.x();
            break;
        case 4: // Bottom-right
            right = newPosition.x();
            bottom = newPosition.y();
            break;
        case 5: // Bottom-center
            bottom = newPosition.y();
            break;
        case 6: // Bottom-left
            left = newPosition.x();
            bottom = newPosition.y();
            break;
        case 7: // Left-center
            left = newPosition.x();
            break;
        default:
            return;
    }

    constexpr float minDimension = 20.0f;

    // Enforce minimum dimensions
    if (right - left < minDimension) {
        if (handleIndex == 0 || handleIndex == 6 || handleIndex == 7) {
            left = right - minDimension;
        } else {
            right = left + minDimension;
        }
    }

    if (bottom - top < minDimension) {
        if (handleIndex == 0 || handleIndex == 1 || handleIndex == 2) {
            top = bottom - minDimension;
        } else {
            bottom = top + minDimension;
        }
    }

    // m_position is the screen-top-left of the text.
    // In Y-up world coords, screen-top = world max Y = QRectF bottom.
    m_position.setX(left);
    m_position.setY(bottom);

    float newWidth = std::max(1.0f, right - left);
    float newHeight = std::max(1.0f, bottom - top);

    const float scaleSafe = std::max(0.001f, m_scale);
    m_textBoxWidth = newWidth / scaleSafe;
    m_textBoxHeight = newHeight / scaleSafe;
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

QJsonObject DimensionPrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["startX"] = m_start.x();
    json["startY"] = m_start.y();
    json["endX"] = m_end.x();
    json["endY"] = m_end.y();
    json["unitsString"] = m_unitsString;
    json["measurementValue"] = m_measurementValue;
    json["pixelsPerUnit"] = m_pixelsPerUnit;
    return json;
}

void DimensionPrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_start = QVector2D(json["startX"].toDouble(), json["startY"].toDouble());
    m_end = QVector2D(json["endX"].toDouble(), json["endY"].toDouble());
    m_unitsString = json["unitsString"].toString("mm");
    m_pixelsPerUnit = static_cast<float>(json["pixelsPerUnit"].toDouble(1.0));
    if (json.contains("measurementValue")) {
        m_measurementValue = json["measurementValue"].toDouble();
    } else {
        recalculateMeasurement();
    }
}

QJsonObject TextPrimitive::toJson() const
{
    QJsonObject json = DrawingPrimitive::toJson();
    json["positionX"] = m_position.x();
    json["positionY"] = m_position.y();
    json["text"] = m_text;
    json["fontFamily"] = m_fontFamily;
    json["fontSize"] = m_fontSize;
    json["bold"] = m_bold;
    json["italic"] = m_italic;
    json["underline"] = m_underline;
    json["baselineShift"] = static_cast<int>(m_baselineShift);
    json["rotation"] = m_rotation;
    json["scale"] = m_scale;
    json["followsSpline"] = m_followsSpline;
    json["splineId"] = m_splineId.toString();
    json["pathOffset"] = m_pathOffset;
    json["alignment"] = static_cast<int>(m_alignment);
    json["letterSpacing"] = m_letterSpacing;
    json["lineSpacing"] = m_lineSpacing;
    json["textBoxWidth"] = m_textBoxWidth;
    json["textBoxHeight"] = m_textBoxHeight;
    
    QJsonObject shadow;
    shadow["enabled"] = m_dropShadow.enabled;
    shadow["color"] = m_dropShadow.color.name(QColor::HexArgb);
    shadow["offsetX"] = m_dropShadow.offsetX;
    shadow["offsetY"] = m_dropShadow.offsetY;
    shadow["blur"] = m_dropShadow.blur;
    shadow["angle"] = m_dropShadow.angle;
    shadow["distance"] = m_dropShadow.distance;
    json["dropShadow"] = shadow;
    
    QJsonObject stroke;
    stroke["enabled"] = m_stroke.enabled;
    stroke["color"] = m_stroke.color.name(QColor::HexArgb);
    stroke["width"] = m_stroke.width;
    json["stroke"] = stroke;
    
    QJsonObject gradient;
    gradient["enabled"] = m_gradient.enabled;
    gradient["startColor"] = m_gradient.startColor.name(QColor::HexArgb);
    gradient["endColor"] = m_gradient.endColor.name(QColor::HexArgb);
    gradient["angle"] = m_gradient.angle;
    json["gradient"] = gradient;
    
    return json;
}

void TextPrimitive::fromJson(const QJsonObject& json)
{
    DrawingPrimitive::fromJson(json);
    m_position = QVector2D(json["positionX"].toDouble(), json["positionY"].toDouble());
    m_text = json["text"].toString();
    m_fontFamily = json["fontFamily"].toString("Arial");
    m_fontSize = json["fontSize"].toDouble(20.0);
    m_bold = json["bold"].toBool(false);
    m_italic = json["italic"].toBool(false);
    m_underline = json["underline"].toBool(false);
    m_baselineShift = static_cast<BaselineShift>(json["baselineShift"].toInt(0));
    m_rotation = json["rotation"].toDouble(0.0);
    m_scale = json["scale"].toDouble(1.0);
    m_followsSpline = json["followsSpline"].toBool(false);
    if (json.contains("splineId")) {
        m_splineId = QUuid(json["splineId"].toString());
    }
    m_pathOffset = json["pathOffset"].toDouble(0.0);
    m_alignment = static_cast<TextAlignment>(json["alignment"].toInt(0));
    m_letterSpacing = json["letterSpacing"].toDouble(0.0);
    m_lineSpacing = json["lineSpacing"].toDouble(1.0);
    m_textBoxWidth = json["textBoxWidth"].toDouble(0.0);
    m_textBoxHeight = json["textBoxHeight"].toDouble(0.0);
    
    if (json.contains("dropShadow")) {
        QJsonObject shadow = json["dropShadow"].toObject();
        m_dropShadow.enabled = shadow["enabled"].toBool(false);
        m_dropShadow.color = QColor(shadow["color"].toString("#80000000"));
        m_dropShadow.offsetX = shadow["offsetX"].toDouble(0.0);
        m_dropShadow.offsetY = shadow["offsetY"].toDouble(0.0);
        m_dropShadow.blur = shadow["blur"].toDouble(0.0);
        m_dropShadow.angle = shadow["angle"].toDouble(45.0);
        m_dropShadow.distance = shadow["distance"].toDouble(0.0);
    }
    
    if (json.contains("stroke")) {
        QJsonObject stroke = json["stroke"].toObject();
        m_stroke.enabled = stroke["enabled"].toBool(false);
        m_stroke.color = QColor(stroke["color"].toString("#000000"));
        m_stroke.width = stroke["width"].toDouble(2.0);
    }
    
    if (json.contains("gradient")) {
        QJsonObject gradient = json["gradient"].toObject();
        m_gradient.enabled = gradient["enabled"].toBool(false);
        m_gradient.startColor = QColor(gradient["startColor"].toString("#FFFFFF"));
        m_gradient.endColor = QColor(gradient["endColor"].toString("#000000"));
        m_gradient.angle = gradient["angle"].toDouble(0.0);
    }
}
