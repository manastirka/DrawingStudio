#include "DrawingCanvas.h"

#include "CanvasCoords.h"
#include "CanvasKeyMap.h"
#include "CanvasPrimitiveOps.h"
#include "CanvasZoom.h"
#include "ClassicTextTool.h"
#include "Commands.h"
#include "ControlPointEditController.h"
#include "DrawingPrimitive.h"
#include "DrawingToolRegistry.h"
#include "InteractionCancel.h"
#include "InteractionPhase.h"
#include "LayerManager.h"
#include "MoveController.h"
#include "SelectionGestureController.h"
#include "SelectionManager.h"
#include "TextResizeOps.h"
#include "ToolCursor.h"
#include "ViewPanController.h"
#include "tools/IDrawingTool.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>

// Input events (refactor E11) — mouse / wheel / keys live here so
// DrawingCanvas.cpp stays focused on paint + document ops.

void DrawingCanvas::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    const QVector2D worldPos = screenToWorld(event->pos());
    emit coordinatesChanged(worldPos);

    // Spline attach mode (text-on-path)
    if (m_splineSelectionMode && event->button() == Qt::LeftButton &&
        m_textAwaitingSpline) {
        const float minDist = 10.0f / m_zoomLevel;
        if (DrawingPrimitive *clickedPrim = CanvasPrimitiveOps::findSplineAt(
                m_layerManager, m_primitives, worldPos, minDist)) {
            m_textAwaitingSpline->setFollowsSpline(true);
            m_textAwaitingSpline->setSplineId(clickedPrim->id());
            setSplineSelectionMode(false);
            emit selectionChanged();
            update();
        }
        return;
    }

    // Pipette
    if (m_selectionManager->isPipetteMode() &&
        event->button() == Qt::LeftButton) {
        const QColor pickedColor = getColorAtPosition(worldPos);
        m_selectionManager->setPipetteMode(false);
        setCursor(Qt::ArrowCursor);
        selectByColor(pickedColor, m_selectionManager->pipetteTolerance());
        return;
    }

    // Middle-button pan
    if (event->button() == Qt::MiddleButton) {
        ViewPanHost pan = makeViewPanHost();
        ViewPanController::begin(pan, event->pos());
        return;
    }

    if (IDrawingTool *tool = DrawingToolRegistry::toolFor(m_currentTool)) {
        ToolHost host = makeToolHost();
        tool->onPress(host, event);
    }

    emit coordinatesChanged(worldPos);
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QVector2D worldPos = screenToWorld(event->pos());
    emit coordinatesChanged(worldPos);

    if (m_showCursorPreview) {
        m_cursorPreviewPos = worldPos;
    }

    InteractionPhase::MoveContext ctx;
    ctx.editingControlPoints = m_isEditingControlPoints;
    ctx.brushing = m_isBrushing;
    ctx.blurring = m_isBlurring;
    ctx.panning = m_isPanning;
    ctx.selecting = m_selectionManager->isSelecting();
    ctx.resizingObject = m_isResizingObject && m_resizingObject;
    ctx.rotatingObject = m_isRotatingObject && m_rotatingObject;
    ctx.moving = m_isMoving;
    ctx.isDrawing = m_isDrawing;
    ctx.isTextTool = (m_currentTool == DrawingTool::Text);
    ctx.isSelectTool = (m_currentTool == DrawingTool::Select);

    switch (InteractionPhase::classifyMove(ctx)) {
    case InteractionPhase::Move::ControlPointEdit: {
        ControlPointEditHost cp = makeControlPointEditHost();
        ControlPointEditController::update(cp, worldPos, event->modifiers());
        return;
    }
    case InteractionPhase::Move::BrushOrBlur:
        if (IDrawingTool *tool = DrawingToolRegistry::toolFor(m_currentTool)) {
            ToolHost host = makeToolHost();
            tool->onMove(host, event);
            m_lastMousePos = event->pos();
            return;
        }
        break;
    case InteractionPhase::Move::Pan: {
        ViewPanHost pan = makeViewPanHost();
        ViewPanController::update(pan, event->pos());
        return;
    }
    case InteractionPhase::Move::Marquee: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onMarqueeUpdate(g, worldPos);
        return;
    }
    case InteractionPhase::Move::ResizeObject: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onResizeDrag(g, worldPos,
                                                 event->modifiers());
        return;
    }
    case InteractionPhase::Move::RotateObject: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onRotateDrag(g, worldPos,
                                                 event->modifiers());
        return;
    }
    case InteractionPhase::Move::MoveSelection: {
        MoveHost mhost = makeMoveHost();
        MoveController::onDrag(mhost, worldPos);
        return;
    }
    case InteractionPhase::Move::ActiveTool:
        if (IDrawingTool *tool = DrawingToolRegistry::toolFor(m_currentTool)) {
            ToolHost host = makeToolHost();
            tool->onMove(host, event);
            m_lastMousePos = event->pos();
            return;
        }
        break;
    case InteractionPhase::Move::SelectHover: {
        if (m_showCursorPreview) {
            update();
        }
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onSelectHover(g, worldPos);
        m_lastMousePos = event->pos();
        return;
    }
    case InteractionPhase::Move::Idle:
        break;
    }

    if (m_showCursorPreview) {
        update();
    }
    m_lastMousePos = event->pos();
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_longPressTimer->isActive()) {
        m_longPressTimer->stop();
        m_longPressText = nullptr;
    }

    InteractionPhase::ReleaseContext ctx;
    ctx.leftButton = (event->button() == Qt::LeftButton);
    ctx.middleButton = (event->button() == Qt::MiddleButton);
    ctx.brushing = m_isBrushing;
    ctx.blurring = m_isBlurring;
    ctx.editingControlPoints = m_isEditingControlPoints;
    ctx.resizingObject = m_isResizingObject;
    ctx.rotatingObject = m_isRotatingObject;
    ctx.rotatingText = m_isRotatingText;
    ctx.resizingText = m_isResizingText;
    ctx.moving = m_isMoving;
    ctx.isTextTool = (m_currentTool == DrawingTool::Text);
    ctx.panning = m_isPanning;
    ctx.selecting = m_selectionManager->isSelecting();
    ctx.isDrawing = m_isDrawing;

    switch (InteractionPhase::classifyRelease(ctx)) {
    case InteractionPhase::Release::BrushOrBlur:
        if (IDrawingTool *tool = DrawingToolRegistry::toolFor(m_currentTool)) {
            ToolHost host = makeToolHost();
            tool->onRelease(host, event);
            m_lastMousePos = event->pos();
            return;
        }
        break;
    case InteractionPhase::Release::ControlPointEdit: {
        ControlPointEditHost cp = makeControlPointEditHost();
        ControlPointEditController::finish(cp);
        return;
    }
    case InteractionPhase::Release::ResizeObject: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onResizeFinish(g);
        return;
    }
    case InteractionPhase::Release::RotateObject: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onRotateFinish(g);
        return;
    }
    case InteractionPhase::Release::RotateText:
        m_isRotatingText = false;
        m_rotatingTextPrimitive = nullptr;
        setCursor(Qt::ArrowCursor);
        return;
    case InteractionPhase::Release::ResizeText: {
        if (m_resizingTextPrimitive) {
            const auto before = TextResizeOps::fromInitial(m_initialTextBounds,
                                                           m_initialFontSize);
            const auto after = TextResizeOps::capture(m_resizingTextPrimitive);
            if (TextResizeOps::shouldCommit(before, after)) {
                auto command =
                    std::make_unique<ResizeTextCommand>(m_resizingTextPrimitive);
                command->storeOldState(before.position, before.width,
                                       before.height, before.fontSize);
                command->storeNewState(after.position, after.width, after.height,
                                       after.fontSize);
                emit commandRequested(command.release());
            }
        }
        m_isResizingText = false;
        m_resizingTextPrimitive = nullptr;
        setCursor(Qt::ArrowCursor);
        return;
    }
    case InteractionPhase::Release::MoveSelection: {
        MoveHost mhost = makeMoveHost();
        MoveController::onFinish(mhost);
        m_originalPositions.clear();
        return;
    }
    case InteractionPhase::Release::TextTool:
        if (IDrawingTool *tool =
                DrawingToolRegistry::toolFor(DrawingTool::Text)) {
            ToolHost host = makeToolHost();
            tool->onRelease(host, event);
        }
        return;
    case InteractionPhase::Release::Pan: {
        ViewPanHost pan = makeViewPanHost();
        ViewPanController::end(pan);
        return;
    }
    case InteractionPhase::Release::Marquee: {
        SelectionGestureHost g = makeSelectionGestureHost();
        SelectionGestureController::onMarqueeFinish(g);
        return;
    }
    case InteractionPhase::Release::DrawingSession:
        if (IDrawingTool *tool = DrawingToolRegistry::toolFor(m_currentTool)) {
            ToolHost host = makeToolHost();
            tool->onRelease(host, event);
            m_lastMousePos = event->pos();
            return;
        }
        break;
    case InteractionPhase::Release::None:
        break;
    }

    m_lastMousePos = event->pos();
}

void DrawingCanvas::wheelEvent(QWheelEvent *event)
{
    const float deltaY = CanvasZoom::resolveScrollDeltaY(event->pixelDelta(),
                                                         event->angleDelta());
    if (std::abs(deltaY) < 0.1f) {
        event->accept();
        return;
    }

    qDebug() << "Wheel event - deltaY:" << deltaY
             << "pixelDelta:" << event->pixelDelta()
             << "angleDelta:" << event->angleDelta();

    const QVector2D mouseOffset = CanvasCoords::mouseOffsetFromCenter(
        event->position().toPoint(), width(), height());

    if (CanvasZoom::applyZoomToCursor(m_zoomLevel, m_viewCenter,
                                      m_zoomSensitivity, deltaY, mouseOffset)) {
        qDebug() << "Zoom level:" << m_zoomLevel;
        emit zoomChanged(m_zoomLevel);
        update();
    }

    event->accept();
}

void DrawingCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton &&
        m_currentTool == DrawingTool::Text) {
        if (IDrawingTool *tool =
                DrawingToolRegistry::toolFor(DrawingTool::Text)) {
            ToolHost host = makeToolHost();
            tool->onPress(host, event);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QVector2D worldPos = screenToWorld(event->pos());
    TextPrimitive *textPrim =
        CanvasPrimitiveOps::findTextAt(m_layerManager, m_primitives, worldPos);
    if (!textPrim) {
        return;
    }

    ClassicTextTool *textTool = m_classicTextTool;
    if (textTool) {
        emit toolChangeRequested(DrawingTool::Text);
        textTool->selectText(textPrim);
        textTool->startEditing(textPrim);
        update();
        event->accept();
        return;
    }

    clearSelection();
    textPrim->setSelected(true);
    m_selectionManager->addToSelection(textPrim);
    emit selectionChanged();
    qDebug() << "Double-clicked text - selected for editing";
    event->accept();
}

void DrawingCanvas::keyPressEvent(QKeyEvent *event)
{
    using A = CanvasKeyMap::Action;
    switch (CanvasKeyMap::mapPress(event->key(), event->modifiers())) {
    case A::SpacePanBegin:
        if (!m_isPanning) {
            m_isPanning = true;
            setCursor(Qt::SizeAllCursor);
        }
        break;
    case A::EscapeCancel: {
        InteractionCancel::Targets t;
        t.isDrawing = &m_isDrawing;
        t.isPanning = &m_isPanning;
        t.setIsSelecting = [this](bool v) {
            m_selectionManager->setIsSelecting(v);
        };
        t.clearSelection = [this]() { clearSelection(); };
        t.setArrowCursor = [this]() { setCursor(Qt::ArrowCursor); };
        t.requestUpdate = [this]() { update(); };
        InteractionCancel::applyEscape(t);
        break;
    }
    case A::DeleteSelection:
        deleteSelectedPrimitivesWithCommand();
        break;
    case A::ZoomIn:
        zoomIn();
        break;
    case A::ZoomOut:
        zoomOut();
        break;
    case A::ZoomFit:
        zoomFit();
        break;
    case A::MaskPrev:
        emit maskNavigationRequested(-1);
        event->accept();
        break;
    case A::MaskNext:
        emit maskNavigationRequested(1);
        event->accept();
        break;
    case A::MaskInvert:
        emit maskInvertRequested();
        event->accept();
        break;
    case A::PassThrough:
    case A::None:
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void DrawingCanvas::keyReleaseEvent(QKeyEvent *event)
{
    using A = CanvasKeyMap::Action;
    if (CanvasKeyMap::mapRelease(event->key()) == A::SpacePanEnd) {
        InteractionCancel::endSpacePan(&m_isPanning, [this]() {
            setCursor(Qt::ArrowCursor);
        });
        return;
    }
    QWidget::keyReleaseEvent(event);
}
