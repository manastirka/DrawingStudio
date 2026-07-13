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

// Edit / tool / view commands (refactor E29).

bool DrawingCommandDispatcher::tryExecuteEdit(const QString &action, const QJsonObject &params)
{
    if (action == "deselect") {
    m_ctx.canvas->clearSelection();
        return true;
    }

    if (action == "set_grid") {
    m_ctx.canvas->setGridVisible(getBool(params, "visible", false));
    m_ctx.canvas->setSnapEnabled(getBool(params, "snap", false));
        return true;
    }

    if (action == "set_background") {
    m_ctx.canvas->setBackgroundColor(parseColor(params, "color", QColor("#ffffff")));
    if (params.contains("paperColor"))
        m_ctx.canvas->setPaperColor(parseColor(params, "paperColor", QColor("#ffffff")));
        return true;
    }

    if (action == "set_rulers") {
    m_ctx.canvas->setRulersVisible(getBool(params, "visible", false));
        return true;
    }

    if (action == "set_color") {
    m_ctx.canvas->setDefaultDrawingColor(parseColor(params, "color"));
        return true;
    }

    if (action == "set_line_width") {
    m_ctx.canvas->setDefaultLineWidth(getDouble(params, "width", 2.0));
        return true;
    }

    if (action == "set_fill") {
    m_ctx.canvas->setDefaultFillEnabled(getBool(params, "enabled", true));
    if (params.contains("color"))
        m_ctx.canvas->setDefaultFillColor(parseColor(params, "color"));
        return true;
    }

    if (action == "select_all") {
    if (m_ctx.selectAll) m_ctx.selectAll();
        return true;
    }

    if (action == "delete_selected") {
    if (m_ctx.deleteSelected) m_ctx.deleteSelected();
        return true;
    }

    if (action == "copy_selected") {
    if (m_ctx.copySelected) m_ctx.copySelected();
    QJsonObject result;
    result["success"] = (m_ctx.clipboardSize ? m_ctx.clipboardSize() > 0 : false);
    result["count"] = (m_ctx.clipboardSize ? m_ctx.clipboardSize() : 0);
    m_lastResult = result;
        return true;
    }

    if (action == "cut_selected") {
    if (m_ctx.copySelected) m_ctx.copySelected();
    const int count = (m_ctx.clipboardSize ? m_ctx.clipboardSize() : 0);
    if (count > 0)
        if (m_ctx.deleteSelected) m_ctx.deleteSelected();
    QJsonObject result;
    result["success"] = count > 0;
    result["count"] = count;
    m_lastResult = result;
        return true;
    }

    if (action == "paste") {
    const int before = m_ctx.clipboardSize ? m_ctx.clipboardSize() : 0;
    if (m_ctx.pasteClipboard) m_ctx.pasteClipboard();
    QJsonObject result;
    result["success"] = before > 0;
    result["count"] = before;
    m_lastResult = result;
        return true;
    }

    if (action == "duplicate_selected") {
    const int count = m_ctx.canvas ? static_cast<int>(m_ctx.canvas->selectedObjects().size()) : 0;
    if (m_ctx.duplicateSelected) m_ctx.duplicateSelected();
    QJsonObject result;
    result["success"] = count > 0;
    result["count"] = count;
    m_lastResult = result;
        return true;
    }

    if (action == "set_tool") {
    const QString tool = params.value("tool").toString().trimmed().toLower();
    DrawingTool mapped = DrawingTool::Select;
    bool ok = true;
    if (tool == "select") mapped = DrawingTool::Select;
    else if (tool == "move") mapped = DrawingTool::Move;
    else if (tool == "line") mapped = DrawingTool::Line;
    else if (tool == "curve") mapped = DrawingTool::Curve;
    else if (tool == "bezier" || tool == "beziercurve") mapped = DrawingTool::BezierCurve;
    else if (tool == "spline") mapped = DrawingTool::Spline;
    else if (tool == "polygon") mapped = DrawingTool::Polygon;
    else if (tool == "arc") mapped = DrawingTool::Arc;
    else if (tool == "circle") mapped = DrawingTool::Circle;
    else if (tool == "rectangle" || tool == "rect") mapped = DrawingTool::Rectangle;
    else if (tool == "ellipse") mapped = DrawingTool::Ellipse;
    else if (tool == "angleline" || tool == "angle_line") mapped = DrawingTool::AngleLine;
    else if (tool == "eraser") mapped = DrawingTool::Eraser;
    else if (tool == "fill") mapped = DrawingTool::Fill;
    else if (tool == "brush") mapped = DrawingTool::Brush;
    else if (tool == "blur") mapped = DrawingTool::Blur;
    else if (tool == "measure") mapped = DrawingTool::Measure;
    else if (tool == "image") mapped = DrawingTool::Image;
    else if (tool == "text") mapped = DrawingTool::Text;
    else ok = false;

    QJsonObject result;
    if (ok) {
        if (m_ctx.activateTool) m_ctx.activateTool(mapped);
        result["success"] = true;
        result["tool"] = tool;
    } else {
        result["success"] = false;
        result["error"] = QStringLiteral("Unknown tool: %1").arg(tool);
    }
    m_lastResult = result;
        return true;
    }

    if (action == "clear_canvas") {
    if (m_ctx.layerManager) {
        m_ctx.layerManager->clearLayers();
    }
    m_ctx.canvas->clearPrimitives();
    m_ctx.canvas->update();
        return true;
    }

    if (action == "zoom_fit") {
    m_ctx.canvas->zoomFit();
        return true;
    }

    if (action == "undo") {
    if (m_ctx.undo) m_ctx.undo();
        return true;
    }

    if (action == "redo") {
    if (m_ctx.redo) m_ctx.redo();
        return true;
    }

    return false;
}
