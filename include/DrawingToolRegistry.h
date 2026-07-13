#pragma once

#include "DrawingTool.h"

class IDrawingTool;

/**
 * Maps DrawingTool enum → live IDrawingTool instance (refactor E11).
 * Tools are process-lifetime singletons; canvas never owns them.
 */
namespace DrawingToolRegistry {

/** @return tool implementation, or null if unsupported */
IDrawingTool *toolFor(DrawingTool tool);

} // namespace DrawingToolRegistry
