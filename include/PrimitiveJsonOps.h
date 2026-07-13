#pragma once

#include <QJsonObject>
#include <vector>

class DrawingPrimitive;

/**
 * JSON state snapshots for undoable multi-object transforms (refactor E14).
 * Used by group/ungroup and similar storeTransformation commands.
 */
namespace PrimitiveJsonOps {

/**
 * Capture toJson() for each non-null primitive in @p objects.
 * Order matches input (nulls skipped).
 */
std::vector<QJsonObject>
snapshotStates(const std::vector<DrawingPrimitive *> &objects);

} // namespace PrimitiveJsonOps
