#include "ToolCursor.h"

namespace ToolCursor {

Qt::CursorShape forTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select:
        return Qt::ArrowCursor;
    case DrawingTool::Move:
        return Qt::SizeAllCursor;
    case DrawingTool::Text:
        return Qt::IBeamCursor;
    case DrawingTool::Eraser:
    default:
        return Qt::CrossCursor;
    }
}

bool showsSizePreview(DrawingTool tool)
{
    return tool == DrawingTool::Brush || tool == DrawingTool::Eraser ||
           tool == DrawingTool::Blur;
}

float previewRadiusWorld(DrawingTool tool, float brushSize, float eraserSize)
{
    if (tool == DrawingTool::Brush || tool == DrawingTool::Blur) {
        return brushSize * 0.5f;
    }
    if (tool == DrawingTool::Eraser) {
        return eraserSize * 0.5f;
    }
    return 0.0f;
}

} // namespace ToolCursor
