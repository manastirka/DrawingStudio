#include "DrawingCanvas.h"

#include "tools/IDrawingTool.h"
#include "MoveController.h"
#include "SelectController.h"
#include "SelectionGestureController.h"
#include "ControlPointEditController.h"
#include "ViewPanController.h"
#include "AirbrushEngine.h"
#include "CanvasPrimitiveOps.h"
#include "CanvasContextMenu.h"
#include "Commands.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "SelectionManager.h"
#include "ClassicTextTool.h"

#include <QJsonObject>
#include <QMenu>
#include <QPoint>
#include <QToolTip>
#include <QTimer>
#include <QUuid>
#include <memory>
#include <vector>

// Host factory methods (refactor E1) — kept as DrawingCanvas members
// but implemented in this translation unit to slim DrawingCanvas.cpp.

SelectHost DrawingCanvas::makeSelectHost()
{
  SelectHost host;
  host.screenToWorld = [this](const QPoint &p) { return screenToWorld(p); };
  host.selectedObjects = [this]() { return selectedObjects(); };
  host.forEachVisiblePrimitiveTopFirst =
      [this](const std::function<void(DrawingPrimitive *)> &fn) {
        CanvasPrimitiveOps::forEachVisibleTopFirst(m_layerManager, m_primitives,
                                                   fn);
      };
  host.toObjectLocal = [this](const DrawingPrimitive *o, const QVector2D &p) {
    return toObjectLocal(o, p);
  };
  host.hitTestSelectionHandle =
      [this](const QRectF &br, const QVector2D &p, bool canRotate) {
        return hitTestSelectionHandle(br, p, canRotate);
      };
  host.cursorForSelectionHandle = [this](int i) {
    return cursorForSelectionHandle(i);
  };
  host.supportsRotationHandle = [this](const DrawingPrimitive *o) {
    return supportsRotationHandle(o);
  };
  host.objectRotationDegrees = [this](const DrawingPrimitive *o) {
    return objectRotationDegrees(o);
  };
  host.findControlPointAt = [this](const QVector2D &p, float tol) {
    return findControlPointAt(p, tol);
  };
  host.startControlPointEdit = [this](DrawingPrimitive *p, int i) {
    startControlPointEdit(p, i);
  };
  host.clearSelection = [this]() { clearSelection(); };
  host.addToSelection = [this](DrawingPrimitive *p) {
    m_selectionManager->addToSelection(p);
  };
  host.removeFromSelection = [this](DrawingPrimitive *p) {
    m_selectionManager->removeFromSelection(p);
  };
  host.selectGroupMembers = [this](DrawingPrimitive *p) {
    selectGroupMembers(p);
  };
  host.emitSelectionChanged = [this]() { emit selectionChanged(); };
  host.setSmartHint = [this](const QString &h) { emit smartHintChanged(h); };
  host.requestUpdate = [this]() { update(); };
  host.setCursor = [this](Qt::CursorShape c) { setCursor(c); };
  // E9: CanvasContextMenu::execMaskEditMenu owns UI; host still issues commands
  host.showImageEditContextMenu =
      [this](const QPoint &globalPos, ImagePrimitive *img, int cpIndex,
             int segmentIndex) {
        if (!img) {
          return;
        }
        using A = CanvasContextMenu::MaskEditAction;
        const A action = CanvasContextMenu::execMaskEditMenu(
            this, globalPos, cpIndex >= 0, segmentIndex >= 0);
        if (action == A::DeleteControlPoint && cpIndex >= 0) {
          auto command =
              std::make_unique<DeleteMaskControlPointCommand>(img, cpIndex);
          emit commandRequested(command.release());
          update();
        } else if (action == A::InsertControlPoint && segmentIndex >= 0) {
          const QPoint local = mapFromGlobal(globalPos);
          const QVector2D wp = screenToWorld(local);
          auto command = std::make_unique<InsertMaskControlPointCommand>(
              img, segmentIndex, QPointF(wp.x(), wp.y()));
          emit commandRequested(command.release());
          update();
        }
      };
  host.beginCopyAlongConnectedLineMove = [this]() {
    return beginCopyAlongConnectedLineMove();
  };
  host.isCopyAlongModifier = [](Qt::KeyboardModifiers m) {
    return DrawingCanvas::isCopyAlongModifier(m);
  };
  host.resolveLineMoveConstraint = [this](const LinePrimitive *l) {
    return resolveLineMoveConstraint(l);
  };
  host.zoomLevel = [this]() { return m_zoomLevel; };
  host.forceAdditive = [this]() { return m_forceAdditiveSelection; };
  host.forceSubtractive = [this]() { return m_forceSubtractiveSelection; };
  host.selectionMode = [this]() {
    return m_selectionMode == SelectionMode::Lasso ? 1 : 0;
  };
  host.isMoving = &m_isMoving;
  host.isCopyAlongMove = &m_isCopyAlongMove;
  host.moveStartPos = &m_moveStartPos;
  host.totalMoveOffset = &m_totalMoveOffset;
  host.isResizingObject = &m_isResizingObject;
  host.resizingObject = &m_resizingObject;
  host.resizeHandleIndex = &m_resizeHandleIndex;
  host.resizeOrigBounds = &m_resizeOrigBounds;
  host.resizeOrigControlPoints = &m_resizeOrigControlPoints;
  host.resizeOrigState = &m_resizeOrigState;
  host.isRotatingObject = &m_isRotatingObject;
  host.rotatingObject = &m_rotatingObject;
  host.rotationPivot = &m_rotationPivot;
  host.rotateOrigState = &m_rotateOrigState;
  host.rotationStartAngle = &m_rotationStartAngle;
  host.initialRotation = &m_initialRotation;
  host.editingPrimitive = &m_editingPrimitive;
  host.drawStartPos = &m_drawStartPos;
  host.lassoPoints = &m_lassoPoints;
  host.selectionOperation = &m_selectionOperation;
  host.setIsSelecting = [this](bool v) {
    m_selectionManager->setIsSelecting(v);
  };
  host.setSelectionRect = [this](const QRectF &r) {
    m_selectionManager->setSelectionRect(r);
  };
  return host;
}

MoveHost DrawingCanvas::makeMoveHost()
{
  MoveHost host;
  host.screenToWorld = [this](const QPoint &p) { return screenToWorld(p); };
  host.selectedObjects = [this]() { return selectedObjects(); };
  host.findPrimitiveAt = [this](const QVector2D &p) {
    return findPrimitiveAt(p);
  };
  host.findControlPointAt = [this](const QVector2D &p, float tol) {
    return findControlPointAt(p, tol);
  };
  host.startControlPointEdit = [this](DrawingPrimitive *p, int i) {
    startControlPointEdit(p, i);
  };
  host.clearSelection = [this]() { clearSelection(); };
  host.addToSelection = [this](DrawingPrimitive *p) {
    m_selectionManager->addToSelection(p);
  };
  host.emitSelectionChanged = [this]() { emit selectionChanged(); };
  host.setSmartHint = [this](const QString &h) { emit smartHintChanged(h); };
  host.requestUpdate = [this]() { update(); };
  host.setCursor = [this](Qt::CursorShape c) { setCursor(c); };
  host.isMoveToolActive = [this]() {
    return m_currentTool == DrawingTool::Move;
  };
  host.constrainDelta = [this](const QVector2D &d, Qt::KeyboardModifiers m) {
    return constrainDeltaAlongConnectedLine(d, m);
  };
  host.updateAlignmentGuides = [this](const QVector2D &p) {
    updateAlignmentGuides(p);
  };
  host.emitMoveCommand =
      [this](const std::vector<DrawingPrimitive *> &objs,
             const QVector2D &offset) {
        auto command = std::make_unique<MovePrimitivesCommand>(objs, offset);
        emit commandRequested(command.release());
      };
  host.deleteSelectedWithCommand = [this]() {
    deleteSelectedPrimitivesWithCommand();
  };
  host.controlPointTolerance = [this]() { return 8.0f / m_zoomLevel; };
  host.objectHitTolerance = [this]() { return 5.0f / m_zoomLevel; };
  host.isMoving = &m_isMoving;
  host.isCopyAlongMove = &m_isCopyAlongMove;
  host.moveStartPos = &m_moveStartPos;
  host.totalMoveOffset = &m_totalMoveOffset;
  host.editingPrimitive = &m_editingPrimitive;
  return host;
}

SelectionGestureHost DrawingCanvas::makeSelectionGestureHost()
{
  SelectionGestureHost host;
  host.toObjectLocal = [this](const DrawingPrimitive *o, const QVector2D &p) {
    return toObjectLocal(o, p);
  };
  host.computeResizedBounds =
      [this](const QRectF &orig, int hi, const QVector2D &wp,
             Qt::KeyboardModifiers mods) {
        return computeResizedBounds(orig, hi, wp, mods);
      };
  host.applyObjectRotation = [this](DrawingPrimitive *o, float d) {
    applyObjectRotation(o, d);
  };
  host.emitTransformCommand =
      [this](DrawingPrimitive *o, const QJsonObject &oldS,
             const QJsonObject &newS, const QString &label, bool isRotate) {
        auto command = std::make_unique<TransformPrimitivesCommand>(
            std::vector<DrawingPrimitive *>{o},
            isRotate ? TransformPrimitivesCommand::Rotate
                     : TransformPrimitivesCommand::Resize,
            label);
        command->storeTransformation({oldS}, {newS});
        emit commandRequested(command.release());
      };
  host.requestUpdate = [this]() { update(); };
  host.emitSelectionChanged = [this]() { emit selectionChanged(); };
  host.setCursor = [this](Qt::CursorShape c) { setCursor(c); };
  host.selectedObjects = [this]() { return selectedObjects(); };
  host.hitTestHandle =
      [this](const QRectF &br, const QVector2D &p, bool rot) {
        return hitTestSelectionHandle(br, p, rot);
      };
  host.cursorForHandle = [this](int i) { return cursorForSelectionHandle(i); };
  host.supportsRotation = [this](const DrawingPrimitive *o) {
    return supportsRotationHandle(o);
  };
  host.zoomLevel = [this]() { return m_zoomLevel; };
  host.selectInRect =
      [this](const QRectF &r, SelectionManager::SelectionOperation op) {
        selectObjectsInRect(r, op);
      };
  host.selectInLasso =
      [this](const QPolygonF &p, SelectionManager::SelectionOperation op) {
        selectObjectsInLasso(p, op);
      };
  host.isResizing = &m_isResizingObject;
  host.resizingObject = &m_resizingObject;
  host.resizeHandleIndex = &m_resizeHandleIndex;
  host.resizeOrigBounds = &m_resizeOrigBounds;
  host.resizeOrigControlPoints = &m_resizeOrigControlPoints;
  host.resizeOrigState = &m_resizeOrigState;
  host.isRotating = &m_isRotatingObject;
  host.rotatingObject = &m_rotatingObject;
  host.rotationPivot = &m_rotationPivot;
  host.rotateOrigState = &m_rotateOrigState;
  host.rotationStartAngle = &m_rotationStartAngle;
  host.initialRotation = &m_initialRotation;
  host.drawStartPos = &m_drawStartPos;
  host.lassoPoints = &m_lassoPoints;
  host.selectionOperation = &m_selectionOperation;
  host.isSelecting = [this]() { return m_selectionManager->isSelecting(); };
  host.setIsSelecting = [this](bool v) {
    m_selectionManager->setIsSelecting(v);
  };
  host.selectionRect = [this]() {
    return m_selectionManager->selectionRect();
  };
  host.setSelectionRect = [this](const QRectF &r) {
    m_selectionManager->setSelectionRect(r);
  };
  host.selectionMode = [this]() {
    return m_selectionMode == SelectionMode::Lasso ? 1 : 0;
  };
  return host;
}

ControlPointEditHost DrawingCanvas::makeControlPointEditHost()
{
  ControlPointEditHost host;
  host.snapToGrid = [this](const QVector2D &p) { return snapToGrid(p); };
  host.findPrimitiveById = [this](const QUuid &id) {
    return findPrimitiveById(id);
  };
  host.resolveLineMoveConstraint = [this](const LinePrimitive *l) {
    return resolveLineMoveConstraint(l);
  };
  host.projectOntoSegment =
      [this](const QVector2D &p, const QVector2D &a, const QVector2D &b) {
        return projectPointOntoLineSegment(p, a, b);
      };
  host.requestUpdate = [this]() { update(); };
  host.emitMaskCpCommand =
      [this](DrawingPrimitive *prim, int index, const QPointF &from,
             const QPointF &to) {
        auto *img = dynamic_cast<ImagePrimitive *>(prim);
        if (!img) {
          return;
        }
        auto command = std::make_unique<ModifyMaskControlPointCommand>(
            img, index, from, to);
        emit commandRequested(command.release());
      };
  host.emitCpCommand =
      [this](DrawingPrimitive *prim, int index, const QVector2D &from,
             const QVector2D &to) {
        auto command = std::make_unique<ModifyControlPointCommand>(
            prim, index, from, to);
        emit commandRequested(command.release());
      };
  host.emitTransformCommand =
      [this](DrawingPrimitive *prim, const QJsonObject &oldS,
             const QJsonObject &newS, const QString &label) {
        auto command = std::make_unique<TransformPrimitivesCommand>(
            std::vector<DrawingPrimitive *>{prim},
            TransformPrimitivesCommand::Resize, label);
        command->storeTransformation({oldS}, {newS});
        emit commandRequested(command.release());
      };
  host.isEditing = &m_isEditingControlPoints;
  host.selectedIndex = &m_selectedControlPoint;
  host.editingPrimitive = &m_editingPrimitive;
  host.originalPosition = &m_originalControlPointPosition;
  host.origState = &m_controlEditOrigState;
  return host;
}

ViewPanHost DrawingCanvas::makeViewPanHost()
{
  ViewPanHost host;
  host.zoomLevel = &m_zoomLevel;
  host.viewCenter = &m_viewCenter;
  host.lastMousePos = &m_lastMousePos;
  host.isPanning = &m_isPanning;
  host.requestUpdate = [this]() { update(); };
  host.setPanCursor = [this]() { setCursor(Qt::SizeAllCursor); };
  host.setArrowCursor = [this]() { setCursor(Qt::ArrowCursor); };
  return host;
}


ToolHost DrawingCanvas::makeToolHost()
{
  // E10: callbacks + session pointers filled separately
  ToolHost host;
  bindToolHostCallbacks(host);
  bindToolHostSession(host);
  return host;
}

void DrawingCanvas::bindToolHostCallbacks(ToolHost &host)
{
  host.screenToWorld = [this](const QPoint &p) { return screenToWorld(p); };
  host.snapToGrid = [this](const QVector2D &p) { return snapToGrid(p); };
  host.snapToEndpoint = [this](const QVector2D &p) {
    return snapToLineEndpoint(p);
  };
  host.smartConstraints =
      [this](const QVector2D &p, Qt::KeyboardModifiers mods, QString *hint) {
        return applySmartDrawingConstraints(p, mods, hint);
      };
  host.commitPrimitive = [this](std::unique_ptr<DrawingPrimitive> prim) {
    addPrimitiveWithCommand(std::move(prim));
  };
  host.requestUpdate = [this]() { update(); };
  host.setSmartHint = [this](const QString &hint) {
    emit smartHintChanged(hint);
  };
  host.unitsString = [this]() { return getUnitsString(); };
  host.pixelsPerUnit = [this]() {
    return static_cast<float>(pixelsPerUnit());
  };
  host.circleThroughPoints =
      [this](const QVector2D &p1, const QVector2D &p2, const QVector2D &p3,
             QVector2D &centerOut, float &radiusOut) {
        return computeCircleThroughPoints(p1, p2, p3, centerOut, radiusOut);
      };
  host.snapAngleEndpoint =
      [this](const QVector2D &origin, const QVector2D &rawEnd,
             Qt::KeyboardModifiers mods) {
        return snapAngleLineEndpoint(origin, rawEnd, mods);
      };
  host.hasPrimitives = [this]() {
    return CanvasPrimitiveOps::hasAny(m_layerManager, m_primitives);
  };
  host.beginControlPointEdit = [this](DrawingPrimitive *prim, int index) {
    startControlPointEdit(prim, index);
  };
  host.hasLayerManager = [this]() { return m_layerManager != nullptr; };
  host.collectHitPrimitives = [this](const QVector2D &pos, float radius) {
    return CanvasPrimitiveOps::collectHitsAt(m_layerManager, m_primitives, pos,
                                             radius);
  };
  host.eraseLegacyDirect = [this](const QVector2D &pos, float radius) {
    if (!CanvasPrimitiveOps::eraseLegacyContaining(m_primitives, pos, radius)) {
      return false;
    }
    m_selectionManager->clearSelection();
    return true;
  };
  host.deletePrimitives = [this](const std::vector<DrawingPrimitive *> &list) {
    if (list.empty()) {
      return;
    }
    auto command = std::make_unique<DeletePrimitivesCommand>(this, list);
    emit commandRequested(command.release());
  };
  host.findFillTarget = [this](const QVector2D &pos,
                               float tolerance) -> DrawingPrimitive * {
    return CanvasPrimitiveOps::findFillTarget(m_layerManager, pos, tolerance);
  };
  host.commitPropertyChange =
      [this](DrawingPrimitive *p, const QJsonObject &oldState,
             const QString &description) {
        if (!p) {
          return;
        }
        auto cmd = std::make_unique<ModifyPrimitiveCommand>(p, description);
        cmd->storePropertyChange(oldState, p->toJson());
        cmd->markAlreadyApplied();
        if (cmd->hasStateChange()) {
          emit commandRequested(cmd.release());
        }
      };
  host.clearSelection = [this]() { clearSelection(); };
  host.selectOnly = [this](DrawingPrimitive *p) {
    clearSelection();
    if (!p) {
      return;
    }
    p->setSelected(true);
    m_selectionManager->addToSelection(p);
    emit selectionChanged();
  };
  host.showTooltip = [this](const QString &text, const QPoint &globalPos) {
    QToolTip::showText(globalPos, text, this);
  };
  host.startAirbrushTimer = [this]() {
    AirbrushEngine::beginStroke(m_airbrushState, m_airbrushPos);
    if (m_airbrushTimer) {
      m_airbrushTimer->start();
    }
  };
  host.stopAirbrushTimer = [this]() {
    if (m_airbrushTimer) {
      m_airbrushTimer->stop();
    }
  };
  host.clearAirbrushDrips = [this]() {
    AirbrushEngine::clearDrips(m_airbrushState);
  };
  host.selectPress = [this](QMouseEvent *e) { handleSelectTool(e); };
  host.movePress = [this](QMouseEvent *e) { handleMoveTool(e); };
  host.ensureTextToolActive = [this]() {
    if (m_classicTextTool && !m_classicTextTool->isActive()) {
      m_classicTextTool->activate();
    }
  };
  host.textPress = [this](const QVector2D &p) {
    if (m_classicTextTool) {
      m_classicTextTool->mousePress(p);
    }
  };
  host.textMove = [this](const QVector2D &p) {
    if (m_classicTextTool) {
      m_classicTextTool->mouseMove(p);
    }
  };
  host.textRelease = [this](const QVector2D &p) {
    if (m_classicTextTool) {
      m_classicTextTool->mouseRelease(p);
    }
  };
  host.textDoubleClick = [this](const QVector2D &p) {
    if (m_classicTextTool) {
      m_classicTextTool->mouseDoubleClick(p);
    }
  };
  host.dialogParent = [this]() -> QWidget * {
    return window() ? window() : static_cast<QWidget *>(this);
  };
  host.runDeferred = [this](std::function<void()> fn) {
    QTimer::singleShot(0, this, std::move(fn));
  };
}

void DrawingCanvas::bindToolHostSession(ToolHost &host)
{
  host.defaultColor = m_defaultDrawingColor;
  host.defaultLineStyle = m_defaultLineStyle;
  host.defaultLineWidth = m_defaultLineWidth;
  host.defaultFillEnabled = m_defaultFillEnabled;
  host.defaultFillColor = m_defaultFillColor;
  host.eraserSize = m_eraserSize;
  host.fillSplashMode = (m_fillMode == FillMode::Splash);
  host.brushSize = m_brushSize;
  host.brushHardness = m_brushHardness;
  host.isDrawing = &m_isDrawing;
  host.drawStart = &m_drawStartPos;
  host.drawCurrent = &m_drawCurrentPos;
  host.currentPrimitive = &m_currentPrimitive;
  host.lastSmartHint = &m_lastSmartHint;
  host.isBrushing = &m_isBrushing;
  host.isBlurring = &m_isBlurring;
  host.brushStroke = &m_brushStroke;
  host.brushParticleScale = &m_brushParticleScale;
  host.brushParticleAlpha = &m_brushParticleAlpha;
  host.airbrushPos = &m_airbrushPos;
  host.arcStage = &m_arcStage;
  host.arcStart = &m_arcStart;
  host.arcEnd = &m_arcEnd;
  host.angleLineStage = &m_angleLineStage;
  host.angleBaselineStart = &m_angleBaselineStart;
  host.angleBaselineEnd = &m_angleBaselineEnd;
  host.angleBaselinePrimitiveId = &m_angleBaselinePrimitiveId;
  host.angleBaselineConstraintDir = &m_angleBaselineConstraintDir;
  host.bezierCreationStage = &m_bezierCreationStage;
}

