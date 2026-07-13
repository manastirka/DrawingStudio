#pragma once

#include "DrawingTool.h"

#include <Qt>

/**
 * Pure tool → cursor / preview policy (refactor D5).
 */
namespace ToolCursor {

/** Default cursor when entering canvas or switching tools. */
Qt::CursorShape forTool(DrawingTool tool);

/** Soft ring preview for brush / blur / eraser. */
bool showsSizePreview(DrawingTool tool);

/**
 * World-space radius for soft cursor ring (0 if tool has no size preview).
 */
float previewRadiusWorld(DrawingTool tool, float brushSize, float eraserSize);

} // namespace ToolCursor
