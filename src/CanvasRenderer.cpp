#include "CanvasRenderer.h"

#include <QPainterPath>
#include <QPolygonF>
#include <cstddef>

CanvasRenderer::CanvasRenderer()
    : m_painter(nullptr)
    , m_initialized(false)
{
}

CanvasRenderer::~CanvasRenderer()
{
}

void CanvasRenderer::initialize()
{
    m_initialized = true;
}

void CanvasRenderer::release()
{
    m_painter = nullptr;
    m_initialized = false;
}

void CanvasRenderer::setPainter(QPainter* painter)
{
    m_painter = painter;
}

void CanvasRenderer::setBlendEnabled(bool /*enabled*/)
{
    // QPainter always supports alpha blending — nothing to do
}

CanvasRenderer::Batch CanvasRenderer::begin(Mode mode, float lineWidth)
{
    Batch batch;
    batch.mode = mode;
    batch.lineWidth = lineWidth;
    return batch;
}

CanvasRenderer::Batch CanvasRenderer::beginPoints(float pointSize)
{
    Batch batch;
    batch.mode = Mode::Points;
    batch.pointSize = pointSize;
    batch.usePointSize = true;
    return batch;
}

void CanvasRenderer::addVertex(Batch& batch, const QVector2D& position, const QColor& color)
{
    batch.vertices.push_back({position, QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF())});
}

void CanvasRenderer::addVertex(Batch& batch, const QVector2D& position, const QVector4D& color)
{
    batch.vertices.push_back({position, color});
}

static QColor vec4ToColor(const QVector4D& v)
{
    return QColor::fromRgbF(v.x(), v.y(), v.z(), v.w());
}

void CanvasRenderer::submit(const Batch& batch)
{
    if (!m_painter || batch.vertices.empty()) {
        return;
    }

    switch (batch.mode) {
    case Mode::Lines: {
        QPen pen(Qt::black, batch.lineWidth);
        pen.setCosmetic(true);
        m_painter->setBrush(Qt::NoBrush);
        for (size_t i = 0; i + 1 < batch.vertices.size(); i += 2) {
            const auto& v0 = batch.vertices[i];
            const auto& v1 = batch.vertices[i + 1];
            pen.setColor(vec4ToColor(v0.color));
            m_painter->setPen(pen);
            m_painter->drawLine(QPointF(v0.position.x(), v0.position.y()),
                                QPointF(v1.position.x(), v1.position.y()));
        }
        break;
    }
    case Mode::LineStrip: {
        if (batch.vertices.size() < 2) break;
        QPen pen(vec4ToColor(batch.vertices[0].color), batch.lineWidth);
        pen.setCosmetic(true);
        m_painter->setPen(pen);
        m_painter->setBrush(Qt::NoBrush);
        for (size_t i = 0; i + 1 < batch.vertices.size(); ++i) {
            const auto& v0 = batch.vertices[i];
            const auto& v1 = batch.vertices[i + 1];
            pen.setColor(vec4ToColor(v0.color));
            m_painter->setPen(pen);
            m_painter->drawLine(QPointF(v0.position.x(), v0.position.y()),
                                QPointF(v1.position.x(), v1.position.y()));
        }
        break;
    }
    case Mode::LineLoop: {
        if (batch.vertices.size() < 2) break;
        QPolygonF poly;
        poly.reserve(batch.vertices.size());
        for (const auto& v : batch.vertices) {
            poly << QPointF(v.position.x(), v.position.y());
        }
        QPen pen(vec4ToColor(batch.vertices[0].color), batch.lineWidth);
        pen.setCosmetic(true);
        m_painter->setPen(pen);
        m_painter->setBrush(Qt::NoBrush);
        m_painter->drawPolygon(poly);
        break;
    }
    case Mode::TriangleFan: {
        if (batch.vertices.size() < 3) break;
        QPolygonF poly;
        poly.reserve(batch.vertices.size());
        for (const auto& v : batch.vertices) {
            poly << QPointF(v.position.x(), v.position.y());
        }
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(vec4ToColor(batch.vertices[0].color));
        m_painter->drawPolygon(poly);
        break;
    }
    case Mode::Triangles: {
        for (size_t i = 0; i + 2 < batch.vertices.size(); i += 3) {
            QPolygonF tri;
            tri << QPointF(batch.vertices[i].position.x(), batch.vertices[i].position.y())
                << QPointF(batch.vertices[i+1].position.x(), batch.vertices[i+1].position.y())
                << QPointF(batch.vertices[i+2].position.x(), batch.vertices[i+2].position.y());
            m_painter->setPen(Qt::NoPen);
            m_painter->setBrush(vec4ToColor(batch.vertices[i].color));
            m_painter->drawPolygon(tri);
        }
        break;
    }
    case Mode::TriangleStrip: {
        if (batch.vertices.size() < 3) break;
        for (size_t i = 0; i + 2 < batch.vertices.size(); ++i) {
            QPolygonF tri;
            if (i % 2 == 0) {
                tri << QPointF(batch.vertices[i].position.x(), batch.vertices[i].position.y())
                    << QPointF(batch.vertices[i+1].position.x(), batch.vertices[i+1].position.y())
                    << QPointF(batch.vertices[i+2].position.x(), batch.vertices[i+2].position.y());
            } else {
                tri << QPointF(batch.vertices[i+1].position.x(), batch.vertices[i+1].position.y())
                    << QPointF(batch.vertices[i].position.x(), batch.vertices[i].position.y())
                    << QPointF(batch.vertices[i+2].position.x(), batch.vertices[i+2].position.y());
            }
            m_painter->setPen(Qt::NoPen);
            m_painter->setBrush(vec4ToColor(batch.vertices[i].color));
            m_painter->drawPolygon(tri);
        }
        break;
    }
    case Mode::Points: {
        float ps = batch.usePointSize ? batch.pointSize : 4.0f;
        for (const auto& v : batch.vertices) {
            m_painter->setPen(Qt::NoPen);
            m_painter->setBrush(vec4ToColor(v.color));
            m_painter->drawEllipse(QPointF(v.position.x(), v.position.y()), ps * 0.5, ps * 0.5);
        }
        break;
    }
    }
}
