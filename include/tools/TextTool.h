#pragma once

#include "tools/IDrawingTool.h"

/** Text tool — forwards events to ClassicTextTool via host callbacks. */
class TextTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
