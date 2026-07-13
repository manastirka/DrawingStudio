#pragma once

#include "tools/IDrawingTool.h"

/**
 * Angle-line tool (multi-stage):
 *  1) press–drag–release baseline (construction dashed line)
 *  2) press–drag–release angled segment from baseline end (15° snaps)
 * Right-click resets. After a segment, baseline stays armed for more segments.
 */
class AngleLineTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
