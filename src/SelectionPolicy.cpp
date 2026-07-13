#include "SelectionPolicy.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QTransform>
#include <cmath>

namespace SelectionPolicy {

bool showsGeometryControlPoints(const DrawingPrimitive *obj)
{
    if (!obj) {
        return false;
    }
    switch (obj->type()) {
    case PrimitiveType::Image:
    case PrimitiveType::Text:
    case PrimitiveType::Rectangle:
    case PrimitiveType::Ellipse:
    case PrimitiveType::Circle:
        return false;
    default:
        return true;
    }
}

bool supportsRotationHandle(const DrawingPrimitive *obj)
{
    if (!obj) {
        return false;
    }
    switch (obj->type()) {
    case PrimitiveType::Image:
    case PrimitiveType::Text:
    case PrimitiveType::Rectangle:
    case PrimitiveType::Ellipse:
    case PrimitiveType::Circle:
    case PrimitiveType::Polygon:
        return true;
    default:
        return false;
    }
}

float objectRotationDegrees(const DrawingPrimitive *obj)
{
    if (!obj) {
        return 0.0f;
    }
    if (auto *img = dynamic_cast<const ImagePrimitive *>(obj)) {
        return img->rotation();
    }
    if (auto *txt = dynamic_cast<const TextPrimitive *>(obj)) {
        return txt->rotation() * 180.0f / static_cast<float>(M_PI);
    }
    return obj->rotationDegrees();
}

void applyObjectRotation(DrawingPrimitive *obj, float degrees)
{
    if (!obj) {
        return;
    }
    if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
        img->setRotation(degrees);
    } else if (auto *txt = dynamic_cast<TextPrimitive *>(obj)) {
        txt->setRotation(degrees * static_cast<float>(M_PI) / 180.0f);
    } else {
        obj->setRotationDegrees(degrees);
    }
}

bool usesExternalRotation(const DrawingPrimitive *obj)
{
    if (!obj) {
        return false;
    }
    if (obj->type() == PrimitiveType::Image ||
        obj->type() == PrimitiveType::Text) {
        return false;
    }
    return std::fabs(obj->rotationDegrees()) > 0.01f;
}

QVector2D toObjectLocal(const DrawingPrimitive *obj, const QVector2D &worldPos)
{
    if (!usesExternalRotation(obj)) {
        return worldPos;
    }
    const QPointF c = obj->boundingRect().center();
    QTransform t;
    t.translate(c.x(), c.y());
    t.rotate(-obj->rotationDegrees());
    t.translate(-c.x(), -c.y());
    const QPointF p = t.map(worldPos.toPointF());
    return QVector2D(static_cast<float>(p.x()), static_cast<float>(p.y()));
}

} // namespace SelectionPolicy
