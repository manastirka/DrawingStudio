#pragma once

#include "tools/IDrawingTool.h"

/**
 * Dimension / measure drag tool (press → drag → release).
 * Places a DimensionPrimitive with live length hint.
 */
class MeasureTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
