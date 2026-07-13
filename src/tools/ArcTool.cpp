#include "tools/ArcTool.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

QVector2D snappedWorldPos(const ToolHost &host, const QPoint &screenPos)
{
    QVector2D world =
        host.screenToWorld ? host.screenToWorld(screenPos) : QVector2D();
    if (host.snapToGrid) {
        world = host.snapToGrid(world);
    }
    if (host.snapToEndpoint) {
        world = host.snapToEndpoint(world);
    }
    return world;
}

bool updateArcFromThirdPoint(ToolHost &host, const QVector2D &third)
{
    if (!host.currentPrimitive || !*host.currentPrimitive || !host.arcStart ||
        !host.arcEnd || !host.circleThroughPoints) {
        return false;
    }
    auto *arc = dynamic_cast<ArcPrimitive *>(host.currentPrimitive->get());
    if (!arc) {
        return false;
    }

    QVector2D center;
    float radius = 0.0f;
    if (!host.circleThroughPoints(*host.arcStart, *host.arcEnd, third, center,
                                  radius)) {
        return false;
    }

    arc->setCenter(center);
    arc->setRadius(radius);
    const float startAngle =
        std::atan2(host.arcStart->y() - center.y(),
                   host.arcStart->x() - center.x()) *
        180.0f / kPi;
    const float endAngle =
        std::atan2(host.arcEnd->y() - center.y(),
                   host.arcEnd->x() - center.x()) *
        180.0f / kPi;
    arc->setStartAngle(startAngle);
    arc->setEndAngle(endAngle);
    return true;
}

void clearSession(ToolHost &host)
{
    if (host.currentPrimitive) {
        host.currentPrimitive->reset();
    }
    if (host.isDrawing) {
        *host.isDrawing = false;
    }
    if (host.arcStage) {
        *host.arcStage = 0;
    }
    if (host.lastSmartHint && !host.lastSmartHint->isEmpty() &&
        host.setSmartHint) {
        host.lastSmartHint->clear();
        host.setSmartHint(QString());
    }
    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
} // namespace

void ArcTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !host.arcStage || !host.arcStart || !host.arcEnd ||
        !host.currentPrimitive || !host.drawCurrent) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        clearSession(host);
        if (host.setSmartHint) {
            host.setSmartHint(
                QStringLiteral("Arc cancelled — click start, then end, then "
                               "drag the bulge"));
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QVector2D pos = snappedWorldPos(host, event->pos());
    *host.drawCurrent = pos;

    const int stage = *host.arcStage;
    if (stage == 0) {
        *host.arcStart = pos;
        *host.arcStage = 1;
        *host.isDrawing = true;
        if (host.drawStart) {
            *host.drawStart = pos;
        }
        if (host.setSmartHint) {
            host.setSmartHint(
                QStringLiteral("Arc: start set — click the end point"));
        }
        qDebug() << "ArcTool: start" << *host.arcStart;
    } else if (stage == 1) {
        *host.arcEnd = pos;
        auto arc = std::make_unique<ArcPrimitive>();
        arc->setColor(host.defaultColor);
        arc->setLineStyle(host.defaultLineStyle);
        arc->setLineWidth(host.defaultLineWidth);
        *host.currentPrimitive = std::move(arc);
        *host.arcStage = 2;
        // Seed geometry with a third point near the chord midpoint offset
        updateArcFromThirdPoint(host, pos);
        if (host.setSmartHint) {
            host.setSmartHint(
                QStringLiteral("Arc: drag to set bulge, release to place · "
                               "Right-click cancels"));
        }
        qDebug() << "ArcTool: end" << *host.arcEnd << "— drag bulge";
    } else if (stage == 2) {
        // Hold + drag adjusts; commit on release
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void ArcTool::onMove(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.arcStage ||
        !host.drawCurrent) {
        return;
    }

    QVector2D worldPos =
        host.screenToWorld ? host.screenToWorld(event->pos()) : QVector2D();
    *host.drawCurrent =
        host.snapToGrid ? host.snapToGrid(worldPos) : worldPos;

    QString smartHint;
    if (host.smartConstraints) {
        *host.drawCurrent = host.smartConstraints(
            *host.drawCurrent, event->modifiers(), &smartHint);
    }

    if (*host.arcStage == 2) {
        QVector2D third = *host.drawCurrent;
        if (host.snapToEndpoint) {
            third = host.snapToEndpoint(third);
        }
        if (updateArcFromThirdPoint(host, third)) {
            if (auto *arc = dynamic_cast<ArcPrimitive *>(
                    host.currentPrimitive->get())) {
                smartHint = QStringLiteral("Arc r=%1 · release to place")
                                .arg(arc->radius(), 0, 'f', 1);
            }
        }
    } else if (*host.arcStage == 1 && host.setSmartHint) {
        smartHint = QStringLiteral("Arc: click end point");
    }

    if (host.lastSmartHint && host.setSmartHint &&
        !smartHint.isEmpty() && smartHint != *host.lastSmartHint) {
        *host.lastSmartHint = smartHint;
        host.setSmartHint(smartHint);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void ArcTool::onRelease(ToolHost &host, QMouseEvent *event)
{
    if (!host.isDrawing || !*host.isDrawing || !host.arcStage) {
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    // Only stage 2 finalizes (stages 0/1 are click-to-advance)
    if (*host.arcStage != 2 || !host.currentPrimitive ||
        !*host.currentPrimitive) {
        return;
    }

    // Final geometry from current cursor
    if (host.drawCurrent) {
        QVector2D third = *host.drawCurrent;
        if (host.snapToEndpoint) {
            third = host.snapToEndpoint(third);
        }
        updateArcFromThirdPoint(host, third);
    }

    bool shouldAdd = false;
    if (auto *arc =
            dynamic_cast<ArcPrimitive *>(host.currentPrimitive->get())) {
        shouldAdd = arc->radius() >= 1.0f;
    }

    if (shouldAdd && host.commitPrimitive) {
        (*host.currentPrimitive)->setColor(host.defaultColor);
        (*host.currentPrimitive)->setSelected(true);
        host.commitPrimitive(std::move(*host.currentPrimitive));
    } else {
        host.currentPrimitive->reset();
    }

    *host.isDrawing = false;
    *host.arcStage = 0;

    if (host.lastSmartHint && host.setSmartHint) {
        host.lastSmartHint->clear();
        host.setSmartHint(QString());
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}
