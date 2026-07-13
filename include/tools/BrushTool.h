#pragma once

#include "tools/IDrawingTool.h"

/** Freehand airbrush stroke (press–drag–release). */
class BrushTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
