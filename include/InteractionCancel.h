#pragma once

#include <functional>

/**
 * Escape / cancel transient canvas interaction (refactor E10).
 * Pure flag resets; UI side effects via callbacks.
 */
namespace InteractionCancel {

struct Targets {
    bool *isDrawing = nullptr;
    bool *isPanning = nullptr;
    std::function<void(bool)> setIsSelecting;
    std::function<void()> clearSelection;
    std::function<void()> setArrowCursor;
    std::function<void()> requestUpdate;
};

/** Clear drawing / marquee / pan; restore arrow cursor; refresh. */
void applyEscape(const Targets &t);

/** End space-bar pan if active. */
void endSpacePan(bool *isPanning, const std::function<void()> &setArrowCursor);

} // namespace InteractionCancel
