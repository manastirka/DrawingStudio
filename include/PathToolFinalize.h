#pragma once

#include <memory>

class DrawingPrimitive;

/**
 * Pure helpers for incomplete path primitives when switching tools (D5).
 */
namespace PathToolFinalize {

/**
 * Whether a draft primitive is complete enough to commit on tool switch.
 * Returns false for null / unknown types that should be discarded when empty.
 */
bool shouldCommitOnToolSwitch(const DrawingPrimitive *primitive);

} // namespace PathToolFinalize
