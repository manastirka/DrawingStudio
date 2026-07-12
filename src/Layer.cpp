#include "Layer.h"
#include "DrawingPrimitive.h"
#include <algorithm>

Layer::Layer(const QString& name)
    : m_id(QUuid::createUuid())
    , m_name(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(1.0f)
    , m_color(QColor(128, 128, 128)) // Default gray color for layer identification
    , m_blendMode(BlendMode::Normal)
    , m_zOrder(0)
{
}

Layer::Layer(const QUuid& id, const QString& name)
    : m_id(id.isNull() ? QUuid::createUuid() : id)
    , m_name(name)
    , m_visible(true)
    , m_locked(false)
    , m_opacity(1.0f)
    , m_color(QColor(128, 128, 128)) // Default gray color for layer identification
    , m_blendMode(BlendMode::Normal)
    , m_zOrder(0)
{
}

Layer::~Layer()
{
    clearPrimitives();
}

void Layer::setName(const QString& name)
{
    m_name = name;
}

void Layer::setVisible(bool visible)
{
    m_visible = visible;
}

void Layer::setLocked(bool locked)
{
    m_locked = locked;
}

void Layer::setOpacity(float opacity)
{
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void Layer::setColor(const QColor& color)
{
    m_color = color;
}

void Layer::setBlendMode(BlendMode mode)
{
    m_blendMode = mode;
}

void Layer::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive)
{
    if (primitive) {
        m_primitives.push_back(std::move(primitive));
    }
}

void Layer::removePrimitive(DrawingPrimitive* primitive)
{
    auto it = std::find_if(m_primitives.begin(), m_primitives.end(),
        [primitive](const std::unique_ptr<DrawingPrimitive>& p) {
            return p.get() == primitive;
        });
    
    if (it != m_primitives.end()) {
        m_primitives.erase(it);
    }
}

void Layer::clearPrimitives()
{
    m_primitives.clear();
}
