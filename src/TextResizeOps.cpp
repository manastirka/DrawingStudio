#include "TextResizeOps.h"

#include "DrawingPrimitive.h"

namespace TextResizeOps {

State capture(const TextPrimitive *text)
{
    State s;
    if (!text) {
        return s;
    }
    s.position = text->position();
    s.width = text->textBoxWidth();
    s.height = text->textBoxHeight();
    s.fontSize = text->fontSize();
    return s;
}

State fromInitial(const QRectF &initialBounds, float initialFontSize)
{
    State s;
    s.position = QVector2D(static_cast<float>(initialBounds.x()),
                           static_cast<float>(initialBounds.y()));
    s.width = static_cast<float>(initialBounds.width());
    s.height = static_cast<float>(initialBounds.height());
    s.fontSize = initialFontSize;
    return s;
}

bool shouldCommit(const State &before, const State &after)
{
    // Legacy DrawingCanvas condition (x or width only)
    return after.position.x() != before.position.x() ||
           after.width != before.width;
}

} // namespace TextResizeOps
