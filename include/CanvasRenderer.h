#pragma once

#include <QColor>
#include <QPainter>
#include <QVector2D>
#include <QVector4D>
#include <vector>

class CanvasRenderer
{
public:
    struct Vertex {
        QVector2D position;
        QVector4D color;
    };

    enum class Mode {
        Points,
        Lines,
        LineStrip,
        LineLoop,
        Triangles,
        TriangleStrip,
        TriangleFan
    };

    struct Batch {
        Mode mode;
        float lineWidth = 1.0f;
        float pointSize = 1.0f;
        bool usePointSize = false;
        std::vector<Vertex> vertices;
    };

    CanvasRenderer();
    ~CanvasRenderer();

    void initialize();
    void release();

    void setPainter(QPainter* painter);
    QPainter* painter() const { return m_painter; }

    void setBlendEnabled(bool enabled);

    Batch begin(Mode mode, float lineWidth = 1.0f);
    Batch beginPoints(float pointSize);
    void addVertex(Batch& batch, const QVector2D& position, const QColor& color);
    void addVertex(Batch& batch, const QVector2D& position, const QVector4D& color);
    void submit(const Batch& batch);

private:
    QPainter* m_painter = nullptr;
    bool m_initialized = false;
};
