#include "ToolSuggestionService.h"

namespace {

ToolSuggestionAction chip(const char *id, const char *label, const char *tip)
{
    return {QString::fromLatin1(id), QString::fromLatin1(label), QString::fromLatin1(tip)};
}

QString baseHintForTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select:
        return QStringLiteral("Select: click object, drag box/lasso · Shift=add · Alt=subtract");
    case DrawingTool::Move:
        return QStringLiteral("Move: drag selection · arrows nudge · snap helps alignment");
    case DrawingTool::Line:
        return QStringLiteral("Line: drag to draw · Shift=axis lock · endpoints magnetize when enabled");
    case DrawingTool::Curve:
        return QStringLiteral("Curve: click points · click near start to close · right-click cancel");
    case DrawingTool::BezierCurve:
        return QStringLiteral("Bezier: drag end point · Shift+click adjusts handles · chains from last end");
    case DrawingTool::Spline:
        return QStringLiteral("Spline: click to add points · right-click or double-click to finish");
    case DrawingTool::Polygon:
        return QStringLiteral("Polygon: click vertices · click near start to close");
    case DrawingTool::Rectangle:
        return QStringLiteral("Rectangle: drag · Shift=square · near-square snaps when close");
    case DrawingTool::Ellipse:
        return QStringLiteral("Ellipse: drag · Shift=circle · near-circle snaps when close");
    case DrawingTool::Circle:
        return QStringLiteral("Circle: drag from center to set radius");
    case DrawingTool::Arc:
        return QStringLiteral("Arc: set start, end, then bend through a third point");
    case DrawingTool::AngleLine:
        return QStringLiteral("Angle line: drag baseline, release, then drag angled segment (15° snap)");
    case DrawingTool::Eraser:
        return QStringLiteral("Eraser: drag over strokes · adjust size for precision");
    case DrawingTool::Fill:
        return QStringLiteral("Fill: click a closed shape or region to flood-fill");
    case DrawingTool::Brush:
        return QStringLiteral("Brush: drag to paint · size & hardness in the options bar");
    case DrawingTool::Blur:
        return QStringLiteral("Blur: drag to soften · works best on images and dense strokes");
    case DrawingTool::Measure:
        return QStringLiteral("Measure: drag a line to read length in current units");
    case DrawingTool::Image:
        return QStringLiteral("Image: place/select a photo · Detect Subjects finds cutouts");
    case DrawingTool::Text:
        return QStringLiteral("Text: click to place · double-click to edit · align in properties");
    }
    return QStringLiteral("Ready");
}

} // namespace

ToolSuggestionResult ToolSuggestionService::resolve(const ToolSuggestionContext &ctx)
{
    ToolSuggestionResult out;
    out.statusHint = ctx.liveDrawHint.isEmpty() ? baseHintForTool(ctx.tool) : ctx.liveDrawHint;

    auto add = [&](ToolSuggestionAction a) {
        if (out.chips.size() >= 3) {
            return;
        }
        for (const auto &existing : out.chips) {
            if (existing.id == a.id) {
                return;
            }
        }
        out.chips.push_back(std::move(a));
    };

    // Empty canvas onboarding
    if (ctx.canvasEmpty && ctx.selectionCount == 0) {
        out.statusHint = QStringLiteral("Empty canvas — import a photo, draw a shape, or add text");
        add(chip("import_image", "Import image", "Open an image onto the canvas"));
        add(chip("switch_rectangle", "Draw shape", "Switch to the rectangle tool"));
        add(chip("switch_text", "Add text", "Switch to the text tool"));
        return out;
    }

    // Selection-aware guidance
    if (ctx.hasImage) {
        if (!ctx.imageHasMasks) {
            out.statusHint = QStringLiteral("Image selected — Detect Subjects to cut out people/objects (Ctrl+D)");
            add(chip("detect_subjects", "Detect subjects", "Run subject detection on the selected image"));
        } else {
            out.statusHint = QStringLiteral("Masks ready — extract the subject or remove the background");
            add(chip("extract_subject", "Extract", "Extract the active mask as a new object"));
            add(chip("remove_background", "Remove BG", "Keep the subject and delete the background"));
            add(chip("mask_settings", "Mask settings", "Feather, expand, and refine the mask"));
        }
    } else if (ctx.hasText) {
        out.statusHint = QStringLiteral("Text selected — edit in the panel · alignment applies inside the text box");
        add(chip("switch_text", "Edit text", "Activate the text tool for this object"));
    } else if (ctx.selectionCount >= 2) {
        out.statusHint = QStringLiteral("%1 objects selected — drag to move together · Delete to remove")
                             .arg(ctx.selectionCount);
    }

    // Tool-specific chips
    switch (ctx.tool) {
    case DrawingTool::Line:
    case DrawingTool::Measure:
        if (!ctx.snapEnabled) {
            add(chip("enable_snap", "Enable snap", "Snap drawing to the grid"));
        }
        if (!ctx.magneticEnabled) {
            add(chip("enable_magnetic", "Magnetic ends", "Snap line ends to nearby endpoints"));
        }
        break;
    case DrawingTool::Rectangle:
    case DrawingTool::Ellipse:
        add(chip("hint_shift_lock", "Shift = lock", "Hold Shift while dragging to lock square/circle"));
        break;
    case DrawingTool::Brush:
    case DrawingTool::Eraser:
    case DrawingTool::Blur:
        add(chip("hint_size", "Size slider", "Use the options bar size control for finer work"));
        break;
    case DrawingTool::Image:
        if (!ctx.hasImage) {
            add(chip("import_image", "Import image", "Choose an image file to place"));
        }
        break;
    case DrawingTool::Select:
        if (ctx.selectionCount == 0 && !ctx.canvasEmpty) {
            add(chip("select_all", "Select all", "Select every object on the canvas"));
        }
        break;
    case DrawingTool::Text:
        add(chip("hint_text_box", "Resize box", "With Select, drag text handles to set a wrap box"));
        break;
    default:
        break;
    }

    if (!ctx.gridVisible && (ctx.tool == DrawingTool::Line || ctx.tool == DrawingTool::Rectangle)) {
        add(chip("show_grid", "Show grid", "Toggle the drawing grid"));
    }

    return out;
}
