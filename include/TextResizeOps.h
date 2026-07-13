#pragma once

#include <QRectF>
#include <QVector2D>

class TextPrimitive;

/**
 * Text-box resize finish evaluation (refactor E10).
 * Pure compare of initial bounds vs final text metrics for ResizeTextCommand.
 */
namespace TextResizeOps {

struct State {
    QVector2D position;
    float width = 0.f;
    float height = 0.f;
    float fontSize = 0.f;
};

/** Capture live text geometry. */
State capture(const TextPrimitive *text);

/**
 * Build "before" state from drag-start bounds + font size
 * (matches DrawingCanvas m_initialTextBounds / m_initialFontSize).
 */
State fromInitial(const QRectF &initialBounds, float initialFontSize);

/**
 * Whether a ResizeTextCommand should be emitted.
 * Preserves legacy rule: only position.x or width change counts.
 */
bool shouldCommit(const State &before, const State &after);

} // namespace TextResizeOps
