#pragma once

#include "tools/IDrawingTool.h"

/**
 * Ellipse drag tool — bounding-box from press to cursor (press → drag → release).
 */
class EllipseTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
