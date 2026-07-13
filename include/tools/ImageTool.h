#pragma once

#include "tools/IDrawingTool.h"

/** Click-to-import image (deferred file dialog, places ImagePrimitive). */
class ImageTool : public IDrawingTool {
public:
    void onPress(ToolHost &host, QMouseEvent *event) override;
    void onMove(ToolHost &host, QMouseEvent *event) override;
    void onRelease(ToolHost &host, QMouseEvent *event) override;
};
