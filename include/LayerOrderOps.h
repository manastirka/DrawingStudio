#pragma once

#include <vector>

class DrawingPrimitive;
class LayerManager;

/**
 * Z-order operations on a selection (refactor E9).
 * Delegates to LayerManager per-object APIs.
 */
namespace LayerOrderOps {

enum class Op {
    ToFront,
    ToBack,
    Forward,
    Backward
};

/**
 * Apply @p op to each non-null primitive in @p selected via @p layers.
 * No-op when layers is null.
 */
void apply(LayerManager *layers,
           const std::vector<DrawingPrimitive *> &selected, Op op);

} // namespace LayerOrderOps
