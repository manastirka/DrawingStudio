#include "tools/TextTool.h"

void TextTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.screenToWorld) {
        return;
    }
    if (host.ensureTextToolActive) {
        host.ensureTextToolActive();
    }

    const QVector2D worldPos = host.screenToWorld(event->pos());
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (host.textPress) {
            host.textPress(worldPos);
        }
        break;
    case QEvent::MouseButtonDblClick:
        if (host.textDoubleClick) {
            host.textDoubleClick(worldPos);
        }
        break;
    default:
        if (event->button() == Qt::LeftButton && host.textPress) {
            host.textPress(worldPos);
        }
        break;
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void TextTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.screenToWorld || !host.textMove) {
        return;
    }
    if (host.ensureTextToolActive) {
        host.ensureTextToolActive();
    }
    host.textMove(host.screenToWorld(event->pos()));
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void TextTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.screenToWorld || !host.textRelease) {
        return;
    }
    if (host.ensureTextToolActive) {
        host.ensureTextToolActive();
    }
    host.textRelease(host.screenToWorld(event->pos()));
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
