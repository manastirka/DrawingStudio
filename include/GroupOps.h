#pragma once

#include <QUuid>
#include <functional>
#include <memory>
#include <vector>

class DrawingPrimitive;
class LayerManager;

/**
 * Soft-group helpers (shared groupId) — refactor E9.
 * Command construction stays on DrawingCanvas.
 */
namespace GroupOps {

using PrimitiveList = std::vector<std::unique_ptr<DrawingPrimitive>>;

/** True if selection can form a group (≥2 non-null objects). */
bool canGroup(const std::vector<DrawingPrimitive *> &selected);

/** True if any selected object already has a groupId. */
bool anyGrouped(const std::vector<DrawingPrimitive *> &selected);

/**
 * Assign a fresh group id to all non-null selected objects.
 * @return the new group id, or null if nothing was grouped
 */
QUuid assignNewGroup(const std::vector<DrawingPrimitive *> &selected);

/** Clear groupId on all non-null selected objects. */
void clearGroups(const std::vector<DrawingPrimitive *> &selected);

/**
 * Collect visible unlocked primitives matching @p groupId (top→bottom not required).
 */
std::vector<DrawingPrimitive *>
collectMembers(LayerManager *layers, const PrimitiveList &legacy,
               const QUuid &groupId);

} // namespace GroupOps
