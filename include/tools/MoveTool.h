#pragma once

#include "tools/IDrawingTool.h"

/**
 * Move tool façade — press begins move / control-point edit via canvas.
 * Drag uses canvas m_isMoving + handleMoveOperation.
 */
class MoveTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
