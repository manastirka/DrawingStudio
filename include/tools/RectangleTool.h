#pragma once

#include "tools/IDrawingTool.h"

/**
 * Axis-aligned rectangle drag tool (press → drag → release).
 */
class RectangleTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
