#pragma once

#include <QVector2D>

class DrawingPrimitive;

/**
 * Type-based selection / rotation policy (refactor E8).
 * Pure rules: which objects get geometry CPs, rotation handles, how
 * rotation is stored (Image degrees, Text radians, others degrees).
 */
namespace SelectionPolicy {

/** Path/control-point editing (not bbox-only shapes). */
bool showsGeometryControlPoints(const DrawingPrimitive *obj);

/** Rotation stem handle on selection chrome. */
bool supportsRotationHandle(const DrawingPrimitive *obj);

/** Rotation in degrees for UI / handles (Text converts from radians). */
float objectRotationDegrees(const DrawingPrimitive *obj);

/** Apply rotation in degrees (Text stores radians). */
void applyObjectRotation(DrawingPrimitive *obj, float degrees);

/**
 * True when the object uses DrawingPrimitive::rotationDegrees() for
 * external paint/handle transforms (not Image/Text self-rotation).
 */
bool usesExternalRotation(const DrawingPrimitive *obj);

/** Map world point into object-local space when external rotation applies. */
QVector2D toObjectLocal(const DrawingPrimitive *obj, const QVector2D &worldPos);

} // namespace SelectionPolicy
