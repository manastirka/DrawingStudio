#include "DrawingCanvas.h"

#include "CanvasContextMenu.h"
#include "CanvasCoords.h"
#include "CanvasRenderer.h"
#include "CanvasZoom.h"
#include "AirbrushEngine.h"
#include "GridManager.h"
#include "ImagePrimitive.h"
#include "LayerOrderOps.h"
#include "PaperModel.h"
#include "SelectionManager.h"
#include "ToolCursor.h"

#include <QContextMenuEvent>
#include <QDebug>
#include <QEnterEvent>
#include <QMenu>
#include <QTimer>

// setCurrentTool / add/delete/group → DrawingCanvasDocument.cpp (E13)
// selection/geometry façades → DrawingCanvasFacades.cpp (E13)
// paint → DrawingCanvasPaint.cpp (E12); input → DrawingCanvasInput.cpp (E11)
// hosts → DrawingCanvasHosts.cpp (E1)

DrawingCanvas::DrawingCanvas(QWidget *parent)
    : QWidget(parent), m_viewCenter(0.0f, 0.0f), m_zoomLevel(1.0f),
      m_gridManager(std::make_unique<GridManager>()),
      m_selectionManager(std::make_unique<SelectionManager>()),
      m_selectionMode(SelectionMode::Rectangle),
      m_selectionOperation(SelectionManager::SelectionOperation::Replace),
      m_forceAdditiveSelection(false),
      m_forceSubtractiveSelection(false),
      m_backgroundColor(QColor(255, 255, 255)) // White background
      ,
      m_defaultDrawingColor(QColor(0, 0, 0)) // Black default drawing color
      ,
      m_defaultFillEnabled(false),
      m_defaultFillColor(QColor(0, 0, 0))
      ,
      m_zoomSensitivity(1.09f) // Default zoom sensitivity (1.09 = 9% increase
                               // per scroll, 40% slower than 1.15)
      ,
      m_rulersVisible(true),
      m_currentTool(DrawingTool::Select), m_pixelSnapEnabled(false),
      m_showCursorPreview(false), m_snapIndicatorActive(false),
      m_isDrawing(false), m_isPanning(false), m_bezierCreationStage(0),
      m_fillMode(FillMode::Normal), m_isBrushing(false), m_isBlurring(false),
      m_cursorPreviewPos(0.0f, 0.0f), m_snapIndicatorPos(0.0f, 0.0f),
      m_brushSize(10.0f), m_eraserSize(20.0f), m_brushHardness(0.5f),
      m_airbrushTimer(nullptr), m_airbrushPos(0.0f, 0.0f),
      m_defaultLineStyle(Qt::SolidLine), m_defaultLineWidth(2.0f),
      m_isEditingControlPoints(false), m_selectedControlPoint(-1),
      m_editingPrimitive(nullptr), m_isMoving(false),
      m_moveStartPos(0.0f, 0.0f), m_totalMoveOffset(0.0f, 0.0f),
      m_isResizingObject(false), m_resizingObject(nullptr),
      m_resizeHandleIndex(-1),
      m_isRotatingObject(false), m_rotatingObject(nullptr),
      m_rotationPivot(0.0, 0.0),
      m_isRotatingText(false), m_isResizingText(false),
      m_rotatingTextPrimitive(nullptr), m_resizingTextPrimitive(nullptr),
      m_splineSelectionMode(false), m_textAwaitingSpline(nullptr),
      m_rotationStartAngle(0.0f), m_initialRotation(0.0f),
      m_resizeCornerIndex(-1), m_resizeStartPos(0.0f, 0.0f),
      m_initialScale(1.0f), m_initialFontSize(14), m_contextMenu(nullptr),
      m_project(nullptr), m_layerManager(nullptr),
      m_renderer(std::make_unique<CanvasRenderer>()), m_angleLineStage(0),
      m_angleBaselineStart(0.0f, 0.0f), m_angleBaselineEnd(0.0f, 0.0f),
      m_angleBaselinePrimitiveId(), m_angleBaselineConstraintDir(0.0f, 0.0f),
      m_arcStage(0), m_arcStart(0.0f, 0.0f),
      m_arcEnd(0.0f, 0.0f), m_showAlignmentGuides(true) {
  setFocusPolicy(Qt::WheelFocus);
  setAttribute(Qt::WA_AcceptTouchEvents, false);
  setupContextMenu();
  setMouseTracking(true);

  // Airbrush spray timer (~60 FPS) — pure sim in AirbrushEngine (D1)
  m_airbrushTimer = new QTimer(this);
  m_airbrushTimer->setInterval(16);
  connect(m_airbrushTimer, &QTimer::timeout, this, [this]() {
    if (!m_isBrushing) {
      return;
    }
    AirbrushEngine::Params params;
    params.brushSize = m_brushSize;
    params.hardness = m_brushHardness;
    params.dt = 0.016f;
    if (AirbrushEngine::tick(m_airbrushState, m_airbrushPos, params,
                             m_brushStroke, m_brushParticleScale,
                             m_brushParticleAlpha)) {
      update();
    }
  });

  connect(m_gridManager.get(), &GridManager::gridChanged, this,
          [this]() { update(); });

  connect(m_selectionManager.get(), &SelectionManager::selectionChanged, this,
          &DrawingCanvas::selectionChanged);
  connect(m_selectionManager.get(), &SelectionManager::selectionChanged, this,
          [this]() { update(); });

  // Long-press timer for text box move
  m_longPressTimer = new QTimer(this);
  m_longPressTimer->setSingleShot(true);
  m_longPressTimer->setInterval(500);
  m_longPressText = nullptr;
  m_longPressStartPos = QVector2D(0.0f, 0.0f);

  connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
    if (m_longPressText && m_currentTool == DrawingTool::Select) {
      m_isMoving = true;
      m_moveStartPos = m_longPressStartPos;
      m_totalMoveOffset = QVector2D(0.0f, 0.0f);
      m_originalPositions.clear();
      m_originalPositions.push_back(m_longPressText->position());
      setCursor(Qt::SizeAllCursor);
      qDebug() << "Long press activated - move mode enabled for text box";
    }
  });
}

DrawingCanvas::~DrawingCanvas() {
}

// setupWorldTransform / paintEvent / render* → DrawingCanvasPaint.cpp (E12)
// mouse / wheel / key events → DrawingCanvasInput.cpp (E11)

void DrawingCanvas::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
}

void DrawingCanvas::contextMenuEvent(QContextMenuEvent *event)
{
  qDebug() << "Context menu triggered at:" << event->pos();

  if (m_contextMenu) {
    delete m_contextMenu;
    m_contextMenu = nullptr;
  }
  setupContextMenu();

  if (m_contextMenu) {
    m_contextMenu->exec(event->globalPos());
  } else {
    qDebug() << "Context menu is null!";
  }
}

void DrawingCanvas::enterEvent(QEnterEvent *event)
{
  setFocus(Qt::MouseFocusReason);
  setCursor(ToolCursor::forTool(m_currentTool));
  QWidget::enterEvent(event);
}

void DrawingCanvas::setupContextMenu()
{
  // E9: LayerOrderOps owns z-order; CanvasContextMenu builds the menu
  CanvasContextMenu::Actions actions;
  actions.deleteSelected = [this]() { deleteSelectedPrimitives(); };
  actions.bringToFront = [this]() {
    LayerOrderOps::apply(m_layerManager, selectedObjects(),
                         LayerOrderOps::Op::ToFront);
    update();
  };
  actions.sendToBack = [this]() {
    LayerOrderOps::apply(m_layerManager, selectedObjects(),
                         LayerOrderOps::Op::ToBack);
    update();
  };
  actions.bringForward = [this]() {
    LayerOrderOps::apply(m_layerManager, selectedObjects(),
                         LayerOrderOps::Op::Forward);
    update();
  };
  actions.sendBackward = [this]() {
    LayerOrderOps::apply(m_layerManager, selectedObjects(),
                         LayerOrderOps::Op::Backward);
    update();
  };
  actions.zoomFit = [this]() { zoomFit(); };
  actions.zoomActual = [this]() { zoomActual(); };

  m_contextMenu = CanvasContextMenu::build(
      this, !selectedObjects().empty(), actions);
}

void DrawingCanvas::zoomIn() {
  // E6: CanvasZoom owns factor clamp
  if (CanvasZoom::zoomByFactor(m_zoomLevel, CanvasZoom::kButtonZoomFactor)) {
    qDebug() << "ZoomIn - new level:" << m_zoomLevel;
    emit zoomChanged(m_zoomLevel);
    update();
  }
}

void DrawingCanvas::zoomOut() {
  if (CanvasZoom::zoomByFactor(m_zoomLevel,
                               1.0f / CanvasZoom::kButtonZoomFactor)) {
    qDebug() << "ZoomOut - new level:" << m_zoomLevel;
    emit zoomChanged(m_zoomLevel);
    update();
  }
}

void DrawingCanvas::zoomFit() {
  CanvasZoom::resetView(m_zoomLevel, m_viewCenter);
  emit zoomChanged(m_zoomLevel);
  update();
}

void DrawingCanvas::zoomActual() {
  CanvasZoom::resetView(m_zoomLevel, m_viewCenter);
  emit zoomChanged(m_zoomLevel);
  update();
}

void DrawingCanvas::setGridVisible(bool visible) {
  m_gridManager->setVisible(visible);
}

bool DrawingCanvas::isGridVisible() const { return m_gridManager->isVisible(); }

void DrawingCanvas::setSnapEnabled(bool enabled) {
  m_gridManager->setSnapEnabled(enabled);
}

bool DrawingCanvas::isSnapEnabled() const {
  return m_gridManager->isSnapEnabled();
}

void DrawingCanvas::setGridSize(float size) {
  m_gridManager->setGridSize(size);
}

float DrawingCanvas::gridSize() const { return m_gridManager->gridSize(); }

void DrawingCanvas::setGridColor(const QColor &color) {
  m_gridManager->setGridColor(color);
}

const QColor &DrawingCanvas::gridColor() const {
  return m_gridManager->gridColor();
}

void DrawingCanvas::setBackgroundColor(const QColor &color) {
  m_backgroundColor = color;
  update();
}

void DrawingCanvas::setPaperColor(const QColor &color) {
  m_paper.setColor(color);
  update();
}

void DrawingCanvas::setDefaultDrawingColor(const QColor &color) {
  m_defaultDrawingColor = color;
}

void DrawingCanvas::setZoomSensitivity(float sensitivity) {
  // E13: CanvasZoom owns sensitivity clamp
  m_zoomSensitivity = CanvasZoom::clampSensitivity(sensitivity);
}

void DrawingCanvas::setRulersVisible(bool visible) {
  m_rulersVisible = visible;
  update();
}

void DrawingCanvas::setSelectionMode(SelectionMode mode) {
  m_selectionMode = mode;
}

void DrawingCanvas::setAdditiveSelection(bool enabled) {
  m_forceAdditiveSelection = enabled;
  if (enabled) {
    m_forceSubtractiveSelection = false;
  }
}

void DrawingCanvas::setSubtractiveSelection(bool enabled) {
  m_forceSubtractiveSelection = enabled;
  if (enabled) {
    m_forceAdditiveSelection = false;
  }
}

void DrawingCanvas::setProject(DrawingProject *project) {
  m_project = project;
  update();
}

void DrawingCanvas::setLayerManager(LayerManager *layerManager) {
  m_layerManager = layerManager;
  if (m_selectionManager) {
    m_selectionManager->setLayerManager(layerManager);
  }
}

QVector2D DrawingCanvas::screenToWorld(const QPoint &screenPos) const {
  return CanvasCoords::screenToWorld(screenPos, width(), height(), m_zoomLevel,
                                     m_viewCenter, m_pixelSnapEnabled);
}

QPoint DrawingCanvas::worldToScreen(const QVector2D &worldPos) const {
  return CanvasCoords::worldToScreen(worldPos, width(), height(), m_zoomLevel,
                                     m_viewCenter);
}

QVector2D DrawingCanvas::snapToGrid(const QVector2D &pos) const {
  return m_gridManager->snapToGrid(pos);
}

QImage DrawingCanvas::renderToImage(int w, int h) {
  if (w <= 0)
    w = this->width();
  if (h <= 0)
    h = this->height();

  QImage image = this->grab().toImage();

  if (image.width() != w || image.height() != h) {
    image = image.scaled(w, h, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  }

  qDebug() << "Rendered canvas to image:" << image.width() << "x"
           << image.height();
  return image;
}

QString DrawingCanvas::getUnitsString() const {
  return m_unitsConverter.unitsString();
}

float DrawingCanvas::worldToUnits(float worldDistance) const {
  return m_unitsConverter.worldToUnits(worldDistance);
}

float DrawingCanvas::unitsToWorld(float unitDistance) const {
  return m_unitsConverter.unitsToWorld(unitDistance);
}

double DrawingCanvas::pixelsPerUnit() const {
  return m_unitsConverter.pixelsPerUnit();
}

QImage DrawingCanvas::getImageFromPrimitive(ImagePrimitive *img) {
  if (!img) return QImage();
  return img->image();
}

void DrawingCanvas::updateStatusBar() {
  update();
}

void DrawingCanvas::onAdvancedTextEditorRequested() {
  emit advancedTextEditorRequested();
}
