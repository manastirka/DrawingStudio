#pragma once

#include "tools/IDrawingTool.h"

/**
 * Three-point arc tool:
 *  1) click start
 *  2) click end (creates preview ArcPrimitive)
 *  3) drag third point (bulge) → release to commit
 * Right-click cancels.
 */
class ArcTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
