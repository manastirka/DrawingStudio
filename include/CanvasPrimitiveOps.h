#pragma once

#include <QRectF>
#include <QString>
#include <QUuid>
#include <QVector2D>
#include <functional>
#include <memory>
#include <set>
#include <vector>

class DrawingPrimitive;
class LayerManager;
class TextPrimitive;

/**
 * Layer / legacy primitive store helpers (refactor E2).
 * Pure traversal and mutation helpers used by DrawingCanvas.
 */
namespace CanvasPrimitiveOps {

using PrimitiveList = std::vector<std::unique_ptr<DrawingPrimitive>>;

DrawingPrimitive *findById(LayerManager *layers, const PrimitiveList &legacy,
                           const QUuid &id);

/** Top-most hit (reverse layer/primitive order). Skips locked layers. */
DrawingPrimitive *findAt(LayerManager *layers, const PrimitiveList &legacy,
                         const QVector2D &pos, float tolerance);

/**
 * Remove primitives whose ids are in @p ids.
 * @return true if any were removed
 */
bool eraseByIds(LayerManager *layers, PrimitiveList &legacy,
                const std::set<QUuid> &ids);

/** Invoke @p fn for each visible unlocked primitive, top → bottom. */
void forEachVisibleTopFirst(
    LayerManager *layers, const PrimitiveList &legacy,
    const std::function<void(DrawingPrimitive *)> &fn);

/** Collect start/end points of visible line primitives. */
std::vector<QVector2D> collectLineEndpoints(LayerManager *layers,
                                            const PrimitiveList &legacy);

/**
 * Insert primitive into layer manager or legacy list (refactor E3).
 * Resolves/creates target layer, assigns layerId, transfers ownership.
 * @return raw pointer after move (valid while stored), or null on failure
 */
DrawingPrimitive *insert(LayerManager *layers, PrimitiveList &legacy,
                         std::unique_ptr<DrawingPrimitive> primitive);

/** True if newly added primitive should be auto-selected (flag or Text). */
bool shouldAutoSelect(const DrawingPrimitive *primitive);

/**
 * Top-most TextPrimitive under @p pos (visible layers only).
 * Spline-following text uses @p splineTolerance; otherwise @p normalTolerance.
 * Used by double-click-to-edit (refactor E5).
 */
TextPrimitive *findTextAt(LayerManager *layers, const PrimitiveList &legacy,
                          const QVector2D &pos, float normalTolerance = 20.0f,
                          float splineTolerance = 30.0f);

/**
 * Bounding rects of visible, non-selected primitives (alignment snap targets).
 * Skips null layers / null prims / selected items.
 */
std::vector<QRectF> collectUnselectedBounds(LayerManager *layers,
                                            const PrimitiveList &legacy);

/** True if any primitive exists in layers or legacy list. */
bool hasAny(LayerManager *layers, const PrimitiveList &legacy);

/**
 * All visible unlocked primitives whose containsPoint hits @p pos.
 * Used by eraser collect-then-delete path (tool host).
 */
std::vector<DrawingPrimitive *> collectHitsAt(LayerManager *layers,
                                              const PrimitiveList &legacy,
                                              const QVector2D &pos,
                                              float radius);

/**
 * Top-most fillable shape under @p pos (visible unlocked layers).
 * Rect/circle/ellipse also hit via boundingRect when containsPoint fails.
 */
DrawingPrimitive *findFillTarget(LayerManager *layers, const QVector2D &pos,
                                 float tolerance);

/**
 * Erase legacy list entries that contain @p pos within @p radius.
 * @return true if any were removed
 */
bool eraseLegacyContaining(PrimitiveList &legacy, const QVector2D &pos,
                           float radius);

/**
 * Update all DimensionPrimitive unit labels after units change (E8).
 */
void syncDimensionUnits(LayerManager *layers, PrimitiveList &legacy,
                        const QString &unitsString, float pixelsPerUnit);

/**
 * Top-most visible SplinePrimitive under @p pos (E8 — text-on-path pick).
 */
DrawingPrimitive *findSplineAt(LayerManager *layers, const PrimitiveList &legacy,
                               const QVector2D &pos, float tolerance);

/** Collect non-null primitive ids (E10 — delete selection). */
std::set<QUuid> collectIds(const std::vector<DrawingPrimitive *> &primitives);

} // namespace CanvasPrimitiveOps
