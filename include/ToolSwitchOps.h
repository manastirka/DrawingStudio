#pragma once

#include "DrawingTool.h"

/**
 * Pure tool-switch decisions (refactor E13).
 * Side effects (commit, timer stop) stay on DrawingCanvas.
 */
namespace ToolSwitchOps {

/** True when switching to a different tool (session must reset). */
inline bool isSwitch(DrawingTool from, DrawingTool to)
{
    return from != to;
}

/** Leaving Text tool for any other tool. */
inline bool isLeavingTextTool(DrawingTool from, DrawingTool to)
{
    return from == DrawingTool::Text && to != DrawingTool::Text;
}

/** Reset multi-stage creation counters used by Bezier / AngleLine / Arc. */
void resetCreationStages(int &bezierStage, int &angleLineStage, int &arcStage);

} // namespace ToolSwitchOps
