#pragma once

#include <QObject>
#include <QVector2D>
#include <QColor>
#include <QRectF>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <Qt>
#include <algorithm>
#include <cmath>
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
    Dimension,
    Image
};

class DrawingPrimitive : public QObject
{
    Q_OBJECT
    
public:
    static constexpr qsizetype kMaxSerializedPointsPerPrimitive = 10000;
    static constexpr double kMaxSerializedCoordinateMagnitude = 1.0e9;
    static constexpr float kMaxLineWidth = 1000.0f;
    static constexpr float kMaxShadowBlur = 1000.0f;
    static constexpr float kMaxShadowOffset = 10000.0f;
    static bool isSupportedPoint(const QVector2D &point) {
        return std::isfinite(point.x()) && std::isfinite(point.y())
            && std::abs(point.x()) <= kMaxSerializedCoordinateMagnitude
            && std::abs(point.y()) <= kMaxSerializedCoordinateMagnitude;
    }

    explicit DrawingPrimitive(PrimitiveType type = PrimitiveType::Line, QObject* parent = nullptr);
    virtual ~DrawingPrimitive() = default;
    
    // Core interface
    virtual void render(QPainter* painter) const = 0;
    virtual void renderControlPoints(QPainter* painter) const;
    virtual std::vector<QVector2D> getControlPoints() const;
    virtual void setControlPointPosition(int index, const QVector2D& position);
    virtual QRectF boundingRect() const = 0;
    virtual bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const = 0;
    virtual std::unique_ptr<DrawingPrimitive> clone() const = 0;
    virtual void translate(const QVector2D& offset) = 0;
    
    // Serialization
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);
    static bool validateJson(const QJsonObject &json, QString *error = nullptr);
    static qsizetype serializedPointCount(const QJsonObject &json);
    static std::unique_ptr<DrawingPrimitive> createFromJson(const QJsonObject& json);
    
    // Properties
    PrimitiveType type() const { return m_type; }
    QColor color() const { return m_color; }
    void setColor(const QColor &color) {
        if (color.isValid()) m_color = color;
    }

    // Opacity multiplier used for layer opacity
    float opacityMultiplier() const { return m_opacityMultiplier; }
    void setOpacityMultiplier(float multiplier);

    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor &color) {
        if (color.isValid()) {
            m_fillColor = color;
            m_hasFillColor = true;
        }
    }
    bool hasFillColor() const { return m_hasFillColor; }
    void clearFillColor() { m_hasFillColor = false; }
    
    float lineWidth() const { return m_lineWidth; }
    void setLineWidth(float width);
    
    Qt::PenStyle lineStyle() const { return m_lineStyle; }
    void setLineStyle(Qt::PenStyle style);
    
    bool isSelected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }
    
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    // Shape rotation in degrees (Image/Text keep their own rotation APIs)
    float rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(float degrees);

    // Soft grouping — same non-null id moves/selects together
    QUuid groupId() const { return m_groupId; }
    void setGroupId(const QUuid &id) { m_groupId = id; }

    /** Copy shared style/transform fields onto a clone target. */
    void applyCommonPropertiesTo(DrawingPrimitive *dst) const;
    
    // Gradient fill
    enum class GradientFillType { None, Linear, Radial };
    GradientFillType gradientFillType() const { return m_gradientFillType; }
    void setGradientFillType(GradientFillType type) { m_gradientFillType = type; }
    QColor gradientStartColor() const { return m_gradientStartColor; }
    void setGradientStartColor(const QColor& c) { m_gradientStartColor = c; }
    QColor gradientEndColor() const { return m_gradientEndColor; }
    void setGradientEndColor(const QColor& c) { m_gradientEndColor = c; }
    float gradientAngle() const { return m_gradientAngle; }
    void setGradientAngle(float angle);
    QBrush createGradientBrush(const QRectF& bounds) const;

    // Shadow properties
    bool shadowEnabled() const { return m_shadowEnabled; }
    void setShadowEnabled(bool enabled) { m_shadowEnabled = enabled; }
    
    float shadowOffsetX() const { return m_shadowOffsetX; }
    float shadowOffsetY() const { return m_shadowOffsetY; }
    void setShadowOffset(float x, float y);
    
    float shadowBlur() const { return m_shadowBlur; }
    void setShadowBlur(float blur);
    
    QColor shadowColor() const { return m_shadowColor; }
    void setShadowColor(const QColor& color) {
        if (color.isValid()) m_shadowColor = color;
    }
    
    // Layer assignment
    const QUuid& layerId() const { return m_layerId; }
    void setLayerId(const QUuid& layerId) { m_layerId = layerId; }
    
    // Unique ID for each primitive
    const QUuid& id() const { return m_id; }
    
protected:
    // Helper to render shadow (call before rendering main shape)
    void renderShadow(QPainter* painter) const;
    PrimitiveType m_type;
    QColor m_color;
    QColor m_fillColor;
    float m_lineWidth;
    Qt::PenStyle m_lineStyle;
    bool m_selected;
    bool m_visible;
    bool m_hasFillColor;
    float m_opacityMultiplier = 1.0f;
    float m_rotationDegrees = 0.0f;
    QUuid m_id; // Unique ID for this primitive
    QUuid m_layerId; // Layer this primitive belongs to
    QUuid m_groupId; // Soft group membership (null = ungrouped)
    
    // Gradient fill
    GradientFillType m_gradientFillType = GradientFillType::None;
    QColor m_gradientStartColor;
    QColor m_gradientEndColor;
    float m_gradientAngle = 0.0f; // degrees, 0=left-to-right, 90=bottom-to-top

    // Shadow properties
    bool m_shadowEnabled = false;
    float m_shadowOffsetX = 5.0f;
    float m_shadowOffsetY = 5.0f;
    float m_shadowBlur = 5.0f;
    QColor m_shadowColor = QColor(0, 0, 0, 128);
};

// Concrete primitive implementations
class LinePrimitive : public DrawingPrimitive
{
public:
    LinePrimitive(const QVector2D &start = QVector2D(), const QVector2D &end = QVector2D());
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
    QVector2D startPoint() const { return m_start; }
    QVector2D endPoint() const { return m_end; }
    void setStartPoint(const QVector2D &point) {
        if (isSupportedPoint(point)) m_start = point;
    }
    void setEndPoint(const QVector2D &point) {
        if (isSupportedPoint(point)) m_end = point;
    }

    // Angle-line chain: slide moves along the previously connected segment
    QUuid connectedLineId() const { return m_connectedLineId; }
    void setConnectedLineId(const QUuid &id) { m_connectedLineId = id; }
    QVector2D moveConstraintDirection() const { return m_moveConstraintDirection; }
    void setMoveConstraintDirection(const QVector2D &dir) {
        if (isSupportedPoint(dir)) m_moveConstraintDirection = dir;
    }
    
private:
    QVector2D m_start;
    QVector2D m_end;
    QUuid m_connectedLineId;
    QVector2D m_moveConstraintDirection; // unit vector; null length = unconstrained
};

class RectanglePrimitive : public DrawingPrimitive
{
public:
    RectanglePrimitive(const QVector2D &topLeft = QVector2D(), const QVector2D &bottomRight = QVector2D());
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
    QVector2D topLeft() const { return m_topLeft; }
    QVector2D bottomRight() const { return m_bottomRight; }
    void setTopLeft(const QVector2D &point) {
        if (isSupportedPoint(point)) m_topLeft = point;
    }
    void setBottomRight(const QVector2D &point) {
        if (isSupportedPoint(point)) m_bottomRight = point;
    }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    float cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(float radius) {
        if (std::isfinite(radius))
            m_cornerRadius = std::clamp(
                radius, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
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
    
    void renderRoundedRectangle(QPainter* painter, float left, float top, float right, float bottom, float radius, bool filled) const;
};

class EllipsePrimitive : public DrawingPrimitive
{
public:
    EllipsePrimitive(const QVector2D &center = QVector2D(), float radiusX = 50.0f, float radiusY = 50.0f);
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
    QVector2D center() const { return m_center; }
    float radiusX() const { return m_radiusX; }
    float radiusY() const { return m_radiusY; }
    void setCenter(const QVector2D &center) {
        if (isSupportedPoint(center)) m_center = center;
    }
    void setRadiusX(float radius) {
        if (std::isfinite(radius))
            m_radiusX = std::clamp(
                radius, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    void setRadiusY(float radius) {
        if (std::isfinite(radius))
            m_radiusY = std::clamp(
                radius, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    // Visual properties
    int subdivisions() const { return m_subdivisions; }
    void setSubdivisions(int subdivisions) {
        m_subdivisions = std::clamp(subdivisions, 4, 4096);
    }
    
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
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void addControlPoint(const QVector2D &point);
    void setControlPoint(int index, const QVector2D &point);
    void removeControlPoint(int index);
    const std::vector<QVector2D>& controlPoints() const { return m_controlPoints; }
    void clearControlPoints() { m_controlPoints.clear(); }
    
    bool isClosed() const { return m_closed; }
    void setClosed(bool closed) { m_closed = closed; }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    int curveType() const { return m_curveType; }
    void setCurveType(int type) { m_curveType = qBound(0, type, 2); }
    
    bool showControlPolygon() const { return m_showControlPolygon; }
    void setShowControlPolygon(bool show) { m_showControlPolygon = show; }
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
private:
    std::vector<QVector2D> m_controlPoints;
    bool m_closed;
    bool m_filled = false;
    int m_curveType = 0; // 0=Linear, 1=Quadratic, 2=Cubic, etc.
    bool m_showControlPolygon = false;
    
    QVector2D evaluateBezier(const std::vector<QVector2D>& points, float t) const;
    std::vector<QVector2D> generateQuadraticBezierCurve() const;
    std::vector<QVector2D> generateCubicBezierCurve() const;
    std::vector<QVector2D> generateCatmullRomCurve() const;
    QVector2D catmullRom(const QVector2D& p0, const QVector2D& p1, 
                        const QVector2D& p2, const QVector2D& p3, float t) const;
};

// Advanced curve primitive for precise Bézier curves
class BezierCurvePrimitive : public DrawingPrimitive
{
public:
    BezierCurvePrimitive();
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    void setStartPoint(const QVector2D &point) { if (!m_controlPoints.empty()) m_controlPoints[0] = point; }
    void setEndPoint(const QVector2D &point) { if (m_controlPoints.size() > 3) m_controlPoints[3] = point; }
    void setControlPoint1(const QVector2D &point) { if (m_controlPoints.size() > 1) m_controlPoints[1] = point; }
    void setControlPoint2(const QVector2D &point) { if (m_controlPoints.size() > 2) m_controlPoints[2] = point; }
    
    void setControlPoints(const std::vector<QVector2D> &points);
    const std::vector<QVector2D>& controlPoints() const { return m_controlPoints; }
    
    QVector2D evaluateAt(float t) const; // t from 0 to 1
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    // Special rendering properties
    bool showControlLines() const { return m_showControlLines; }
    void setShowControlLines(bool show) { m_showControlLines = show; }
    
    int subdivisionLevel() const { return m_subdivisionLevel; }
    void setSubdivisionLevel(int level) {
        m_subdivisionLevel = qBound(10, level, 200);
    }
    
    bool autoTangents() const { return m_autoTangents; }
    void setAutoTangents(bool auto_tangents) { m_autoTangents = auto_tangents; }
    
    bool symmetricHandles() const { return m_symmetricHandles; }
    void setSymmetricHandles(bool symmetric) { m_symmetricHandles = symmetric; }
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
private:
    std::vector<QVector2D> m_controlPoints;
    bool m_filled = false;
    
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
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
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
    void setSmoothness(float smoothness) {
        if (std::isfinite(smoothness))
            m_smoothness = qBound(0.0f, smoothness, 1.0f);
    }
    
    int interpolationType() const { return m_interpolationType; }
    void setInterpolationType(int type) {
        m_interpolationType = qBound(0, type, 2);
    }
    
    float tension() const { return m_tension; }
    void setTension(float tension) {
        if (std::isfinite(tension))
            m_tension = qBound(0.0f, tension, 1.0f);
    }
    
    bool autoSmooth() const { return m_autoSmooth; }
    void setAutoSmooth(bool autoSmooth) { m_autoSmooth = autoSmooth; }
    
    bool showPoints() const { return m_showPoints; }
    void setShowPoints(bool show) { m_showPoints = show; }
    
    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
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
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D center() const { return m_center; }
    void setCenter(const QVector2D &center) {
        if (isSupportedPoint(center)) m_center = center;
    }
    
    float radius() const { return m_radius; }
    void setRadius(float radius) {
        if (std::isfinite(radius))
            m_radius = std::clamp(
                radius, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    float startAngle() const { return m_startAngle; }
    void setStartAngle(float angle) {
        if (std::isfinite(angle))
            m_startAngle = std::clamp(angle, -3600.0f, 3600.0f);
    }
    
    float endAngle() const { return m_endAngle; }
    void setEndAngle(float angle) {
        if (std::isfinite(angle))
            m_endAngle = std::clamp(angle, -3600.0f, 3600.0f);
    }
    
    QVector2D startPoint() const;
    QVector2D endPoint() const;

    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
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
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D center() const { return m_center; }
    void setCenter(const QVector2D &center) {
        if (isSupportedPoint(center)) m_center = center;
    }
    
    float radius() const { return m_radius; }
    void setRadius(float radius) {
        if (std::isfinite(radius))
            m_radius = std::clamp(
                radius, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    bool filled() const { return m_filled; }
    void setFilled(bool filled) { m_filled = filled; }
    
    bool showCenterPoint() const { return m_showCenterPoint; }
    void setShowCenterPoint(bool show) { m_showCenterPoint = show; }
    
    bool showQuadrants() const { return m_showQuadrants; }
    void setShowQuadrants(bool show) { m_showQuadrants = show; }

    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
    
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
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
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

    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
private:
    std::vector<QVector2D> m_points;
    bool m_closed;
    bool m_filled;
};

class DimensionPrimitive : public DrawingPrimitive
{
public:
    DimensionPrimitive(const QVector2D &start = QVector2D(), const QVector2D &end = QVector2D());
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    QVector2D startPoint() const { return m_start; }
    QVector2D endPoint() const { return m_end; }
    void setStartPoint(const QVector2D &point) {
        if (isSupportedPoint(point)) m_start = point;
    }
    void setEndPoint(const QVector2D &point) {
        if (isSupportedPoint(point)) m_end = point;
    }
    
    // Dimension-specific properties
    void setUnitsString(const QString &units) { m_unitsString = units; }
    void setMeasurementValue(float value) {
        if (std::isfinite(value))
            m_measurementValue = std::clamp(
                value,
                -static_cast<float>(kMaxSerializedCoordinateMagnitude),
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    void setPixelsPerUnit(float ppu);
    float pixelsPerUnit() const { return m_pixelsPerUnit; }
    /// Length in display units, always derived from geometry.
    float measuredLength() const;
    void recalculateMeasurement();
    QString getDisplayText() const;
    
    // Getters for rendering
    QVector2D getStart() const { return m_start; }
    QVector2D getEnd() const { return m_end; }
    float getMeasurementValue() const { return m_measurementValue; }
    QString getUnitsString() const { return m_unitsString; }

    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
private:
    QVector2D m_start;
    QVector2D m_end;
    QString m_unitsString;
    float m_measurementValue;
    float m_pixelsPerUnit = 1.0f;
    
    void renderArrows(QPainter* painter, const QVector2D &start, const QVector2D &end, const QVector2D &direction) const;
    void renderText(QPainter* painter, const QVector2D &position, const QString &text) const;
};

class TextPrimitive : public DrawingPrimitive
{
public:
    static constexpr qsizetype kMaxTextCharacters = 1000000;
    static constexpr qsizetype kMaxFontFamilyCharacters = 1024;
    static constexpr float kMaxFontSize = 10000.0f;

    TextPrimitive(const QVector2D &position = QVector2D(), const QString &text = "Text");
    
    void render(QPainter* painter) const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point, float tolerance = 2.0f) const override;
    std::unique_ptr<DrawingPrimitive> clone() const override;
    void translate(const QVector2D& offset) override;
    std::vector<QVector2D> getControlPoints() const override;
    void setControlPointPosition(int index, const QVector2D& position) override;
    
    // Text-specific properties
    QString text() const { return m_text; }
    void setText(const QString &text) {
        m_text = text.left(kMaxTextCharacters);
    }
    
    QVector2D position() const { return m_position; }
    void setPosition(const QVector2D &position) {
        if (isSupportedPoint(position)) m_position = position;
    }
    
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family) {
        m_fontFamily = family.left(kMaxFontFamilyCharacters);
    }
    
    float fontSize() const { return m_fontSize; }
    void setFontSize(float size) {
        if (std::isfinite(size))
            m_fontSize = std::clamp(size, 1.0f, kMaxFontSize);
    }
    
    bool isBold() const { return m_bold; }
    void setBold(bool bold) { m_bold = bold; }
    
    bool isItalic() const { return m_italic; }
    void setItalic(bool italic) { m_italic = italic; }

    bool isUnderline() const { return m_underline; }
    void setUnderline(bool underline) { m_underline = underline; }

    enum class BaselineShift {
        Normal,
        Subscript,
        Superscript
    };

    BaselineShift baselineShift() const { return m_baselineShift; }
    void setBaselineShift(BaselineShift shift) {
        const int value = static_cast<int>(shift);
        if (value >= 0 && value <= 2) m_baselineShift = shift;
    }
    
    float rotation() const { return m_rotation; }
    void setRotation(float rotation) {
        if (std::isfinite(rotation))
            m_rotation = std::clamp(rotation, -3600.0f, 3600.0f);
    }
    
    float scale() const { return m_scale; }
    void setScale(float scale) {
        if (std::isfinite(scale))
            m_scale = std::clamp(scale, 0.001f, 1000.0f);
    }
    
    // Text on path properties
    bool followsSpline() const { return m_followsSpline; }
    void setFollowsSpline(bool follows) { m_followsSpline = follows; }
    
    QUuid splineId() const { return m_splineId; }
    void setSplineId(const QUuid& id) { m_splineId = id; }
    
    float pathOffset() const { return m_pathOffset; }
    void setPathOffset(float offset) {
        if (std::isfinite(offset))
            m_pathOffset = std::clamp(
                offset,
                -static_cast<float>(kMaxSerializedCoordinateMagnitude),
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    // Text formatting
    enum class TextAlignment {
        Left,
        Center,
        Right,
        Justify
    };
    
    TextAlignment alignment() const { return m_alignment; }
    void setAlignment(TextAlignment align) {
        const int value = static_cast<int>(align);
        if (value >= 0 && value <= 3) m_alignment = align;
    }
    
    float letterSpacing() const { return m_letterSpacing; }
    void setLetterSpacing(float spacing) {
        if (std::isfinite(spacing))
            m_letterSpacing = std::clamp(spacing, -1000.0f, 1000.0f);
    }
    
    float lineSpacing() const { return m_lineSpacing; }
    void setLineSpacing(float spacing) {
        if (std::isfinite(spacing))
            m_lineSpacing = std::clamp(spacing, 0.01f, 100.0f);
    }
    
    // Text box dimensions
    float textBoxWidth() const { return m_textBoxWidth; }
    void setTextBoxWidth(float width) {
        if (std::isfinite(width))
            m_textBoxWidth = std::clamp(
                width, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    float textBoxHeight() const { return m_textBoxHeight; }
    void setTextBoxHeight(float height) {
        if (std::isfinite(height))
            m_textBoxHeight = std::clamp(
                height, 0.0f,
                static_cast<float>(kMaxSerializedCoordinateMagnitude));
    }
    
    // Text effects
    bool shadowEnabled() const { return m_dropShadow.enabled; }
    void setShadowEnabled(bool enabled) { m_dropShadow.enabled = enabled; }
    QColor shadowColor() const { return m_dropShadow.color; }
    void setShadowColor(const QColor& color) {
        if (color.isValid()) m_dropShadow.color = color;
    }
    float shadowOffsetX() const { return m_dropShadow.offsetX; }
    void setShadowOffsetX(float x) {
        if (std::isfinite(x))
            m_dropShadow.offsetX = std::clamp(x, -10000.0f, 10000.0f);
    }
    float shadowOffsetY() const { return m_dropShadow.offsetY; }
    void setShadowOffsetY(float y) {
        if (std::isfinite(y))
            m_dropShadow.offsetY = std::clamp(y, -10000.0f, 10000.0f);
    }
    float shadowBlur() const { return m_dropShadow.blur; }
    void setShadowBlur(float blur) {
        if (std::isfinite(blur))
            m_dropShadow.blur = std::clamp(blur, 0.0f, 1000.0f);
    }
    float shadowAngle() const { return m_dropShadow.angle; }
    void setShadowAngle(float angle) {
        if (std::isfinite(angle))
            m_dropShadow.angle = std::clamp(angle, -360.0f, 360.0f);
    }
    float shadowDistance() const { return m_dropShadow.distance; }
    void setShadowDistance(float distance) {
        if (std::isfinite(distance))
            m_dropShadow.distance = std::clamp(distance, 0.0f, 10000.0f);
    }
    
    bool strokeEnabled() const { return m_stroke.enabled; }
    void setStrokeEnabled(bool enabled) { m_stroke.enabled = enabled; }
    QColor strokeColor() const { return m_stroke.color; }
    void setStrokeColor(const QColor& color) {
        if (color.isValid()) m_stroke.color = color;
    }
    float strokeWidth() const { return m_stroke.width; }
    void setStrokeWidth(float width) {
        if (std::isfinite(width))
            m_stroke.width = std::clamp(width, 0.0f, 1000.0f);
    }
    
    bool gradientEnabled() const { return m_gradient.enabled; }
    void setGradientEnabled(bool enabled) { m_gradient.enabled = enabled; }
    QColor gradientStartColor() const { return m_gradient.startColor; }
    void setGradientStartColor(const QColor& color) {
        if (color.isValid()) m_gradient.startColor = color;
    }
    QColor gradientEndColor() const { return m_gradient.endColor; }
    void setGradientEndColor(const QColor& color) {
        if (color.isValid()) m_gradient.endColor = color;
    }
    float gradientAngle() const { return m_gradient.angle; }
    void setGradientAngle(float angle) {
        if (std::isfinite(angle))
            m_gradient.angle = std::clamp(angle, -360.0f, 360.0f);
    }
    
    // Selection and transformation handles
    void renderSelectionHandles(QPainter* painter) const;
    bool isPointOnHandle(const QVector2D& point, int* handleIndex = nullptr) const;
    QRectF getHandleRect(int handleIndex) const;
    void resizeFromHandle(int handleIndex, const QVector2D& newPosition);

    // Serialization
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
private:
    QVector2D m_position;
    QString m_text;
    QString m_fontFamily;
    float m_fontSize;
    bool m_bold;
    bool m_italic;
    bool m_underline;
    BaselineShift m_baselineShift;
    float m_rotation;  // Rotation angle in radians
    float m_scale;     // Scale factor
    
    // Text on path
    bool m_followsSpline;
    QUuid m_splineId;
    float m_pathOffset;  // Offset along the path (0.0 to 1.0)
    
    // Text formatting
    TextAlignment m_alignment;
    float m_letterSpacing;  // Extra spacing between letters (in pixels)
    float m_lineSpacing;    // Line height multiplier (1.0 = normal)
    
    // Text box dimensions (0 = auto-size)
    float m_textBoxWidth;
    float m_textBoxHeight;
    
    // Text effects
    struct DropShadow {
        bool enabled = false;
        QColor color = QColor(0, 0, 0, 128);
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float blur = 0.0f;
        float angle = 45.0f;
        float distance = 0.0f;
    } m_dropShadow;
    
    struct Stroke {
        bool enabled = false;
        QColor color = Qt::black;
        float width = 2.0f;
    } m_stroke;
    
    struct GradientFill {
        bool enabled = false;
        QColor startColor = Qt::white;
        QColor endColor = Qt::black;
        float angle = 0.0f;
    } m_gradient;
};
