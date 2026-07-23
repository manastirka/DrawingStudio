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
#include <initializer_list>
#include <limits>
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

void DrawingPrimitive::setOpacityMultiplier(float multiplier)
{
    if (std::isfinite(multiplier))
        m_opacityMultiplier = std::clamp(multiplier, 0.0f, 1.0f);
}

void DrawingPrimitive::setLineWidth(float width)
{
    if (std::isfinite(width))
        m_lineWidth = std::clamp(width, 0.0f, kMaxLineWidth);
}

void DrawingPrimitive::setLineStyle(Qt::PenStyle style)
{
    const int value = static_cast<int>(style);
    if (value >= static_cast<int>(Qt::NoPen)
        && value <= static_cast<int>(Qt::CustomDashLine)) {
        m_lineStyle = style;
    }
}

void DrawingPrimitive::setRotationDegrees(float degrees)
{
    if (std::isfinite(degrees))
        m_rotationDegrees = std::clamp(degrees, -360.0f, 360.0f);
}

void DrawingPrimitive::setGradientAngle(float angle)
{
    if (std::isfinite(angle))
        m_gradientAngle = std::clamp(angle, -360.0f, 360.0f);
}

void DrawingPrimitive::setShadowOffset(float x, float y)
{
    if (std::isfinite(x))
        m_shadowOffsetX = std::clamp(x, -kMaxShadowOffset, kMaxShadowOffset);
    if (std::isfinite(y))
        m_shadowOffsetY = std::clamp(y, -kMaxShadowOffset, kMaxShadowOffset);
}

void DrawingPrimitive::setShadowBlur(float blur)
{
    if (std::isfinite(blur))
        m_shadowBlur = std::clamp(blur, 0.0f, kMaxShadowBlur);
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
    setColor(QColor(json["color"].toString(m_color.name(QColor::HexArgb))));
    const QColor loadedFill(
        json["fillColor"].toString(m_fillColor.name(QColor::HexArgb)));
    if (loadedFill.isValid())
        m_fillColor = loadedFill;
    m_hasFillColor = json["hasFillColor"].toBool(m_hasFillColor);
    setLineWidth(static_cast<float>(
        json["lineWidth"].toDouble(m_lineWidth)));
    setLineStyle(static_cast<Qt::PenStyle>(
        json["lineStyle"].toInt(static_cast<int>(m_lineStyle))));
    m_selected = json["selected"].toBool(m_selected);
    m_visible = json["visible"].toBool(m_visible);
    setOpacityMultiplier(static_cast<float>(
        json["opacityMultiplier"].toDouble(m_opacityMultiplier)));
    m_layerId = QUuid(json["layerId"].toString(m_layerId.toString()));

    m_shadowEnabled = json["shadowEnabled"].toBool(m_shadowEnabled);
    setShadowOffset(
        static_cast<float>(json["shadowOffsetX"].toDouble(m_shadowOffsetX)),
        static_cast<float>(json["shadowOffsetY"].toDouble(m_shadowOffsetY)));
    setShadowBlur(static_cast<float>(
        json["shadowBlur"].toDouble(m_shadowBlur)));
    setShadowColor(QColor(
        json["shadowColor"].toString(m_shadowColor.name(QColor::HexArgb))));
    setRotationDegrees(static_cast<float>(
        json["rotationDegrees"].toDouble(m_rotationDegrees)));
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
    dst->setOpacityMultiplier(m_opacityMultiplier);
    dst->setVisible(m_visible);
    dst->setRotationDegrees(m_rotationDegrees);
    dst->setGroupId(m_groupId);
    dst->setShadowEnabled(m_shadowEnabled);
    dst->setShadowBlur(m_shadowBlur);
    dst->setShadowOffset(m_shadowOffsetX, m_shadowOffsetY);
    dst->setShadowColor(m_shadowColor);
}

qsizetype DrawingPrimitive::serializedPointCount(const QJsonObject &json)
{
    const int typeValue = json.value(QStringLiteral("type")).toInt(-1);
    switch (static_cast<PrimitiveType>(typeValue)) {
    case PrimitiveType::Curve:
    case PrimitiveType::BezierCurve:
        return json.value(QStringLiteral("controlPoints")).isArray()
                   ? json.value(QStringLiteral("controlPoints")).toArray().size()
                   : 0;
    case PrimitiveType::Spline:
    case PrimitiveType::Polygon:
        return json.value(QStringLiteral("points")).isArray()
                   ? json.value(QStringLiteral("points")).toArray().size()
                   : 0;
    case PrimitiveType::Image: {
        qsizetype count =
            json.value(QStringLiteral("maskContour")).isArray()
                ? json.value(QStringLiteral("maskContour")).toArray().size()
                : 0;
        const QJsonArray candidates =
            json.value(QStringLiteral("maskCandidates")).toArray();
        for (const QJsonValue &candidateValue : candidates) {
            if (!candidateValue.isObject())
                continue;
            const QJsonValue contour =
                candidateValue.toObject().value(QStringLiteral("contour"));
            if (!contour.isArray())
                continue;
            const qsizetype remaining =
                std::numeric_limits<qsizetype>::max() - count;
            if (contour.toArray().size() > remaining)
                return std::numeric_limits<qsizetype>::max();
            count += contour.toArray().size();
        }
        return count;
    }
    default:
        return 0;
    }
}

bool DrawingPrimitive::validateJson(const QJsonObject &json, QString *error)
{
    const auto fail = [error](const QString &reason) {
        if (error)
            *error = reason;
        return false;
    };

    const QJsonValue typeValue = json.value(QStringLiteral("type"));
    if (!typeValue.isDouble())
        return fail(QStringLiteral("primitive type is missing or invalid"));
    const double rawType = typeValue.toDouble();
    if (!std::isfinite(rawType) || std::floor(rawType) != rawType
        || rawType < static_cast<int>(PrimitiveType::Line)
        || rawType > static_cast<int>(PrimitiveType::Image)) {
        return fail(QStringLiteral("primitive type is unsupported"));
    }

    const auto validateOptionalNumber = [&json, &fail](const char *key) {
        const QString field = QString::fromLatin1(key);
        if (!json.contains(field))
            return true;
        const QJsonValue value = json.value(field);
        if (!value.isDouble() || !std::isfinite(value.toDouble()))
            return fail(field + QStringLiteral(" is not a finite number"));
        return true;
    };
    for (const char *key : {"lineWidth", "opacityMultiplier",
                            "rotationDegrees", "shadowOffsetX",
                            "shadowOffsetY", "shadowBlur"}) {
        if (!validateOptionalNumber(key))
            return false;
    }

    if (json.contains(QStringLiteral("lineStyle"))) {
        const QJsonValue styleValue = json.value(QStringLiteral("lineStyle"));
        const double rawStyle = styleValue.toDouble(-1);
        if (!styleValue.isDouble() || std::floor(rawStyle) != rawStyle
            || rawStyle < static_cast<int>(Qt::NoPen)
            || rawStyle > static_cast<int>(Qt::CustomDashLine)) {
            return fail(QStringLiteral("lineStyle is invalid"));
        }
    }

    for (const char *key : {"color", "fillColor", "shadowColor"}) {
        const QString field = QString::fromLatin1(key);
        if (json.contains(field)
            && (!json.value(field).isString()
                || !QColor(json.value(field).toString()).isValid())) {
            return fail(field + QStringLiteral(" is invalid"));
        }
    }

    const auto validateUuid =
        [&json, &fail](const char *key, bool allowNull) {
            const QString field = QString::fromLatin1(key);
            if (!json.contains(field))
                return true;
            const QJsonValue value = json.value(field);
            if (!value.isString())
                return fail(field + QStringLiteral(" is invalid"));
            const QString text = value.toString();
            const QUuid id(text);
            const bool canonicalNull = text == QUuid().toString();
            if ((id.isNull() && !canonicalNull)
                || (!allowNull && id.isNull())) {
                return fail(field + QStringLiteral(" is invalid"));
            }
            return true;
        };
    if (!validateUuid("id", false)
        || !validateUuid("layerId", true)
        || !validateUuid("groupId", false)) {
        return false;
    }

    const auto validateRequiredNumber =
        [&json, &fail](const char *key, double minimum, double maximum) {
            const QString field = QString::fromLatin1(key);
            const QJsonValue value = json.value(field);
            if (!value.isDouble() || !std::isfinite(value.toDouble()))
                return fail(field + QStringLiteral(" is missing or invalid"));
            const double number = value.toDouble();
            if (number < minimum || number > maximum)
                return fail(field + QStringLiteral(" is outside the supported range"));
            return true;
        };
    const auto validateCoordinates =
        [&validateRequiredNumber](std::initializer_list<const char *> keys) {
            for (const char *key : keys) {
                if (!validateRequiredNumber(
                        key, -DrawingPrimitive::kMaxSerializedCoordinateMagnitude,
                        DrawingPrimitive::kMaxSerializedCoordinateMagnitude)) {
                    return false;
                }
            }
            return true;
        };
    const auto validateNonNegativeGeometry =
        [&validateRequiredNumber](std::initializer_list<const char *> keys,
                                  bool allowZero = true) {
            const double minimum = allowZero
                                       ? 0.0
                                       : std::numeric_limits<double>::min();
            for (const char *key : keys) {
                if (!validateRequiredNumber(
                        key, minimum,
                        DrawingPrimitive::kMaxSerializedCoordinateMagnitude)) {
                    return false;
                }
            }
            return true;
        };
    const auto validateObjectNumber =
        [&fail](const QJsonObject &object, const char *key, double minimum,
                double maximum, const QString &prefix = QString()) {
            const QString field = QString::fromLatin1(key);
            if (!object.contains(field))
                return true;
            const QJsonValue value = object.value(field);
            const double number = value.toDouble();
            if (!value.isDouble() || !std::isfinite(number)
                || number < minimum || number > maximum) {
                return fail(prefix + field + QStringLiteral(" is invalid"));
            }
            return true;
        };
    const auto validateObjectBool =
        [&fail](const QJsonObject &object, const char *key,
                const QString &prefix = QString()) {
            const QString field = QString::fromLatin1(key);
            if (object.contains(field) && !object.value(field).isBool())
                return fail(prefix + field + QStringLiteral(" is invalid"));
            return true;
        };
    const auto validateObjectColor =
        [&fail](const QJsonObject &object, const char *key,
                const QString &prefix = QString()) {
            const QString field = QString::fromLatin1(key);
            if (object.contains(field)
                && (!object.value(field).isString()
                    || !QColor(object.value(field).toString()).isValid())) {
                return fail(prefix + field + QStringLiteral(" is invalid"));
            }
            return true;
        };
    const auto validateObjectInteger =
        [&fail](const QJsonObject &object, const char *key, int minimum,
                int maximum, const QString &prefix = QString()) {
            const QString field = QString::fromLatin1(key);
            if (!object.contains(field))
                return true;
            const QJsonValue value = object.value(field);
            const double number = value.toDouble();
            if (!value.isDouble() || !std::isfinite(number)
                || std::floor(number) != number
                || number < minimum || number > maximum) {
                return fail(prefix + field + QStringLiteral(" is invalid"));
            }
            return true;
        };

    QString pointKey;
    switch (static_cast<PrimitiveType>(static_cast<int>(rawType))) {
    case PrimitiveType::Line:
        if (!validateCoordinates({"startX", "startY", "endX", "endY"}))
            return false;
        if (!validateUuid("connectedLineId", false))
            return false;
        return true;
    case PrimitiveType::Rectangle:
        if (!validateCoordinates(
                {"topLeftX", "topLeftY", "bottomRightX", "bottomRightY"})) {
            return false;
        }
        return true;
    case PrimitiveType::Ellipse:
        if (!validateCoordinates({"centerX", "centerY"})
            || !validateNonNegativeGeometry({"radiusX", "radiusY"})) {
            return false;
        }
        return true;
    case PrimitiveType::Circle:
        if (!validateCoordinates({"centerX", "centerY"})
            || !validateNonNegativeGeometry({"radius"})) {
            return false;
        }
        return true;
    case PrimitiveType::Arc:
        if (!validateCoordinates({"centerX", "centerY"})
            || !validateNonNegativeGeometry({"radius"})
            || !validateCoordinates({"startAngle", "endAngle"})) {
            return false;
        }
        return true;
    case PrimitiveType::Curve:
    case PrimitiveType::BezierCurve:
        pointKey = QStringLiteral("controlPoints");
        break;
    case PrimitiveType::Spline:
    case PrimitiveType::Polygon:
        pointKey = QStringLiteral("points");
        break;
    case PrimitiveType::Dimension:
        if (!validateCoordinates({"startX", "startY", "endX", "endY"}))
            return false;
        return true;
    case PrimitiveType::Text:
        if (!validateCoordinates({"positionX", "positionY"}))
            return false;
        if (json.contains(QStringLiteral("text"))
            && (!json.value(QStringLiteral("text")).isString()
                || json.value(QStringLiteral("text")).toString().size()
                       > TextPrimitive::kMaxTextCharacters)) {
            return fail(QStringLiteral("text is invalid or exceeds the limit"));
        }
        if (json.contains(QStringLiteral("fontFamily"))
            && (!json.value(QStringLiteral("fontFamily")).isString()
                || json.value(QStringLiteral("fontFamily")).toString().size()
                       > TextPrimitive::kMaxFontFamilyCharacters)) {
            return fail(QStringLiteral("fontFamily is invalid or exceeds the limit"));
        }
        for (const char *key :
             {"bold", "italic", "underline", "followsSpline"}) {
            if (!validateObjectBool(json, key))
                return false;
        }
        if (!validateObjectInteger(json, "baselineShift", 0, 2)
            || !validateObjectInteger(json, "alignment", 0, 3)
            || !validateObjectNumber(json, "fontSize", 1.0,
                                     TextPrimitive::kMaxFontSize)
            || !validateObjectNumber(json, "rotation", -3600.0, 3600.0)
            || !validateObjectNumber(json, "scale", 0.001, 1000.0)
            || !validateObjectNumber(
                json, "pathOffset", -kMaxSerializedCoordinateMagnitude,
                kMaxSerializedCoordinateMagnitude)
            || !validateObjectNumber(json, "letterSpacing", -1000.0, 1000.0)
            || !validateObjectNumber(json, "lineSpacing", 0.01, 100.0)
            || !validateObjectNumber(
                json, "textBoxWidth", 0.0,
                kMaxSerializedCoordinateMagnitude)
            || !validateObjectNumber(
                json, "textBoxHeight", 0.0,
                kMaxSerializedCoordinateMagnitude)) {
            return false;
        }
        if (json.contains(QStringLiteral("splineId"))) {
            const QJsonValue splineId = json.value(QStringLiteral("splineId"));
            const QString idText = splineId.toString();
            if (!splineId.isString()
                || (QUuid(idText).isNull()
                    && idText != QUuid().toString())) {
                return fail(QStringLiteral("splineId is invalid"));
            }
        }
        for (const char *objectKey : {"dropShadow", "stroke", "gradient"}) {
            const QString field = QString::fromLatin1(objectKey);
            if (json.contains(field) && !json.value(field).isObject())
                return fail(field + QStringLiteral(" is invalid"));
        }
        if (json.contains(QStringLiteral("dropShadow"))) {
            const QJsonObject shadow =
                json.value(QStringLiteral("dropShadow")).toObject();
            if (!validateObjectBool(shadow, "enabled", "dropShadow.")
                || !validateObjectColor(shadow, "color", "dropShadow.")
                || !validateObjectNumber(shadow, "offsetX", -10000.0,
                                         10000.0, "dropShadow.")
                || !validateObjectNumber(shadow, "offsetY", -10000.0,
                                         10000.0, "dropShadow.")
                || !validateObjectNumber(shadow, "blur", 0.0, 1000.0,
                                         "dropShadow.")
                || !validateObjectNumber(shadow, "angle", -360.0, 360.0,
                                         "dropShadow.")
                || !validateObjectNumber(shadow, "distance", 0.0, 10000.0,
                                         "dropShadow.")) {
                return false;
            }
        }
        if (json.contains(QStringLiteral("stroke"))) {
            const QJsonObject stroke =
                json.value(QStringLiteral("stroke")).toObject();
            if (!validateObjectBool(stroke, "enabled", "stroke.")
                || !validateObjectColor(stroke, "color", "stroke.")
                || !validateObjectNumber(stroke, "width", 0.0, 1000.0,
                                         "stroke.")) {
                return false;
            }
        }
        if (json.contains(QStringLiteral("gradient"))) {
            const QJsonObject gradient =
                json.value(QStringLiteral("gradient")).toObject();
            if (!validateObjectBool(gradient, "enabled", "gradient.")
                || !validateObjectColor(gradient, "startColor", "gradient.")
                || !validateObjectColor(gradient, "endColor", "gradient.")
                || !validateObjectNumber(gradient, "angle", -360.0, 360.0,
                                         "gradient.")) {
                return false;
            }
        }
        return true;
    case PrimitiveType::Image:
        if (!validateCoordinates({"posX", "posY"})
            || !validateNonNegativeGeometry({"sizeX", "sizeY"}, false)
            || !validateCoordinates({"rotation"})) {
            return false;
        }
        for (const char *key :
             {"maintainAspectRatio", "maskInverted", "maskOverlayVisible"}) {
            if (!validateObjectBool(json, key))
                return false;
        }
        if (!validateObjectInteger(
                json, "contourSmoothness", 0,
                ImagePrimitive::kMaxContourSmoothness)
            || !validateObjectInteger(json, "maskFeather", 0,
                                      ImagePrimitive::kMaxMaskFeather)
            || !validateObjectInteger(json, "maskBlur", 0,
                                      ImagePrimitive::kMaxMaskBlur)
            || !validateObjectInteger(json, "maskExpand",
                                      -ImagePrimitive::kMaxMaskExpand,
                                      ImagePrimitive::kMaxMaskExpand)) {
            return false;
        }
        {
            qsizetype totalMaskPoints = 0;
            const auto validateMaskContour =
                [&fail, &totalMaskPoints](const QJsonArray &contour,
                                         const QString &field) {
                    if (contour.size()
                        > ImagePrimitive::kMaxSerializedMaskPoints
                              - totalMaskPoints) {
                        return fail(QStringLiteral(
                            "serialized mask points exceed the limit"));
                    }
                    totalMaskPoints += contour.size();
                    for (const QJsonValue &pointValue : contour) {
                        if (!pointValue.isObject())
                            return fail(field + QStringLiteral(
                                                    " point is invalid"));
                        const QJsonObject point = pointValue.toObject();
                        const QJsonValue x = point.value(QStringLiteral("x"));
                        const QJsonValue y = point.value(QStringLiteral("y"));
                        if (!x.isDouble() || !y.isDouble()
                            || !std::isfinite(x.toDouble())
                            || !std::isfinite(y.toDouble())
                            || std::abs(x.toDouble())
                                   > kMaxSerializedCoordinateMagnitude
                            || std::abs(y.toDouble())
                                   > kMaxSerializedCoordinateMagnitude) {
                            return fail(field + QStringLiteral(
                                                    " point is invalid"));
                        }
                    }
                    return true;
                };

            if (json.contains(QStringLiteral("maskContour"))) {
                const QJsonValue contour =
                    json.value(QStringLiteral("maskContour"));
                if (!contour.isArray()
                    || !validateMaskContour(contour.toArray(),
                                            QStringLiteral("maskContour"))) {
                    return false;
                }
            }

            qsizetype candidateCount = 0;
            if (json.contains(QStringLiteral("maskCandidates"))) {
                const QJsonValue candidatesValue =
                    json.value(QStringLiteral("maskCandidates"));
                if (!candidatesValue.isArray())
                    return fail(QStringLiteral("maskCandidates is invalid"));
                const QJsonArray candidates = candidatesValue.toArray();
                candidateCount = candidates.size();
                if (candidateCount
                    > ImagePrimitive::kMaxSerializedMaskCandidates) {
                    return fail(QStringLiteral(
                        "maskCandidates exceeds the limit"));
                }
                for (const QJsonValue &candidateValue : candidates) {
                    if (!candidateValue.isObject())
                        return fail(QStringLiteral(
                            "mask candidate is invalid"));
                    const QJsonObject candidate = candidateValue.toObject();
                    if (!candidate.value(QStringLiteral("contour")).isArray())
                        return fail(QStringLiteral(
                            "mask candidate contour is invalid"));
                    if (!validateObjectInteger(
                            candidate, "id",
                            std::numeric_limits<int>::min(),
                            std::numeric_limits<int>::max(),
                            QStringLiteral("maskCandidate."))
                        || !validateObjectNumber(
                            candidate, "score", 0.0, 1.0,
                            QStringLiteral("maskCandidate."))
                        || !validateObjectNumber(
                            candidate, "stability", 0.0, 1.0,
                            QStringLiteral("maskCandidate."))
                        || !validateObjectNumber(
                            candidate, "predicted_iou", 0.0, 1.0,
                            QStringLiteral("maskCandidate."))
                        || !validateObjectNumber(
                            candidate, "area_percent", 0.0, 100.0,
                            QStringLiteral("maskCandidate."))
                        || !validateMaskContour(
                            candidate.value(QStringLiteral("contour")).toArray(),
                            QStringLiteral("maskCandidate.contour"))) {
                        return false;
                    }
                }
            }

            if (json.contains(QStringLiteral("selectedMaskIndex"))) {
                if (!validateObjectInteger(
                        json, "selectedMaskIndex", -1,
                        candidateCount > 0
                            ? static_cast<int>(candidateCount - 1)
                            : -1)) {
                    return false;
                }
            }
            if (json.contains(QStringLiteral("selectedMaskIndices"))) {
                const QJsonValue selectedValue =
                    json.value(QStringLiteral("selectedMaskIndices"));
                if (!selectedValue.isArray())
                    return fail(QStringLiteral(
                        "selectedMaskIndices is invalid"));
                const QJsonArray selected = selectedValue.toArray();
                if (selected.size()
                    > ImagePrimitive::kMaxSerializedMaskCandidates) {
                    return fail(QStringLiteral(
                        "selectedMaskIndices exceeds the limit"));
                }
                for (const QJsonValue &indexValue : selected) {
                    const double index = indexValue.toDouble(-1.0);
                    if (!indexValue.isDouble() || std::floor(index) != index
                        || index < 0.0 || index >= candidateCount) {
                        return fail(QStringLiteral(
                            "selectedMaskIndices contains an invalid index"));
                    }
                }
            }
        }
        return true;
    default:
        return true;
    }

    const QJsonValue pointsValue = json.value(pointKey);
    if (!pointsValue.isArray())
        return fail(QStringLiteral("primitive point array is missing or invalid"));
    const QJsonArray points = pointsValue.toArray();
    if (points.size() > kMaxSerializedPointsPerPrimitive)
        return fail(QStringLiteral("primitive point array exceeds the limit"));

    for (const QJsonValue &pointValue : points) {
        if (!pointValue.isObject())
            return fail(QStringLiteral("primitive point is not an object"));
        const QJsonObject point = pointValue.toObject();
        const QJsonValue x = point.value(QStringLiteral("x"));
        const QJsonValue y = point.value(QStringLiteral("y"));
        if (!x.isDouble() || !y.isDouble()
            || !std::isfinite(x.toDouble()) || !std::isfinite(y.toDouble())
            || std::abs(x.toDouble()) > kMaxSerializedCoordinateMagnitude
            || std::abs(y.toDouble()) > kMaxSerializedCoordinateMagnitude) {
            return fail(QStringLiteral("primitive point coordinate is invalid"));
        }
    }
    return true;
}

std::unique_ptr<DrawingPrimitive> DrawingPrimitive::createFromJson(const QJsonObject& json)
{
    QString validationError;
    if (!validateJson(json, &validationError)) {
        qWarning() << "Error: Invalid serialized primitive:" << validationError;
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

    // Image JSON contains an embedded binary payload. Invalid image data must
    // invalidate the primitive so a project load fails before replacing the
    // current document instead of silently installing a blank image.
    if (type == PrimitiveType::Image) {
        auto *image = static_cast<ImagePrimitive *>(primitive.get());
        if (!json.value(QStringLiteral("imageData")).isString()
            || image->image().isNull()) {
            qWarning() << "Error: Invalid serialized image primitive";
            return nullptr;
        }
    }
    
    return primitive;
}

void DrawingPrimitive::renderShadow(QPainter* painter) const
{
    if (!m_shadowEnabled || m_shadowBlur <= 0 || !painter) return;
    // Shadow rendering is handled per-primitive in their render() methods
}
