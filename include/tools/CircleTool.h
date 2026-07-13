#pragma once

#include "tools/IDrawingTool.h"

/**
 * Circle drag tool — center at press, radius follows cursor (press → drag → release).
 */
class CircleTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
