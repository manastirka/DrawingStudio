#pragma once

#include "tools/IDrawingTool.h"

/**
 * Selection tool façade — press dispatches to canvas select logic
 * (handles, marquee, multi-select). Move/release stay on canvas gesture flags.
 */
class SelectTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
