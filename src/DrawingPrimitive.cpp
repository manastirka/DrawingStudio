#include "DrawingPrimitive.h"
#include <QOpenGLFunctions>
#include <cmath>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Silence OpenGL deprecation warnings on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

// Base DrawingPrimitive implementation
DrawingPrimitive::DrawingPrimitive(PrimitiveType type)
    : m_type(type)
    , m_color(255, 255, 255)  // White default
    , m_lineWidth(1.0f)
    , m_selected(false)
    , m_visible(true)
{
}

void DrawingPrimitive::renderControlPoints() const
{
    if (!m_selected) return;
    
    auto controlPoints = getControlPoints();
    if (controlPoints.empty()) return;
    
    glPointSize(8.0f);
    glColor3f(1.0f, 1.0f, 1.0f); // White control points
    
    glBegin(GL_POINTS);
    for (const auto& point : controlPoints) {
        glVertex2f(point.x(), point.y());
    }
    glEnd();
    
    // Draw control point outlines
    glPointSize(10.0f);
    glColor3f(0.0f, 0.0f, 0.0f); // Black outline
    
    glBegin(GL_POINTS);
    for (const auto& point : controlPoints) {
        glVertex2f(point.x(), point.y());
    }
    glEnd();
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

// LinePrimitive implementation
LinePrimitive::LinePrimitive(const QVector2D &start, const QVector2D &end)
    : DrawingPrimitive(PrimitiveType::Line)
    , m_start(start)
    , m_end(end)
{
}

void LinePrimitive::render() const
{
    if (!m_visible) return;
    
    // Set line width
    glLineWidth(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    
    // Set color
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color; // Orange when selected
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    
    // Draw line
    glBegin(GL_LINES);
    glVertex2f(m_start.x(), m_start.y());
    glVertex2f(m_end.x(), m_end.y());
    glEnd();
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
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
}

// RectanglePrimitive implementation
RectanglePrimitive::RectanglePrimitive(const QVector2D &topLeft, const QVector2D &bottomRight)
    : DrawingPrimitive(PrimitiveType::Rectangle)
    , m_topLeft(topLeft)
    , m_bottomRight(bottomRight)
    , m_filled(false)
{
}

void RectanglePrimitive::render() const
{
    if (!m_visible) return;
    
    // Set line width
    glLineWidth(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    
    // Set color
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    
    float left = m_topLeft.x();
    float top = m_topLeft.y();
    float right = m_bottomRight.x();
    float bottom = m_bottomRight.y();
    
    // Render with corner radius if specified
    if (m_cornerRadius > 0.0f) {
        // Debug: Show when rendering rounded rectangles
        // qDebug() << "Rendering rounded rectangle with radius:" << m_cornerRadius;
        renderRoundedRectangle(left, top, right, bottom, m_cornerRadius, m_filled);
    } else {
        // Standard rectangle
        if (m_filled) {
            glBegin(GL_QUADS);
            glVertex2f(left, top);
            glVertex2f(right, top);
            glVertex2f(right, bottom);
            glVertex2f(left, bottom);
            glEnd();
        } else {
            glBegin(GL_LINE_LOOP);
            glVertex2f(left, top);
            glVertex2f(right, top);
            glVertex2f(right, bottom);
            glVertex2f(left, bottom);
            glEnd();
        }
    }
}

QRectF RectanglePrimitive::boundingRect() const
{
    float minX = std::min(m_topLeft.x(), m_bottomRight.x());
    float minY = std::min(m_topLeft.y(), m_bottomRight.y());
    float maxX = std::max(m_topLeft.x(), m_bottomRight.x());
    float maxY = std::max(m_topLeft.y(), m_bottomRight.y());
    
    float padding = m_lineWidth * 2.0f;
    return QRectF(minX - padding, minY - padding,
                  (maxX - minX) + 2*padding, (maxY - minY) + 2*padding);
}

bool RectanglePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    float minX = std::min(m_topLeft.x(), m_bottomRight.x());
    float minY = std::min(m_topLeft.y(), m_bottomRight.y());
    float maxX = std::max(m_topLeft.x(), m_bottomRight.x());
    float maxY = std::max(m_topLeft.y(), m_bottomRight.y());
    
    if (m_filled) {
        // Point inside rectangle
        return point.x() >= minX && point.x() <= maxX &&
               point.y() >= minY && point.y() <= maxY;
    } else {
        // Improved border detection - check if point is near any edge
        bool nearLeft = std::abs(point.x() - minX) <= tolerance && point.y() >= minY && point.y() <= maxY;
        bool nearRight = std::abs(point.x() - maxX) <= tolerance && point.y() >= minY && point.y() <= maxY;
        bool nearTop = std::abs(point.y() - minY) <= tolerance && point.x() >= minX && point.x() <= maxX;
        bool nearBottom = std::abs(point.y() - maxY) <= tolerance && point.x() >= minX && point.x() <= maxX;
        
        return nearLeft || nearRight || nearTop || nearBottom;
    }
}

void RectanglePrimitive::renderRoundedRectangle(float left, float top, float right, float bottom, float radius, bool filled) const
{
    // Clamp radius to prevent oversized corners
    float width = right - left;
    float height = bottom - top;
    float maxRadius = std::min(width, height) * 0.5f;
    radius = std::min(radius, maxRadius);
    
    if (radius < 0.1f) {
        // Fall back to regular rectangle for very small radius
        if (filled) {
            glBegin(GL_QUADS);
            glVertex2f(left, top);
            glVertex2f(right, top);
            glVertex2f(right, bottom);
            glVertex2f(left, bottom);
            glEnd();
        } else {
            glBegin(GL_LINE_LOOP);
            glVertex2f(left, top);
            glVertex2f(right, top);
            glVertex2f(right, bottom);
            glVertex2f(left, bottom);
            glEnd();
        }
        return;
    }
    
    const int cornerSegments = 8; // Number of segments per corner
    
    if (filled) {
        glBegin(GL_TRIANGLE_FAN);
        // Center point
        glVertex2f((left + right) * 0.5f, (top + bottom) * 0.5f);
    } else {
        glBegin(GL_LINE_LOOP);
    }
    
    // Top-right corner
    float centerX = right - radius;
    float centerY = top + radius;
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = -M_PI * 0.5f + (M_PI * 0.5f * i / cornerSegments);
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        glVertex2f(x, y);
    }
    
    // Bottom-right corner
    centerX = right - radius;
    centerY = bottom - radius;
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = 0.0f + (M_PI * 0.5f * i / cornerSegments);
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        glVertex2f(x, y);
    }
    
    // Bottom-left corner
    centerX = left + radius;
    centerY = bottom - radius;
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = M_PI * 0.5f + (M_PI * 0.5f * i / cornerSegments);
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        glVertex2f(x, y);
    }
    
    // Top-left corner
    centerX = left + radius;
    centerY = top + radius;
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = M_PI + (M_PI * 0.5f * i / cornerSegments);
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        glVertex2f(x, y);
    }
    
    glEnd();
}

std::unique_ptr<DrawingPrimitive> RectanglePrimitive::clone() const
{
    auto cloned = std::make_unique<RectanglePrimitive>(m_topLeft, m_bottomRight);
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setFilled(m_filled);
    cloned->setCornerRadius(m_cornerRadius);
    cloned->setMaintainAspectRatio(m_maintainAspectRatio);
    cloned->setCenterOnResize(m_centerOnResize);
    return cloned;
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

void EllipsePrimitive::render() const
{
    if (!m_visible) return;
    
    // Set line width
    glLineWidth(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    
    // Set color
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    
    // Use dynamic subdivision level
    const int segments = std::max(8, std::min(128, m_subdivisions));
    
    if (m_filled) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(m_center.x(), m_center.y()); // Center
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * M_PI * i / segments;
            float x = m_center.x() + m_radiusX * cos(angle);
            float y = m_center.y() + m_radiusY * sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * M_PI * i / segments;
            float x = m_center.x() + m_radiusX * cos(angle);
            float y = m_center.y() + m_radiusY * sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
    }
    
    // Draw axes if enabled
    if (m_showAxes) {
        glColor3f(0.7f, 0.7f, 0.7f); // Light gray for axes
        glLineWidth(1.0f);
        
        glBegin(GL_LINES);
        // Horizontal axis
        glVertex2f(m_center.x() - m_radiusX, m_center.y());
        glVertex2f(m_center.x() + m_radiusX, m_center.y());
        
        // Vertical axis  
        glVertex2f(m_center.x(), m_center.y() - m_radiusY);
        glVertex2f(m_center.x(), m_center.y() + m_radiusY);
        glEnd();
        
        // Draw center point
        glColor3f(0.5f, 0.5f, 0.5f);
        glPointSize(4.0f);
        glBegin(GL_POINTS);
        glVertex2f(m_center.x(), m_center.y());
        glEnd();
    }
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
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setFilled(m_filled);
    cloned->setSubdivisions(m_subdivisions);
    cloned->setShowAxes(m_showAxes);
    cloned->setLockAspectRatio(m_lockAspectRatio);
    return cloned;
}

// CurvePrimitive implementation
CurvePrimitive::CurvePrimitive()
    : DrawingPrimitive(PrimitiveType::Curve)
    , m_closed(false)
{
}

void CurvePrimitive::render() const
{
    if (!m_visible || m_controlPoints.size() < 2) return;
    
    // Set line width
    glLineWidth(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    
    // Set color
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    
    // Render main curve based on curve type
    if (m_curveType == 0) {
        // Linear curve (polyline)
        GLenum mode = m_closed ? GL_LINE_LOOP : GL_LINE_STRIP;
        glBegin(mode);
        for (const auto& point : m_controlPoints) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
    } else {
        // For other curve types, render as smooth curves
        // For now, still render as polyline but could be enhanced with actual Bezier/spline interpolation
        GLenum mode = m_closed ? GL_LINE_LOOP : GL_LINE_STRIP;
        glBegin(mode);
        for (const auto& point : m_controlPoints) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
    }
    
    // Show control polygon if enabled
    if (m_showControlPolygon && m_controlPoints.size() > 1) {
        // Draw control polygon with dotted lines
        glLineWidth(1.0f);
        glColor3f(0.7f, 0.7f, 0.7f); // Light gray for control polygon
        
        // Enable line stipple for dotted effect
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x5555); // Dotted pattern
        
        glBegin(GL_LINE_STRIP);
        for (const auto& point : m_controlPoints) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
        
        // Draw control points as small squares
        glDisable(GL_LINE_STIPPLE);
        glColor3f(1.0f, 0.0f, 0.0f); // Red for control points
        float pointSize = 3.0f;
        
        for (const auto& point : m_controlPoints) {
            glBegin(GL_LINE_LOOP);
            glVertex2f(point.x() - pointSize, point.y() - pointSize);
            glVertex2f(point.x() + pointSize, point.y() - pointSize);
            glVertex2f(point.x() + pointSize, point.y() + pointSize);
            glVertex2f(point.x() - pointSize, point.y() + pointSize);
            glEnd();
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

// DimensionPrimitive implementation
DimensionPrimitive::DimensionPrimitive(const QVector2D &start, const QVector2D &end)
    : DrawingPrimitive(PrimitiveType::Dimension)
    , m_start(start)
    , m_end(end)
    , m_unitsString("mm")
    , m_measurementValue(0.0f)
{
    setColor(QColor(255, 200, 0)); // Yellow/orange for dimensions
}

void DimensionPrimitive::render() const
{
    if (!m_visible) return;
    
    // Set dimension color (yellow/orange)
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glLineWidth(1.0f);
    
    // Calculate dimension line offset (above the measured line)
    QVector2D direction = (m_end - m_start).normalized();
    QVector2D perpendicular(-direction.y(), direction.x());
    float offset = 20.0f; // Offset distance from the measured line
    
    QVector2D dimStart = m_start + perpendicular * offset;
    QVector2D dimEnd = m_end + perpendicular * offset;
    
    // Draw extension lines
    glBegin(GL_LINES);
    // Extension line 1
    glVertex2f(m_start.x(), m_start.y());
    glVertex2f(dimStart.x(), dimStart.y());
    // Extension line 2
    glVertex2f(m_end.x(), m_end.y());
    glVertex2f(dimEnd.x(), dimEnd.y());
    // Main dimension line
    glVertex2f(dimStart.x(), dimStart.y());
    glVertex2f(dimEnd.x(), dimEnd.y());
    glEnd();
    
    // Draw arrows
    renderArrows(dimStart, dimEnd, direction);
    
    // Draw measurement text
    QVector2D textPos = (dimStart + dimEnd) * 0.5f + perpendicular * 5.0f;
    QString displayText = getDisplayText();
    renderText(textPos, displayText);
}

void DimensionPrimitive::renderArrows(const QVector2D &start, const QVector2D &end, const QVector2D &direction) const
{
    float arrowSize = 5.0f;
    QVector2D perpendicular(-direction.y(), direction.x());
    
    // Arrow at start
    glBegin(GL_LINES);
    glVertex2f(start.x(), start.y());
    glVertex2f(start.x() + direction.x() * arrowSize + perpendicular.x() * arrowSize * 0.5f,
               start.y() + direction.y() * arrowSize + perpendicular.y() * arrowSize * 0.5f);
    glVertex2f(start.x(), start.y());
    glVertex2f(start.x() + direction.x() * arrowSize - perpendicular.x() * arrowSize * 0.5f,
               start.y() + direction.y() * arrowSize - perpendicular.y() * arrowSize * 0.5f);
    
    // Arrow at end
    glVertex2f(end.x(), end.y());
    glVertex2f(end.x() - direction.x() * arrowSize + perpendicular.x() * arrowSize * 0.5f,
               end.y() - direction.y() * arrowSize + perpendicular.y() * arrowSize * 0.5f);
    glVertex2f(end.x(), end.y());
    glVertex2f(end.x() - direction.x() * arrowSize - perpendicular.x() * arrowSize * 0.5f,
               end.y() - direction.y() * arrowSize - perpendicular.y() * arrowSize * 0.5f);
    glEnd();
}

void DimensionPrimitive::renderText(const QVector2D &position, const QString &text) const
{
    // Simple text rendering using OpenGL (basic approach)
    // In a full implementation, you'd use proper text rendering
    glRasterPos2f(position.x(), position.y());
    // For now, we'll skip the actual text rendering as it requires more complex setup
    // The dimension line and arrows provide visual feedback
}

QString DimensionPrimitive::getDisplayText() const
{
    return QString("%1%2").arg(m_measurementValue, 0, 'f', 1).arg(m_unitsString);
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
        // Recalculate measurement
        m_measurementValue = (m_end - m_start).length();
    } else if (index == 1) {
        m_end = position;
        // Recalculate measurement
        m_measurementValue = (m_end - m_start).length();
    }
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
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setUnitsString(m_unitsString);
    cloned->setMeasurementValue(m_measurementValue);
    return cloned;
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

void BezierCurvePrimitive::render() const
{
    if (!m_visible || m_controlPoints.size() < 4) return;
    
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glLineWidth(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    
    // Get effective control points (with autoTangents and symmetricHandles applied)
    std::vector<QVector2D> effectivePoints = getEffectiveControlPoints();
    
    // Draw the Bézier curve with dynamic subdivision level
    int segments = std::max(10, std::min(200, m_subdivisionLevel)); // Clamp between 10-200
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        QVector2D point = evaluateCubicBezier(effectivePoints[0], effectivePoints[1], 
                                              effectivePoints[2], effectivePoints[3], t);
        glVertex2f(point.x(), point.y());
    }
    glEnd();
    
    // Draw control lines if enabled (either selected or showControlLines is true)
    if (m_selected || m_showControlLines) {
        // Draw control lines first (behind the points)
        glColor3f(0.6f, 0.6f, 0.6f); // Light gray for control lines
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        // Draw tangent lines: start->control1 and control2->end
        if (effectivePoints.size() >= 4) {
            glVertex2f(effectivePoints[0].x(), effectivePoints[0].y());
            glVertex2f(effectivePoints[1].x(), effectivePoints[1].y());
            glVertex2f(effectivePoints[2].x(), effectivePoints[2].y());
            glVertex2f(effectivePoints[3].x(), effectivePoints[3].y());
        }
        glEnd();
        
        // Draw control points with better visual handles (use original points for editing)
        for (int i = 0; i < m_controlPoints.size(); ++i) {
            const auto& point = m_controlPoints[i];
        
        // Also draw effective control points if different (in lighter color)
        if (m_autoTangents || m_symmetricHandles) {
            glColor3f(0.8f, 0.8f, 0.8f); // Very light gray for effective points
            for (int i = 0; i < effectivePoints.size(); ++i) {
                if (i == 1 || i == 2) { // Only show effective control handles
                    const auto& effPoint = effectivePoints[i];
                    
                    GLfloat matrix[16];
                    glGetFloatv(GL_PROJECTION_MATRIX, matrix);
                    float zoomFactor = matrix[0] > 0 ? matrix[0] : 1.0f;
                    float size = 0.6f / (zoomFactor * 200.0f);
                    
                    glLineWidth(1.0f);
                    glBegin(GL_LINE_LOOP);
                    glVertex2f(effPoint.x() - size, effPoint.y() - size);
                    glVertex2f(effPoint.x() + size, effPoint.y() - size);
                    glVertex2f(effPoint.x() + size, effPoint.y() + size);
                    glVertex2f(effPoint.x() - size, effPoint.y() + size);
                    glEnd();
                }
            }
        }
            
            // Different colors for different control points
            if (i == 0 || i == 3) {
                // Start and end points - blue
                glColor3f(0.0f, 0.0f, 1.0f);
            } else {
                // Control points - red
                glColor3f(1.0f, 0.0f, 0.0f);
            }
            
            // Draw zoom-independent control point handles
            // Get the current projection matrix to calculate screen-space size
            GLfloat matrix[16];
            glGetFloatv(GL_PROJECTION_MATRIX, matrix);
            
            // Extract zoom level from projection matrix (assuming orthographic projection)
            // The projection matrix element [0] contains the scaling factor
            float zoomFactor = matrix[0] > 0 ? matrix[0] : 1.0f;
            
            // Calculate size that remains constant regardless of zoom
            float pixelSize = 0.8f; // Even smaller target size in pixels
            float size = pixelSize / (zoomFactor * 200.0f); // Larger divisor for smaller points
            
            glLineWidth(1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(point.x() - size, point.y() - size);
            glVertex2f(point.x() + size, point.y() - size);
            glVertex2f(point.x() + size, point.y() + size);
            glVertex2f(point.x() - size, point.y() + size);
            glEnd();
            
            // Fill the center for better visibility
            glBegin(GL_QUADS);
            glVertex2f(point.x() - size*0.4f, point.y() - size*0.4f);
            glVertex2f(point.x() + size*0.4f, point.y() - size*0.4f);
            glVertex2f(point.x() + size*0.4f, point.y() + size*0.4f);
            glVertex2f(point.x() - size*0.4f, point.y() + size*0.4f);
            glEnd();
        }
    }
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
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setShowControlLines(m_showControlLines);
    cloned->setSubdivisionLevel(m_subdivisionLevel);
    cloned->setAutoTangents(m_autoTangents);
    cloned->setSymmetricHandles(m_symmetricHandles);
    return cloned;
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

void ArcPrimitive::render() const
{
    if (!m_visible) return;
    
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glLineWidth(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    
    // Calculate number of segments for smooth arc
    float angleSpan = std::abs(m_endAngle - m_startAngle);
    int segments = std::max(6, static_cast<int>(angleSpan / 5.0f)); // At least 6 segments
    
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = m_startAngle + t * (m_endAngle - m_startAngle);
        float radians = angle * M_PI / 180.0f;
        
        float x = m_center.x() + m_radius * std::cos(radians);
        float y = m_center.y() + m_radius * std::sin(radians);
        glVertex2f(x, y);
    }
    glEnd();
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
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
}

// CirclePrimitive Implementation
CirclePrimitive::CirclePrimitive(const QVector2D &center, float radius)
    : DrawingPrimitive(PrimitiveType::Circle), m_center(center), m_radius(radius), m_filled(false)
{
}

void CirclePrimitive::render() const
{
    if (!m_visible) return;
    
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glLineWidth(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    
    const int segments = 64; // High precision for circles
    
    // Draw main circle
    if (m_filled) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(m_center.x(), m_center.y()); // Center point
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * M_PI * i / segments;
            float x = m_center.x() + m_radius * std::cos(angle);
            float y = m_center.y() + m_radius * std::sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
    } else {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * M_PI * i / segments;
            float x = m_center.x() + m_radius * std::cos(angle);
            float y = m_center.y() + m_radius * std::sin(angle);
            glVertex2f(x, y);
        }
        glEnd();
    }
    
    // Draw center point if enabled
    if (m_showCenterPoint) {
        glColor3f(0.5f, 0.5f, 0.5f); // Gray color for guides
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        glVertex2f(m_center.x(), m_center.y());
        glEnd();
        
        // Draw center point outline
        glColor3f(0.0f, 0.0f, 0.0f); // Black outline
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glVertex2f(m_center.x(), m_center.y());
        glEnd();
    }
    
    // Draw quadrant lines if enabled
    if (m_showQuadrants) {
        glColor3f(0.7f, 0.7f, 0.7f); // Light gray for guides
        glLineWidth(1.0f);
        
        glBegin(GL_LINES);
        // Horizontal line
        glVertex2f(m_center.x() - m_radius, m_center.y());
        glVertex2f(m_center.x() + m_radius, m_center.y());
        
        // Vertical line
        glVertex2f(m_center.x(), m_center.y() - m_radius);
        glVertex2f(m_center.x(), m_center.y() + m_radius);
        glEnd();
    }
}

QRectF CirclePrimitive::boundingRect() const
{
    return QRectF(m_center.x() - m_radius - 5, m_center.y() - m_radius - 5,
                  2 * m_radius + 10, 2 * m_radius + 10);
}

bool CirclePrimitive::containsPoint(const QVector2D &point, float tolerance) const
{
    float distance = (point - m_center).length();
    if (m_filled) {
        return distance <= m_radius + tolerance;
    } else {
        return std::abs(distance - m_radius) <= tolerance;
    }
}

std::unique_ptr<DrawingPrimitive> CirclePrimitive::clone() const
{
    auto cloned = std::make_unique<CirclePrimitive>(m_center, m_radius);
    cloned->setFilled(m_filled);
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    cloned->setShowCenterPoint(m_showCenterPoint);
    cloned->setShowQuadrants(m_showQuadrants);
    return cloned;
}

// SplinePrimitive Implementation - For smooth, realistic guitar curves
SplinePrimitive::SplinePrimitive()
    : DrawingPrimitive(PrimitiveType::Spline)
    , m_closed(false)
    , m_filled(false)
    , m_smoothness(0.5f)
{
}

void SplinePrimitive::render() const
{
    if (!m_visible || m_points.size() < 2) return;
    
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color;
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glLineWidth(m_selected ? m_lineWidth * 2.0f : m_lineWidth);
    
    // Generate smooth spline points
    std::vector<QVector2D> splinePoints = generateSpline();
    
    if (m_filled && splinePoints.size() >= 3) {
        // Draw filled shape using triangle fan
        glBegin(GL_TRIANGLE_FAN);
        
        // Calculate center point for triangle fan
        QVector2D center(0, 0);
        for (const auto& point : splinePoints) {
            center += point;
        }
        center /= static_cast<float>(splinePoints.size());
        glVertex2f(center.x(), center.y());
        
        // Draw triangles to each edge
        for (size_t i = 0; i < splinePoints.size(); ++i) {
            glVertex2f(splinePoints[i].x(), splinePoints[i].y());
        }
        if (m_closed) {
            glVertex2f(splinePoints[0].x(), splinePoints[0].y()); // Close the shape
        }
        glEnd();
    }
    
    // Draw outline
    if (!m_filled || m_selected) {
        GLenum mode = m_closed ? GL_LINE_LOOP : GL_LINE_STRIP;
        glBegin(mode);
        for (const auto& point : splinePoints) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
    }
    
    // Show control points if enabled
    if (m_showPoints && !m_points.empty()) {
        
        // Draw control polygon (connecting lines between control points) in a subtle way
        // This shows the original control point structure
        if (m_points.size() > 1) {
            glColor3f(0.9f, 0.9f, 0.9f); // Very light gray for control polygon
            glLineWidth(0.5f);
            
            // Enable line stipple for dotted effect
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x5555); // More subtle dotted pattern
            
            glBegin(GL_LINE_STRIP);
            for (const auto& point : m_points) {
                glVertex2f(point.x(), point.y());
            }
            glEnd();
            
            glDisable(GL_LINE_STIPPLE);
        }
        
        // Draw control points themselves - these are now ON the spline curve
        glColor3f(0.0f, 0.8f, 0.0f); // Green to indicate they're ON the curve
        float pointSize = 5.0f; // Slightly larger since they're important
        
        for (size_t i = 0; i < m_points.size(); ++i) {
            const auto& point = m_points[i];
            
            // All points are now the same style since they're all ON the curve
            // Use circles to indicate they're interpolation points
            glBegin(GL_LINE_LOOP);
            const int segments = 12; // More segments for smoother circles
            for (int j = 0; j < segments; ++j) {
                float angle = 2.0f * M_PI * j / segments;
                float x = point.x() + pointSize * std::cos(angle);
                float y = point.y() + pointSize * std::sin(angle);
                glVertex2f(x, y);
            }
            glEnd();
            
            // Fill the center with a different color for start/end points
            if (i == 0 || i == m_points.size() - 1) {
                glColor3f(0.0f, 1.0f, 0.0f); // Bright green for start/end
            } else {
                glColor3f(0.0f, 0.6f, 0.0f); // Darker green for interior points
            }
            
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(point.x(), point.y()); // Center
            for (int j = 0; j <= segments; ++j) {
                float angle = 2.0f * M_PI * j / segments;
                float x = point.x() + pointSize * 0.7f * std::cos(angle);
                float y = point.y() + pointSize * 0.7f * std::sin(angle);
                glVertex2f(x, y);
            }
            glEnd();
            
            // Reset color for next point's outline
            glColor3f(0.0f, 0.8f, 0.0f);
        }
    }
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
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
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

// PolygonPrimitive implementation
PolygonPrimitive::PolygonPrimitive()
    : DrawingPrimitive(PrimitiveType::Polygon)
    , m_closed(true)
    , m_filled(false)
{
}

void PolygonPrimitive::render() const
{
    if (!m_visible || m_points.size() < 2) {
        return;
    }
    
    // Set line width
    glLineWidth(m_lineWidth * (m_selected ? 2.0f : 1.0f));
    
    // Set color
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color; // Orange when selected
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    
    // Only debug the test rectangle
    if (m_points.size() == 4) {
        qDebug() << "Rendering TEST PolygonPrimitive with" << m_points.size() << "points, color:" << drawColor.name();
    }
    
    if (m_filled) {
        // Render filled polygon
        glBegin(GL_POLYGON);
        for (const auto& point : m_points) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
    } else {
        // Render outline
        if (m_closed) {
            glBegin(GL_LINE_LOOP);
        } else {
            glBegin(GL_LINE_STRIP);
        }
        
        for (const auto& point : m_points) {
            glVertex2f(point.x(), point.y());
        }
        glEnd();
    }
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
    
    // Check if point is close to any edge
    for (size_t i = 0; i < m_points.size(); ++i) {
        size_t nextI = (i + 1) % m_points.size();
        if (!m_closed && nextI == 0) break; // Don't check last-to-first edge if not closed
        
        // Distance from point to line segment
        QVector2D lineVec = m_points[nextI] - m_points[i];
        QVector2D pointVec = point - m_points[i];
        
        float lineLength = lineVec.length();
        if (lineLength < 0.001f) {
            if ((point - m_points[i]).length() <= tolerance) return true;
            continue;
        }
        
        float t = QVector2D::dotProduct(pointVec, lineVec) / (lineLength * lineLength);
        t = std::max(0.0f, std::min(1.0f, t)); // Clamp to line segment
        
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
    cloned->setColor(m_color);
    cloned->setLineWidth(m_lineWidth);
    cloned->setVisible(m_visible);
    return cloned;
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
