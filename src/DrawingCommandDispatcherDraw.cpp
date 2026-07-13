#include "DrawingCommandDispatcher.h"

#include "DrawingPrimitive.h"
#include "DXFExporter.h"
#include "EdgeSelectionTool.h"
#include "ImagePrimitive.h"
#include "ImageToDrawingEngine.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QVector2D>

#include <memory>
#include <vector>

// Draw commands (refactor E29).

bool DrawingCommandDispatcher::tryExecuteDraw(const QString &action, const QJsonObject &params)
{
    if (action == "draw_line") {
    auto line = std::make_unique<LinePrimitive>(
        QVector2D(getDouble(params, "x1"), getDouble(params, "y1")),
        QVector2D(getDouble(params, "x2"), getDouble(params, "y2")));
    line->setColor(parseColor(params, "color"));
    line->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    if (params.contains("lineStyle")) {
        int style = params["lineStyle"].toInt(1);
        line->setLineStyle(static_cast<Qt::PenStyle>(style));
    }
    applyCommonParams(line.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(line));
        return true;
    }

    if (action == "draw_rectangle") {
    auto rect = std::make_unique<RectanglePrimitive>(
        QVector2D(getDouble(params, "x1"), getDouble(params, "y1")),
        QVector2D(getDouble(params, "x2"), getDouble(params, "y2")));
    rect->setColor(parseColor(params, "color"));
    rect->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    if (getBool(params, "fill")) {
        rect->setFilled(true);
        rect->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
    }
    if (params.contains("cornerRadius"))
        rect->setCornerRadius(getDouble(params, "cornerRadius"));
    // Gradient implies fill
    if (params.contains("gradient"))
        rect->setFilled(true);
    applyCommonParams(rect.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(rect));
        return true;
    }

    if (action == "draw_circle") {
    auto circle = std::make_unique<CirclePrimitive>(
        QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
        getDouble(params, "radius", 50.0));
    circle->setColor(parseColor(params, "color"));
    circle->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    if (getBool(params, "fill")) {
        circle->setFilled(true);
        circle->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
    }
    if (params.contains("gradient"))
        circle->setFilled(true);
    applyCommonParams(circle.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(circle));
        return true;
    }

    if (action == "draw_ellipse") {
    auto ellipse = std::make_unique<EllipsePrimitive>(
        QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
        getDouble(params, "rx", 50.0),
        getDouble(params, "ry", 30.0));
    ellipse->setColor(parseColor(params, "color"));
    ellipse->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    if (getBool(params, "fill")) {
        ellipse->setFilled(true);
        ellipse->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
    }
    if (params.contains("gradient"))
        ellipse->setFilled(true);
    applyCommonParams(ellipse.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(ellipse));
        return true;
    }

    if (action == "draw_polygon") {
    auto polygon = std::make_unique<PolygonPrimitive>();
    QJsonArray pointsArr = params["points"].toArray();
    for (const auto& pt : pointsArr) {
        QJsonObject p = pt.toObject();
        polygon->addPoint(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
    }
    polygon->setColor(parseColor(params, "color"));
    polygon->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    polygon->setClosed(getBool(params, "closed", true));
    if (getBool(params, "fill")) {
        polygon->setFilled(true);
        polygon->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
    }
    if (params.contains("gradient"))
        polygon->setFilled(true);
    applyCommonParams(polygon.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(polygon));
        return true;
    }

    if (action == "draw_text") {
    auto text = std::make_unique<TextPrimitive>(
        QVector2D(getDouble(params, "x"), getDouble(params, "y")),
        params["text"].toString("Text"));
    text->setColor(parseColor(params, "color"));
    if (params.contains("fontFamily"))
        text->setFontFamily(params["fontFamily"].toString());
    if (params.contains("fontSize"))
        text->setFontSize(getDouble(params, "fontSize", 24.0));
    text->setBold(getBool(params, "bold"));
    text->setItalic(getBool(params, "italic"));
    applyCommonParams(text.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(text));
        return true;
    }

    if (action == "draw_line_multi") {
    // Polyline: sequence of connected line segments
    QJsonArray pointsArr = params["points"].toArray();
    QColor color = parseColor(params, "color");
    float lineWidth = getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth());
    for (int i = 0; i + 1 < pointsArr.size(); ++i) {
        QJsonObject p1 = pointsArr[i].toObject();
        QJsonObject p2 = pointsArr[i + 1].toObject();
        auto line = std::make_unique<LinePrimitive>(
            QVector2D(p1["x"].toDouble(), p1["y"].toDouble()),
            QVector2D(p2["x"].toDouble(), p2["y"].toDouble()));
        line->setColor(color);
        line->setLineWidth(lineWidth);
        applyCommonParams(line.get(), params);
        m_ctx.canvas->addPrimitiveWithCommand(std::move(line));
    }
        return true;
    }

    if (action == "draw_bezier") {
    auto bezier = std::make_unique<BezierCurvePrimitive>();
    QJsonArray pointsArr = params["points"].toArray();
    std::vector<QVector2D> pts;
    for (const auto& pt : pointsArr) {
        QJsonObject p = pt.toObject();
        pts.push_back(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
    }
    bezier->setControlPoints(pts);
    bezier->setColor(parseColor(params, "color"));
    bezier->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    applyCommonParams(bezier.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(bezier));
        return true;
    }

    if (action == "draw_spline") {
    auto spline = std::make_unique<SplinePrimitive>();
    QJsonArray pointsArr = params["points"].toArray();
    for (const auto& pt : pointsArr) {
        QJsonObject p = pt.toObject();
        spline->addPoint(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
    }
    spline->setColor(parseColor(params, "color"));
    spline->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    if (params.contains("smoothness"))
        spline->setSmoothness(getDouble(params, "smoothness", 0.5));
    spline->setClosed(getBool(params, "closed"));
    if (getBool(params, "fill")) {
        spline->setFilled(true);
        spline->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
    }
    if (params.contains("gradient"))
        spline->setFilled(true);
    applyCommonParams(spline.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(spline));
        return true;
    }

    if (action == "draw_arc") {
    auto arc = std::make_unique<ArcPrimitive>(
        QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
        getDouble(params, "radius", 50.0),
        getDouble(params, "startAngle", 0.0),
        getDouble(params, "endAngle", 90.0));
    arc->setColor(parseColor(params, "color"));
    arc->setLineWidth(getDouble(params, "lineWidth", m_ctx.canvas->defaultLineWidth()));
    applyCommonParams(arc.get(), params);
    m_ctx.canvas->addPrimitiveWithCommand(std::move(arc));
        return true;
    }

    return false;
}
