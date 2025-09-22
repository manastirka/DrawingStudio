#include "GuitarProject.h"
#include "GuitarComponent.h"
#include "DrawingPrimitive.h"
#include <QJsonArray>
#include <algorithm>

void GuitarProject::addComponent(std::unique_ptr<GuitarComponent> component)
{
    m_components.push_back(std::move(component));
}

void GuitarProject::removeComponent(GuitarComponent* component)
{
    auto it = std::find_if(m_components.begin(), m_components.end(),
                          [component](const std::unique_ptr<GuitarComponent>& ptr) {
                              return ptr.get() == component;
                          });
    
    if (it != m_components.end()) {
        // Remove from selection if selected
        deselectComponent(component);
        m_components.erase(it);
    }
}

GuitarComponent* GuitarProject::findComponentAt(const QVector2D &position, float tolerance) const
{
    for (const auto& component : m_components) {
        if (component->containsPoint(position)) {
            return component.get();
        }
    }
    return nullptr;
}

void GuitarProject::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive)
{
    m_primitives.push_back(std::move(primitive));
}

void GuitarProject::removePrimitive(DrawingPrimitive* primitive)
{
    auto it = std::find_if(m_primitives.begin(), m_primitives.end(),
                          [primitive](const std::unique_ptr<DrawingPrimitive>& ptr) {
                              return ptr.get() == primitive;
                          });
    
    if (it != m_primitives.end()) {
        m_primitives.erase(it);
    }
}

void GuitarProject::clearPrimitives()
{
    m_primitives.clear();
}

void GuitarProject::clearSelection()
{
    for (auto* component : m_selectedComponents) {
        component->setSelected(false);
    }
    m_selectedComponents.clear();
}

void GuitarProject::selectComponent(GuitarComponent* component)
{
    if (component && std::find(m_selectedComponents.begin(), m_selectedComponents.end(), component) == m_selectedComponents.end()) {
        component->setSelected(true);
        m_selectedComponents.push_back(component);
    }
}

void GuitarProject::deselectComponent(GuitarComponent* component)
{
    auto it = std::find(m_selectedComponents.begin(), m_selectedComponents.end(), component);
    if (it != m_selectedComponents.end()) {
        component->setSelected(false);
        m_selectedComponents.erase(it);
    }
}

QJsonObject GuitarProject::serialize() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["scaleLength"] = m_scaleLength;
    obj["stringCount"] = m_stringCount;
    
    // Serialize components
    QJsonArray componentsArray;
    for (const auto& component : m_components) {
        componentsArray.append(component->serialize());
    }
    obj["components"] = componentsArray;
    
    // Serialize primitives
    QJsonArray primitivesArray;
    for (const auto& primitive : m_primitives) {
        // TODO: Add serialize method to DrawingPrimitive
        // primitivesArray.append(primitive->serialize());
    }
    obj["primitives"] = primitivesArray;
    
    return obj;
}

void GuitarProject::deserialize(const QJsonObject &json)
{
    m_name = json["name"].toString();
    m_scaleLength = json["scaleLength"].toDouble();
    m_stringCount = json["stringCount"].toInt();
    
    // Clear existing data
    clearSelection();
    m_components.clear();
    m_primitives.clear();
    
    // Deserialize components
    QJsonArray componentsArray = json["components"].toArray();
    for (const auto& value : componentsArray) {
        QJsonObject componentObj = value.toObject();
        // TODO: Factory method to create components from JSON
        // auto component = ComponentFactory::createFromJson(componentObj);
        // if (component) {
        //     addComponent(std::move(component));
        // }
    }
    
    // Deserialize primitives
    QJsonArray primitivesArray = json["primitives"].toArray();
    for (const auto& value : primitivesArray) {
        QJsonObject primitiveObj = value.toObject();
        // TODO: Factory method to create primitives from JSON
        // auto primitive = PrimitiveFactory::createFromJson(primitiveObj);
        // if (primitive) {
        //     addPrimitive(std::move(primitive));
        // }
    }
}