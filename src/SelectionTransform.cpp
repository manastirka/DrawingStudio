#include "SelectionTransform.h"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace SelectionTransform {

float handleHalfSize(float zoomLevel)
{
    // ~7px screen square
    return 3.5f / zoomLevel;
}

float handleHitRadius(float zoomLevel)
{
    return 11.0f / zoomLevel;
}

void handlePositions(const QRectF &br, float zoomLevel, QPointF out[9])
{
    out[0] = br.topLeft();
    out[1] = QPointF(br.center().x(), br.top());
    out[2] = br.topRight();
    out[3] = QPointF(br.right(), br.center().y());
    out[4] = br.bottomRight();
    out[5] = QPointF(br.center().x(), br.bottom());
    out[6] = br.bottomLeft();
    out[7] = QPointF(br.left(), br.center().y());
    // Rotation handle sits above the top edge (world Y increases upward after flip)
    const float rotateOffset = 22.0f / zoomLevel;
    out[kHandleRotate] = QPointF(br.center().x(), br.top() + rotateOffset);
}

Qt::CursorShape cursorForHandle(int index)
{
    static const Qt::CursorShape cursors[8] = {
        Qt::SizeFDiagCursor, // 0 TL
        Qt::SizeVerCursor,   // 1 T
        Qt::SizeBDiagCursor, // 2 TR
        Qt::SizeHorCursor,   // 3 R
        Qt::SizeFDiagCursor, // 4 BR
        Qt::SizeVerCursor,   // 5 B
        Qt::SizeBDiagCursor, // 6 BL
        Qt::SizeHorCursor,   // 7 L
    };
    if (index == kHandleRotate)
        return Qt::PointingHandCursor;
    if (index >= 0 && index < 8)
        return cursors[index];
    return Qt::ArrowCursor;
}

int hitTest(const QRectF &br, const QVector2D &worldPos, float zoomLevel,
            bool includeRotate)
{
    QPointF handles[9];
    handlePositions(br, zoomLevel, handles);
    const float hit = handleHitRadius(zoomLevel);
    const int count = includeRotate ? 9 : 8;

    int best = -1;
    float bestDist = hit;
    for (int i = 0; i < count; ++i) {
        if (i == kHandleRotate && !includeRotate)
            continue;
        const float d =
            QVector2D(worldPos - QVector2D(handles[i])).length();
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

QRectF computeResizedBounds(const QRectF &orig, int handleIndex,
                            const QVector2D &worldPos,
                            Qt::KeyboardModifiers mods)
{
    const bool keepAspect = mods.testFlag(Qt::ShiftModifier);
    const bool fromCenter = mods.testFlag(Qt::AltModifier);
    const QPointF pos = worldPos.toPointF();
    const QPointF center = orig.center();
    const float aspect =
        (orig.height() > 0.001) ? static_cast<float>(orig.width() / orig.height())
                                : 1.0f;

    QRectF nr = orig;

    if (fromCenter) {
        float halfW = static_cast<float>(orig.width() * 0.5);
        float halfH = static_cast<float>(orig.height() * 0.5);
        switch (handleIndex) {
        case 0: // TL
        case 2: // TR
        case 4: // BR
        case 6: // BL
            halfW = std::abs(static_cast<float>(pos.x() - center.x()));
            halfH = std::abs(static_cast<float>(pos.y() - center.y()));
            if (keepAspect) {
                if (halfW / aspect > halfH)
                    halfH = halfW / aspect;
                else
                    halfW = halfH * aspect;
            }
            break;
        case 1: // T
        case 5: // B
            halfH = std::abs(static_cast<float>(pos.y() - center.y()));
            if (keepAspect)
                halfW = halfH * aspect;
            break;
        case 3: // R
        case 7: // L
            halfW = std::abs(static_cast<float>(pos.x() - center.x()));
            if (keepAspect)
                halfH = halfW / aspect;
            break;
        default:
            break;
        }
        halfW = std::max(0.5f, halfW);
        halfH = std::max(0.5f, halfH);
        return QRectF(center.x() - halfW, center.y() - halfH, halfW * 2, halfH * 2);
    }

    QPointF anchor;
    switch (handleIndex) {
    case 0:
        anchor = orig.bottomRight();
        break;
    case 1:
        anchor = QPointF(orig.center().x(), orig.bottom());
        break;
    case 2:
        anchor = orig.bottomLeft();
        break;
    case 3:
        anchor = QPointF(orig.left(), orig.center().y());
        break;
    case 4:
        anchor = orig.topLeft();
        break;
    case 5:
        anchor = QPointF(orig.center().x(), orig.top());
        break;
    case 6:
        anchor = orig.topRight();
        break;
    case 7:
        anchor = QPointF(orig.right(), orig.center().y());
        break;
    default:
        return orig;
    }

    QPointF moved = pos;
    if (keepAspect && (handleIndex % 2 == 0)) {
        float dx = static_cast<float>(moved.x() - anchor.x());
        float dy = static_cast<float>(moved.y() - anchor.y());
        const float sx = (dx >= 0.0f) ? 1.0f : -1.0f;
        const float sy = (dy >= 0.0f) ? 1.0f : -1.0f;
        if (std::abs(dx) / aspect >= std::abs(dy)) {
            dx = sx * std::abs(dx);
            dy = sy * (std::abs(dx) / aspect);
        } else {
            dy = sy * std::abs(dy);
            dx = sx * (std::abs(dy) * aspect);
        }
        moved = QPointF(anchor.x() + dx, anchor.y() + dy);
    } else if (keepAspect && (handleIndex == 1 || handleIndex == 5)) {
        float h = std::max(
            0.5f, static_cast<float>(std::abs(moved.y() - anchor.y())));
        float w = h * aspect;
        return QRectF(center.x() - w * 0.5f, std::min(anchor.y(), moved.y()), w, h)
            .normalized();
    } else if (keepAspect && (handleIndex == 3 || handleIndex == 7)) {
        float w = std::max(
            0.5f, static_cast<float>(std::abs(moved.x() - anchor.x())));
        float h = w / aspect;
        return QRectF(std::min(anchor.x(), moved.x()), center.y() - h * 0.5f, w, h)
            .normalized();
    }

    switch (handleIndex) {
    case 0:
        nr.setTopLeft(moved);
        break;
    case 1:
        nr.setTop(moved.y());
        break;
    case 2:
        nr.setTopRight(moved);
        break;
    case 3:
        nr.setRight(moved.x());
        break;
    case 4:
        nr.setBottomRight(moved);
        break;
    case 5:
        nr.setBottom(moved.y());
        break;
    case 6:
        nr.setBottomLeft(moved);
        break;
    case 7:
        nr.setLeft(moved.x());
        break;
    }
    return nr.normalized();
}

void drawHandles(QPainter &painter, const QRectF &br, float zoomLevel,
                 bool showRotate)
{
    QPointF handles[9];
    handlePositions(br, zoomLevel, handles);

    QPen boxPen(QColor(59, 130, 246), 1.25);
    boxPen.setCosmetic(true);
    boxPen.setStyle(Qt::DashLine);
    boxPen.setDashPattern({4, 3});
    painter.setPen(boxPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(br);

    const float hs = handleHalfSize(zoomLevel);
    const float edgeHs = hs * 0.85f;

    QPen handlePen(QColor(37, 99, 235), 1.25);
    handlePen.setCosmetic(true);

    for (int i = 0; i < 8; ++i) {
        const bool corner = (i % 2 == 0);
        const float s = corner ? hs : edgeHs;
        QRectF r(handles[i].x() - s, handles[i].y() - s, s * 2, s * 2);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(15, 23, 42, 35));
        painter.drawRoundedRect(
            r.adjusted(-0.6f / zoomLevel, -0.6f / zoomLevel, 0.6f / zoomLevel,
                       0.6f / zoomLevel),
            1.2f / zoomLevel, 1.2f / zoomLevel);

        painter.setPen(handlePen);
        painter.setBrush(Qt::white);
        if (corner) {
            painter.drawRoundedRect(r, 1.1f / zoomLevel, 1.1f / zoomLevel);
        } else {
            painter.drawRoundedRect(r, s * 0.45f, s * 0.45f);
        }
    }

    if (showRotate) {
        const QPointF top = handles[1];
        const QPointF rot = handles[kHandleRotate];

        QPen stemPen(QColor(59, 130, 246), 1.2);
        stemPen.setCosmetic(true);
        painter.setPen(stemPen);
        painter.drawLine(top, rot);

        const float rs = hs * 1.15f;
        painter.setBrush(Qt::white);
        painter.setPen(handlePen);
        painter.drawEllipse(rot, rs, rs);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(59, 130, 246));
        painter.drawEllipse(rot, rs * 0.35f, rs * 0.35f);
    }
}

void drawHandlesForObject(QPainter &painter, const QRectF &boundingRect,
                          float zoomLevel, bool showRotate,
                          bool usesExternalRotation, float rotationDegrees)
{
    if (boundingRect.isEmpty()) {
        return;
    }
    if (usesExternalRotation) {
        painter.save();
        const QPointF c = boundingRect.center();
        painter.translate(c);
        painter.rotate(static_cast<double>(rotationDegrees));
        painter.translate(-c);
        drawHandles(painter, boundingRect, zoomLevel, showRotate);
        painter.restore();
    } else {
        drawHandles(painter, boundingRect, zoomLevel, showRotate);
    }
}

} // namespace SelectionTransform
