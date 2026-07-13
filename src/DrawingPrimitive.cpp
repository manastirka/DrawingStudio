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
#include <QDebug>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <vector>
#include <array>
// Base DrawingPrimitive (refactor E21).

// --- DrawingPrimitive ---

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

