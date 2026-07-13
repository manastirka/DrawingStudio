#pragma once

#include "MathExpression.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVector>

/** Plot y = f(x) to a QImage with axes / grid / labels options. */
class MathGraphRenderer
{
public:
    struct Options {
        QString expression;
        double xMin = -6.283185307179586; // -2π
        double xMax = 6.283185307179586;  // +2π
        double yMin = -2.0;
        double yMax = 2.0;
        bool autoY = true;
        int samples = 600;
        int width = 800;
        int height = 500;
        int margin = 48;

        bool showAxes = true;
        bool showGrid = true;
        bool showTicks = true;
        bool showLabels = true;
        bool showTitle = true;
        bool showOrigin = true;

        QColor background = QColor(252, 252, 254);
        QColor gridColor = QColor(220, 224, 230);
        QColor axisColor = QColor(40, 44, 52);
        QColor curveColor = QColor(30, 110, 220);
        QColor textColor = QColor(40, 44, 52);
        float curveWidth = 2.4f;
        QFont font = QFont(QStringLiteral("Helvetica Neue"), 11);

        /** Optional overrides (physics dependency plots). */
        QString title;   // empty → "y = <expression>"
        QString xLabel = QStringLiteral("x");
        QString yLabel = QStringLiteral("y");

        /** Extra named bindings (physics fixed parameters). */
        QHash<QString, double> namedValues;
    };

    struct Result {
        QImage image;
        QString error;
        int pointCount = 0;
    };

    static Result render(const Options &opt);
};
