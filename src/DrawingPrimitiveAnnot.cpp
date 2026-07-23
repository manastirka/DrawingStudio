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
// Dimension + Text primitives (refactor E21).

// --- DimensionPrimitive ---

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
    if (std::isfinite(ppu))
        m_pixelsPerUnit = std::clamp(ppu, 0.0001f, 1.0e9f);
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

std::vector<QVector2D> DimensionPrimitive::getControlPoints() const
{
    return {m_start, m_end};
}

void DimensionPrimitive::setControlPointPosition(int index, const QVector2D& position)
{
    if (!isSupportedPoint(position))
        return;
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

// --- TextPrimitive ---

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
    if (!isSupportedPoint(position))
        return;
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
