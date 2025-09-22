#pragma once

#include <QVector2D>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <vector>

class GuitarComponent;
class DrawingPrimitive;

class GuitarProject
{
public:
    GuitarProject() = default;
    ~GuitarProject() = default;
    
    // Component management
    void addComponent(std::unique_ptr<GuitarComponent> component);
    void removeComponent(GuitarComponent* component);
    GuitarComponent* findComponentAt(const QVector2D &position, float tolerance = 10.0f) const;
    const std::vector<std::unique_ptr<GuitarComponent>>& components() const { return m_components; }
    
    // Primitive management
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void removePrimitive(DrawingPrimitive* primitive);
    const std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() const { return m_primitives; }
    void clearPrimitives();
    
    // Selection management
    void clearSelection();
    void selectComponent(GuitarComponent* component);
    void deselectComponent(GuitarComponent* component);
    const std::vector<GuitarComponent*>& selectedComponents() const { return m_selectedComponents; }
    
    // Project properties
    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }
    
    void setScaleLength(float scaleLength) { m_scaleLength = scaleLength; }
    float scaleLength() const { return m_scaleLength; }
    
    void setStringCount(int stringCount) { m_stringCount = stringCount; }
    int stringCount() const { return m_stringCount; }
    
    // Serialization
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    
private:
    QString m_name = "Untitled Guitar";
    float m_scaleLength = 648.0f; // 25.5" scale length in mm
    int m_stringCount = 6;
    
    std::vector<std::unique_ptr<GuitarComponent>> m_components;
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
    std::vector<GuitarComponent*> m_selectedComponents;
};