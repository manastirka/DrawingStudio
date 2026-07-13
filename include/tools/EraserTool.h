#pragma once

#include "tools/IDrawingTool.h"

/**
 * Click-to-erase tool — deletes primitives under the eraser radius.
 * Press only; move/release are no-ops.
 */
class EraserTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
