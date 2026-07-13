#pragma once

/**
 * Priority classification for mouse move / release (refactor E11).
 * Mirrors DrawingCanvas if-chain order so dispatch stays pure + testable.
 */
namespace InteractionPhase {

enum class Move {
    ControlPointEdit,
    BrushOrBlur,
    Pan,
    Marquee,
    ResizeObject,
    RotateObject,
    MoveSelection,
    ActiveTool,   // Text tool or isDrawing geometry session
    SelectHover,  // Select idle hover over handles
    Idle,         // nothing interactive (may still update cursor preview)
};

struct MoveContext {
    bool editingControlPoints = false;
    bool brushing = false;
    bool blurring = false;
    bool panning = false;
    bool selecting = false;
    bool resizingObject = false;
    bool rotatingObject = false;
    bool moving = false;
    bool isDrawing = false;
    bool isTextTool = false;
    bool isSelectTool = false;
};

/** First matching phase in canvas priority order. */
Move classifyMove(const MoveContext &c);

enum class Release {
    BrushOrBlur,
    ControlPointEdit,
    ResizeObject,
    RotateObject,
    RotateText,
    ResizeText,
    MoveSelection,
    TextTool,
    Pan,
    Marquee,
    DrawingSession,
    None,
};

struct ReleaseContext {
    bool leftButton = false;
    bool middleButton = false;
    bool brushing = false;
    bool blurring = false;
    bool editingControlPoints = false;
    bool resizingObject = false;
    bool rotatingObject = false;
    bool rotatingText = false;
    bool resizingText = false;
    bool moving = false;
    bool isTextTool = false;
    bool panning = false;
    bool selecting = false;
    bool isDrawing = false;
};

Release classifyRelease(const ReleaseContext &c);

} // namespace InteractionPhase
