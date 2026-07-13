#include "tools/MoveTool.h"

void MoveTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (host.movePress) {
        host.movePress(event);
    }
}

void MoveTool::onMove(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Active move uses canvas m_isMoving + handleMoveOperation.
}

void MoveTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // finishMoveOperation stays on canvas mouseReleaseEvent.
}
