#pragma once

#include "tools/IDrawingTool.h"

/**
 * Multi-click free curve tool.
 * Left-click adds control points; click near start closes; right-click finalizes.
 */
class CurveTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
