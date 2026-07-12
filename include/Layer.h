#pragma once

#include <QString>
#include <QColor>
#include <QUuid>
#include <memory>
#include <vector>

class DrawingPrimitive;

class Layer
{
public:
    explicit Layer(const QString& name = "Layer");
    explicit Layer(const QUuid& id, const QString& name = "Layer");
    ~Layer();

    // Basic properties
    const QUuid& id() const { return m_id; }
    const QString& name() const { return m_name; }
    void setName(const QString& name);

    // Visibility and rendering
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);
    
    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);
    
    float opacity() const { return m_opacity; }
    void setOpacity(float opacity); // 0.0 to 1.0
    
    // Layer color (for identification in UI)
    const QColor& color() const { return m_color; }
    void setColor(const QColor& color);
    
    // Blend mode
    enum class BlendMode {
        Normal,
        Multiply,
        Screen,
        Overlay,
        Darken,
        Lighten
    };
    
    BlendMode blendMode() const { return m_blendMode; }
    void setBlendMode(BlendMode mode);
    
    // Primitive management
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void removePrimitive(DrawingPrimitive* primitive);
    void clearPrimitives();
    
    const std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() const { return m_primitives; }
    std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() { return m_primitives; }
    
    size_t primitiveCount() const { return m_primitives.size(); }
    bool isEmpty() const { return m_primitives.empty(); }
    
    // Layer ordering/z-index
    int zOrder() const { return m_zOrder; }
    void setZOrder(int order) { m_zOrder = order; }

private:
    QUuid m_id;
    QString m_name;
    bool m_visible;
    bool m_locked;
    float m_opacity;
    QColor m_color;
    BlendMode m_blendMode;
    int m_zOrder;
    
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
};
