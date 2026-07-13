#pragma once

#include "tools/IDrawingTool.h"

/** Freehand blur stroke (press–drag–release). */
class BlurTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
