#include "tools/AngleLineTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
const QColor kBaselineColor(59, 130, 246, 160);

void setHint(ToolHost &host, const QString &hint)
{
    if (host.lastSmartHint) {
        *host.lastSmartHint = hint;
    }
    if (host.setSmartHint) {
        host.setSmartHint(hint);
    }
}

void resetAngleSession(ToolHost &host, bool clearBaselineMeta)
{
    if (host.currentPrimitive) {
        host.currentPrimitive->reset();
    }
    if (host.isDrawing) {
        *host.isDrawing = false;
    }
    if (host.angleLineStage) {
        *host.angleLineStage = 0;
    }
    if (clearBaselineMeta) {
        if (host.angleBaselinePrimitiveId) {
            *host.angleBaselinePrimitiveId = QUuid();
        }
        if (host.angleBaselineConstraintDir) {
            *host.angleBaselineConstraintDir = QVector2D();
        }
    }
}

QVector2D gridWorld(const ToolHost &host, const QPoint &screen)
{
    QVector2D world =
        host.screenToWorld ? host.screenToWorld(screen) : QVector2D();
    return host.snapToGrid ? host.snapToGrid(world) : world;
}

QString angleHint(const QVector2D &baseStart, const QVector2D &baseEnd,
                  const QVector2D &segEnd)
{
    const QVector2D delta = segEnd - baseEnd;
    float absDeg =
        std::atan2(delta.y(), delta.x()) * 180.0f / kPi;
    const QVector2D base = baseEnd - baseStart;
    float baseDeg =
        std::atan2(base.y(), base.x()) * 180.0f / kPi;
    float rel = absDeg - baseDeg;
    while (rel > 180.0f)
        rel -= 360.0f;
    while (rel < -180.0f)
        rel += 360.0f;
    return QStringLiteral("Angle: %1° from baseline (%2° absolute) · "
                          "Shift=5° · Alt=free")
        .arg(rel, 0, 'f', 0)
        .arg(absDeg, 0, 'f', 0);
}
} // namespace

void AngleLineTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.angleLineStage || !host.angleBaselineStart ||
        !host.angleBaselineEnd || !host.drawStart || !host.drawCurrent ||
        !host.currentPrimitive) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        resetAngleSession(host, /*clearBaselineMeta=*/true);
        setHint(host,
                QStringLiteral(
                    "Angle line: drag a baseline, then drag the angled segment"));
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QVector2D worldPos = gridWorld(host, event->pos());
    const int stage = *host.angleLineStage;

    if (stage == 0 || stage == 1) {
        // Start / restart baseline press–drag–release
        *host.angleBaselineStart = worldPos;
        *host.angleBaselineEnd = worldPos;
        *host.drawStart = worldPos;
        *host.drawCurrent = worldPos;
        host.currentPrimitive->reset();
        *host.isDrawing = true;
        *host.angleLineStage = 1;
        setHint(host, QStringLiteral(
                          "Drag to set the reference baseline, then release"));
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    if (stage == 2) {
        // Press–drag angled segment from baseline end
        *host.drawStart = *host.angleBaselineEnd;
        *host.drawCurrent = worldPos;
        QVector2D end = worldPos;
        if (host.snapAngleEndpoint) {
            end = host.snapAngleEndpoint(*host.angleBaselineEnd, worldPos,
                                         event->modifiers());
        }
        auto line =
            std::make_unique<LinePrimitive>(*host.angleBaselineEnd, end);
        line->setColor(host.defaultColor);
        line->setLineStyle(host.defaultLineStyle);
        line->setLineWidth(host.defaultLineWidth);
        *host.currentPrimitive = std::move(line);
        *host.isDrawing = true;
        setHint(host,
                QStringLiteral(
                    "Drag angled line · snaps to 15° from baseline · "
                    "Shift=5° · Alt=free · Right-click cancels"));
        if (host.requestUpdate) {
            host.requestUpdate();
        }
    }
}

void AngleLineTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.angleLineStage ||
        !host.drawCurrent || !host.angleBaselineEnd) {
        return;
    }

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawCurrent =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;

    // Soft smart constraints (axis lock) still apply to free cursor before
    // angle snap — matches legacy shared move block order.
    QString smartHint;
    if (host.smartConstraints) {
        *host.drawCurrent = host.smartConstraints(
            *host.drawCurrent, event->modifiers(), &smartHint);
    }

    const int stage = *host.angleLineStage;
    if (stage == 1) {
        *host.angleBaselineEnd = *host.drawCurrent;
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    if (stage == 2 && host.currentPrimitive && *host.currentPrimitive) {
        if (auto *line =
                dynamic_cast<LinePrimitive *>(host.currentPrimitive->get())) {
            QVector2D rawEnd = *host.drawCurrent;
            if (host.snapToEndpoint) {
                rawEnd = host.snapToEndpoint(rawEnd);
            }
            QVector2D snapped = rawEnd;
            if (host.snapAngleEndpoint) {
                snapped = host.snapAngleEndpoint(*host.angleBaselineEnd, rawEnd,
                                                 event->modifiers());
            }
            line->setStartPoint(*host.angleBaselineEnd);
            line->setEndPoint(snapped);
            if (host.angleBaselineStart) {
                smartHint = angleHint(*host.angleBaselineStart,
                                      *host.angleBaselineEnd, snapped);
            }
        }
    }

    if (!smartHint.isEmpty() && host.lastSmartHint && host.setSmartHint &&
        smartHint != *host.lastSmartHint) {
        *host.lastSmartHint = smartHint;
        host.setSmartHint(smartHint);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void AngleLineTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.angleLineStage) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const int stage = *host.angleLineStage;

    // Stage 1: commit baseline construction line, arm stage 2
    if (stage == 1) {
        if (!host.angleBaselineStart || !host.angleBaselineEnd ||
            !host.drawCurrent) {
            return;
        }
        *host.angleBaselineEnd =
            host.snapToGrid ? host.snapToGrid(*host.drawCurrent)
                            : *host.drawCurrent;
        const float baseLen =
            (*host.angleBaselineEnd - *host.angleBaselineStart).length();
        if (baseLen >= 2.0f) {
            auto baseline = std::make_unique<LinePrimitive>(
                *host.angleBaselineStart, *host.angleBaselineEnd);
            baseline->setColor(kBaselineColor);
            baseline->setLineStyle(Qt::DashLine);
            baseline->setLineWidth(1.0f);
            if (host.angleBaselinePrimitiveId) {
                *host.angleBaselinePrimitiveId = baseline->id();
            }
            QVector2D baseDir =
                *host.angleBaselineEnd - *host.angleBaselineStart;
            if (baseDir.length() > 0.001f) {
                baseDir.normalize();
            }
            if (host.angleBaselineConstraintDir) {
                *host.angleBaselineConstraintDir = baseDir;
            }
            if (host.commitPrimitive) {
                host.commitPrimitive(std::move(baseline));
            }
            *host.angleLineStage = 2;
            *host.isDrawing = false;
            if (host.currentPrimitive) {
                host.currentPrimitive->reset();
            }
            setHint(host,
                    QStringLiteral(
                        "Baseline set — click and drag the angled line "
                        "from the end point"));
        } else {
            *host.angleLineStage = 0;
            *host.isDrawing = false;
            setHint(host, QStringLiteral("Baseline too short — drag again"));
        }
        if (host.requestUpdate) {
            host.requestUpdate();
        }
        return;
    }

    // Stage 2: commit angled segment; keep baseline armed
    if (stage == 2 && host.currentPrimitive && *host.currentPrimitive) {
        auto *line =
            dynamic_cast<LinePrimitive *>(host.currentPrimitive->get());
        if (!line) {
            return;
        }

        QVector2D end = line->endPoint();
        if (host.snapAngleEndpoint && host.angleBaselineEnd) {
            end = host.snapAngleEndpoint(*host.angleBaselineEnd, end,
                                         event->modifiers());
            line->setStartPoint(*host.angleBaselineEnd);
            line->setEndPoint(end);
        }
        if (host.angleBaselinePrimitiveId &&
            !host.angleBaselinePrimitiveId->isNull()) {
            line->setConnectedLineId(*host.angleBaselinePrimitiveId);
        }
        if (host.angleBaselineConstraintDir &&
            host.angleBaselineConstraintDir->lengthSquared() > 1e-8f) {
            line->setMoveConstraintDirection(*host.angleBaselineConstraintDir);
        }

        const float len = (line->endPoint() - line->startPoint()).length();
        const bool shouldAdd = len >= 2.0f;
        if (shouldAdd && host.commitPrimitive) {
            line->setColor(host.defaultColor);
            line->setSelected(true);
            host.commitPrimitive(std::move(*host.currentPrimitive));
            *host.isDrawing = false;
            // Keep stage 2 for another angled segment
            setHint(host,
                    QStringLiteral(
                        "Line added — drag another angled segment, or "
                        "right-click to reset"));
        } else {
            host.currentPrimitive->reset();
            *host.isDrawing = false;
        }
        if (host.requestUpdate) {
            host.requestUpdate();
        }
    }
}
