#include "DrawingCanvas.h"

#include "AlignmentGuides.h"
#include "CanvasGeometry.h"
#include "CanvasPrimitiveOps.h"
#include "ControlPointEditController.h"
#include "ControlPointHit.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"
#include "LineMoveOps.h"
#include "MagneticSnap.h"
#include "MoveController.h"
#include "ObjectLayoutOps.h"
#include "PaperModel.h"
#include "SelectController.h"
#include "SelectionManager.h"
#include "SelectionPolicy.h"
#include "SelectionTransform.h"
#include "SmartDrawingConstraints.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <vector>

// Thin façades + interaction helpers (refactor E13).

namespace {

SmartDrawingConstraints::ToolKind smartToolKind(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Line:
    case DrawingTool::Measure:
        return SmartDrawingConstraints::ToolKind::LineOrMeasure;
    case DrawingTool::Rectangle:
        return SmartDrawingConstraints::ToolKind::Rectangle;
    case DrawingTool::Ellipse:
        return SmartDrawingConstraints::ToolKind::Ellipse;
    default:
        return SmartDrawingConstraints::ToolKind::Other;
    }
}

} // namespace

void DrawingCanvas::selectionHandlePositions(const QRectF &br,
                                             QPointF out[9]) const
{
    SelectionTransform::handlePositions(br, m_zoomLevel, out);
}

float DrawingCanvas::selectionHandleHalfSize() const
{
    return SelectionTransform::handleHalfSize(m_zoomLevel);
}

float DrawingCanvas::selectionHandleHitRadius() const
{
    return SelectionTransform::handleHitRadius(m_zoomLevel);
}

bool DrawingCanvas::showsGeometryControlPoints(
    const DrawingPrimitive *obj) const
{
    return SelectionPolicy::showsGeometryControlPoints(obj);
}

bool DrawingCanvas::supportsRotationHandle(const DrawingPrimitive *obj) const
{
    return SelectionPolicy::supportsRotationHandle(obj);
}

float DrawingCanvas::objectRotationDegrees(const DrawingPrimitive *obj) const
{
    return SelectionPolicy::objectRotationDegrees(obj);
}

void DrawingCanvas::applyObjectRotation(DrawingPrimitive *obj, float degrees)
{
    SelectionPolicy::applyObjectRotation(obj, degrees);
}

bool DrawingCanvas::usesExternalRotation(const DrawingPrimitive *obj) const
{
    return SelectionPolicy::usesExternalRotation(obj);
}

QVector2D DrawingCanvas::toObjectLocal(const DrawingPrimitive *obj,
                                       const QVector2D &worldPos) const
{
    return SelectionPolicy::toObjectLocal(obj, worldPos);
}

Qt::CursorShape DrawingCanvas::cursorForSelectionHandle(int index) const
{
    return SelectionTransform::cursorForHandle(index);
}

void DrawingCanvas::drawSelectionHandles(QPainter &painter, const QRectF &br,
                                         bool showRotate) const
{
    SelectionTransform::drawHandles(painter, br, m_zoomLevel, showRotate);
}

int DrawingCanvas::hitTestSelectionHandle(const QRectF &br,
                                          const QVector2D &worldPos,
                                          bool includeRotate) const
{
    return SelectionTransform::hitTest(br, worldPos, m_zoomLevel, includeRotate);
}

QRectF DrawingCanvas::computeResizedBounds(const QRectF &orig, int handleIndex,
                                           const QVector2D &worldPos,
                                           Qt::KeyboardModifiers mods) const
{
    return SelectionTransform::computeResizedBounds(orig, handleIndex, worldPos,
                                                    mods);
}

void DrawingCanvas::handleSelectTool(QMouseEvent *event)
{
    SelectHost host = makeSelectHost();
    SelectController::onPress(host, event);
}

void DrawingCanvas::handleMoveTool(QMouseEvent *event)
{
    MoveHost host = makeMoveHost();
    MoveController::onPress(host, event);
}

void DrawingCanvas::handleMoveOperation(const QVector2D &worldPos)
{
    MoveHost host = makeMoveHost();
    MoveController::onDrag(host, worldPos);
}

void DrawingCanvas::finishMoveOperation()
{
    MoveHost host = makeMoveHost();
    MoveController::onFinish(host);
    m_originalPositions.clear();
}

QVector2D DrawingCanvas::snapAngleLineEndpoint(
    const QVector2D &origin, const QVector2D &rawEnd,
    Qt::KeyboardModifiers mods) const
{
    return CanvasGeometry::snapAngleLineEndpoint(
        origin, rawEnd, m_angleBaselineStart, m_angleBaselineEnd, mods);
}

DrawingPrimitive *DrawingCanvas::findPrimitiveById(const QUuid &id) const
{
    return CanvasPrimitiveOps::findById(m_layerManager, m_primitives, id);
}

QVector2D DrawingCanvas::resolveLineMoveConstraint(
    const LinePrimitive *line) const
{
    return LineMoveOps::resolveConstraint(
        line, [this](const QUuid &id) { return findPrimitiveById(id); });
}

QVector2D DrawingCanvas::constrainDeltaAlongConnectedLine(
    const QVector2D &delta, Qt::KeyboardModifiers mods) const
{
    return LineMoveOps::constrainDelta(
        delta, mods, selectedObjects(),
        [this](const QUuid &id) { return findPrimitiveById(id); });
}

QVector2D DrawingCanvas::projectPointOntoLineSegment(const QVector2D &point,
                                                     const QVector2D &a,
                                                     const QVector2D &b) const
{
    return CanvasGeometry::projectPointOntoLineSegment(point, a, b);
}

bool DrawingCanvas::computeCircleThroughPoints(const QVector2D &p1,
                                               const QVector2D &p2,
                                               const QVector2D &p3,
                                               QVector2D &centerOut,
                                               float &radiusOut)
{
    return CanvasGeometry::computeCircleThroughPoints(p1, p2, p3, centerOut,
                                                      radiusOut);
}

void DrawingCanvas::selectObjectsInRect(
    const QRectF &rect, SelectionManager::SelectionOperation operation)
{
    m_selectionManager->selectObjectsInRect(rect, m_primitives, operation);
}

void DrawingCanvas::selectObjectsInLasso(
    const QPolygonF &polygon, SelectionManager::SelectionOperation operation)
{
    m_selectionManager->selectObjectsInLasso(polygon, m_primitives, operation);
}

bool DrawingCanvas::isCopyAlongModifier(Qt::KeyboardModifiers mods)
{
    return LineMoveOps::isCopyAlongModifier(mods);
}

bool DrawingCanvas::beginCopyAlongConnectedLineMove()
{
    if (m_isCopyAlongMove) {
        return true;
    }

    const auto sources = LineMoveOps::collectConstrainedLines(
        selectedObjects(),
        [this](const QUuid &id) { return findPrimitiveById(id); });
    if (sources.empty()) {
        return false;
    }

    clearSelection();
    for (LinePrimitive *src : sources) {
        if (!src) {
            continue;
        }
        auto clone = src->clone();
        if (!clone) {
            continue;
        }
        clone->setSelected(true);
        DrawingPrimitive *raw = clone.get();
        addPrimitiveWithCommand(std::move(clone));
        if (raw) {
            m_selectionManager->addToSelection(raw);
        }
    }

    m_isCopyAlongMove = true;
    emit selectionChanged();
    emit smartHintChanged(
        QStringLiteral("Copying along connected line · release to place"));
    update();
    return true;
}

void DrawingCanvas::startControlPointEdit(DrawingPrimitive *primitive,
                                          int controlPointIndex)
{
    ControlPointEditHost host = makeControlPointEditHost();
    ControlPointEditController::start(host, primitive, controlPointIndex);
}

void DrawingCanvas::updateControlPoint(const QVector2D &newPos)
{
    ControlPointEditHost host = makeControlPointEditHost();
    ControlPointEditController::update(host, newPos,
                                       QApplication::keyboardModifiers());
}

void DrawingCanvas::finishControlPointEdit()
{
    ControlPointEditHost host = makeControlPointEditHost();
    ControlPointEditController::finish(host);
}

void DrawingCanvas::setMagneticConnectionEnabled(bool enabled)
{
    m_magneticSnap.setEnabled(enabled);
}

void DrawingCanvas::setMagneticConnectionTolerance(float tolerance)
{
    m_magneticSnap.setTolerance(tolerance);
}

QVector2D DrawingCanvas::findNearestLineEndpoint(const QVector2D &pos,
                                                 float tolerance)
{
    const auto endpoints =
        CanvasPrimitiveOps::collectLineEndpoints(m_layerManager, m_primitives);
    return MagneticSnap::nearestWithin(pos, tolerance, endpoints);
}

bool DrawingCanvas::pointInPolygon(
    const QVector2D &point, const std::vector<QVector2D> &polygon) const
{
    return CanvasGeometry::pointInPolygon(point, polygon);
}

QString DrawingCanvas::paperFormatName() const
{
    return m_paper.formatName();
}

void DrawingCanvas::setPaperFormat(PaperFormat format)
{
    m_paper.setFormat(format);
    if (m_gridManager) {
        m_gridManager->setPaperSize(m_paper.sizeMm());
    }
    update();
}

DrawingPrimitive *DrawingCanvas::findPrimitiveAt(const QVector2D &pos,
                                                 float tolerance)
{
    return CanvasPrimitiveOps::findAt(m_layerManager, m_primitives, pos,
                                      tolerance);
}

int DrawingCanvas::findControlPointAt(const QVector2D &pos, float tolerance)
{
    if (!m_editingPrimitive || !m_editingPrimitive->isSelected()) {
        return -1;
    }
    return ControlPointHit::firstWithin(m_editingPrimitive->getControlPoints(),
                                        pos, tolerance);
}

QVector2D DrawingCanvas::snapToLineEndpoint(const QVector2D &pos)
{
    const auto endpoints =
        CanvasPrimitiveOps::collectLineEndpoints(m_layerManager, m_primitives);
    return m_magneticSnap.snap(pos, endpoints);
}

QVector2D DrawingCanvas::applySmartDrawingConstraints(
    const QVector2D &rawPos, Qt::KeyboardModifiers mods, QString *hintOut)
{
    return SmartDrawingConstraints::apply(smartToolKind(m_currentTool),
                                          m_drawStartPos, rawPos, mods,
                                          hintOut);
}

void DrawingCanvas::alignSelectedObjects(AlignmentType alignmentType)
{
    const auto &selected = m_selectionManager->selectedObjects();
    if (selected.empty()) {
        return;
    }

    // E13: PaperModel owns world page rect
    const QRectF pageRect =
        m_paper.worldRect(m_unitsConverter.pixelsPerMM());

    ObjectLayoutOps::alignObjects(
        selected,
        static_cast<ObjectLayoutOps::Align>(static_cast<int>(alignmentType)),
        pageRect);
    update();
}

void DrawingCanvas::distributeSelectedObjects(bool horizontal)
{
    ObjectLayoutOps::distributeObjects(m_selectionManager->selectedObjects(),
                                       horizontal);
    update();
}

void DrawingCanvas::updateAlignmentGuides(const QVector2D &mousePos)
{
    if (!m_showAlignmentGuides) {
        m_alignmentGuides.clear();
        m_snapIndicatorActive = false;
        return;
    }

    const std::vector<QRectF> bounds =
        CanvasPrimitiveOps::collectUnselectedBounds(m_layerManager,
                                                    m_primitives);
    const AlignmentGuides::Result result =
        AlignmentGuides::compute(mousePos, m_zoomLevel, bounds);

    m_alignmentGuides = result.guides;
    m_snapIndicatorActive = result.snapActive;
    m_snapIndicatorPos = result.snapPos;
}

bool DrawingCanvas::isObjectAtAlignmentPosition(DrawingPrimitive *obj,
                                                float tolerance)
{
    if (!obj) {
        return false;
    }
    return AlignmentGuides::objectNearGuides(obj->boundingRect(),
                                             m_alignmentGuides, tolerance);
}

QColor DrawingCanvas::getColorAtPosition(const QVector2D &pos)
{
    const QImage image = grab().toImage();
    const QPoint screenPos = worldToScreen(pos);

    if (screenPos.x() >= 0 && screenPos.x() < image.width() &&
        screenPos.y() >= 0 && screenPos.y() < image.height()) {
        return image.pixelColor(screenPos.x(), screenPos.y());
    }

    return QColor(255, 255, 255);
}

void DrawingCanvas::setSplineSelectionMode(bool enabled,
                                           TextPrimitive *textPrim)
{
    m_splineSelectionMode = enabled;
    m_textAwaitingSpline = textPrim;

    if (enabled) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
        m_textAwaitingSpline = nullptr;
    }

    update();
}

void DrawingCanvas::setSplineVisibilityForText(TextPrimitive *textPrim,
                                               bool visible)
{
    if (!textPrim || !textPrim->followsSpline()) {
        return;
    }
    if (DrawingPrimitive *spline = findPrimitiveById(textPrim->splineId())) {
        spline->setVisible(visible);
        update();
    }
}

bool DrawingCanvas::getSplineVisibilityForText(TextPrimitive *textPrim) const
{
    if (!textPrim || !textPrim->followsSpline()) {
        return true;
    }
    const DrawingPrimitive *spline = findPrimitiveById(textPrim->splineId());
    return spline ? spline->isVisible() : true;
}
