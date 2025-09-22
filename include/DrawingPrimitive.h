#pragma once

#include <QVector2D>
#include <QColor>
#include <QRectF>
#include <vector>
#include <memory>

enum class PrimitiveType {
    Line,
    Curve,
    BezierCurve,
    Spline,
    Arc,
    Circle,
    Rectangle,
    Ellipse,
    Polygon,
    Text,
    Dimension
};

class DrawingPrimitive
{
public:
    DrawingPrimitive(PrimitiveType type = PrimitiveType::Line);
    virtual ~DrawingPrimitive() = default;
    
    // Core interface
    virtual void render() const = 0;
    virtual void renderControlPoints() const;
    virtual std::vector<QVector2D> getControlPoints() const;
    virtual void setControlPointPosition(int index, const QVector2D& position);
    virtual QRectF boundingRect() const = 0;
    virtual bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const = 0;
    virtual std::unique_ptr<DrawingPrimitive> clone() const = 0;
    
    // Properties
    PrimitiveType type() const { return m_type; }
    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }
    
    float lineWidth() const { return m_lineWidth; }
    void setLineWidth(float width) { m_lineWidth = width; }
    
    bool isSelected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }
    
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    
protected:
    PrimitiveType m_type;
    QColor m_color;
    float m_lineWidth;
    bool m_selected;
    bool m_visible;
};

// Concrete primitive implementations
class LinePrimitive : public DrawingPrimitive
{
public:
    LinePrimitive(const QVector2D &start = QVector2D(), const QVector2D &end = QVector2D());
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D startPoint() const { return m_start; }
    QVector2D endPoint() const { return m_end; }
    void setStartPoint(const QVector2D &point) { m_start = point; }
    void setEndPoint(const QVector2D &point) { m_end = point; }
    
private:
    QVector2D m_start;
    QVector2D m_end;
};

class RectanglePrimitive : public DrawingPrimitive
{
public:
    RectanglePrimitive(const QVector2D &topLeft = QVector2D(), const QVector2D &bottomRight = QVector2D());
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D topLeft() const { return m_topLeft; }
    QVector2D bottomRight() const { return m_bottomRight; }
    void setTopLeft(const QVector2D &point) { m_topLeft = point; }
    void setBottomRight(const QVector2D &point) { m_bottomRight = point; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    float cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(float radius) { m_cornerRadius = radius; }
    
    bool maintainAspectRatio() const { return m_maintainAspectRatio; }
    void setMaintainAspectRatio(bool maintain) { m_maintainAspectRatio = maintain; }
    
    bool centerOnResize() const { return m_centerOnResize; }
    void setCenterOnResize(bool center) { m_centerOnResize = center; }
    
private:
    QVector2D m_topLeft;
    QVector2D m_bottomRight;
    bool m_filled;
    float m_cornerRadius = 0.0f;
    bool m_maintainAspectRatio = false;
    bool m_centerOnResize = false;
    
    void renderRoundedRectangle(float left, float top, float right, float bottom, float radius, bool filled) const;
};

class EllipsePrimitive : public DrawingPrimitive
{
public:
    EllipsePrimitive(const QVector2D &center = QVector2D(), float radiusX = 50.0f, float radiusY = 50.0f);
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D center() const { return m_center; }
    float radiusX() const { return m_radiusX; }
    float radiusY() const { return m_radiusY; }
    void setCenter(const QVector2D &center) { m_center = center; }
    void setRadiusX(float radius) { m_radiusX = radius; }
    void setRadiusY(float radius) { m_radiusY = radius; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    // Visual properties
    int subdivisions() const { return m_subdivisions; }
    void setSubdivisions(int subdivisions) { m_subdivisions = subdivisions; }
    
    bool showAxes() const { return m_showAxes; }
    void setShowAxes(bool show) { m_showAxes = show; }
    
    bool lockAspectRatio() const { return m_lockAspectRatio; }
    void setLockAspectRatio(bool lock) { m_lockAspectRatio = lock; }
    
private:
    QVector2D m_center;
    float m_radiusX;
    float m_radiusY;
    bool m_filled;
    int m_subdivisions = 64;
    bool m_showAxes = false;
    bool m_lockAspectRatio = false;
};

class CurvePrimitive : public DrawingPrimitive
{
public:
    CurvePrimitive();
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void addControlPoint(const QVector2D &point);
    void setControlPoint(int index, const QVector2D &point);
    void removeControlPoint(int index);
    const std::vector<QVector2D>& controlPoints() const { return m_controlPoints; }
    void clearControlPoints() { m_controlPoints.clear(); }
    
    bool isClosed() const { return m_closed; }
    void setClosed(bool closed) { m_closed = closed; }
    
    int curveType() const { return m_curveType; }
    void setCurveType(int type) { m_curveType = type; }
    
    bool showControlPolygon() const { return m_showControlPolygon; }
    void setShowControlPolygon(bool show) { m_showControlPolygon = show; }
    
private:
    std::vector<QVector2D> m_controlPoints;
    bool m_closed;
    int m_curveType = 0; // 0=Linear, 1=Quadratic, 2=Cubic, etc.
    bool m_showControlPolygon = false;
    
    QVector2D evaluateBezier(const std::vector<QVector2D>& points, float t) const;
};

// Advanced curve primitive for precise Bézier curves
class BezierCurvePrimitive : public DrawingPrimitive
{
public:
    BezierCurvePrimitive();
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void setStartPoint(const QVector2D &point) { if (!m_controlPoints.empty()) m_controlPoints[0] = point; }
    void setEndPoint(const QVector2D &point) { if (m_controlPoints.size() > 3) m_controlPoints[3] = point; }
    void setControlPoint1(const QVector2D &point) { if (m_controlPoints.size() > 1) m_controlPoints[1] = point; }
    void setControlPoint2(const QVector2D &point) { if (m_controlPoints.size() > 2) m_controlPoints[2] = point; }
    
    void setControlPoints(const std::vector<QVector2D> &points);
    const std::vector<QVector2D>& controlPoints() const { return m_controlPoints; }
    
    QVector2D evaluateAt(float t) const; // t from 0 to 1
    
    // Special rendering properties
    bool showControlLines() const { return m_showControlLines; }
    void setShowControlLines(bool show) { m_showControlLines = show; }
    
    int subdivisionLevel() const { return m_subdivisionLevel; }
    void setSubdivisionLevel(int level) { m_subdivisionLevel = level; }
    
    bool autoTangents() const { return m_autoTangents; }
    void setAutoTangents(bool auto_tangents) { m_autoTangents = auto_tangents; }
    
    bool symmetricHandles() const { return m_symmetricHandles; }
    void setSymmetricHandles(bool symmetric) { m_symmetricHandles = symmetric; }
    
private:
    std::vector<QVector2D> m_controlPoints;
    
    // Special rendering properties
    bool m_showControlLines = true;
    int m_subdivisionLevel = 50;
    bool m_autoTangents = false;
    bool m_symmetricHandles = false;
    
    QVector2D evaluateCubicBezier(const QVector2D &p0, const QVector2D &p1, 
                                  const QVector2D &p2, const QVector2D &p3, float t) const;
    std::vector<QVector2D> getEffectiveControlPoints() const;
};

// Spline primitive for smooth interpolated curves
class SplinePrimitive : public DrawingPrimitive
{
public:
    SplinePrimitive();
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void addPoint(const QVector2D &point);
    void setPoint(int index, const QVector2D &point);
    void removePoint(int index);
    const std::vector<QVector2D>& points() const { return m_points; }
    void clearPoints() { m_points.clear(); }
    
    bool isClosed() const { return m_closed; }
    void setClosed(bool closed) { m_closed = closed; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    float smoothness() const { return m_smoothness; }
    void setSmoothness(float smoothness) { m_smoothness = smoothness; }
    
    int interpolationType() const { return m_interpolationType; }
    void setInterpolationType(int type) { m_interpolationType = type; }
    
    float tension() const { return m_tension; }
    void setTension(float tension) { m_tension = tension; }
    
    bool autoSmooth() const { return m_autoSmooth; }
    void setAutoSmooth(bool autoSmooth) { m_autoSmooth = autoSmooth; }
    
    bool showPoints() const { return m_showPoints; }
    void setShowPoints(bool show) { m_showPoints = show; }
    
private:
    std::vector<QVector2D> m_points;
    bool m_closed;
    bool m_filled;
    float m_smoothness; // 0.0 = linear, 1.0 = very smooth
    int m_interpolationType = 0; // 0=Catmull-Rom, 1=B-Spline, 2=Bezier
    float m_tension = 0.5f; // Spline tension parameter
    bool m_autoSmooth = false; // Auto-smooth control points
    bool m_showPoints = true; // Show control points
    
    std::vector<QVector2D> generateSpline() const;
    QVector2D catmullRom(const QVector2D& p0, const QVector2D& p1, const QVector2D& p2, const QVector2D& p3, float t) const;
};

// Arc primitive for precise circular arcs
class ArcPrimitive : public DrawingPrimitive
{
public:
    ArcPrimitive(const QVector2D &center = QVector2D(), float radius = 50.0f, 
                 float startAngle = 0.0f, float endAngle = 90.0f);
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D center() const { return m_center; }
    void setCenter(const QVector2D &center) { m_center = center; }
    
    float radius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; }
    
    float startAngle() const { return m_startAngle; }
    void setStartAngle(float angle) { m_startAngle = angle; }
    
    float endAngle() const { return m_endAngle; }
    void setEndAngle(float angle) { m_endAngle = angle; }
    
    QVector2D startPoint() const;
    QVector2D endPoint() const;
    
private:
    QVector2D m_center;
    float m_radius;
    float m_startAngle; // in degrees
    float m_endAngle;   // in degrees
};

// Circle primitive (more precise than ellipse with equal radii)
class CirclePrimitive : public DrawingPrimitive
{
public:
    CirclePrimitive(const QVector2D &center = QVector2D(), float radius = 50.0f);
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D center() const { return m_center; }
    void setCenter(const QVector2D &center) { m_center = center; }
    
    float radius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    bool showCenterPoint() const { return m_showCenterPoint; }
    void setShowCenterPoint(bool show) { m_showCenterPoint = show; }
    
    bool showQuadrants() const { return m_showQuadrants; }
    void setShowQuadrants(bool show) { m_showQuadrants = show; }
    
private:
    QVector2D m_center;
    float m_radius;
    bool m_filled;
    bool m_showCenterPoint = false;
    bool m_showQuadrants = false;
};

class PolygonPrimitive : public DrawingPrimitive
{
public:
    PolygonPrimitive();
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void addPoint(const QVector2D &point);
    void setPoint(int index, const QVector2D &point);
    void removePoint(int index);
    const std::vector<QVector2D>& points() const { return m_points; }
    void clearPoints() { m_points.clear(); }
    
    bool isClosed() const { return m_closed; }
    void setClosed(bool closed) { m_closed = closed; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
private:
    std::vector<QVector2D> m_points;
    bool m_closed;
    bool m_filled;
};

class DimensionPrimitive : public DrawingPrimitive
{
public:
    DimensionPrimitive(const QVector2D &start = QVector2D(), const QVector2D &end = QVector2D());
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D startPoint() const { return m_start; }
    QVector2D endPoint() const { return m_end; }
    void setStartPoint(const QVector2D &point) { m_start = point; }
    void setEndPoint(const QVector2D &point) { m_end = point; }
    
    // Dimension-specific properties
    void setUnitsString(const QString &units) { m_unitsString = units; }
    void setMeasurementValue(float value) { m_measurementValue = value; }
    QString getDisplayText() const;
    
    // Getters for rendering
    QVector2D getStart() const { return m_start; }
    QVector2D getEnd() const { return m_end; }
    float getMeasurementValue() const { return m_measurementValue; }
    QString getUnitsString() const { return m_unitsString; }
    
private:
    QVector2D m_start;
    QVector2D m_end;
    QString m_unitsString;
    float m_measurementValue;
    
    void renderArrows(const QVector2D &start, const QVector2D &end, const QVector2D &direction) const;
    void renderText(const QVector2D &position, const QString &text) const;
};
