#include "tools/FillTool.h"

#include "DrawingPrimitive.h"

#include <QDateTime>
#include <QDebug>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kFillTol = 5.0f;
constexpr float kPi = 3.14159265358979323846f;

void createSplash(ToolHost &host, const QVector2D &center, float scale)
{
    if (!host.commitPrimitive) {
        return;
    }
    std::srand(static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch()));

    const int blobCount = 3 + (std::rand() % 3);
    for (int i = 0; i < blobCount; ++i) {
        const float offsetR = static_cast<float>(std::rand() % 10) * scale;
        const float angle =
            static_cast<float>(std::rand() % 360) * kPi / 180.0f;
        const QVector2D blobPos =
            center + QVector2D(std::cos(angle) * offsetR,
                               std::sin(angle) * offsetR);
        const float r =
            (15.0f + static_cast<float>(std::rand() % 10)) * scale;

        auto blob = std::make_unique<CirclePrimitive>(blobPos, r);
        blob->setFilled(true);
        blob->setColor(host.defaultColor);
        blob->setFillColor(host.defaultColor);
        host.commitPrimitive(std::move(blob));
    }

    const int dropCount = 8 + (std::rand() % 8);
    for (int k = 0; k < dropCount; ++k) {
        const float angle =
            static_cast<float>(std::rand() % 360) * kPi / 180.0f;
        const float dist =
            (30.0f + static_cast<float>(std::rand() % 40)) * scale;
        const QVector2D dropPos =
            center + QVector2D(std::cos(angle) * dist, std::sin(angle) * dist);
        const float r =
            (2.0f + static_cast<float>(std::rand() % 4)) * scale;

        auto drop = std::make_unique<CirclePrimitive>(dropPos, r);
        drop->setFilled(true);
        drop->setColor(host.defaultColor);
        drop->setFillColor(host.defaultColor);
        host.commitPrimitive(std::move(drop));
    }
}

bool applyFillToPrimitive(DrawingPrimitive *primitive, const QColor &color)
{
    if (!primitive) {
        return false;
    }

    switch (primitive->type()) {
    case PrimitiveType::Rectangle: {
        auto *p = static_cast<RectanglePrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    case PrimitiveType::Circle: {
        auto *p = static_cast<CirclePrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    case PrimitiveType::Ellipse: {
        auto *p = static_cast<EllipsePrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    case PrimitiveType::Polygon: {
        auto *p = static_cast<PolygonPrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    case PrimitiveType::Spline: {
        auto *p = static_cast<SplinePrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    case PrimitiveType::Curve: {
        auto *p = static_cast<CurvePrimitive *>(primitive);
        p->setFilled(true);
        p->setFillColor(color);
        return true;
    }
    default:
        return false;
    }
}
} // namespace

void FillTool::onPress(ToolHost &host, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.screenToWorld) {
        return;
    }

    const QVector2D pos = host.screenToWorld(event->pos());
    const QPoint tip = event->globalPosition().toPoint();
    const bool splash = host.fillSplashMode;

    DrawingPrimitive *target = nullptr;
    if (host.findFillTarget) {
        target = host.findFillTarget(pos, kFillTol);
    }

    if (target) {
        const QJsonObject oldState = target->toJson();
        if (applyFillToPrimitive(target, host.defaultColor)) {
            if (splash) {
                switch (target->type()) {
                case PrimitiveType::Rectangle: {
                    const QRectF br = target->boundingRect();
                    createSplash(host,
                                 QVector2D(static_cast<float>(br.right()),
                                           static_cast<float>(br.bottom())),
                                 0.6f);
                    break;
                }
                case PrimitiveType::Circle: {
                    auto *c = static_cast<CirclePrimitive *>(target);
                    createSplash(host, c->center(), 0.7f);
                    break;
                }
                default:
                    break;
                }
            }

            if (host.commitPropertyChange) {
                host.commitPropertyChange(target, oldState,
                                          QStringLiteral("Fill"));
            }
            if (host.selectOnly) {
                host.selectOnly(target);
            }
        }
    } else if (splash) {
        createSplash(host, pos, 1.0f);
    } else if (host.showTooltip) {
        host.showTooltip(
            QStringLiteral(
                "Fill works inside closed shapes.\nClick a shape or enable "
                "Splash Mode."),
            tip);
    }

    if (host.requestUpdate) {
        host.requestUpdate();
    }
}

void FillTool::onMove(ToolHost & /*host*/, QMouseEvent * /*event*/) {}

void FillTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/) {}
