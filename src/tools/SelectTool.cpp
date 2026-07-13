#include "tools/SelectTool.h"

void SelectTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (host.selectPress) {
        host.selectPress(event);
    }
}

void SelectTool::onMove(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Marquee / handle drag stays on canvas (m_selectionManager / flags).
}

void SelectTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/)
{
    // Selection finalize stays on canvas mouseReleaseEvent.
}
