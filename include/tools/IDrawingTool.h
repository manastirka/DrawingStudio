#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPoint>
#include <QString>
#include <QUuid>
#include <QVector2D>
#include <Qt>
#include <functional>
#include <memory>
#include <vector>

class DrawingPrimitive;
class QWidget;

/**
 * Host callbacks + shared drawing session pointers provided by DrawingCanvas.
 * Tools must not store canvas pointers permanently — use host each event.
 */
struct ToolHost {
    std::function<QVector2D(const QPoint &)> screenToWorld;
    std::function<QVector2D(const QVector2D &)> snapToGrid;
    std::function<QVector2D(const QVector2D &)> snapToEndpoint;
    std::function<QVector2D(const QVector2D &, Qt::KeyboardModifiers, QString *)>
        smartConstraints;
    std::function<void(std::unique_ptr<DrawingPrimitive>)> commitPrimitive;
    std::function<void()> requestUpdate;
    std::function<void(const QString &)> setSmartHint;
    /// Units label for dimension tools (e.g. "mm", "in").
    std::function<QString()> unitsString;
    /// Canvas scale: pixels per display unit.
    std::function<float()> pixelsPerUnit;
    /// Fit circle through three points (Arc stage-2 preview / commit).
    std::function<bool(const QVector2D &, const QVector2D &, const QVector2D &,
                       QVector2D &, float &)>
        circleThroughPoints;
    /// Snap angled segment endpoint relative to baseline (15° / Shift=5° / Alt=free).
    std::function<QVector2D(const QVector2D & /*origin*/,
                            const QVector2D & /*rawEnd*/,
                            Qt::KeyboardModifiers)>
        snapAngleEndpoint;
    /// True if canvas already has any primitive (bezier chain continue guard).
    std::function<bool()> hasPrimitives;
    /// Begin editing a control point on the given primitive (canvas gesture).
    std::function<void(DrawingPrimitive *, int /*cpIndex*/)> beginControlPointEdit;

    // --- Eraser / Fill (B14) ---
    /// Visible unlocked layer primitives that contain pos within radius.
    std::function<std::vector<DrawingPrimitive *>(const QVector2D &pos,
                                                  float radius)>
        collectHitPrimitives;
    /// Legacy non-layer storage erase; returns true if anything removed.
    std::function<bool(const QVector2D &pos, float radius)> eraseLegacyDirect;
    /// Delete via undoable command.
    std::function<void(const std::vector<DrawingPrimitive *> &)> deletePrimitives;
    /// Top-most fillable shape under pos (or null).
    std::function<DrawingPrimitive *(const QVector2D &pos, float tolerance)>
        findFillTarget;
    /// Emit ModifyPrimitiveCommand after in-place property edits.
    std::function<void(DrawingPrimitive *, const QJsonObject &oldState,
                       const QString &description)>
        commitPropertyChange;
    std::function<void()> clearSelection;
    std::function<void(DrawingPrimitive *)> selectOnly;
    std::function<void(const QString &text, const QPoint &globalPos)> showTooltip;
    std::function<bool()> hasLayerManager;

    // --- Brush / Blur (B15) ---
    std::function<void()> startAirbrushTimer;
    std::function<void()> stopAirbrushTimer;
    std::function<void()> clearAirbrushDrips;

    // --- Select / Move / Text / Image host bridges (B16–B17) ---
    /// Full Select press (resize/rotate/marquee) still lives on canvas.
    std::function<void(QMouseEvent *)> selectPress;
    /// Full Move press (begin drag / CP edit) still lives on canvas.
    std::function<void(QMouseEvent *)> movePress;
    std::function<void()> ensureTextToolActive;
    std::function<void(const QVector2D &)> textPress;
    std::function<void(const QVector2D &)> textMove;
    std::function<void(const QVector2D &)> textRelease;
    std::function<void(const QVector2D &)> textDoubleClick;
    /// Parent widget for modal dialogs (image import).
    std::function<QWidget *()> dialogParent;
    /// Run work on next event-loop tick (image file dialog deferral).
    std::function<void(std::function<void()>)> runDeferred;

    QColor defaultColor = Qt::black;
    Qt::PenStyle defaultLineStyle = Qt::SolidLine;
    float defaultLineWidth = 2.0f;
    bool defaultFillEnabled = false;
    QColor defaultFillColor = Qt::black;
    float eraserSize = 20.0f;
    bool fillSplashMode = false; // true = FillMode::Splash
    float brushSize = 10.0f;
    float brushHardness = 0.5f;

    // Shared session (owned by canvas)
    bool *isDrawing = nullptr;
    QVector2D *drawStart = nullptr;
    QVector2D *drawCurrent = nullptr;
    std::unique_ptr<DrawingPrimitive> *currentPrimitive = nullptr;
    QString *lastSmartHint = nullptr;

    // Brush / blur stroke session (owned by canvas)
    bool *isBrushing = nullptr;
    bool *isBlurring = nullptr;
    std::vector<QVector2D> *brushStroke = nullptr;
    std::vector<float> *brushParticleScale = nullptr;
    std::vector<float> *brushParticleAlpha = nullptr;
    QVector2D *airbrushPos = nullptr;

    // Arc multi-stage session (owned by canvas; used by ArcTool)
    int *arcStage = nullptr;       // 0 idle, 1 start set, 2 end set / drag bulge
    QVector2D *arcStart = nullptr;
    QVector2D *arcEnd = nullptr;

    // AngleLine multi-stage session (owned by canvas; used by AngleLineTool)
    int *angleLineStage = nullptr; // 0 idle, 1 baseline drag, 2 angled segment
    QVector2D *angleBaselineStart = nullptr;
    QVector2D *angleBaselineEnd = nullptr;
    QUuid *angleBaselinePrimitiveId = nullptr;
    QVector2D *angleBaselineConstraintDir = nullptr;

    // Bezier creation stage (owned by canvas; used by BezierTool)
    // 0 idle / ready for new curve, 1 press–drag end point
    int *bezierCreationStage = nullptr;
};

/**
 * Drawing tool interface — press / move / release owned by one tool class.
 * B7–B17: all DrawingTool values have an IDrawingTool implementation.
 */
class IDrawingTool {
public:
    virtual ~IDrawingTool() = default;

    virtual void onPress(ToolHost &host, QMouseEvent *event) = 0;
    virtual void onMove(ToolHost &host, QMouseEvent *event) = 0;
    virtual void onRelease(ToolHost &host, QMouseEvent *event) = 0;
};
