#include "OverlayRenderer.h"

#include <QPainter>
#include <QPen>

void OverlayRenderer::drawControlPoint(QPainter &painter, const QVector2D &point,
                                       float zoomLevel, bool highlighted)
{
    const float r = (highlighted ? 4.2f : 3.4f) / zoomLevel;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 23, 42, 40));
    painter.drawEllipse(QPointF(point.x(), point.y()), r + 0.7f / zoomLevel,
                        r + 0.7f / zoomLevel);

    QPen ring(highlighted ? QColor(34, 197, 94) : QColor(37, 99, 235), 1.3);
    ring.setCosmetic(true);
    painter.setPen(ring);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(point.x(), point.y()), r, r);

    painter.setPen(Qt::NoPen);
    painter.setBrush(highlighted ? QColor(34, 197, 94) : QColor(59, 130, 246));
    painter.drawEllipse(QPointF(point.x(), point.y()), r * 0.32f, r * 0.32f);
}

void OverlayRenderer::drawSnapIndicator(QPainter &painter,
                                        const QPoint &screenCenter)
{
    QColor indicator(74, 144, 226, 220);
    painter.setPen(QPen(indicator, 2.0));
    painter.setBrush(QColor(74, 144, 226, 60));
    painter.drawEllipse(screenCenter, 4, 4);
    painter.drawLine(screenCenter.x() - 6, screenCenter.y(),
                     screenCenter.x() + 6, screenCenter.y());
    painter.drawLine(screenCenter.x(), screenCenter.y() - 6, screenCenter.x(),
                     screenCenter.y() + 6);
}

void OverlayRenderer::drawCursorPreviewRing(QPainter &painter,
                                            const QPoint &screenCenter,
                                            float radiusScreenPx)
{
    if (radiusScreenPx <= 0.0f) {
        return;
    }
    QColor ringColor(120, 180, 255, 220);
    painter.setPen(QPen(ringColor, 1.5));
    painter.setBrush(QColor(120, 180, 255, 40));
    painter.drawEllipse(screenCenter, static_cast<int>(radiusScreenPx),
                        static_cast<int>(radiusScreenPx));
}

void OverlayRenderer::drawLassoPolygon(QPainter &painter,
                                       const QPolygonF &screenPoly)
{
    if (screenPoly.size() < 2) {
        return;
    }
    painter.setPen(QPen(QColor(74, 144, 226, 200), 1.5, Qt::DashLine));
    painter.setBrush(QColor(74, 144, 226, 40));
    painter.drawPolygon(screenPoly);
}

void OverlayRenderer::drawAlignmentGuides(QPainter &painter,
                                          const std::vector<GuideLine> &guides)
{
    for (const auto &guide : guides) {
        QPen guidePen(guide.color, 2.0);
        guidePen.setStyle(Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);

        if (guide.axis == GuideLine::Axis::Vertical) {
            painter.drawLine(QPointF(guide.position, -10000.0),
                             QPointF(guide.position, 10000.0));
        } else {
            painter.drawLine(QPointF(-10000.0, guide.position),
                             QPointF(10000.0, guide.position));
        }
    }
}

QPolygonF OverlayRenderer::lassoScreenPolygon(
    const QPolygonF &worldPoints,
    const std::function<QPoint(const QVector2D &)> &worldToScreen)
{
    QPolygonF screenPoly;
    if (!worldToScreen || worldPoints.size() < 2) {
        return screenPoly;
    }
    screenPoly.reserve(worldPoints.size());
    for (const QPointF &pt : worldPoints) {
        screenPoly << worldToScreen(QVector2D(static_cast<float>(pt.x()),
                                              static_cast<float>(pt.y())))
                          .toPointF();
    }
    return screenPoly;
}

std::vector<OverlayRenderer::GuideLine>
OverlayRenderer::guideLinesFrom(const std::vector<AlignmentGuides::Guide> &guides,
                                const QColor &objectColor)
{
    std::vector<GuideLine> lines;
    lines.reserve(guides.size());
    for (const auto &g : guides) {
        if (!g.isActive) {
            continue;
        }
        GuideLine gl;
        gl.color = objectColor;
        if (g.axis == AlignmentGuides::Axis::Vertical) {
            gl.axis = GuideLine::Axis::Vertical;
            gl.position = g.position.x();
        } else {
            gl.axis = GuideLine::Axis::Horizontal;
            gl.position = g.position.y();
        }
        lines.push_back(gl);
    }
    return lines;
}
