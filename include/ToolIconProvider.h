#pragma once

#include "DrawingCanvas.h"

#include <QIcon>
#include <QKeySequence>
#include <QSize>
#include <QString>
#include <functional>

class QPainter;
class QRectF;

/**
 * Tool icons, display names, and default shortcuts.
 * Extracted from MainWindow (refactor A11).
 */
class ToolIconProvider {
public:
    static QIcon createMonoIcon(const std::function<void(QPainter &, const QRectF &)> &draw);

    static QIcon createSelectIcon();
    static QIcon createLineIcon();
    static QIcon createAngleLineIcon();
    static QIcon createCurveIcon();
    static QIcon createBezierIcon();
    static QIcon createSplineIcon();
    static QIcon createPolygonIcon();
    static QIcon createRectangleIcon();
    static QIcon createEllipseIcon();
    static QIcon createCircleIcon();
    static QIcon createArcIcon();
    static QIcon createEraserIcon();
    static QIcon createFillIcon();
    static QIcon createBrushIcon();
    static QIcon createBlurIcon();
    static QIcon createMeasureIcon();
    static QIcon createImageIcon();
    static QIcon createHandIcon();
    static QIcon createTextIcon();

    static QIcon iconForTool(DrawingTool tool);
    static QString displayNameForTool(DrawingTool tool);
    static QKeySequence defaultShortcutForTool(DrawingTool tool);

    static QIcon loadCustomIcon(const QString &iconName,
                                std::function<QIcon()> fallbackGenerator);
    static QIcon loadIconFromFile(const QString &filename);
    static QIcon loadSVGIcon(const QString &iconName, const QSize &size = QSize(48, 48));
    static QIcon loadAIIcon(const QString &toolName, const QSize &size = QSize(48, 48));
};
