#pragma once

/**
 * Canvas drawing / interaction tool id.
 * Shared by DrawingCanvas, ToolCursor, ToolIconProvider, etc.
 */
enum class DrawingTool {
    Select,
    Move,
    Line,
    Curve,
    BezierCurve,
    Spline,
    Polygon,
    Arc,
    Circle,
    Rectangle,
    Ellipse,
    AngleLine,
    Eraser,
    Fill,
    Brush,
    Blur,
    Measure,
    Image,
    Text
};
