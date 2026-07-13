#pragma once

#include "tools/IDrawingTool.h"

/**
 * Straight-line drag tool (press → drag → release).
 */
class LineTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
