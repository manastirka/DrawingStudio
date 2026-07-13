#include "CanvasPrimitiveOps.h"

#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QString>
#include <algorithm>

namespace CanvasPrimitiveOps {

DrawingPrimitive *findById(LayerManager *layers, const PrimitiveList &legacy,
                           const QUuid &id)
{
    if (id.isNull()) {
        return nullptr;
    }
    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer) {
                continue;
            }
            for (const auto &prim : layer->primitives()) {
                if (prim && prim->id() == id) {
                    return prim.get();
                }
            }
        }
    }
    for (const auto &prim : legacy) {
        if (prim && prim->id() == id) {
            return prim.get();
        }
    }
    return nullptr;
}

DrawingPrimitive *findAt(LayerManager *layers, const PrimitiveList &legacy,
                         const QVector2D &pos, float tolerance)
{
    if (layers) {
        const auto &layerList = layers->layers();
        for (auto it = layerList.rbegin(); it != layerList.rend(); ++it) {
            const auto &layer = *it;
            if (!layer || !layer->isVisible() || layer->isLocked()) {
                continue;
            }
            const auto &primitives = layer->primitives();
            for (auto primIt = primitives.rbegin(); primIt != primitives.rend();
                 ++primIt) {
                const auto &primitive = *primIt;
                if (primitive && primitive->isVisible() &&
                    primitive->containsPoint(pos, tolerance)) {
                    return primitive.get();
                }
            }
        }
        return nullptr;
    }

    for (auto it = legacy.rbegin(); it != legacy.rend(); ++it) {
        const auto &primitive = *it;
        if (primitive && primitive->isVisible() &&
            primitive->containsPoint(pos, tolerance)) {
            return primitive.get();
        }
    }
    return nullptr;
}

bool eraseByIds(LayerManager *layers, PrimitiveList &legacy,
                const std::set<QUuid> &ids)
{
    if (ids.empty()) {
        return false;
    }
    bool changed = false;
    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible() || layer->isLocked()) {
                continue;
            }
            auto &layerPrimitives = layer->primitives();
            auto it = std::remove_if(
                layerPrimitives.begin(), layerPrimitives.end(),
                [&ids](const std::unique_ptr<DrawingPrimitive> &p) {
                    return p && ids.count(p->id()) > 0;
                });
            if (it != layerPrimitives.end()) {
                changed = true;
                layerPrimitives.erase(it, layerPrimitives.end());
            }
        }
    } else {
        auto it = std::remove_if(
            legacy.begin(), legacy.end(),
            [&ids](const std::unique_ptr<DrawingPrimitive> &p) {
                return p && ids.count(p->id()) > 0;
            });
        if (it != legacy.end()) {
            changed = true;
            legacy.erase(it, legacy.end());
        }
    }
    return changed;
}

void forEachVisibleTopFirst(
    LayerManager *layers, const PrimitiveList &legacy,
    const std::function<void(DrawingPrimitive *)> &fn)
{
    if (!fn) {
        return;
    }
    if (layers) {
        const auto &layerList = layers->layers();
        for (auto it = layerList.rbegin(); it != layerList.rend(); ++it) {
            const auto &layer = *it;
            if (!layer || !layer->isVisible() || layer->isLocked()) {
                continue;
            }
            for (auto primIt = layer->primitives().rbegin();
                 primIt != layer->primitives().rend(); ++primIt) {
                fn(primIt->get());
            }
        }
    } else {
        for (auto it = legacy.rbegin(); it != legacy.rend(); ++it) {
            fn(it->get());
        }
    }
}

std::vector<QVector2D> collectLineEndpoints(LayerManager *layers,
                                            const PrimitiveList &legacy)
{
    std::vector<QVector2D> endpoints;
    auto addFromLine = [&](LinePrimitive *line) {
        if (!line || !line->isVisible()) {
            return;
        }
        endpoints.push_back(line->startPoint());
        endpoints.push_back(line->endPoint());
    };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            for (const auto &primitive : layer->primitives()) {
                if (!primitive || !primitive->isVisible()) {
                    continue;
                }
                addFromLine(dynamic_cast<LinePrimitive *>(primitive.get()));
            }
        }
    } else {
        for (const auto &primitive : legacy) {
            if (primitive) {
                addFromLine(dynamic_cast<LinePrimitive *>(primitive.get()));
            }
        }
    }
    return endpoints;
}

DrawingPrimitive *insert(LayerManager *layers, PrimitiveList &legacy,
                         std::unique_ptr<DrawingPrimitive> primitive)
{
    if (!primitive) {
        return nullptr;
    }

    if (!layers) {
        qDebug() << "CanvasPrimitiveOps::insert legacy type"
                 << static_cast<int>(primitive->type())
                 << "total:" << legacy.size() + 1;
        DrawingPrimitive *raw = primitive.get();
        legacy.push_back(std::move(primitive));
        return raw;
    }

    qDebug() << "CanvasPrimitiveOps::insert type"
             << static_cast<int>(primitive->type()) << "to layer store";

    Layer *targetLayer = nullptr;
    if (!primitive->layerId().isNull()) {
        targetLayer = layers->getLayer(primitive->layerId());
    }
    if (!targetLayer) {
        targetLayer = layers->activeLayer();
        if (!targetLayer) {
            targetLayer = layers->createLayer(QStringLiteral("Background"));
            layers->setActiveLayer(targetLayer);
        }
        if (targetLayer) {
            primitive->setLayerId(targetLayer->id());
        }
    }

    DrawingPrimitive *raw = primitive.get();
    if (targetLayer) {
        if (layers->activeLayer() &&
            targetLayer->id() == layers->activeLayer()->id()) {
            layers->addPrimitiveToActiveLayer(std::move(primitive));
        } else {
            layers->addPrimitiveToLayer(targetLayer->id(), std::move(primitive));
        }
    } else {
        // No layer available — fall back to legacy
        legacy.push_back(std::move(primitive));
    }
    return raw;
}

bool shouldAutoSelect(const DrawingPrimitive *primitive)
{
    if (!primitive) {
        return false;
    }
    return primitive->isSelected() ||
           primitive->type() == PrimitiveType::Text;
}

std::vector<QRectF> collectUnselectedBounds(LayerManager *layers,
                                            const PrimitiveList &legacy)
{
    std::vector<QRectF> bounds;
    auto addIf = [&](DrawingPrimitive *prim) {
        if (prim && !prim->isSelected()) {
            bounds.push_back(prim->boundingRect());
        }
    };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            for (const auto &prim : layer->primitives()) {
                addIf(prim.get());
            }
        }
    } else {
        for (const auto &prim : legacy) {
            addIf(prim.get());
        }
    }
    return bounds;
}

bool hasAny(LayerManager *layers, const PrimitiveList &legacy)
{
    if (!legacy.empty()) {
        return true;
    }
    if (!layers) {
        return false;
    }
    for (const auto &layer : layers->layers()) {
        if (layer && !layer->primitives().empty()) {
            return true;
        }
    }
    return false;
}

std::vector<DrawingPrimitive *> collectHitsAt(LayerManager *layers,
                                              const PrimitiveList &legacy,
                                              const QVector2D &pos,
                                              float radius)
{
    std::vector<DrawingPrimitive *> hits;
    auto tryAdd = [&](DrawingPrimitive *p) {
        if (p && p->containsPoint(pos, radius)) {
            hits.push_back(p);
        }
    };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible() || layer->isLocked()) {
                continue;
            }
            for (const auto &primitive : layer->primitives()) {
                tryAdd(primitive.get());
            }
        }
    } else {
        for (const auto &primitive : legacy) {
            tryAdd(primitive.get());
        }
    }
    return hits;
}

DrawingPrimitive *findFillTarget(LayerManager *layers, const QVector2D &pos,
                                 float tolerance)
{
    if (!layers) {
        return nullptr;
    }
    const auto &layerList = layers->layers();
    for (auto layerIt = layerList.rbegin(); layerIt != layerList.rend();
         ++layerIt) {
        const auto &layer = *layerIt;
        if (!layer || !layer->isVisible() || layer->isLocked()) {
            continue;
        }
        const auto &primitives = layer->primitives();
        for (auto primIt = primitives.rbegin(); primIt != primitives.rend();
             ++primIt) {
            const auto &primitive = *primIt;
            if (!primitive) {
                continue;
            }
            bool hit = primitive->containsPoint(pos, tolerance);
            if (!hit) {
                const auto t = primitive->type();
                if (t == PrimitiveType::Rectangle ||
                    t == PrimitiveType::Circle ||
                    t == PrimitiveType::Ellipse) {
                    hit = primitive->boundingRect().contains(pos.x(), pos.y());
                }
            }
            if (hit) {
                return primitive.get();
            }
        }
    }
    return nullptr;
}

bool eraseLegacyContaining(PrimitiveList &legacy, const QVector2D &pos,
                           float radius)
{
    auto it = std::remove_if(
        legacy.begin(), legacy.end(),
        [&pos, radius](const std::unique_ptr<DrawingPrimitive> &primitive) {
            return primitive && primitive->containsPoint(pos, radius);
        });
    if (it == legacy.end()) {
        return false;
    }
    legacy.erase(it, legacy.end());
    return true;
}

void syncDimensionUnits(LayerManager *layers, PrimitiveList &legacy,
                        const QString &unitsString, float pixelsPerUnit)
{
    auto syncList = [&](const PrimitiveList &list) {
        for (const auto &primitive : list) {
            if (auto *dim =
                    dynamic_cast<DimensionPrimitive *>(primitive.get())) {
                dim->setUnitsString(unitsString);
                dim->setPixelsPerUnit(pixelsPerUnit);
                dim->recalculateMeasurement();
            }
        }
    };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (layer) {
                syncList(layer->primitives());
            }
        }
    } else {
        syncList(legacy);
    }
}

DrawingPrimitive *findSplineAt(LayerManager *layers, const PrimitiveList &legacy,
                               const QVector2D &pos, float tolerance)
{
    DrawingPrimitive *hit = nullptr;
    forEachVisibleTopFirst(layers, legacy, [&](DrawingPrimitive *prim) {
        if (hit || !prim || prim->type() != PrimitiveType::Spline) {
            return;
        }
        if (prim->containsPoint(pos, tolerance)) {
            hit = prim;
        }
    });
    return hit;
}

std::set<QUuid> collectIds(const std::vector<DrawingPrimitive *> &primitives)
{
    std::set<QUuid> ids;
    for (DrawingPrimitive *primitive : primitives) {
        if (primitive) {
            ids.insert(primitive->id());
        }
    }
    return ids;
}

TextPrimitive *findTextAt(LayerManager *layers, const PrimitiveList &legacy,
                          const QVector2D &pos, float normalTolerance,
                          float splineTolerance)
{
    auto tryHit = [&](DrawingPrimitive *raw) -> TextPrimitive * {
        auto *text = dynamic_cast<TextPrimitive *>(raw);
        if (!text) {
            return nullptr;
        }
        const float tol =
            text->followsSpline() ? splineTolerance : normalTolerance;
        if (text->containsPoint(pos, tol)) {
            return text;
        }
        return nullptr;
    };

    if (layers) {
        const auto &layerList = layers->layers();
        for (auto it = layerList.rbegin(); it != layerList.rend(); ++it) {
            const auto &layer = *it;
            if (!layer || !layer->isVisible()) {
                continue;
            }
            const auto &primitives = layer->primitives();
            for (auto primIt = primitives.rbegin();
                 primIt != primitives.rend(); ++primIt) {
                if (TextPrimitive *hit = tryHit(primIt->get())) {
                    return hit;
                }
            }
        }
        return nullptr;
    }

    for (auto it = legacy.rbegin(); it != legacy.rend(); ++it) {
        if (TextPrimitive *hit = tryHit(it->get())) {
            return hit;
        }
    }
    return nullptr;
}

} // namespace CanvasPrimitiveOps
