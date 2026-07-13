#include "DrawingToolRegistry.h"

#include "tools/IDrawingTool.h"
#include "tools/LineTool.h"
#include "tools/RectangleTool.h"
#include "tools/CircleTool.h"
#include "tools/EllipseTool.h"
#include "tools/MeasureTool.h"
#include "tools/ArcTool.h"
#include "tools/AngleLineTool.h"
#include "tools/BezierTool.h"
#include "tools/CurveTool.h"
#include "tools/SplineTool.h"
#include "tools/PolygonTool.h"
#include "tools/EraserTool.h"
#include "tools/FillTool.h"
#include "tools/BrushTool.h"
#include "tools/BlurTool.h"
#include "tools/SelectTool.h"
#include "tools/MoveTool.h"
#include "tools/ImageTool.h"
#include "tools/TextTool.h"

namespace {

LineTool g_lineTool;
RectangleTool g_rectangleTool;
CircleTool g_circleTool;
EllipseTool g_ellipseTool;
MeasureTool g_measureTool;
ArcTool g_arcTool;
AngleLineTool g_angleLineTool;
BezierTool g_bezierTool;
CurveTool g_curveTool;
SplineTool g_splineTool;
PolygonTool g_polygonTool;
EraserTool g_eraserTool;
FillTool g_fillTool;
BrushTool g_brushTool;
BlurTool g_blurTool;
SelectTool g_selectTool;
MoveTool g_moveTool;
ImageTool g_imageTool;
TextTool g_textTool;

} // namespace

namespace DrawingToolRegistry {

IDrawingTool *toolFor(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Line:
        return &g_lineTool;
    case DrawingTool::Rectangle:
        return &g_rectangleTool;
    case DrawingTool::Circle:
        return &g_circleTool;
    case DrawingTool::Ellipse:
        return &g_ellipseTool;
    case DrawingTool::Measure:
        return &g_measureTool;
    case DrawingTool::Arc:
        return &g_arcTool;
    case DrawingTool::AngleLine:
        return &g_angleLineTool;
    case DrawingTool::BezierCurve:
        return &g_bezierTool;
    case DrawingTool::Curve:
        return &g_curveTool;
    case DrawingTool::Spline:
        return &g_splineTool;
    case DrawingTool::Polygon:
        return &g_polygonTool;
    case DrawingTool::Eraser:
        return &g_eraserTool;
    case DrawingTool::Fill:
        return &g_fillTool;
    case DrawingTool::Brush:
        return &g_brushTool;
    case DrawingTool::Blur:
        return &g_blurTool;
    case DrawingTool::Select:
        return &g_selectTool;
    case DrawingTool::Move:
        return &g_moveTool;
    case DrawingTool::Image:
        return &g_imageTool;
    case DrawingTool::Text:
        return &g_textTool;
    default:
        return nullptr;
    }
}

} // namespace DrawingToolRegistry
