#include "SmartDrawingConstraints.h"

#include <QtMath>

#include <cmath>

namespace SmartDrawingConstraints {

QVector2D apply(ToolKind kind, const QVector2D &start, const QVector2D &rawPos,
                Qt::KeyboardModifiers mods, QString *hintOut)
{
    if (hintOut) {
        hintOut->clear();
    }

    const bool shift = mods & Qt::ShiftModifier;
    const bool alt = mods & Qt::AltModifier;
    QVector2D pos = rawPos;

    auto setHint = [&](const QString &text) {
        if (hintOut) {
            *hintOut = text;
        }
    };

    switch (kind) {
    case ToolKind::LineOrMeasure: {
        QVector2D delta = pos - start;
        if (shift) {
            if (std::abs(delta.x()) >= std::abs(delta.y())) {
                pos.setY(start.y());
                setHint(QStringLiteral("Shift: horizontal lock"));
            } else {
                pos.setX(start.x());
                setHint(QStringLiteral("Shift: vertical lock"));
            }
        } else {
            const float len = delta.length();
            if (len > 8.0f) {
                const float angle =
                    std::atan2(delta.y(), delta.x()) * 180.0f /
                    static_cast<float>(M_PI);
                const float snapped = std::round(angle / 45.0f) * 45.0f;
                if (std::abs(angle - snapped) < 6.0f) {
                    setHint(QStringLiteral("Near %1° — hold Shift to lock axis")
                                .arg(static_cast<int>(snapped)));
                }
            }
        }
        break;
    }
    case ToolKind::Rectangle: {
        float w = pos.x() - start.x();
        float h = pos.y() - start.y();
        const float aw = std::abs(w);
        const float ah = std::abs(h);
        if (aw < 1.0f && ah < 1.0f) {
            break;
        }
        const float ratio = aw > 0.0f ? ah / aw : 1.0f;
        const bool nearSquare = ratio > 0.88f && ratio < 1.12f;
        if (shift || (!alt && nearSquare)) {
            const float side = std::max(aw, ah);
            pos.setX(start.x() + std::copysign(side, w == 0.0f ? 1.0f : w));
            pos.setY(start.y() + std::copysign(side, h == 0.0f ? 1.0f : h));
            setHint(shift ? QStringLiteral("Shift: square locked")
                          : QStringLiteral("Predicted square — hold Alt to freeform"));
        } else if (nearSquare) {
            setHint(QStringLiteral("Almost square — hold Shift to lock"));
        }
        break;
    }
    case ToolKind::Ellipse: {
        float w = pos.x() - start.x();
        float h = pos.y() - start.y();
        const float aw = std::abs(w);
        const float ah = std::abs(h);
        if (aw < 1.0f && ah < 1.0f) {
            break;
        }
        const float ratio = aw > 0.0f ? ah / aw : 1.0f;
        const bool nearCircle = ratio > 0.88f && ratio < 1.12f;
        if (shift || (!alt && nearCircle)) {
            const float side = std::max(aw, ah);
            pos.setX(start.x() + std::copysign(side, w == 0.0f ? 1.0f : w));
            pos.setY(start.y() + std::copysign(side, h == 0.0f ? 1.0f : h));
            setHint(shift ? QStringLiteral("Shift: circle locked")
                          : QStringLiteral("Predicted circle — hold Alt to freeform"));
        } else if (nearCircle) {
            setHint(QStringLiteral("Almost circle — hold Shift to lock"));
        }
        break;
    }
    case ToolKind::Other:
    default:
        break;
    }

    return pos;
}

} // namespace SmartDrawingConstraints
