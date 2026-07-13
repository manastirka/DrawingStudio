#pragma once

#include "tools/IDrawingTool.h"

/**
 * Multi-click spline tool.
 * Left-click adds points; click near start closes; right-click finalizes.
 */
class SplineTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
