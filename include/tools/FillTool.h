#pragma once

#include "tools/IDrawingTool.h"

/**
 * Click-to-fill tool — fills closed shapes under the cursor.
 * Splash mode creates organic circle splatters when enabled.
 * Press only; move/release are no-ops.
 */
class FillTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
