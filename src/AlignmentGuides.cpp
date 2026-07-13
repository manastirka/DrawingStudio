#include "AlignmentGuides.h"

#include <cmath>
#include <limits>

namespace AlignmentGuides {

Result compute(const QVector2D &mousePos, float zoomLevel,
               const std::vector<QRectF> &bounds)
{
    Result result;
    result.snapPos = mousePos;

    if (zoomLevel <= 0.f) {
        return result;
    }

    const float snapTolerance = 10.0f / zoomLevel;
    float bestDistance = std::numeric_limits<float>::max();

    std::vector<float> horizontalGuides;
    std::vector<float> verticalGuides;
    horizontalGuides.reserve(bounds.size() * 3);
    verticalGuides.reserve(bounds.size() * 3);

    for (const QRectF &b : bounds) {
        horizontalGuides.push_back(static_cast<float>(b.top()));
        horizontalGuides.push_back(static_cast<float>(b.bottom()));
        horizontalGuides.push_back(static_cast<float>(b.center().y()));
        verticalGuides.push_back(static_cast<float>(b.left()));
        verticalGuides.push_back(static_cast<float>(b.right()));
        verticalGuides.push_back(static_cast<float>(b.center().x()));
    }

    float bestHDist = std::numeric_limits<float>::max();
    float bestHPos = 0.f;
    bool foundH = false;
    for (float guideY : horizontalGuides) {
        const float dist = std::abs(mousePos.y() - guideY);
        if (dist < snapTolerance && dist < bestHDist) {
            bestHDist = dist;
            bestHPos = guideY;
            foundH = true;
        }
    }
    if (foundH) {
        result.guides.push_back(
            {QVector2D(0.f, bestHPos), Axis::Horizontal, true});
        if (bestHDist < bestDistance) {
            bestDistance = bestHDist;
            result.snapPos = QVector2D(mousePos.x(), bestHPos);
        }
    }

    float bestVDist = std::numeric_limits<float>::max();
    float bestVPos = 0.f;
    bool foundV = false;
    for (float guideX : verticalGuides) {
        const float dist = std::abs(mousePos.x() - guideX);
        if (dist < snapTolerance && dist < bestVDist) {
            bestVDist = dist;
            bestVPos = guideX;
            foundV = true;
        }
    }
    if (foundV) {
        result.guides.push_back(
            {QVector2D(bestVPos, 0.f), Axis::Vertical, true});
        if (bestVDist < bestDistance) {
            bestDistance = bestVDist;
            result.snapPos = QVector2D(bestVPos, mousePos.y());
        }
    }

    result.snapActive = !result.guides.empty();
    return result;
}

bool objectNearGuides(const QRectF &bounds, const std::vector<Guide> &guides,
                      float tolerance)
{
    for (const auto &guide : guides) {
        if (!guide.isActive) {
            continue;
        }
        if (guide.axis == Axis::Horizontal) {
            if (std::abs(bounds.top() - guide.position.y()) < tolerance ||
                std::abs(bounds.bottom() - guide.position.y()) < tolerance ||
                std::abs(bounds.center().y() - guide.position.y()) <
                    tolerance) {
                return true;
            }
        } else {
            if (std::abs(bounds.left() - guide.position.x()) < tolerance ||
                std::abs(bounds.right() - guide.position.x()) < tolerance ||
                std::abs(bounds.center().x() - guide.position.x()) <
                    tolerance) {
                return true;
            }
        }
    }
    return false;
}

} // namespace AlignmentGuides
