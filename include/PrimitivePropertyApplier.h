#pragma once

#include <QString>
#include <QVariant>

class DrawingPrimitive;

/**
 * Applies a named property change to a drawing primitive.
 * Extracted from MainWindow (refactor A8).
 */
class PrimitivePropertyApplier {
public:
    static void apply(DrawingPrimitive *primitive, const QString &propertyName,
                      const QVariant &value);
};
