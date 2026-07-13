#pragma once

#include "tools/IDrawingTool.h"

/**
 * Multi-click polygon tool.
 * Left-click adds vertices; click near start closes; right-click finalizes
 * (min 3 points, non-trivial bounds). Applies default fill on commit.
 */
class PolygonTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
