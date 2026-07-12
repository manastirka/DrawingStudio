#pragma once

#include "DrawingCanvas.h"
#include <QString>
#include <QStringList>
#include <vector>

/**
 * Resolves contextual tool guidance: status hints + clickable suggestion chips.
 * Pure logic — no widgets.
 */
struct ToolSuggestionAction {
    QString id;      // e.g. "detect_subjects"
    QString label;   // chip text
    QString tip;     // tooltip
};

struct ToolSuggestionResult {
    QString statusHint;
    std::vector<ToolSuggestionAction> chips;
};

struct ToolSuggestionContext {
    DrawingTool tool = DrawingTool::Select;
    int selectionCount = 0;
    bool hasImage = false;
    bool hasText = false;
    bool imageHasMasks = false;
    bool snapEnabled = true;
    bool magneticEnabled = false;
    bool gridVisible = true;
    bool canvasEmpty = false;
    QString liveDrawHint; // transient hint from canvas while drawing
};

class ToolSuggestionService
{
public:
    static ToolSuggestionResult resolve(const ToolSuggestionContext &ctx);
};
