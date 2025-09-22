#include "GuitarComponent.h"
#include "DrawingPrimitive.h"
#include <QJsonDocument>
#include <QOpenGLFunctions>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Silence OpenGL deprecation warnings on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

// Base GuitarComponent implementation
GuitarComponent::GuitarComponent(ComponentType type, const QVector2D &position)
    : m_type(type)
    , m_position(position)
    , m_rotation(0.0f)
    , m_scale(1.0f, 1.0f)
    , m_visible(true)
    , m_selected(false)
    , m_name("Untitled Component")
{
}

QJsonObject GuitarComponent::serialize() const
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(m_type);
    obj["name"] = m_name;
    obj["position_x"] = m_position.x();
    obj["position_y"] = m_position.y();
    obj["rotation"] = m_rotation;
    obj["scale_x"] = m_scale.x();
    obj["scale_y"] = m_scale.y();
    obj["visible"] = m_visible;
    obj["selected"] = m_selected;
    obj["properties"] = getProperties();
    return obj;
}

void GuitarComponent::deserialize(const QJsonObject &json)
{
    m_type = static_cast<ComponentType>(json["type"].toInt());
    m_name = json["name"].toString();
    m_position = QVector2D(json["position_x"].toDouble(), json["position_y"].toDouble());
    m_rotation = json["rotation"].toDouble();
    m_scale = QVector2D(json["scale_x"].toDouble(), json["scale_y"].toDouble());
    m_visible = json["visible"].toBool();
    m_selected = json["selected"].toBool();
    
    if (json.contains("properties")) {
        setProperties(json["properties"].toObject());
    }
}

void GuitarComponent::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive)
{
    m_primitives.push_back(std::move(primitive));
}

// PickupComponent implementation
PickupComponent::PickupComponent(const QVector2D &position)
    : GuitarComponent(ComponentType::Pickup, position)
    , m_pickupType(PickupType::Single)
    , m_color(Qt::black)
{
    m_name = "Pickup";
}

QJsonObject PickupComponent::getProperties() const
{
    QJsonObject obj;
    obj["pickupType"] = static_cast<int>(m_pickupType);
    obj["color"] = m_color.name();
    return obj;
}

void PickupComponent::setProperties(const QJsonObject &props)
{
    if (props.contains("pickupType")) {
        m_pickupType = static_cast<PickupType>(props["pickupType"].toInt());
    }
    if (props.contains("color")) {
        m_color = QColor(props["color"].toString());
    }
}

void PickupComponent::render() const
{
    if (!m_visible) return;
    
    QRectF rect = boundingRect();
    QColor drawColor = m_selected ? QColor(255, 165, 0) : m_color; // Orange when selected
    
    // Set line width
    float lineWidth = m_selected ? 2.0f : 1.0f;
    glLineWidth(lineWidth);
    
    // Fill the pickup rectangle
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glBegin(GL_QUADS);
    glVertex2f(rect.left(), rect.top());
    glVertex2f(rect.right(), rect.top());
    glVertex2f(rect.right(), rect.bottom());
    glVertex2f(rect.left(), rect.bottom());
    glEnd();
    
    // Draw outline
    glColor3f(0.8f, 0.8f, 0.8f); // Light gray outline
    glBegin(GL_LINE_LOOP);
    glVertex2f(rect.left(), rect.top());
    glVertex2f(rect.right(), rect.top());
    glVertex2f(rect.right(), rect.bottom());
    glVertex2f(rect.left(), rect.bottom());
    glEnd();
    
    // Draw pickup-specific details
    if (m_pickupType == PickupType::Humbucker) {
        // Draw center divider for humbucker
        float centerX = rect.center().x();
        glBegin(GL_LINES);
        glVertex2f(centerX, rect.top());
        glVertex2f(centerX, rect.bottom());
        glEnd();
    } else if (m_pickupType == PickupType::Single || m_pickupType == PickupType::P90) {
        // Draw pole pieces
        int poles = (m_pickupType == PickupType::P90) ? 2 : 6;
        float spacing = rect.width() / (poles + 1);
        for (int i = 1; i <= poles; ++i) {
            float x = rect.left() + i * spacing;
            float y = rect.center().y();
            // Draw small circles for pole pieces
            glBegin(GL_LINE_LOOP);
            for (int j = 0; j < 8; ++j) {
                float angle = 2.0f * M_PI * j / 8.0f;
                float radius = 1.5f;
                glVertex2f(x + radius * cos(angle), y + radius * sin(angle));
            }
            glEnd();
        }
    }
}

QRectF PickupComponent::boundingRect() const
{
    // Return bounding rectangle based on pickup type
    float width = 30.0f; // Base width in mm
    float height = 10.0f; // Base height in mm
    
    switch (m_pickupType) {
        case PickupType::Humbucker:
            width = 35.0f;
            height = 15.0f;
            break;
        case PickupType::P90:
            width = 32.0f;
            height = 12.0f;
            break;
        default:
            break;
    }
    
    return QRectF(m_position.x() - width/2, m_position.y() - height/2, width, height);
}

bool PickupComponent::containsPoint(const QVector2D &point) const
{
    return boundingRect().contains(QPointF(point.x(), point.y()));
}

// BridgeComponent implementation
BridgeComponent::BridgeComponent(const QVector2D &position)
    : GuitarComponent(ComponentType::Bridge, position)
    , m_bridgeType(BridgeType::Fixed)
    , m_stringCount(6)
{
    m_name = "Bridge";
}

QJsonObject BridgeComponent::getProperties() const
{
    QJsonObject obj;
    obj["bridgeType"] = static_cast<int>(m_bridgeType);
    obj["stringCount"] = m_stringCount;
    return obj;
}

void BridgeComponent::setProperties(const QJsonObject &props)
{
    if (props.contains("bridgeType")) {
        m_bridgeType = static_cast<BridgeType>(props["bridgeType"].toInt());
    }
    if (props.contains("stringCount")) {
        m_stringCount = props["stringCount"].toInt();
    }
}

void BridgeComponent::render() const
{
    if (!m_visible) return;
    
    QRectF rect = boundingRect();
    QColor drawColor = m_selected ? QColor(255, 165, 0) : QColor(150, 150, 150); // Gray bridge, orange when selected
    
    // Set line width
    float lineWidth = m_selected ? 2.0f : 1.0f;
    glLineWidth(lineWidth);
    
    // Fill the bridge rectangle
    glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
    glBegin(GL_QUADS);
    glVertex2f(rect.left(), rect.top());
    glVertex2f(rect.right(), rect.top());
    glVertex2f(rect.right(), rect.bottom());
    glVertex2f(rect.left(), rect.bottom());
    glEnd();
    
    // Draw outline
    glColor3f(0.5f, 0.5f, 0.5f); // Darker gray outline
    glBegin(GL_LINE_LOOP);
    glVertex2f(rect.left(), rect.top());
    glVertex2f(rect.right(), rect.top());
    glVertex2f(rect.right(), rect.bottom());
    glVertex2f(rect.left(), rect.bottom());
    glEnd();
    
    // Draw bridge-specific details
    if (m_bridgeType == BridgeType::Tremolo) {
        // Draw tremolo arm mounting point
        float armX = rect.right() - 5.0f;
        float armY = rect.center().y();
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 8; ++i) {
            float angle = 2.0f * M_PI * i / 8.0f;
            float radius = 3.0f;
            glVertex2f(armX + radius * cos(angle), armY + radius * sin(angle));
        }
        glEnd();
    }
    
    // Draw saddles for strings
    float spacing = rect.width() / (m_stringCount + 1);
    for (int i = 1; i <= m_stringCount; ++i) {
        float x = rect.left() + i * spacing;
        float y = rect.center().y();
        // Draw small rectangles for saddles
        glBegin(GL_QUADS);
        glVertex2f(x - 1.0f, y - 1.5f);
        glVertex2f(x + 1.0f, y - 1.5f);
        glVertex2f(x + 1.0f, y + 1.5f);
        glVertex2f(x - 1.0f, y + 1.5f);
        glEnd();
    }
}

QRectF BridgeComponent::boundingRect() const
{
    float width = 50.0f; // Base width in mm
    float height = 20.0f; // Base height in mm
    
    switch (m_bridgeType) {
        case BridgeType::Tremolo:
            width = 60.0f;
            height = 25.0f;
            break;
        case BridgeType::Tune_o_matic:
            width = 45.0f;
            height = 15.0f;
            break;
        default:
            break;
    }
    
    return QRectF(m_position.x() - width/2, m_position.y() - height/2, width, height);
}

bool BridgeComponent::containsPoint(const QVector2D &point) const
{
    return boundingRect().contains(QPointF(point.x(), point.y()));
}

// TunerComponent implementation
TunerComponent::TunerComponent(const QVector2D &position)
    : GuitarComponent(ComponentType::Tuner, position)
    , m_tunerType(TunerType::Standard)
    , m_tunerCount(6)
{
    m_name = "Tuners";
}

QJsonObject TunerComponent::getProperties() const
{
    QJsonObject obj;
    obj["tunerType"] = static_cast<int>(m_tunerType);
    obj["tunerCount"] = m_tunerCount;
    return obj;
}

void TunerComponent::setProperties(const QJsonObject &props)
{
    if (props.contains("tunerType")) {
        m_tunerType = static_cast<TunerType>(props["tunerType"].toInt());
    }
    if (props.contains("tunerCount")) {
        m_tunerCount = props["tunerCount"].toInt();
    }
}

void TunerComponent::render() const
{
    if (!m_visible) return;
    
    QRectF rect = boundingRect();
    QColor drawColor = m_selected ? QColor(255, 165, 0) : QColor(200, 200, 200); // Light gray tuners, orange when selected
    
    // Set line width
    float lineWidth = m_selected ? 2.0f : 1.0f;
    glLineWidth(lineWidth);
    
    // Draw individual tuners
    float spacing = rect.width() / m_tunerCount;
    float tunerRadius = 8.0f;
    
    for (int i = 0; i < m_tunerCount; ++i) {
        float x = rect.left() + (i + 0.5f) * spacing;
        float y = rect.center().y();
        
        // Draw tuner body (circle)
        glColor3f(drawColor.redF(), drawColor.greenF(), drawColor.blueF());
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y); // center
        for (int j = 0; j <= 16; ++j) {
            float angle = 2.0f * M_PI * j / 16.0f;
            glVertex2f(x + tunerRadius * cos(angle), y + tunerRadius * sin(angle));
        }
        glEnd();
        
        // Draw tuner outline
        glColor3f(0.4f, 0.4f, 0.4f); // Dark gray outline
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < 16; ++j) {
            float angle = 2.0f * M_PI * j / 16.0f;
            glVertex2f(x + tunerRadius * cos(angle), y + tunerRadius * sin(angle));
        }
        glEnd();
        
        // Draw tuning peg/button
        float buttonRadius = tunerRadius * 0.4f;
        glColor3f(0.3f, 0.3f, 0.3f); // Darker for button
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y); // center
        for (int j = 0; j <= 8; ++j) {
            float angle = 2.0f * M_PI * j / 8.0f;
            glVertex2f(x + buttonRadius * cos(angle), y + buttonRadius * sin(angle));
        }
        glEnd();
        
        // Draw string post
        if (m_tunerType == TunerType::Vintage) {
            // Draw vintage-style string post (small line)
            glColor3f(0.6f, 0.6f, 0.6f);
            glBegin(GL_LINES);
            glVertex2f(x, y - tunerRadius);
            glVertex2f(x, y - tunerRadius - 5.0f);
            glEnd();
        }
    }
}

QRectF TunerComponent::boundingRect() const
{
    float width = 80.0f; // Total width for all tuners
    float height = 30.0f; // Height for tuner area
    
    return QRectF(m_position.x() - width/2, m_position.y() - height/2, width, height);
}

bool TunerComponent::containsPoint(const QVector2D &point) const
{
    return boundingRect().contains(QPointF(point.x(), point.y()));
}