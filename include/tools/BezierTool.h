#pragma once

#include "tools/IDrawingTool.h"

/**
 * Cubic Bezier drag tool:
 *  - press to start (optional chain from previous end if click is near it)
 *  - drag end + auto handles at 1/3 and 2/3
 *  - release to commit (min length 2)
 *  - Shift+click during creation edits a control point
 *  - Right-click cancels
 */
class BezierTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
