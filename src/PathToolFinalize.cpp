#include "PathToolFinalize.h"

#include "DrawingPrimitive.h"

namespace PathToolFinalize {

bool shouldCommitOnToolSwitch(const DrawingPrimitive *primitive)
{
    if (!primitive) {
        return false;
    }

    if (auto *curve = dynamic_cast<const CurvePrimitive *>(primitive)) {
        return curve->controlPoints().size() >= 2;
    }
    if (auto *spline = dynamic_cast<const SplinePrimitive *>(primitive)) {
        return spline->points().size() >= 2;
    }
    if (auto *bezier = dynamic_cast<const BezierCurvePrimitive *>(primitive)) {
        const auto &cps = bezier->controlPoints();
        if (cps.size() < 4) {
            return false;
        }
        return (cps.front() - cps.back()).length() >= 2.0f;
    }

    // Lines, shapes, etc. — keep if present
    return true;
}

} // namespace PathToolFinalize
