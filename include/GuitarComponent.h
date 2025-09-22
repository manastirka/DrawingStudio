#pragma once

#include <QVector2D>
#include <QColor>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <vector>

enum class ComponentType {
    // Main Parts
    Body,
    Neck, 
    Headstock,
    
    // Pickups
    Pickup,
    
    // Hardware
    Bridge,
    Tuner,
    Nut,
    Fret,
    Inlay,
    SoundHole,
    Tailpiece,
    TremoloBar,
    StringGuide,
    StrapButton,
    OutputJack,
    
    // Electronics
    Electronics,
    VolumeKnob,
    ToneKnob,
    PickupSwitch,
    Potentiometer,
    Capacitor,
    
    // Neck Hardware
    TrussRodCover,
    NeckPlate,
    FretMarker,
    
    // Body Hardware
    PickupRing,
    PickupBezel,
    ScratchPlate,
    ControlCavityCover,
    
    // Measurements
    DimensionLine,
    AngleMeasure,
    RadiusMeasure,
    
    Custom
};

class DrawingPrimitive;

class GuitarComponent
{
public:
    explicit GuitarComponent(ComponentType type, const QVector2D &position = QVector2D());
    virtual ~GuitarComponent() = default;

    // Basic properties
    ComponentType type() const { return m_type; }
    QVector2D position() const { return m_position; }
    void setPosition(const QVector2D &position) { m_position = position; }
    
    float rotation() const { return m_rotation; }
    void setRotation(float rotation) { m_rotation = rotation; }
    
    QVector2D scale() const { return m_scale; }
    void setScale(const QVector2D &scale) { m_scale = scale; }
    
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    
    bool isSelected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }
    
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    
    // Component-specific properties
    virtual QJsonObject getProperties() const = 0;
    virtual void setProperties(const QJsonObject &props) = 0;
    
    // Rendering
    virtual void render() const = 0;
    virtual QRectF boundingRect() const = 0;
    virtual bool containsPoint(const QVector2D &point) const = 0;
    
    // Serialization
    virtual QJsonObject serialize() const;
    virtual void deserialize(const QJsonObject &json);
    
    // Drawing primitives
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    const std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() const { return m_primitives; }

protected:
    ComponentType m_type;
    QVector2D m_position;
    float m_rotation;
    QVector2D m_scale;
    bool m_visible;
    bool m_selected;
    QString m_name;
    
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
};

// Specific component implementations
class PickupComponent : public GuitarComponent
{
public:
    enum class PickupType { Single, Humbucker, P90 };
    
    PickupComponent(const QVector2D &position = QVector2D());
    
    PickupType pickupType() const { return m_pickupType; }
    void setPickupType(PickupType type) { m_pickupType = type; }
    
    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }
    
    QJsonObject getProperties() const override;
    void setProperties(const QJsonObject &props) override;
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point) const override;

private:
    PickupType m_pickupType;
    QColor m_color;
};

class BridgeComponent : public GuitarComponent
{
public:
    enum class BridgeType { Fixed, Tremolo, Tune_o_matic };
    
    BridgeComponent(const QVector2D &position = QVector2D());
    
    BridgeType bridgeType() const { return m_bridgeType; }
    void setBridgeType(BridgeType type) { m_bridgeType = type; }
    
    int stringCount() const { return m_stringCount; }
    void setStringCount(int count) { m_stringCount = count; }
    
    QJsonObject getProperties() const override;
    void setProperties(const QJsonObject &props) override;
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point) const override;

private:
    BridgeType m_bridgeType;
    int m_stringCount;
};

class TunerComponent : public GuitarComponent
{
public:
    enum class TunerType { Standard, Locking, Vintage };
    
    TunerComponent(const QVector2D &position = QVector2D());
    
    TunerType tunerType() const { return m_tunerType; }
    void setTunerType(TunerType type) { m_tunerType = type; }
    
    int tunerCount() const { return m_tunerCount; }
    void setTunerCount(int count) { m_tunerCount = count; }
    
    QJsonObject getProperties() const override;
    void setProperties(const QJsonObject &props) override;
    
    void render() const override;
    QRectF boundingRect() const override;
    bool containsPoint(const QVector2D &point) const override;

private:
    TunerType m_tunerType;
    int m_tunerCount;
};