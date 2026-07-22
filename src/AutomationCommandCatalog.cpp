#include "AutomationCommandCatalog.h"

namespace {
QJsonArray stringArray(const QStringList &values)
{
    QJsonArray result;
    for (const QString &value : values)
        result.append(value);
    return result;
}

QJsonObject command(const char *action, const char *group,
                    const QStringList &required, const QStringList &optional,
                    const char *description, bool filesystemAccess = false)
{
    QJsonObject result;
    result[QStringLiteral("action")] = QString::fromLatin1(action);
    result[QStringLiteral("group")] = QString::fromLatin1(group);
    result[QStringLiteral("description")] = QString::fromUtf8(description);
    result[QStringLiteral("requiredParams")] = stringArray(required);
    result[QStringLiteral("optionalParams")] = stringArray(optional);
    result[QStringLiteral("filesystemAccess")] = filesystemAccess;

    QJsonArray all = stringArray(required);
    for (const QString &value : optional)
        all.append(value);
    result[QStringLiteral("params")] = all; // Backward-compatible union.
    return result;
}

const QJsonArray &registry()
{
    static const QJsonArray value = [] {
        QJsonArray commands;
        const QStringList commonLine = {"color", "lineWidth", "opacity", "shadow"};
        const QStringList commonFill = {
            "color", "lineWidth", "fill", "fillColor", "opacity", "shadow", "gradient"};

        commands.append(command("draw_line", "draw", {"x1", "y1", "x2", "y2"},
                                commonLine + QStringList{"lineStyle"}, "Draw a line segment"));
        commands.append(command("draw_line_multi", "draw", {"points"}, commonLine,
                                "Polyline; points is [{x,y}, ...]"));
        commands.append(command("draw_rectangle", "draw", {"x1", "y1", "x2", "y2"},
                                commonFill + QStringList{"cornerRadius"},
                                "Rectangle defined by opposite corners"));
        commands.append(command("draw_circle", "draw", {"cx", "cy", "radius"},
                                commonFill, "Circle"));
        commands.append(command("draw_ellipse", "draw", {"cx", "cy", "rx", "ry"},
                                commonFill, "Ellipse defined by center and radii"));
        commands.append(command("draw_polygon", "draw", {"points"},
                                commonFill + QStringList{"closed"},
                                "Polygon; points is [{x,y}, ...]"));
        commands.append(command("draw_arc", "draw",
                                {"cx", "cy", "radius", "startAngle", "endAngle"},
                                commonLine, "Circular arc with start/end angles in degrees"));
        commands.append(command("draw_bezier", "draw", {"points"}, commonLine,
                                "Bezier curve; points is [{x,y}, ...]"));
        commands.append(command("draw_spline", "draw", {"points"},
                                commonFill + QStringList{"smoothness", "closed"},
                                "Spline through [{x,y}, ...]"));
        commands.append(command("draw_text", "draw", {"x", "y", "text"},
                                {"fontFamily", "fontSize", "bold", "italic", "color",
                                 "opacity", "shadow", "gradient"},
                                "Text primitive"));

        commands.append(command("deselect", "edit", {}, {}, "Clear selection"));
        commands.append(command("select_all", "edit", {}, {}, "Select all primitives"));
        commands.append(command("delete_selected", "edit", {}, {}, "Delete selection"));
        commands.append(command("delete_primitive", "edit", {}, {"index"},
                                "Delete primitive by index; -1 selects the last"));
        commands.append(command("copy_selected", "edit", {}, {}, "Copy selection"));
        commands.append(command("cut_selected", "edit", {}, {}, "Cut selection"));
        commands.append(command("paste", "edit", {}, {}, "Paste clipboard"));
        commands.append(command("duplicate_selected", "edit", {}, {}, "Duplicate selection"));
        commands.append(command("clear_canvas", "edit", {}, {}, "Remove all primitives"));
        commands.append(command("undo", "edit", {}, {}, "Undo"));
        commands.append(command("redo", "edit", {}, {}, "Redo"));
        commands.append(command("set_tool", "edit", {"tool"}, {}, "Activate tool by name"));
        commands.append(command("set_grid", "view", {}, {"visible", "snap"},
                                "Set grid and snapping flags"));
        commands.append(command("set_background", "view", {"color"}, {"paperColor"},
                                "Set canvas and optional paper color"));
        commands.append(command("set_rulers", "view", {"visible"}, {},
                                "Show or hide rulers"));
        commands.append(command("set_color", "style", {"color"}, {},
                                "Set default stroke color"));
        commands.append(command("set_line_width", "style", {"width"}, {},
                                "Set default stroke width"));
        commands.append(command("set_fill", "style", {"enabled"}, {"color"},
                                "Set default fill state and color"));
        commands.append(command("zoom_fit", "view", {}, {}, "Zoom to fit content"));

        commands.append(command("import_image", "io", {"path"},
                                {"x", "y", "width", "height"},
                                "Import image file onto canvas", true));
        commands.append(command("export_png", "io", {"path"}, {},
                                "Export canvas as PNG", true));
        commands.append(command("export_image", "io", {"path"}, {"format", "quality"},
                                "Export canvas image", true));
        commands.append(command("export_dxf", "io", {"path"}, {},
                                "Export geometry as DXF", true));
        commands.append(command("list_export_formats", "io", {}, {},
                                "List supported image export formats"));
        commands.append(command("save_project", "io", {"path"}, {},
                                "Save .drawing project", true));
        commands.append(command("open_project", "io", {"path"}, {},
                                "Open .drawing project", true));

        commands.append(command("sample_color", "image", {"pixelX", "pixelY"},
                                {"imageIndex"}, "Sample an image pixel"));
        commands.append(command("sample_colors_grid", "image", {},
                                {"imageIndex", "gridX", "gridY"},
                                "Sample an image color grid"));
        commands.append(command("detect_subjects", "image", {}, {"imageIndex"},
                                "Run subject and mask detection"));
        commands.append(command("get_mask_info", "image", {}, {"imageIndex"},
                                "Return mask candidate information"));
        commands.append(command("next_mask", "image", {}, {}, "Select next mask"));
        commands.append(command("prev_mask", "image", {}, {}, "Select previous mask"));
        commands.append(command("invert_mask", "image", {}, {}, "Invert selected mask"));
        commands.append(command("detect_edges", "image", {},
                                {"imageIndex", "seedX", "seedY", "tolerance"},
                                "Detect an edge-connected image region"));
        commands.append(command("analyze_regions", "image", {},
                                {"imageIndex", "rows", "cols"},
                                "Analyze image regions on a grid"));

        commands.append(command("render_mosaic", "image_to_drawing", {},
                                {"imageIndex", "quality", "mode", "gridX", "gridY",
                                 "blending", "maxDepth", "varianceThreshold"},
                                "Render mosaic geometry from an image"));
        commands.append(command("auto_trace", "image_to_drawing", {},
                                {"imageIndex", "threshold", "simplify", "minLength",
                                 "maxContours", "render", "lineWidth", "color",
                                 "colorMatch", "variableWidth", "gaussianBlur", "opacity"},
                                "Auto-trace an image to vectors"));
        commands.append(command("render_photo_copy", "image_to_drawing", {},
                                {"imageIndex", "quality", "style", "removeImage",
                                 "detailOverlay", "traceEdges"},
                                "Render a photo-copy or sketch interpretation"));
        return commands;
    }();
    return value;
}
} // namespace

QJsonArray AutomationCommandCatalog::commands()
{
    return registry();
}

bool AutomationCommandCatalog::containsAction(const QString &action)
{
    for (const QJsonValue &value : registry()) {
        if (value.toObject().value(QStringLiteral("action")).toString() == action)
            return true;
    }
    return false;
}

bool AutomationCommandCatalog::requiresFilesystemAccess(const QString &action)
{
    for (const QJsonValue &value : registry()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("action")).toString() == action)
            return entry.value(QStringLiteral("filesystemAccess")).toBool(false);
    }
    return false;
}

QStringList AutomationCommandCatalog::missingRequiredParams(
    const QString &action, const QJsonObject &params)
{
    for (const QJsonValue &value : registry()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("action")).toString() != action)
            continue;
        QStringList missing;
        for (const QJsonValue &required :
             entry.value(QStringLiteral("requiredParams")).toArray()) {
            const QString name = required.toString();
            if (!params.contains(name) || params.value(name).isNull()
                || params.value(name).isUndefined()) {
                missing.append(name);
            }
        }
        return missing;
    }
    return {};
}
