#pragma once

#include "UnitsConverter.h"

#include <QPoint>
#include <QVector2D>
#include <functional>

class QPainter;

/**
 * Screen-space ruler ticks and labels.
 * Extracted from DrawingCanvas (refactor B4).
 */
class RulerRenderer {
public:
    static constexpr int WIDTH = 25;
    static constexpr int HEIGHT = 25;

    /** Background + tick marks in screen coordinates. */
    static void renderTicks(QPainter &painter, int widgetWidth, int widgetHeight,
                            float zoomLevel, UnitsConverter::Units units);

    /**
     * Numeric labels. `worldToScreen` converts world → screen (widget coords).
     */
    static void renderTexts(
        QPainter &painter, int widgetWidth, int widgetHeight,
        UnitsConverter::Units units,
        const std::function<QPoint(const QVector2D &)> &worldToScreen);
};
