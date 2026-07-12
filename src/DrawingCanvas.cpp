#include "DrawingCanvas.h"
#include "AdvancedTextEditor.h"
#include "BrushStrokePrimitive.h"
#include "CanvasRenderer.h"
#include "ClassicTextTool.h"
#include "Commands.h"
#include "DrawingPrimitive.h"
#include "DrawingProject.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "MainWindow.h"
#include <QAction>
#include <QApplication>
#include <QVector>
#include <QContextMenuEvent>
#include <QDebug>
#include <QEnterEvent>
#include <QFileDialog>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>
#include <QRandomGenerator>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTimer>
#include <QTransform>
#include <QUuid>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set> // For deleteSelectedPrimitives
#include <vector>

#ifdef HAVE_QT_PDF
#include <QPdfDocument>
#endif

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
      m_paperColor(QColor(255, 255, 255)) // Pure white paper
      ,
      m_defaultDrawingColor(QColor(0, 0, 0)) // Black default drawing color
      ,
      m_defaultFillEnabled(false),
      m_defaultFillColor(QColor(0, 0, 0))
      ,
      m_zoomSensitivity(1.09f) // Default zoom sensitivity (1.09 = 9% increase
                               // per scroll, 40% slower than 1.15)
      ,
      m_paperFormat(PaperFormat::A4) // Default to A4 format
      ,
      m_paperSize(210.0f, 297.0f) // A4 size in mm
      ,
      m_rulersVisible(true), m_magneticConnectionEnabled(false),
      m_magneticConnectionTolerance(DEFAULT_MAGNETIC_TOLERANCE),
      m_currentTool(DrawingTool::Select), m_pixelSnapEnabled(false),
      m_showCursorPreview(false), m_snapIndicatorActive(false),
      m_isDrawing(false), m_isPanning(false), m_bezierCreationStage(0),
      m_fillMode(FillMode::Normal), m_isBrushing(false), m_isBlurring(false),
      m_cursorPreviewPos(0.0f, 0.0f), m_snapIndicatorPos(0.0f, 0.0f),
      m_brushSize(10.0f), m_eraserSize(20.0f), m_brushHardness(0.5f),
      m_airbrushTimer(nullptr), m_airbrushPos(0.0f, 0.0f),
      m_airbrushLastPos(0.0f, 0.0f), m_airbrushSprayLastPos(0.0f, 0.0f),
      m_airbrushStationarySeconds(0.0f),
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
      m_project(nullptr), m_layerManager(nullptr), m_units(Units::Millimeters),
      m_pixelsPerMM(DEFAULT_PIXELS_PER_MM),
      m_renderer(std::make_unique<CanvasRenderer>()), m_angleLineStage(0),
      m_angleBaselineStart(0.0f, 0.0f), m_angleBaselineEnd(0.0f, 0.0f),
      m_angleBaselinePrimitiveId(), m_angleBaselineConstraintDir(0.0f, 0.0f),
      m_arcStage(0), m_arcCenter(0.0f, 0.0f), m_arcStart(0.0f, 0.0f),
      m_arcEnd(0.0f, 0.0f), m_showAlignmentGuides(true) {
  setFocusPolicy(Qt::WheelFocus); // Ensure wheel events are received
  setAttribute(Qt::WA_AcceptTouchEvents,
               false); // Disable touch to avoid conflicts
  setupContextMenu();

  // Airbrush timer (spray while mouse is held down)
  m_airbrushTimer = new QTimer(this);
  m_airbrushTimer->setInterval(16); // ~60 FPS
  connect(m_airbrushTimer, &QTimer::timeout, this, [this]() {
    if (!m_isBrushing)
      return;

    // Spray parameters (tunable)
    const float radius = qMax(2.0f, m_brushSize * 0.65f);
    // Softer airbrush: more, gentler particles.
    const int particlesPerTick =
        qBound(10, static_cast<int>(m_brushSize * 1.8f), 70);
    const float stationaryEps = 0.8f;

    const float moved = (m_airbrushPos - m_airbrushLastPos).length();
    const bool stationary = (moved < stationaryEps);
    if (stationary) {
      m_airbrushStationarySeconds += 0.016f;
    } else {
      m_airbrushStationarySeconds = 0.0f;
      m_airbrushLastPos = m_airbrushPos;
    }

    // Accumulate faster when held still (airbrush builds up).
    const int tickParticles =
        stationary ? qMin(140, particlesPerTick * 2) : particlesPerTick;

    // When the cursor moves, spray along the segment to create a continuous line
    // (like a real airbrush) rather than disconnected circles.
    const float segmentLen = (m_airbrushPos - m_airbrushSprayLastPos).length();
    const float step = qMax(0.8f, radius * 0.22f);
    const int steps = qMax(1, static_cast<int>(std::ceil(segmentLen / step)));

    // Keep total particle budget per tick roughly constant.
    const int perStep = qMax(1, tickParticles / steps);

    for (int s = 1; s <= steps; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(steps);
      const QVector2D center = m_airbrushSprayLastPos + (m_airbrushPos - m_airbrushSprayLastPos) * t;

      for (int i = 0; i < perStep; ++i) {
        // Random polar distribution; hardness controls concentration toward center
        const float u = static_cast<float>(QRandomGenerator::global()->generateDouble());
        const float v = static_cast<float>(QRandomGenerator::global()->generateDouble());
        const float angle = u * 2.0f * static_cast<float>(M_PI);

        // Higher exponent biases particles towards the center (airbrush feel).
        const float exp = 1.15f + (2.2f * qBound(0.0f, m_brushHardness, 1.0f));
        const float r = radius * std::pow(v, exp);

        QVector2D p = center + QVector2D(std::cos(angle) * r, std::sin(angle) * r);
        m_brushStroke.push_back(p);

        // Center-weighted accumulation: stronger in the center, weaker near edges.
        const float nr = qBound(0.0f, r / radius, 1.0f);
        const float sigma = 0.55f;
        const float radial = std::exp(-(nr * nr) / (2.0f * sigma * sigma));

        // Natural look: slight variation in particle size/opacity.
        const float scaleJitter = (0.80f + 0.45f * static_cast<float>(QRandomGenerator::global()->generateDouble())) * (0.85f + 0.25f * radial);
        const float alphaJitter = (0.55f + 0.45f * static_cast<float>(QRandomGenerator::global()->generateDouble())) * radial;
        m_brushParticleScale.push_back(scaleJitter);
        m_brushParticleAlpha.push_back(alphaJitter);
      }
    }

    m_airbrushSprayLastPos = m_airbrushPos;

    // Leak/drip behavior: after holding still for a while, start dripping down.
    const float leakStartSec = 1.3f;
    if (m_airbrushStationarySeconds > leakStartSec) {
      // Spawn a new drip sometimes
      if (QRandomGenerator::global()->bounded(100) < 12) {
        AirbrushDrip drip;
        drip.pos = m_airbrushPos + QVector2D(static_cast<float>(QRandomGenerator::global()->bounded(-2, 3)), 0.0f);
        // Drip should move "down" in the document. In our world coords Y grows up,
        // so use a negative Y velocity.
        drip.vel = QVector2D(0.0f, -(6.0f + static_cast<float>(QRandomGenerator::global()->bounded(0, 9))));
        drip.life = 1.8f + static_cast<float>(QRandomGenerator::global()->bounded(0, 13)) / 10.0f;
        m_airbrushDrips.push_back(drip);
      }

      // Advance drips and leave a trail
      for (auto &drip : m_airbrushDrips) {
        drip.pos += drip.vel * 0.016f;
        drip.vel.setY(drip.vel.y() * 1.02f); // accelerate slightly
        drip.life -= 0.016f;
        m_brushStroke.push_back(drip.pos);
        // Drips are thinner and a bit more opaque than the mist.
        m_brushParticleScale.push_back(0.55f);
        m_brushParticleAlpha.push_back(0.85f);
      }

      // Remove dead drips
      m_airbrushDrips.erase(
          std::remove_if(m_airbrushDrips.begin(), m_airbrushDrips.end(),
                         [](const AirbrushDrip &d) { return d.life <= 0.0f; }),
          m_airbrushDrips.end());
    } else {
      m_airbrushDrips.clear();
    }

    update();
  });

  // Connect grid manager signals
  connect(m_gridManager.get(), &GridManager::gridChanged, this,
          [this]() { update(); });

  // Connect selection manager signals
  connect(m_selectionManager.get(), &SelectionManager::selectionChanged, this,
          &DrawingCanvas::selectionChanged);
  connect(m_selectionManager.get(), &SelectionManager::selectionChanged, this,
          [this]() { update(); });

  // Text editors removed - will be reimplemented with new Text tool

  setMouseTracking(true); // Enable mouse tracking for hover events

  // Initialize long-press timer for text box move
  m_longPressTimer = new QTimer(this);
  m_longPressTimer->setSingleShot(true);
  m_longPressTimer->setInterval(500); // 500ms long press
  m_longPressText = nullptr;
  m_longPressStartPos = QVector2D(0.0f, 0.0f);

  connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
    if (m_longPressText && m_currentTool == DrawingTool::Select) {
      // Activate move mode for this text
      m_isMoving = true;
      m_moveStartPos = m_longPressStartPos;
      m_totalMoveOffset = QVector2D(0.0f, 0.0f);

      // Store original positions for undo
      m_originalPositions.clear();
      m_originalPositions.push_back(m_longPressText->position());

      setCursor(Qt::SizeAllCursor);
      qDebug() << "Long press activated - move mode enabled for text box";
    }
  });
}

DrawingCanvas::~DrawingCanvas() {
}

void DrawingCanvas::setupWorldTransform(QPainter& painter) {
  painter.translate(width() / 2.0, height() / 2.0);  // Center origin
  painter.scale(m_zoomLevel, -m_zoomLevel);            // Zoom + Y-flip
  painter.translate(-m_viewCenter.x(), -m_viewCenter.y()); // Pan
}

void DrawingCanvas::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Clear background
  painter.fillRect(rect(), m_backgroundColor);

  // Set painter for renderer (used by GridManager, SelectionManager, renderPaper)
  m_renderer->setPainter(&painter);

  // Render paper outline first
  renderPaper(painter);

  // Render grid ON TOP of paper (no AA needed for grid/rulers)
  painter.setRenderHint(QPainter::Antialiasing, false);
  renderGrid(painter);

  if (m_rulersVisible) {
    renderRulers(painter);
  }

  painter.setRenderHint(QPainter::Antialiasing, true);
  renderObjects(painter);
  renderSelection(painter);

  // Clear alignment guides when nothing is selected (guides are updated during moves)
  if (selectedObjects().empty()) {
    m_alignmentGuides.clear();
    m_snapIndicatorActive = false;
  }

  renderAlignmentGuides(painter);
  renderTool(painter);
  renderLassoOverlay(painter);
  renderDimensionTexts(painter);
  renderTextPrimitives(painter);
  renderSnapIndicator(painter);
  renderCursorPreview(painter);

  // Render rotation angle indicator if rotating
  if (m_isRotatingText && m_rotatingTextPrimitive) {
    painter.setRenderHint(QPainter::TextAntialiasing);
    float rotationDegrees = m_rotatingTextPrimitive->rotation() * 180.0f / M_PI;
    QRectF bounds = m_rotatingTextPrimitive->boundingRect();
    float centerX = (bounds.left() + bounds.right()) / 2.0f;
    float rotateHandleY = bounds.top() - 20.0f;
    QPoint screenPos = worldToScreen(QVector2D(centerX, rotateHandleY - 15.0f));
    QString angleText = QString("%1°").arg(rotationDegrees, 0, 'f', 1);
    QFont font("Arial", 12, QFont::Bold);
    painter.setFont(font);
    QFontMetrics metrics(font);
    QRect textRect = metrics.boundingRect(angleText);
    QRect bgRect = textRect;
    bgRect.moveCenter(screenPos);
    bgRect.adjust(-5, -3, 5, 3);
    painter.fillRect(bgRect, QColor(0, 0, 0, 180));
    painter.setPen(QColor(100, 255, 100));
    painter.drawText(bgRect, Qt::AlignCenter, angleText);
  }

  if (m_rulersVisible) {
    renderRulerTexts(painter);
  }

  m_renderer->setPainter(nullptr);
}

void DrawingCanvas::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
}

void DrawingCanvas::mousePressEvent(QMouseEvent *event) {
  m_lastMousePos = event->pos();
  QVector2D worldPos = screenToWorld(event->pos());

  emit coordinatesChanged(worldPos);

  // Handle spline selection mode
  if (m_splineSelectionMode && event->button() == Qt::LeftButton && m_textAwaitingSpline) {
    DrawingPrimitive *clickedPrim = nullptr;
    float minDist = 10.0f / m_zoomLevel;

    // Helper lambda to check primitives
    auto checkPrimitive = [&](DrawingPrimitive *prim) {
      if (auto spline = dynamic_cast<SplinePrimitive *>(prim)) {
        if (spline->containsPoint(worldPos, minDist)) {
          clickedPrim = spline;
        }
      }
    };

    if (m_layerManager) {
      const auto &layers = m_layerManager->layers();
      for (const auto &layer : layers) {
        for (const auto &prim : layer->primitives()) {
          checkPrimitive(prim.get());
          if (clickedPrim) break;
        }
        if (clickedPrim) break;
      }
    } else {
      for (const auto &prim : m_primitives) {
        checkPrimitive(prim.get());
        if (clickedPrim) break;
      }
    }

    if (clickedPrim) {
      m_textAwaitingSpline->setFollowsSpline(true);
      m_textAwaitingSpline->setSplineId(clickedPrim->id());
      setSplineSelectionMode(false);
      emit selectionChanged();
      update();
    }
    return;
  }

  // Handle pipette
  if (m_selectionManager->isPipetteMode() && event->button() == Qt::LeftButton) {
    QColor pickedColor = getColorAtPosition(worldPos);
    m_selectionManager->setPipetteMode(false);
    setCursor(Qt::ArrowCursor);
    selectByColor(pickedColor, m_selectionManager->pipetteTolerance());
    return;
  }

  // Panning
  if (event->button() == Qt::MiddleButton) {
    m_isPanning = true;
    setCursor(Qt::SizeAllCursor);
    return;
  }

  switch (m_currentTool) {
  case DrawingTool::Select: handleSelectTool(event); break;
  case DrawingTool::Brush: {
    m_isBrushing = true;
    m_brushStroke.clear();
    m_brushParticleScale.clear();
    m_brushParticleAlpha.clear();
    m_airbrushPos = screenToWorld(event->pos());
    if (m_airbrushTimer) m_airbrushTimer->start();
    break;
  }
  case DrawingTool::Blur:
    m_isBlurring = true;
    m_brushStroke.clear();
    m_brushParticleScale.clear();
    m_brushParticleAlpha.clear();
    m_brushStroke.push_back(screenToWorld(event->pos()));
    break;
  case DrawingTool::Move: handleMoveTool(event); break;
  case DrawingTool::Line: handleLineTool(event); break;
  case DrawingTool::Curve: handleCurveTool(event); break;
  case DrawingTool::BezierCurve: handleBezierTool(event); break;
  case DrawingTool::Spline: handleSplineTool(event); break;
  case DrawingTool::Polygon: handlePolygonTool(event); break;
  case DrawingTool::Arc: handleArcTool(event); break;
  case DrawingTool::Circle: handleCircleTool(event); break;
  case DrawingTool::AngleLine: handleAngleLineTool(event); break;
  case DrawingTool::Rectangle: handleRectangleTool(event); break;
  case DrawingTool::Ellipse: handleEllipseTool(event); break;
  case DrawingTool::Eraser: handleEraserTool(event); break;
  case DrawingTool::Fill: handleFillTool(event); break;
  case DrawingTool::Measure: handleMeasureTool(event); break;
  case DrawingTool::Image: handleImageTool(event); break;
  case DrawingTool::Text: handleTextTool(event); break;
  default: break;
  }

  emit coordinatesChanged(worldPos);
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent *event) {
  QVector2D worldPos = screenToWorld(event->pos());
  emit coordinatesChanged(worldPos);

  if (m_showCursorPreview) {
    m_cursorPreviewPos = worldPos;
  }

  if (m_isEditingControlPoints) {
    updateControlPoint(worldPos);
    return;
  }

  if (m_isBrushing) {
    m_airbrushPos = worldPos;
    update();
    return;
  }
  if (m_isBlurring) {
    m_brushStroke.push_back(worldPos);
    update();
    return;
  }
  if (m_isPanning) {
    QPoint delta = event->pos() - m_lastMousePos;
    QVector2D worldDelta(-delta.x() / m_zoomLevel, delta.y() / m_zoomLevel);
    m_viewCenter += worldDelta;
    m_lastMousePos = event->pos();
    update();
    return;
  }
  if (m_selectionManager->isSelecting()) {
    if (m_selectionMode == SelectionMode::Lasso) {
      if (m_lassoPoints.isEmpty() ||
          QLineF(m_lassoPoints.last(), worldPos.toPointF()).length() >
              (2.0f / m_zoomLevel)) {
        m_lassoPoints << worldPos.toPointF();
      }
    } else {
      m_selectionManager->setSelectionRect(
          QRectF(m_drawStartPos.toPointF(), worldPos.toPointF()).normalized());
    }
    update();
    return;
  }
  if (m_isResizingObject && m_resizingObject) {
    QRectF orig = m_resizeOrigBounds;
    Qt::KeyboardModifiers mods = event->modifiers();
    if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(m_resizingObject)) {
      if (imgPrim->maintainAspectRatio())
        mods |= Qt::ShiftModifier;
    }
    const QVector2D localPos = toObjectLocal(m_resizingObject, worldPos);
    QRectF nr = computeResizedBounds(orig, m_resizeHandleIndex, localPos, mods);
    if (nr.width() > 1.0f && nr.height() > 1.0f) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(m_resizingObject)) {
        imgPrim->setPosition(QVector2D(nr.x(), nr.y()));
        imgPrim->setSize(QVector2D(nr.width(), nr.height()));
      } else if (auto *textPrim = dynamic_cast<TextPrimitive *>(m_resizingObject)) {
        textPrim->setPosition(QVector2D(nr.left(), nr.bottom()));
        float scaleSafe = std::max(0.001f, textPrim->scale());
        textPrim->setTextBoxWidth(nr.width() / scaleSafe);
        textPrim->setTextBoxHeight(nr.height() / scaleSafe);
      } else {
        float sx = nr.width() / orig.width();
        float sy = nr.height() / orig.height();
        const auto &baseCps = (!m_resizeOrigControlPoints.empty())
                                  ? m_resizeOrigControlPoints
                                  : m_resizingObject->getControlPoints();
        for (int i = 0; i < (int)baseCps.size(); ++i) {
          float nx = nr.x() + (baseCps[i].x() - orig.x()) * sx;
          float ny = nr.y() + (baseCps[i].y() - orig.y()) * sy;
          m_resizingObject->setControlPointPosition(i, QVector2D(nx, ny));
        }
      }
      update();
    }
    return;
  }
  if (m_isRotatingObject && m_rotatingObject) {
    const QVector2D pivot(m_rotationPivot);
    const QVector2D delta = worldPos - pivot;
    if (delta.length() > 0.001f) {
      float angle = std::atan2(delta.y(), delta.x()) * 180.0f /
                    static_cast<float>(M_PI);
      // Convert from math angle to UI rotation delta
      float newRot = m_initialRotation + (angle - m_rotationStartAngle);
      if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        // Snap to 15°
        newRot = std::round(newRot / 15.0f) * 15.0f;
      }
      applyObjectRotation(m_rotatingObject, newRot);
      update();
    }
    return;
  }
  if (m_isMoving) {
    handleMoveOperation(worldPos);
    return;
  }

  // Forward mouse move to text tool
  if (m_currentTool == DrawingTool::Text) {
    handleTextTool(event);
    return;
  }

  if (m_isDrawing) {
    m_drawCurrentPos = snapToGrid(worldPos);

    QString smartHint;
    m_drawCurrentPos = applySmartDrawingConstraints(
        m_drawCurrentPos, event->modifiers(), &smartHint);
    if (smartHint != m_lastSmartHint) {
      m_lastSmartHint = smartHint;
      emit smartHintChanged(smartHint);
    }

    // Angle Line baseline drag (no primitive yet)
    if (m_currentTool == DrawingTool::AngleLine && m_angleLineStage == 1) {
      m_angleBaselineEnd = m_drawCurrentPos;
      update();
      return;
    }

    // Update the current primitive's geometry during drag
    if (m_currentPrimitive) {
      switch (m_currentTool) {
        case DrawingTool::Line: {
          if (auto *line = dynamic_cast<LinePrimitive *>(m_currentPrimitive.get())) {
            QVector2D snappedEnd = snapToLineEndpoint(m_drawCurrentPos);
            line->setEndPoint(snappedEnd);
          }
          break;
        }
        case DrawingTool::Measure: {
          if (auto *dim = dynamic_cast<DimensionPrimitive *>(m_currentPrimitive.get())) {
            QVector2D snappedEnd = snapToLineEndpoint(m_drawCurrentPos);
            dim->setEndPoint(snappedEnd);
            dim->setUnitsString(getUnitsString());
            dim->setPixelsPerUnit(static_cast<float>(pixelsPerUnit()));
            dim->recalculateMeasurement();
            emit smartHintChanged(
                QStringLiteral("Measuring: %1").arg(dim->getDisplayText()));
          }
          break;
        }
        case DrawingTool::Rectangle: {
          if (auto *rect = dynamic_cast<RectanglePrimitive *>(m_currentPrimitive.get())) {
            rect->setBottomRight(m_drawCurrentPos);
          }
          break;
        }
        case DrawingTool::Circle: {
          if (auto *circle = dynamic_cast<CirclePrimitive *>(m_currentPrimitive.get())) {
            float radius = (m_drawCurrentPos - m_drawStartPos).length();
            circle->setRadius(radius);
          }
          break;
        }
        case DrawingTool::Ellipse: {
          if (auto *ellipse = dynamic_cast<EllipsePrimitive *>(m_currentPrimitive.get())) {
            QVector2D center = (m_drawStartPos + m_drawCurrentPos) * 0.5f;
            float rx = std::abs(m_drawCurrentPos.x() - m_drawStartPos.x()) * 0.5f;
            float ry = std::abs(m_drawCurrentPos.y() - m_drawStartPos.y()) * 0.5f;
            ellipse->setCenter(center);
            ellipse->setRadiusX(rx);
            ellipse->setRadiusY(ry);
          }
          break;
        }
        case DrawingTool::AngleLine: {
          if (m_angleLineStage == 1) {
            // Live baseline preview endpoint
            m_angleBaselineEnd = m_drawCurrentPos;
          } else if (m_angleLineStage == 2) {
            if (auto *line =
                    dynamic_cast<LinePrimitive *>(m_currentPrimitive.get())) {
              QVector2D rawEnd = snapToLineEndpoint(m_drawCurrentPos);
              QVector2D snapped = snapAngleLineEndpoint(
                  m_angleBaselineEnd, rawEnd, event->modifiers());
              line->setStartPoint(m_angleBaselineEnd);
              line->setEndPoint(snapped);

              QVector2D delta = snapped - m_angleBaselineEnd;
              float absDeg = std::atan2(delta.y(), delta.x()) * 180.0f /
                             static_cast<float>(M_PI);
              QVector2D base = m_angleBaselineEnd - m_angleBaselineStart;
              float baseDeg = std::atan2(base.y(), base.x()) * 180.0f /
                              static_cast<float>(M_PI);
              float rel = absDeg - baseDeg;
              while (rel > 180.0f)
                rel -= 360.0f;
              while (rel < -180.0f)
                rel += 360.0f;
              emit smartHintChanged(
                  QStringLiteral("Angle: %1° from baseline (%2° absolute) · "
                                 "Shift=5° · Alt=free")
                      .arg(rel, 0, 'f', 0)
                      .arg(absDeg, 0, 'f', 0));
            }
          }
          break;
        }
        case DrawingTool::Arc: {
          if (m_arcStage == 2) {
            if (auto *arc = dynamic_cast<ArcPrimitive *>(m_currentPrimitive.get())) {
              // Compute arc through start, end, and current mouse position
              QVector2D arcMid = m_drawCurrentPos;
              QVector2D arcCenter;
              float arcRadius;
              if (computeCircleThroughPoints(m_arcStart, m_arcEnd, arcMid, arcCenter, arcRadius)) {
                arc->setCenter(arcCenter);
                arc->setRadius(arcRadius);
                float startAngle = std::atan2(m_arcStart.y() - arcCenter.y(), m_arcStart.x() - arcCenter.x()) * 180.0f / M_PI;
                float endAngle = std::atan2(m_arcEnd.y() - arcCenter.y(), m_arcEnd.x() - arcCenter.x()) * 180.0f / M_PI;
                arc->setStartAngle(startAngle);
                arc->setEndAngle(endAngle);
              }
            }
          }
          break;
        }
        case DrawingTool::BezierCurve: {
          if (m_bezierCreationStage == 1) {
            if (auto *bezier = dynamic_cast<BezierCurvePrimitive *>(m_currentPrimitive.get())) {
              // Allow snapping to nearby line endpoints for chaining
              QVector2D snappedEnd = snapToLineEndpoint(m_drawCurrentPos);
              bezier->setEndPoint(snappedEnd);
              // Set control points along the segment for a smooth preview
              QVector2D dir = (snappedEnd - m_drawStartPos);
              bezier->setControlPoint1(m_drawStartPos + dir * 0.33f);
              bezier->setControlPoint2(m_drawStartPos + dir * 0.67f);
            }
          }
          break;
        }
        default:
          break;
      }
    }

    update();
  }

  if (m_showCursorPreview) {
    update();
  }

  // Hover cursor: show resize/rotate cursors over selection handles
  if (m_currentTool == DrawingTool::Select && !m_isMoving && !m_isResizingObject &&
      !m_isRotatingObject && !m_isEditingControlPoints &&
      !m_selectionManager->isSelecting()) {
    bool onHandle = false;
    for (auto *obj : selectedObjects()) {
      QRectF br = obj->boundingRect();
      int hit = hitTestSelectionHandle(br, toObjectLocal(obj, worldPos),
                                       supportsRotationHandle(obj));
      if (hit >= 0) {
        setCursor(cursorForSelectionHandle(hit));
        onHandle = true;
        break;
      }
    }
    if (!onHandle) {
      // Check if hovering over a selected object (move cursor)
      bool onSelected = false;
      for (auto *obj : selectedObjects()) {
        if (obj->containsPoint(toObjectLocal(obj, worldPos),
                               5.0f / m_zoomLevel)) {
          setCursor(Qt::SizeAllCursor);
          onSelected = true;
          break;
        }
      }
      if (!onSelected) {
        setCursor(Qt::ArrowCursor);
      }
    }
  }

  m_lastMousePos = event->pos();
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (m_longPressTimer->isActive()) {
    m_longPressTimer->stop();
    m_longPressText = nullptr;
  }

  if ((m_isBrushing || m_isBlurring) && event->button() == Qt::LeftButton) {
    if (m_airbrushTimer) m_airbrushTimer->stop();
    if (m_brushStroke.size() > 1) {
      auto brushStroke = std::make_unique<BrushStrokePrimitive>();
      if (m_isBrushing && !m_brushParticleScale.empty()) {
        brushStroke->setParticles(m_brushStroke, m_brushParticleScale, m_brushParticleAlpha);
      } else {
        brushStroke->setPoints(m_brushStroke);
      }
      brushStroke->setBrushSize(m_brushSize);
      brushStroke->setHardness(m_brushHardness);
      brushStroke->setColor(m_defaultDrawingColor);
      brushStroke->setStrokeType(m_isBlurring ? BrushStrokePrimitive::StrokeType::Blur : BrushStrokePrimitive::StrokeType::Brush);
      addPrimitiveWithCommand(std::move(brushStroke));
    }
    m_brushStroke.clear();
    m_brushParticleScale.clear();
    m_brushParticleAlpha.clear();
    m_airbrushDrips.clear();
    m_isBrushing = false;
    m_isBlurring = false;
    update();
    return;
  }

  if (m_isEditingControlPoints && event->button() == Qt::LeftButton) {
    finishControlPointEdit();
    return;
  }

  if (m_isResizingObject && event->button() == Qt::LeftButton) {
    if (m_resizingObject && !m_resizeOrigState.isEmpty()) {
      QJsonObject newState = m_resizingObject->toJson();
      if (newState != m_resizeOrigState) {
        auto command = std::make_unique<TransformPrimitivesCommand>(
            std::vector<DrawingPrimitive *>{m_resizingObject},
            TransformPrimitivesCommand::Resize, QStringLiteral("Resize"));
        command->storeTransformation({m_resizeOrigState}, {newState});
        emit commandRequested(command.release());
      }
    }
    m_isResizingObject = false;
    m_resizingObject = nullptr;
    m_resizeHandleIndex = -1;
    m_resizeOrigControlPoints.clear();
    m_resizeOrigState = QJsonObject();
    setCursor(Qt::ArrowCursor);
    emit selectionChanged();
    update();
    return;
  }

  if (m_isRotatingObject && event->button() == Qt::LeftButton) {
    if (m_rotatingObject && !m_rotateOrigState.isEmpty()) {
      QJsonObject newState = m_rotatingObject->toJson();
      if (newState != m_rotateOrigState) {
        auto command = std::make_unique<TransformPrimitivesCommand>(
            std::vector<DrawingPrimitive *>{m_rotatingObject},
            TransformPrimitivesCommand::Rotate, QStringLiteral("Rotate"));
        command->storeTransformation({m_rotateOrigState}, {newState});
        emit commandRequested(command.release());
      }
    }
    m_isRotatingObject = false;
    m_rotatingObject = nullptr;
    m_rotateOrigState = QJsonObject();
    setCursor(Qt::ArrowCursor);
    emit selectionChanged();
    update();
    return;
  }

  if (m_isRotatingText && event->button() == Qt::LeftButton) {
    m_isRotatingText = false;
    m_rotatingTextPrimitive = nullptr;
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (m_isResizingText && event->button() == Qt::LeftButton) {
    if (m_resizingTextPrimitive) {
      QVector2D finalPos = m_resizingTextPrimitive->position();
      float finalWidth = m_resizingTextPrimitive->textBoxWidth();
      float finalHeight = m_resizingTextPrimitive->textBoxHeight();
      float finalFontSize = m_resizingTextPrimitive->fontSize();
      if (finalPos.x() != m_initialTextBounds.x() || finalWidth != m_initialTextBounds.width()) {
        auto command = std::make_unique<ResizeTextCommand>(m_resizingTextPrimitive);
        command->storeOldState(QVector2D(m_initialTextBounds.x(), m_initialTextBounds.y()), m_initialTextBounds.width(), m_initialTextBounds.height(), m_initialFontSize);
        command->storeNewState(finalPos, finalWidth, finalHeight, finalFontSize);
        emit commandRequested(command.release());
      }
    }
    m_isResizingText = false;
    m_resizingTextPrimitive = nullptr;
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (m_isMoving && event->button() == Qt::LeftButton) {
    finishMoveOperation();
    return;
  }

  if (m_currentTool == DrawingTool::Text && event->button() == Qt::LeftButton) {
    handleTextTool(event);
    return;
  }

  if (event->button() == Qt::MiddleButton && m_isPanning) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (m_selectionManager->isSelecting() && event->button() == Qt::LeftButton) {
    if (m_selectionMode == SelectionMode::Lasso) {
      selectObjectsInLasso(m_lassoPoints, m_selectionOperation);
      m_lassoPoints.clear();
    } else {
      selectObjectsInRect(m_selectionManager->selectionRect(),
                          m_selectionOperation);
    }
    m_selectionManager->setIsSelecting(false);
    m_selectionOperation = SelectionManager::SelectionOperation::Replace;
    update();
    return;
  }

  if (m_isDrawing && event->button() == Qt::LeftButton) {
    // Angle Line stage 1: lock baseline on release and commit as construction line
    if (m_currentTool == DrawingTool::AngleLine && m_angleLineStage == 1) {
      m_angleBaselineEnd = snapToGrid(m_drawCurrentPos);
      const float baseLen =
          (m_angleBaselineEnd - m_angleBaselineStart).length();
      if (baseLen >= 2.0f) {
        auto baseline = std::make_unique<LinePrimitive>(m_angleBaselineStart,
                                                        m_angleBaselineEnd);
        baseline->setColor(QColor(59, 130, 246, 160));
        baseline->setLineStyle(Qt::DashLine);
        baseline->setLineWidth(1.0f);
        m_angleBaselinePrimitiveId = baseline->id();
        QVector2D baseDir = m_angleBaselineEnd - m_angleBaselineStart;
        if (baseDir.length() > 0.001f)
          baseDir.normalize();
        m_angleBaselineConstraintDir = baseDir;
        addPrimitiveWithCommand(std::move(baseline));

        m_angleLineStage = 2;
        m_isDrawing = false;
        m_currentPrimitive.reset();
        emit smartHintChanged(
            QStringLiteral("Baseline set — click and drag the angled line "
                           "from the end point"));
      } else {
        m_angleLineStage = 0;
        m_isDrawing = false;
        emit smartHintChanged(
            QStringLiteral("Baseline too short — drag again"));
      }
      update();
      return;
    }

    if (m_currentPrimitive) {
        bool shouldAdd = false;
        
        switch (m_currentTool) {
            case DrawingTool::Line: {
                if (auto *line = dynamic_cast<LinePrimitive *>(m_currentPrimitive.get())) {
                    const float len = (line->endPoint() - line->startPoint()).length();
                    shouldAdd = len >= 2.0f;
                }
                break;
            }
            case DrawingTool::Measure: {
                if (auto *dim = dynamic_cast<DimensionPrimitive *>(m_currentPrimitive.get())) {
                    dim->setEndPoint(snapToLineEndpoint(m_drawCurrentPos));
                    dim->setUnitsString(getUnitsString());
                    dim->setPixelsPerUnit(static_cast<float>(pixelsPerUnit()));
                    dim->recalculateMeasurement();
                    const float len = (dim->endPoint() - dim->startPoint()).length();
                    shouldAdd = len >= 2.0f;
                    if (!shouldAdd) {
                        m_currentPrimitive.reset();
                        m_isDrawing = false;
                    }
                }
                break;
            }
            case DrawingTool::Rectangle: {
                if (auto *r = dynamic_cast<RectanglePrimitive *>(m_currentPrimitive.get())) {
                    const auto cps = r->getControlPoints();
                    if (cps.size() >= 2) {
                        const float w = std::abs(cps[1].x() - cps[0].x());
                        const float h = std::abs(cps[2].y() - cps[0].y());
                        shouldAdd = (w >= 2.0f && h >= 2.0f);
                    }
                }
                break;
            }
            case DrawingTool::Ellipse: {
                if (auto *e = dynamic_cast<EllipsePrimitive *>(m_currentPrimitive.get())) {
                    const auto cps = e->getControlPoints();
                    if (cps.size() >= 5) {
                        const float rx = std::abs(cps[2].x() - cps[1].x()) * 0.5f;
                        const float ry = std::abs(cps[4].y() - cps[3].y()) * 0.5f;
                        shouldAdd = (rx >= 1.0f && ry >= 1.0f);
                    }
                }
                break;
            }
            case DrawingTool::Circle: {
                if (auto *c = dynamic_cast<CirclePrimitive *>(m_currentPrimitive.get())) {
                    // Use control points to derive radius (any edge from center)
                    const auto cps = c->getControlPoints();
                    if (cps.size() >= 2) {
                        const float r = (cps[1] - cps[0]).length();
                        shouldAdd = r >= 1.0f;
                    }
                }
                break;
            }
            case DrawingTool::BezierCurve: {
                if (auto *b = dynamic_cast<BezierCurvePrimitive *>(m_currentPrimitive.get())) {
                    if (m_bezierCreationStage == 1) {
                        // Only finalize if curve has non-trivial length
                        const auto &cps = b->controlPoints();
                        const float endDist = (cps.size() >= 4)
                            ? (cps.front() - cps.back()).length()
                            : 0.0f;
                        const float minLen = 2.0f; // ignore tiny clicks
                        if (endDist >= minLen) {
                            m_bezierCreationStage = 0;
                            m_drawStartPos = cps.back();
                            shouldAdd = true;
                            // Keep selected after creation for easier editing
                            m_currentPrimitive->setSelected(true);
                        } else {
                            // Discard trivial curve
                            m_currentPrimitive.reset();
                            m_isDrawing = false;
                            m_bezierCreationStage = 0;
                        }
                    }
                }
                break;
            }
            case DrawingTool::AngleLine: {
                // Stage 2 release: commit the angled segment
                if (m_angleLineStage == 2) {
                    if (auto *line = dynamic_cast<LinePrimitive *>(
                            m_currentPrimitive.get())) {
                        QVector2D end = snapAngleLineEndpoint(
                            m_angleBaselineEnd, line->endPoint(),
                            event->modifiers());
                        line->setStartPoint(m_angleBaselineEnd);
                        line->setEndPoint(end);
                        if (!m_angleBaselinePrimitiveId.isNull())
                          line->setConnectedLineId(m_angleBaselinePrimitiveId);
                        if (m_angleBaselineConstraintDir.lengthSquared() > 1e-8f)
                          line->setMoveConstraintDirection(
                              m_angleBaselineConstraintDir);
                        const float len =
                            (line->endPoint() - line->startPoint()).length();
                        shouldAdd = len >= 2.0f;
                    }
                    if (shouldAdd) {
                        // Keep baseline armed for another angled segment
                        emit smartHintChanged(
                            QStringLiteral("Line added — drag another angled "
                                           "segment, or right-click to reset"));
                    } else {
                        m_currentPrimitive.reset();
                        m_isDrawing = false;
                        update();
                        return;
                    }
                }
                break;
            }
            case DrawingTool::Arc: {
                if (m_arcStage == 2) {
                    m_arcStage = 0;
                    if (auto *arc = dynamic_cast<ArcPrimitive *>(m_currentPrimitive.get())) {
                        // Require non-trivial radius
                        const float r = arc->radius();
                        shouldAdd = r >= 1.0f;
                    }
                }
                break;
            }
            default:
                break;
        }

        if (shouldAdd) {
            // Keep measure dimensions in their tool color; other tools use default stroke.
            if (m_currentTool != DrawingTool::Measure) {
                m_currentPrimitive->setColor(m_defaultDrawingColor);
            }
            // Keep new objects selected for easier follow-up edits
            m_currentPrimitive->setSelected(true);
            addPrimitiveWithCommand(std::move(m_currentPrimitive));
            m_isDrawing = false;
        }
    }
    if (!m_isDrawing && !m_lastSmartHint.isEmpty()) {
      m_lastSmartHint.clear();
      emit smartHintChanged(QString());
    }
    update();
  }
  
  m_lastMousePos = event->pos();
}

void DrawingCanvas::wheelEvent(QWheelEvent *event) {
  const float zoomFactor = m_zoomSensitivity;
  const float minZoom = 0.05f;
  const float maxZoom = 20.0f;

  float oldZoom = m_zoomLevel;

  // Get scroll delta - handle both pixel and angle deltas
  QPoint numPixels = event->pixelDelta();
  QPoint numDegrees = event->angleDelta() / 8;

  float deltaY = 0;

  // Use pixel delta for trackpad (more precise)
  if (!numPixels.isNull() && numPixels.y() != 0) {
    deltaY = numPixels.y();
  }
  // Use angle delta for mouse wheel
  else if (!numDegrees.isNull() && numDegrees.y() != 0) {
    deltaY = numDegrees.y();
  }
  // Try raw angle delta for very small movements
  else if (!event->angleDelta().isNull() && event->angleDelta().y() != 0) {
    deltaY = event->angleDelta().y() / 120.0f * 15.0f;
  }

  // Skip if no meaningful scroll
  if (std::abs(deltaY) < 0.1f) {
    event->accept();
    return;
  }

  qDebug() << "Wheel event - deltaY:" << deltaY << "pixelDelta:" << numPixels
           << "angleDelta:" << event->angleDelta();

  // Apply zoom based on delta direction
  if (deltaY > 0) {
    m_zoomLevel = std::min(maxZoom, m_zoomLevel * zoomFactor);
    qDebug() << "Zooming IN, new level:" << m_zoomLevel;
  } else if (deltaY < 0) {
    m_zoomLevel = std::max(minZoom, m_zoomLevel / zoomFactor);
    qDebug() << "Zooming OUT, new level:" << m_zoomLevel;
  }

  // Keep zoom centered on mouse cursor
  QVector2D mousePos =
      QVector2D(event->position().toPoint().x() - width() / 2.0f,
                height() / 2.0f - event->position().toPoint().y());

  // Adjust view center to keep the point under mouse stable
  QVector2D worldDelta = mousePos * (1.0f / oldZoom - 1.0f / m_zoomLevel);
  m_viewCenter += worldDelta;
  update();

  emit zoomChanged(m_zoomLevel);
  update();

  event->accept();
}

void DrawingCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
  // Forward double-click to text tool first
  if (event->button() == Qt::LeftButton && m_currentTool == DrawingTool::Text) {
    handleTextTool(event);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    QVector2D worldPos = screenToWorld(event->pos());

    // Check if we double-clicked on a text primitive (works with any tool)
    if (m_layerManager) {
      const auto &layers = m_layerManager->layers();

      // Iterate in reverse order to check top layers first
      for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        const auto &layer = *it;
        if (!layer->isVisible()) continue;

        const auto &primitives = layer->primitives();

        // Iterate in reverse to check top primitives first
        for (auto primIt = primitives.rbegin(); primIt != primitives.rend(); ++primIt) {
          const auto &primitive = *primIt;

          if (auto textPrim = dynamic_cast<TextPrimitive *>(primitive.get())) {
            bool isNearText = false;

            // If text follows a spline, check if click is near the spline
            if (textPrim->followsSpline()) {
              // Find the spline (simplified check for now)
               if (textPrim->containsPoint(worldPos, 30.0f)) {
                  isNearText = true;
               }
            } else {
              // Normal text - use bounding rect
              isNearText = textPrim->containsPoint(worldPos, 20.0f);
            }

            if (isNearText) {
              // Double-click on text with any tool: route into the classic
              // text tool for in-place editing.
              MainWindow *mainWindow = qobject_cast<MainWindow *>(window());
              ClassicTextTool *textTool =
                  mainWindow ? mainWindow->getClassicTextTool() : nullptr;
              if (textTool) {
                // Route through MainWindow::textTool() so the toolbar
                // checked state and tool properties stay in sync.
                QMetaObject::invokeMethod(mainWindow, "textTool");
                textTool->selectText(const_cast<TextPrimitive *>(textPrim));
                textTool->startEditing(const_cast<TextPrimitive *>(textPrim));
                update();
                event->accept();
                return;
              }

              // Fallback: just select and show properties.
              clearSelection();
              const_cast<TextPrimitive *>(textPrim)->setSelected(true);
              m_selectionManager->addToSelection(const_cast<TextPrimitive *>(textPrim));
              emit selectionChanged();

              qDebug() << "Double-clicked text - selected for editing";
              event->accept();
              return;
            }
          }
        }
      }
    }
  }
}

void DrawingCanvas::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Space:
    if (!m_isPanning) {
      m_isPanning = true;
      setCursor(Qt::SizeAllCursor);
    }
    break;
  case Qt::Key_Escape:
    m_isDrawing = false;
    m_selectionManager->setIsSelecting(false);
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
    clearSelection();
    update();
    break;
  case Qt::Key_Delete:
  case Qt::Key_Backspace:
    deleteSelectedPrimitivesWithCommand();
    break;
  case Qt::Key_Plus:
  case Qt::Key_Equal: // For US keyboard layout
    zoomIn();
    break;
  case Qt::Key_Minus:
  case Qt::Key_Underscore:
    zoomOut();
    break;
  case Qt::Key_0:
    if (event->modifiers() & Qt::ControlModifier) {
      zoomFit();
    }
    break;
  case Qt::Key_BracketLeft:
    emit maskNavigationRequested(-1);
    event->accept();
    break;
  case Qt::Key_BracketRight:
    emit maskNavigationRequested(1);
    event->accept();
    break;
  case Qt::Key_I:
    if (event->modifiers() & Qt::ControlModifier) {
      emit maskInvertRequested();
      event->accept();
      break;
    }
    Q_FALLTHROUGH();
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

void DrawingCanvas::keyReleaseEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Space:
    if (m_isPanning) {
      m_isPanning = false;
      setCursor(Qt::ArrowCursor);
    }
    break;
  default:
    QWidget::keyReleaseEvent(event);
    break;
  }
}

void DrawingCanvas::contextMenuEvent(QContextMenuEvent *event) {
  qDebug() << "Context menu triggered at:" << event->pos();

  // Regenerate context menu each time to check for selected primitives
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

void DrawingCanvas::enterEvent(QEnterEvent *event) {
  // Ensure the widget has focus when mouse enters to receive wheel events
  setFocus(Qt::MouseFocusReason);

  // Set appropriate cursor when entering canvas
  switch (m_currentTool) {
  case DrawingTool::Select:
    setCursor(Qt::ArrowCursor);
    break;
  case DrawingTool::Move:
    setCursor(Qt::SizeAllCursor);
    break;
  case DrawingTool::Text:
    setCursor(Qt::IBeamCursor);
    break;
  default:
    setCursor(Qt::CrossCursor);
    break;
  }

  QWidget::enterEvent(event);
}

void DrawingCanvas::setupContextMenu() {
  m_contextMenu = new QMenu(this);

  // Check if there's a selected primitive
  bool hasSelectedPrimitive = !selectedObjects().empty();

  // Add basic drawing options
  if (hasSelectedPrimitive) {
    m_contextMenu->addAction("Delete Selected",
                             [this]() { deleteSelectedPrimitives(); });

    m_contextMenu->addSeparator();

  // Align menu removed per request

    // Add ordering menu
    QMenu *orderMenu = m_contextMenu->addMenu("Order");
    orderMenu->addAction("Bring to Front", [this]() {
      for (DrawingPrimitive *obj : selectedObjects()) {
        if (m_layerManager) {
          m_layerManager->bringToFront(obj);
        }
      }
      update();
    });
    orderMenu->addAction("Send to Back", [this]() {
      for (DrawingPrimitive *obj : selectedObjects()) {
        if (m_layerManager) {
          m_layerManager->sendToBack(obj);
        }
      }
      update();
    });
    orderMenu->addSeparator();
    orderMenu->addAction("Bring Forward", [this]() {
      for (DrawingPrimitive *obj : selectedObjects()) {
        if (m_layerManager) {
          m_layerManager->bringForward(obj);
        }
      }
      update();
    });
    orderMenu->addAction("Send Backward", [this]() {
      for (DrawingPrimitive *obj : selectedObjects()) {
        if (m_layerManager) {
          m_layerManager->sendBackward(obj);
        }
      }
      update();
    });

    m_contextMenu->addSeparator();
  }

  // Add view options
  m_contextMenu->addAction("Zoom to Fit", [this]() { zoomFit(); });
  m_contextMenu->addAction("Zoom to Actual Size", [this]() { zoomActual(); });

  // Apply modern context menu styling with 50% opacity
  m_contextMenu->setStyleSheet(R"(
        QMenu {
            background-color: rgba(30, 30, 30, 0.5);
            color: #e8e8e8;
            border: 1px solid rgba(100, 149, 237, 0.3);
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 8px 16px;
            border-radius: 4px;
            margin: 1px;
        }
        QMenu::item:selected {
            background-color: rgba(100, 149, 237, 0.2);
        }
        QMenu::separator {
            height: 1px;
            background-color: rgba(100, 149, 237, 0.2);
            margin: 4px 8px;
        }
    )");
}

void DrawingCanvas::zoomIn() {
  m_zoomLevel *= 1.2f;
  m_zoomLevel = std::min(20.0f, m_zoomLevel);
  qDebug() << "ZoomIn - new level:" << m_zoomLevel;
  update();
  emit zoomChanged(m_zoomLevel);
  update();
}

void DrawingCanvas::zoomOut() {
  m_zoomLevel /= 1.2f;
  m_zoomLevel = std::max(0.05f, m_zoomLevel);
  qDebug() << "ZoomOut - new level:" << m_zoomLevel;
  update();
  emit zoomChanged(m_zoomLevel);
  update();
}

void DrawingCanvas::zoomFit() {
  // Reset to origin and normal zoom
  m_zoomLevel = 1.0f;
  m_viewCenter = QVector2D(0.0f, 0.0f);
  update();
  emit zoomChanged(m_zoomLevel);
  update();
}

void DrawingCanvas::zoomActual() {
  // Reset to origin and 100% zoom
  m_zoomLevel = 1.0f;
  m_viewCenter = QVector2D(0.0f, 0.0f);
  update();
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
  m_paperColor = color;
  update();
}

void DrawingCanvas::setDefaultDrawingColor(const QColor &color) {
  m_defaultDrawingColor = color;
  // No need to update() as this only affects new primitives
}

void DrawingCanvas::setZoomSensitivity(float sensitivity) {
  m_zoomSensitivity = std::max(
      1.01f, std::min(3.0f, sensitivity)); // Clamp between 1.01 and 3.0
}

void DrawingCanvas::setRulersVisible(bool visible) {
  m_rulersVisible = visible;
  update();
}

void DrawingCanvas::setCurrentTool(DrawingTool tool) {
  // Only reset drawing state if switching to a different tool
  if (m_currentTool != tool) {
    qDebug() << "Tool changed from" << static_cast<int>(m_currentTool) << "to"
             << static_cast<int>(tool) << "- resetting drawing state";

    // Finalize any incomplete primitives before switching tools
    if (m_currentPrimitive && m_isDrawing) {
      qDebug() << "Finalizing incomplete primitive before tool switch";

      // For curve-based tools, make sure they have enough points to be valid
      bool shouldFinalize = true;

      if (auto curve =
              dynamic_cast<CurvePrimitive *>(m_currentPrimitive.get())) {
        shouldFinalize = curve->controlPoints().size() >= 2;
      } else if (auto spline = dynamic_cast<SplinePrimitive *>(
                     m_currentPrimitive.get())) {
        shouldFinalize = spline->points().size() >= 2;
      } else if (auto bezier = dynamic_cast<BezierCurvePrimitive *>(
                     m_currentPrimitive.get())) {
        // Require valid number of points and non-trivial length
        const auto &cps = bezier->controlPoints();
        const bool enough = cps.size() >= 4;
        const float len = enough ? (cps.front() - cps.back()).length() : 0.0f;
        shouldFinalize = enough && len >= 2.0f;
      }

      if (shouldFinalize) {
        m_currentPrimitive->setColor(
            m_defaultDrawingColor); // Set final color to default drawing color
        // Preserve selection state if caller marked it

        addPrimitiveWithCommand(std::move(m_currentPrimitive));
        qDebug() << "Primitive finalized and added to canvas";

      } else {
        qDebug() << "Discarding incomplete primitive (not enough points)";
        m_currentPrimitive.reset();
      }
    } else {
      m_currentPrimitive.reset();
    }

    m_isDrawing = false;
    m_selectionManager->setIsSelecting(false);
    m_bezierCreationStage = 0;
    m_angleLineStage = 0;
    m_arcStage = 0;

    // CRITICAL: Stop brush/blur state to prevent runaway airbrush timer
    if (m_isBrushing || m_isBlurring) {
      qDebug() << "setCurrentTool: cleaning up brush/blur state";
      if (m_airbrushTimer) m_airbrushTimer->stop();
      // Commit any in-progress brush stroke before switching
      if (m_brushStroke.size() > 1) {
        auto brushStroke = std::make_unique<BrushStrokePrimitive>();
        if (m_isBrushing && !m_brushParticleScale.empty()) {
          brushStroke->setParticles(m_brushStroke, m_brushParticleScale, m_brushParticleAlpha);
        } else {
          brushStroke->setPoints(m_brushStroke);
        }
        brushStroke->setBrushSize(m_brushSize);
        brushStroke->setHardness(m_brushHardness);
        brushStroke->setColor(m_defaultDrawingColor);
        brushStroke->setStrokeType(m_isBlurring ? BrushStrokePrimitive::StrokeType::Blur : BrushStrokePrimitive::StrokeType::Brush);
        addPrimitiveWithCommand(std::move(brushStroke));
      }
      m_brushStroke.clear();
      m_brushParticleScale.clear();
      m_brushParticleAlpha.clear();
      m_airbrushDrips.clear();
      m_isBrushing = false;
      m_isBlurring = false;
    }

    // Leaving the Text tool: commit any in-progress edit so text is never lost
    // and the editor overlay is cleaned up.
    if (m_currentTool == DrawingTool::Text && tool != DrawingTool::Text) {
      if (MainWindow *mainWindow = qobject_cast<MainWindow *>(window())) {
        if (ClassicTextTool *textTool = mainWindow->getClassicTextTool()) {
          textTool->deactivate();
        }
      }
    }
  }
  m_currentTool = tool;

  // Set appropriate cursor for each tool
  switch (tool) {
  case DrawingTool::Select:
    // Cursor will be handled dynamically in mouseMoveEvent for control point
    // hovering
    setCursor(Qt::ArrowCursor);
    break;
  case DrawingTool::Move:
    setCursor(Qt::SizeAllCursor);
    break;
  case DrawingTool::Text:
    setCursor(Qt::IBeamCursor);
    break;
  case DrawingTool::Eraser:
    setCursor(Qt::CrossCursor);
    break;
  default:
    setCursor(Qt::CrossCursor);
    break;
  }

  m_showCursorPreview =
      (tool == DrawingTool::Brush || tool == DrawingTool::Eraser ||
       tool == DrawingTool::Blur);
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
  // Simple screen to world conversion - origin at center, Y up
  float x = (screenPos.x() - width() / 2.0f) / m_zoomLevel + m_viewCenter.x();
  float y = (height() / 2.0f - screenPos.y()) / m_zoomLevel + m_viewCenter.y();
  if (m_pixelSnapEnabled) {
    x = std::round(x);
    y = std::round(y);
  }
  return QVector2D(x, y);
}

QPoint DrawingCanvas::worldToScreen(const QVector2D &worldPos) const {
  // Simple world to screen conversion
  float x = (worldPos.x() - m_viewCenter.x()) * m_zoomLevel + width() / 2.0f;
  float y = height() / 2.0f - (worldPos.y() - m_viewCenter.y()) * m_zoomLevel;
  return QPoint(static_cast<int>(x), static_cast<int>(y));
}

QVector2D DrawingCanvas::snapToGrid(const QVector2D &pos) const {
  return m_gridManager->snapToGrid(pos);
}

bool DrawingCanvas::selectPrimitiveById(const QUuid &id) {
  if (id.isNull()) return false;

  // Search layers first
  if (m_layerManager) {
    const auto &layers = m_layerManager->layers();
    for (const auto &layer : layers) {
      if (!layer || !layer->isVisible()) continue;
      for (const auto &prim : layer->primitives()) {
        if (prim && prim->id() == id) {
          clearSelection();
          prim->setSelected(true);
          addToSelection(prim.get());
          emit selectionChanged();
          update();
          return true;
        }
      }
    }
  }
  // Fallback to legacy list
  for (const auto &prim : m_primitives) {
    if (prim && prim->id() == id) {
      clearSelection();
      prim->setSelected(true);
      addToSelection(prim.get());
      emit selectionChanged();
      update();
      return true;
    }
  }
  return false;
}

void DrawingCanvas::renderGrid(QPainter& painter) {
  if (!m_renderer) {
    return;
  }

  // Set up world transform for the renderer
  painter.save();
  setupWorldTransform(painter);

  // Update grid manager with current paper settings (in case they changed)
  m_gridManager->setPaperSize(m_paperSize);
  m_gridManager->setPixelsPerMM(m_pixelsPerMM);

  m_gridManager->render(m_renderer.get());

  painter.restore();
}

void DrawingCanvas::renderRulers(QPainter& painter) {
  if (!m_rulersVisible)
    return;

  const int RULER_HEIGHT = 25;
  const int RULER_WIDTH = 25;

  painter.save();
  painter.resetTransform(); // Screen coordinates

  // Backgrounds
  QColor rulerBg(230, 230, 230, 230);
  painter.fillRect(QRect(RULER_WIDTH, 0, width() - RULER_WIDTH, RULER_HEIGHT), rulerBg);
  painter.fillRect(QRect(0, RULER_HEIGHT, RULER_WIDTH, height() - RULER_HEIGHT), rulerBg);

  QColor cornerBg(204, 204, 204, 230);
  painter.fillRect(QRect(0, 0, RULER_WIDTH, RULER_HEIGHT), cornerBg);

  // Draw markings
  QPen tickPen(QColor(51, 51, 51), 1.0);
  painter.setPen(tickPen);

  float majorSpacing = 100.0f;
  if (m_units == Units::Centimeters) majorSpacing = 10.0f;
  else if (m_units == Units::Inches) majorSpacing = 1.0f;

  float majorScreenSpacing = majorSpacing * m_zoomLevel;
  if (majorScreenSpacing < 30) majorScreenSpacing *= 2;

  QPoint originScreen = QPoint(RULER_WIDTH, RULER_HEIGHT);

  // Horizontal ticks
  int leftMarks = (originScreen.x() - RULER_WIDTH) / majorScreenSpacing + 1;
  int rightMarks = (width() - originScreen.x()) / majorScreenSpacing + 1;

  for (int i = -leftMarks; i <= rightMarks; ++i) {
    int x = originScreen.x() + i * majorScreenSpacing;
    if (x < RULER_WIDTH || x >= width()) continue;
    painter.drawLine(x, RULER_HEIGHT - 15, x, RULER_HEIGHT);
  }

  // Vertical ticks
  int topMarks = (originScreen.y() - RULER_HEIGHT) / majorScreenSpacing + 1;
  int bottomMarks = (height() - originScreen.y()) / majorScreenSpacing + 1;

  for (int i = -bottomMarks; i <= topMarks; ++i) {
    int y = originScreen.y() - i * majorScreenSpacing;
    if (y < RULER_HEIGHT || y >= height()) continue;
    painter.drawLine(RULER_WIDTH - 15, y, RULER_WIDTH, y);
  }

  painter.restore();
}

void DrawingCanvas::renderObjects(QPainter& painter) {
  painter.save();
  setupWorldTransform(painter);

  auto renderOne = [&](DrawingPrimitive *primitive, float opacity) {
    if (!primitive || !primitive->isVisible())
      return;
    primitive->setOpacityMultiplier(opacity);

    const bool selfRotates =
        dynamic_cast<ImagePrimitive *>(primitive) != nullptr ||
        dynamic_cast<TextPrimitive *>(primitive) != nullptr;
    const float rot = primitive->rotationDegrees();
    if (!selfRotates && std::fabs(rot) > 0.01f) {
      const QRectF br = primitive->boundingRect();
      const QPointF c = br.center();
      painter.save();
      painter.translate(c);
      painter.rotate(rot);
      painter.translate(-c);
      primitive->render(&painter);
      painter.restore();
    } else {
      primitive->render(&painter);
    }
    primitive->setOpacityMultiplier(1.0f);
  };

  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (layer && layer->isVisible()) {
        float opacity = layer->opacity();
        for (const auto &primitive : layer->primitives()) {
          renderOne(primitive.get(), opacity);
        }
      }
    }
  } else {
    for (const auto &primitive : m_primitives) {
      renderOne(primitive.get(), 1.0f);
    }
  }

  // Current primitive
  if (m_currentPrimitive) {
    m_currentPrimitive->render(&painter);
  }

  // Selected geometry control points (paths / polygons only — bbox shapes use
  // the polished selection handles instead to avoid double-handle clutter)
  for (auto *selectedObj : selectedObjects()) {
    if (!selectedObj || !selectedObj->isSelected())
      continue;
    if (!showsGeometryControlPoints(selectedObj))
      continue;

    auto controlPoints = selectedObj->getControlPoints();
    for (const auto &point : controlPoints) {
      renderControlPoint(painter, point, false);
    }
  }

  painter.restore();
}

void DrawingCanvas::selectionHandlePositions(const QRectF &br,
                                             QPointF out[9]) const {
  out[0] = br.topLeft();
  out[1] = QPointF(br.center().x(), br.top());
  out[2] = br.topRight();
  out[3] = QPointF(br.right(), br.center().y());
  out[4] = br.bottomRight();
  out[5] = QPointF(br.center().x(), br.bottom());
  out[6] = br.bottomLeft();
  out[7] = QPointF(br.left(), br.center().y());
  // Rotation handle sits above the top edge (world Y increases upward after flip)
  const float rotateOffset = 22.0f / m_zoomLevel;
  out[kHandleRotate] = QPointF(br.center().x(), br.top() + rotateOffset);
}

float DrawingCanvas::selectionHandleHalfSize() const {
  // ~7px screen square (constant across zoom)
  return 3.5f / m_zoomLevel;
}

float DrawingCanvas::selectionHandleHitRadius() const {
  return 11.0f / m_zoomLevel;
}

bool DrawingCanvas::showsGeometryControlPoints(
    const DrawingPrimitive *obj) const {
  if (!obj)
    return false;
  // Images / text / simple shapes already have bbox handles
  if (dynamic_cast<const ImagePrimitive *>(obj))
    return false;
  if (dynamic_cast<const TextPrimitive *>(obj))
    return false;
  if (dynamic_cast<const RectanglePrimitive *>(obj))
    return false;
  if (dynamic_cast<const EllipsePrimitive *>(obj))
    return false;
  if (dynamic_cast<const CirclePrimitive *>(obj))
    return false;
  return true;
}

bool DrawingCanvas::supportsRotationHandle(
    const DrawingPrimitive *obj) const {
  if (!obj)
    return false;
  if (dynamic_cast<const ImagePrimitive *>(obj) ||
      dynamic_cast<const TextPrimitive *>(obj))
    return true;
  // BBox-handled shapes
  return dynamic_cast<const RectanglePrimitive *>(obj) ||
         dynamic_cast<const EllipsePrimitive *>(obj) ||
         dynamic_cast<const CirclePrimitive *>(obj) ||
         dynamic_cast<const PolygonPrimitive *>(obj);
}

float DrawingCanvas::objectRotationDegrees(
    const DrawingPrimitive *obj) const {
  if (auto *img = dynamic_cast<const ImagePrimitive *>(obj))
    return img->rotation();
  if (auto *txt = dynamic_cast<const TextPrimitive *>(obj))
    return txt->rotation() * 180.0f / static_cast<float>(M_PI);
  return obj ? obj->rotationDegrees() : 0.0f;
}

void DrawingCanvas::applyObjectRotation(DrawingPrimitive *obj, float degrees) {
  if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
    img->setRotation(degrees);
  } else if (auto *txt = dynamic_cast<TextPrimitive *>(obj)) {
    txt->setRotation(degrees * static_cast<float>(M_PI) / 180.0f);
  } else if (obj) {
    obj->setRotationDegrees(degrees);
  }
}

bool DrawingCanvas::usesExternalRotation(const DrawingPrimitive *obj) const {
  if (!obj)
    return false;
  if (dynamic_cast<const ImagePrimitive *>(obj) ||
      dynamic_cast<const TextPrimitive *>(obj))
    return false;
  return std::fabs(obj->rotationDegrees()) > 0.01f;
}

QVector2D DrawingCanvas::toObjectLocal(const DrawingPrimitive *obj,
                                       const QVector2D &worldPos) const {
  if (!usesExternalRotation(obj))
    return worldPos;
  const QPointF c = obj->boundingRect().center();
  QTransform t;
  t.translate(c.x(), c.y());
  t.rotate(-obj->rotationDegrees());
  t.translate(-c.x(), -c.y());
  const QPointF p = t.map(worldPos.toPointF());
  return QVector2D(static_cast<float>(p.x()), static_cast<float>(p.y()));
}

Qt::CursorShape DrawingCanvas::cursorForSelectionHandle(int index) const {
  // QRectF indices mapped through Y-flip to screen cursors
  static const Qt::CursorShape cursors[8] = {
      Qt::SizeFDiagCursor, // 0 TL → screen BL
      Qt::SizeVerCursor,   // 1 T  → screen B
      Qt::SizeBDiagCursor, // 2 TR → screen BR
      Qt::SizeHorCursor,   // 3 R
      Qt::SizeFDiagCursor, // 4 BR → screen TR
      Qt::SizeVerCursor,   // 5 B  → screen T
      Qt::SizeBDiagCursor, // 6 BL → screen TL
      Qt::SizeHorCursor,   // 7 L
  };
  if (index == kHandleRotate)
    return Qt::PointingHandCursor;
  if (index >= 0 && index < 8)
    return cursors[index];
  return Qt::ArrowCursor;
}

void DrawingCanvas::drawSelectionHandles(QPainter &painter, const QRectF &br,
                                         bool showRotate) const {
  QPointF handles[9];
  selectionHandlePositions(br, handles);

  // Soft selection frame
  QPen boxPen(QColor(59, 130, 246), 1.25);
  boxPen.setCosmetic(true);
  boxPen.setStyle(Qt::DashLine);
  boxPen.setDashPattern({4, 3});
  painter.setPen(boxPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(br);

  const float hs = selectionHandleHalfSize();
  const float edgeHs = hs * 0.85f;

  QPen handlePen(QColor(37, 99, 235), 1.25);
  handlePen.setCosmetic(true);

  for (int i = 0; i < 8; ++i) {
    const bool corner = (i % 2 == 0);
    const float s = corner ? hs : edgeHs;
    QRectF r(handles[i].x() - s, handles[i].y() - s, s * 2, s * 2);

    // Subtle depth: soft outer ring
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 23, 42, 35));
    painter.drawRoundedRect(r.adjusted(-0.6f / m_zoomLevel, -0.6f / m_zoomLevel,
                                       0.6f / m_zoomLevel, 0.6f / m_zoomLevel),
                            1.2f / m_zoomLevel, 1.2f / m_zoomLevel);

    painter.setPen(handlePen);
    painter.setBrush(Qt::white);
    if (corner) {
      painter.drawRoundedRect(r, 1.1f / m_zoomLevel, 1.1f / m_zoomLevel);
    } else {
      // Edge handles: slightly flatter pill
      painter.drawRoundedRect(r, s * 0.45f, s * 0.45f);
    }
  }

  if (showRotate) {
    const QPointF top = handles[1];
    const QPointF rot = handles[kHandleRotate];

    QPen stemPen(QColor(59, 130, 246), 1.2);
    stemPen.setCosmetic(true);
    painter.setPen(stemPen);
    painter.drawLine(top, rot);

    const float rs = hs * 1.15f;
    painter.setBrush(Qt::white);
    painter.setPen(handlePen);
    painter.drawEllipse(rot, rs, rs);

    // Inner accent dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(59, 130, 246));
    painter.drawEllipse(rot, rs * 0.35f, rs * 0.35f);
  }
}

int DrawingCanvas::hitTestSelectionHandle(const QRectF &br,
                                          const QVector2D &worldPos,
                                          bool includeRotate) const {
  QPointF handles[9];
  selectionHandlePositions(br, handles);
  const float hit = selectionHandleHitRadius();
  const int count = includeRotate ? 9 : 8;

  int best = -1;
  float bestDist = hit;
  for (int i = 0; i < count; ++i) {
    if (i == kHandleRotate && !includeRotate)
      continue;
    const float d =
        QVector2D(worldPos - QVector2D(handles[i])).length();
    if (d < bestDist) {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

QRectF DrawingCanvas::computeResizedBounds(const QRectF &orig, int handleIndex,
                                           const QVector2D &worldPos,
                                           Qt::KeyboardModifiers mods) const {
  const bool keepAspect = mods.testFlag(Qt::ShiftModifier);
  const bool fromCenter = mods.testFlag(Qt::AltModifier);
  const QPointF pos = worldPos.toPointF();
  const QPointF center = orig.center();
  const float aspect =
      (orig.height() > 0.001) ? (orig.width() / orig.height()) : 1.0f;

  QRectF nr = orig;

  if (fromCenter) {
    float halfW = orig.width() * 0.5f;
    float halfH = orig.height() * 0.5f;
    switch (handleIndex) {
    case 0: // TL
    case 2: // TR
    case 4: // BR
    case 6: // BL
      halfW = std::abs(pos.x() - center.x());
      halfH = std::abs(pos.y() - center.y());
      if (keepAspect) {
        if (halfW / aspect > halfH)
          halfH = halfW / aspect;
        else
          halfW = halfH * aspect;
      }
      break;
    case 1: // T
    case 5: // B
      halfH = std::abs(pos.y() - center.y());
      if (keepAspect)
        halfW = halfH * aspect;
      break;
    case 3: // R
    case 7: // L
      halfW = std::abs(pos.x() - center.x());
      if (keepAspect)
        halfH = halfW / aspect;
      break;
    default:
      break;
    }
    halfW = std::max(0.5f, halfW);
    halfH = std::max(0.5f, halfH);
    return QRectF(center.x() - halfW, center.y() - halfH, halfW * 2, halfH * 2);
  }

  // Anchor opposite corner/edge, then optionally lock aspect
  QPointF anchor;
  switch (handleIndex) {
  case 0: anchor = orig.bottomRight(); break;
  case 1: anchor = QPointF(orig.center().x(), orig.bottom()); break;
  case 2: anchor = orig.bottomLeft(); break;
  case 3: anchor = QPointF(orig.left(), orig.center().y()); break;
  case 4: anchor = orig.topLeft(); break;
  case 5: anchor = QPointF(orig.center().x(), orig.top()); break;
  case 6: anchor = orig.topRight(); break;
  case 7: anchor = QPointF(orig.right(), orig.center().y()); break;
  default: return orig;
  }

  QPointF moved = pos;
  if (keepAspect && (handleIndex % 2 == 0)) {
    // Corner: lock aspect from the opposite anchor
    float dx = moved.x() - anchor.x();
    float dy = moved.y() - anchor.y();
    const float sx = (dx >= 0.0f) ? 1.0f : -1.0f;
    const float sy = (dy >= 0.0f) ? 1.0f : -1.0f;
    if (std::abs(dx) / aspect >= std::abs(dy)) {
      dx = sx * std::abs(dx);
      dy = sy * (std::abs(dx) / aspect);
    } else {
      dy = sy * std::abs(dy);
      dx = sx * (std::abs(dy) * aspect);
    }
    moved = QPointF(anchor.x() + dx, anchor.y() + dy);
  } else if (keepAspect && (handleIndex == 1 || handleIndex == 5)) {
    float h = std::max(0.5f, static_cast<float>(std::abs(moved.y() - anchor.y())));
    float w = h * aspect;
    return QRectF(center.x() - w * 0.5f, std::min(anchor.y(), moved.y()), w, h)
        .normalized();
  } else if (keepAspect && (handleIndex == 3 || handleIndex == 7)) {
    float w = std::max(0.5f, static_cast<float>(std::abs(moved.x() - anchor.x())));
    float h = w / aspect;
    return QRectF(std::min(anchor.x(), moved.x()), center.y() - h * 0.5f, w, h)
        .normalized();
  }

  switch (handleIndex) {
  case 0: nr.setTopLeft(moved); break;
  case 1: nr.setTop(moved.y()); break;
  case 2: nr.setTopRight(moved); break;
  case 3: nr.setRight(moved.x()); break;
  case 4: nr.setBottomRight(moved); break;
  case 5: nr.setBottom(moved.y()); break;
  case 6: nr.setBottomLeft(moved); break;
  case 7: nr.setLeft(moved.x()); break;
  }
  return nr.normalized();
}

void DrawingCanvas::renderSelection(QPainter& painter) {
  if (!m_renderer) return;

  painter.save();
  setupWorldTransform(painter);
  m_selectionManager->render(m_renderer.get());

  if (m_isEditingControlPoints && m_editingPrimitive) {
    auto controlPoints = m_editingPrimitive->getControlPoints();
    for (const auto &point : controlPoints) {
      renderControlPoint(painter, point, true);
    }
  }

  // Draw polished resize (+ rotate) handles for selected objects
  for (auto *obj : selectedObjects()) {
    QRectF br = obj->boundingRect();
    if (br.isEmpty()) continue;
    if (usesExternalRotation(obj)) {
      painter.save();
      const QPointF c = br.center();
      painter.translate(c);
      painter.rotate(obj->rotationDegrees());
      painter.translate(-c);
      drawSelectionHandles(painter, br, supportsRotationHandle(obj));
      painter.restore();
    } else {
      drawSelectionHandles(painter, br, supportsRotationHandle(obj));
    }
  }

  painter.restore();
}

void DrawingCanvas::renderTool(QPainter& painter) {
  // Brush/Blur should render in-progress stroke exactly like the final primitive
  if ((m_isBrushing || m_isBlurring) && !m_brushStroke.empty()) {
    painter.save();
    setupWorldTransform(painter);

    BrushStrokePrimitive stroke;
    if (m_isBrushing && !m_brushParticleScale.empty() &&
        m_brushParticleScale.size() == m_brushStroke.size() &&
        m_brushParticleAlpha.size() == m_brushStroke.size()) {
      stroke.setParticles(m_brushStroke, m_brushParticleScale, m_brushParticleAlpha);
    } else {
      stroke.setPoints(m_brushStroke);
    }

    stroke.setBrushSize(m_brushSize);
    stroke.setHardness(m_brushHardness);
    stroke.setColor(m_defaultDrawingColor);
    stroke.setStrokeType(m_isBlurring ? BrushStrokePrimitive::StrokeType::Blur
                                      : BrushStrokePrimitive::StrokeType::Brush);

    stroke.render(&painter);
    painter.restore();
    return;
  }

  const bool angleBaselineVisible =
      m_currentTool == DrawingTool::AngleLine && m_angleLineStage >= 1;
  if (!m_isDrawing && !angleBaselineVisible)
    return;

  painter.save();
  setupWorldTransform(painter);

  if (angleBaselineVisible) {
    QPen basePen(QColor(59, 130, 246, 180), 1.25, Qt::DashLine);
    basePen.setCosmetic(true);
    painter.setPen(basePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(m_angleBaselineStart.toPointF(),
                     m_angleBaselineEnd.toPointF());
    painter.setBrush(QColor(59, 130, 246));
    painter.setPen(Qt::NoPen);
    const float r = 3.0f / m_zoomLevel;
    painter.drawEllipse(m_angleBaselineEnd.toPointF(), r, r);
  }

  if (m_isDrawing && m_currentPrimitive) {
    m_currentPrimitive->setColor(m_defaultDrawingColor);
    m_currentPrimitive->render(&painter);
  }

  painter.restore();
}

void DrawingCanvas::renderCursorPreview(QPainter& painter) {
  if (!m_showCursorPreview) {
    return;
  }

  float radius = 0.0f;
  if (m_currentTool == DrawingTool::Brush || m_currentTool == DrawingTool::Blur) {
    radius = m_brushSize * 0.5f;
  } else if (m_currentTool == DrawingTool::Eraser) {
    radius = m_eraserSize * 0.5f;
  }

  if (radius <= 0.0f) {
    return;
  }

  painter.save();
  painter.resetTransform();
  QPoint center = worldToScreen(m_cursorPreviewPos);
  QColor ringColor(120, 180, 255, 220);
  painter.setPen(QPen(ringColor, 1.5));
  painter.setBrush(QColor(120, 180, 255, 40));
  painter.drawEllipse(center, static_cast<int>(radius * m_zoomLevel),
                      static_cast<int>(radius * m_zoomLevel));
  painter.restore();
}

void DrawingCanvas::renderSnapIndicator(QPainter& painter) {
  if (!m_snapIndicatorActive) {
    return;
  }

  painter.save();
  painter.resetTransform();
  QPoint center = worldToScreen(m_snapIndicatorPos);
  QColor indicator(74, 144, 226, 220);
  painter.setPen(QPen(indicator, 2.0));
  painter.setBrush(QColor(74, 144, 226, 60));
  painter.drawEllipse(center, 4, 4);
  painter.drawLine(center.x() - 6, center.y(), center.x() + 6, center.y());
  painter.drawLine(center.x(), center.y() - 6, center.x(), center.y() + 6);
  painter.restore();
}

void DrawingCanvas::renderLassoOverlay(QPainter& painter) {
  if (!m_selectionManager->isSelecting() ||
      m_selectionMode != SelectionMode::Lasso || m_lassoPoints.size() < 2) {
    return;
  }

  QPolygonF screenPoly;
  screenPoly.reserve(m_lassoPoints.size());
  for (const auto &pt : m_lassoPoints) {
    screenPoly << worldToScreen(QVector2D(pt)).toPointF();
  }

  painter.save();
  painter.resetTransform();
  painter.setPen(QPen(QColor(74, 144, 226, 200), 1.5, Qt::DashLine));
  painter.setBrush(QColor(74, 144, 226, 40));
  painter.drawPolygon(screenPoly);
  painter.restore();
}

void DrawingCanvas::handleSelectTool(QMouseEvent *event) {
  QVector2D worldPos = screenToWorld(event->pos());

  // RIGHT-CLICK: Context menu for ImagePrimitive edit mode
  if (event->button() == Qt::RightButton) {
    float lineTolerance = 30.0f;

    for (auto selectedObj : selectedObjects()) {
      if (selectedObj->isSelected()) {
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObj)) {
          if (imgPrim->isEditMode()) {
            QMenu contextMenu(this);
            int cpIndex = imgPrim->getControlPointAt(worldPos, 8.0f);
            int segmentIndex = imgPrim->getNearestContourSegment(worldPos, lineTolerance);

            if (cpIndex >= 0) contextMenu.addAction("🗑️ Delete Control Point");
            if (segmentIndex >= 0) contextMenu.addAction("➕ Add Control Point Here");

            QAction *selected = contextMenu.exec(event->globalPosition().toPoint());
            if (selected) {
              QString text = selected->text();
              if (text.contains("Delete") && cpIndex >= 0) {
                auto command = std::make_unique<DeleteMaskControlPointCommand>(imgPrim, cpIndex);
                emit commandRequested(command.release());
                update();
              } else if (text.contains("Add") && segmentIndex >= 0) {
                QPointF worldPoint(worldPos.x(), worldPos.y());
                auto command = std::make_unique<InsertMaskControlPointCommand>(imgPrim, segmentIndex, worldPoint);
                emit commandRequested(command.release());
                update();
              }
            }
            return;
          }
        }
      }
    }
    return;
  }

  if (event->button() == Qt::LeftButton) {
    // Shift or ⌘/Ctrl = add to selection (macOS multi-select habit).
    // Prefer QApplication::keyboardModifiers — more reliable than event alone.
    const Qt::KeyboardModifiers mods =
        event->modifiers() | QApplication::keyboardModifiers();
    const bool additive =
        m_forceAdditiveSelection ||
        mods.testFlag(Qt::ShiftModifier) ||
        mods.testFlag(Qt::ControlModifier) ||
        mods.testFlag(Qt::MetaModifier);
    const bool subtractive =
        m_forceSubtractiveSelection || mods.testFlag(Qt::AltModifier);
    // Copy-along only when already moving a selection — not on multi-select click.
    const bool copyAlong = false;

    // 1. ImagePrimitive mask control points
    float controlPointTolerance = 20.0f;
    for (auto selectedObj : selectedObjects()) {
      if (selectedObj->isSelected()) {
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObj)) {
          if (imgPrim->isEditMode()) {
            int cpIndex = imgPrim->getControlPointAt(worldPos, controlPointTolerance);
            if (cpIndex >= 0) {
              m_editingPrimitive = selectedObj;
              startControlPointEdit(selectedObj, cpIndex);
              return;
            }
          }
        }
      }
    }

    // 1b. Bezier/Spline control points (non-Image, non-Text)
    for (auto selectedObj : selectedObjects()) {
      if (selectedObj->isSelected() && !dynamic_cast<ImagePrimitive *>(selectedObj)
          && !dynamic_cast<TextPrimitive *>(selectedObj)) {
        m_editingPrimitive = selectedObj;
        float cpTolerance = 8.0f / m_zoomLevel;
        int cpIndex = findControlPointAt(worldPos, cpTolerance);
        if (cpIndex >= 0) {
          startControlPointEdit(selectedObj, cpIndex);
          return;
        }
      }
    }
    m_editingPrimitive = nullptr;

    // Peek: is there an unselected image under the cursor? If so, skip resize
    // handles so clicking another photo adds it instead of resizing the first.
    auto imageUnderCursorUnselected = [&]() -> bool {
      auto check = [&](DrawingPrimitive *p) -> bool {
        if (!p || !p->isVisible() || p->isSelected())
          return false;
        if (!dynamic_cast<ImagePrimitive *>(p))
          return false;
        const float hitTol = qMax(4.0f, 10.0f / m_zoomLevel);
        return p->containsPoint(toObjectLocal(p, worldPos), hitTol);
      };
      if (m_layerManager) {
        for (auto lit = m_layerManager->layers().rbegin();
             lit != m_layerManager->layers().rend(); ++lit) {
          const auto &layer = *lit;
          if (!layer || !layer->isVisible() || layer->isLocked())
            continue;
          for (auto pit = layer->primitives().rbegin();
               pit != layer->primitives().rend(); ++pit) {
            if (check(pit->get()))
              return true;
          }
        }
      } else {
        for (auto it = m_primitives.rbegin(); it != m_primitives.rend(); ++it) {
          if (check(it->get()))
            return true;
        }
      }
      return false;
    };

    // 2. Resize / rotate handles — always available on selected objects.
    // Only skip when an unselected image is under the cursor (add-to-selection).
    if (!subtractive && !imageUnderCursorUnselected()) {
      for (auto *obj : selectedObjects()) {
        QRectF br = obj->boundingRect();
        const bool canRotate = supportsRotationHandle(obj);
        int hit = hitTestSelectionHandle(br, toObjectLocal(obj, worldPos),
                                         canRotate);
        if (hit < 0)
          continue;

        if (hit == kHandleRotate) {
          m_isRotatingObject = true;
          m_rotatingObject = obj;
          m_rotationPivot = br.center();
          m_initialRotation = objectRotationDegrees(obj);
          m_rotateOrigState = obj->toJson();
          const QVector2D delta = worldPos - QVector2D(m_rotationPivot);
          m_rotationStartAngle =
              std::atan2(delta.y(), delta.x()) * 180.0f /
              static_cast<float>(M_PI);
          setCursor(Qt::PointingHandCursor);
          return;
        }

        m_isResizingObject = true;
        m_resizingObject = obj;
        m_resizeHandleIndex = hit;
        m_resizeOrigBounds = br;
        m_resizeOrigControlPoints = obj->getControlPoints();
        m_resizeOrigState = obj->toJson();
        setCursor(cursorForSelectionHandle(hit));
        return;
      }
    }

    // 3. Selection Logic — gather every hit under the cursor (top → bottom)
    QVector<DrawingPrimitive *> hitsUnderCursor;
    auto considerHit = [&](DrawingPrimitive *primitive) {
      if (!primitive || !primitive->isVisible())
        return;
      const float hitTol = qMax(4.0f, 10.0f / m_zoomLevel);
      if (primitive->containsPoint(toObjectLocal(primitive, worldPos), hitTol))
        hitsUnderCursor.append(primitive);
    };

    if (m_layerManager) {
      const auto &layers = m_layerManager->layers();
      for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        const auto &layer = *it;
        if (!layer || !layer->isVisible() || layer->isLocked())
          continue;
        for (auto primIt = layer->primitives().rbegin();
             primIt != layer->primitives().rend(); ++primIt) {
          considerHit(primIt->get());
        }
      }
    } else {
      for (auto it = m_primitives.rbegin(); it != m_primitives.rend(); ++it)
        considerHit(it->get());
    }

    DrawingPrimitive *clickedObject = nullptr;
    if (!hitsUnderCursor.isEmpty()) {
      // Always prefer an unselected image under the cursor so clicking another
      // photo adds it beside the already green-selected one.
      for (DrawingPrimitive *p : hitsUnderCursor) {
        if (dynamic_cast<ImagePrimitive *>(p) && !p->isSelected()) {
          clickedObject = p;
          break;
        }
      }
      if (!clickedObject) {
        for (DrawingPrimitive *p : hitsUnderCursor) {
          if (!p->isSelected()) {
            clickedObject = p;
            break;
          }
        }
      }
      if (!clickedObject)
        clickedObject = hitsUnderCursor.first();
    }

    if (clickedObject) {
      auto *clickedImage = dynamic_cast<ImagePrimitive *>(clickedObject);

      // --- Images ---
      // Green-mask clicks always add (multi subject). Otherwise respect
      // Shift / Multi-select mode so the AI result isn't glued to the original.
      if (clickedImage) {
        if (subtractive) {
          if (clickedImage->isSelected()) {
            clickedImage->setSelected(false);
            m_selectionManager->removeFromSelection(clickedImage);
            emit selectionChanged();
            update();
          }
          return;
        }

        // Clicking a green mask: ADD to multi-selection (keep other greens)
        if (clickedImage->getMaskCandidateCount() > 0) {
          const int maskIdx =
              clickedImage->getMaskIndexAt(worldPos, /*preferUnselected=*/true);
          if (maskIdx != -1) {
            if (!clickedImage->isSelected()) {
              clickedImage->setSelected(true);
              m_selectionManager->addToSelection(clickedImage);
            }
            clickedImage->setMaskOverlayVisible(true);
            clickedImage->addMaskCandidateToSelection(maskIdx);
            emit selectionChanged();
            emit smartHintChanged(
                QStringLiteral("%1 green subject(s) selected — click another to add")
                    .arg(static_cast<int>(
                        clickedImage->selectedMaskIndices().size())));
            update();
            return;
          }
        }

        if (clickedImage->isSelected()) {
          // Already selected → drag-move the whole multi-selection
          m_isMoving = true;
          m_isCopyAlongMove = false;
          m_moveStartPos = worldPos;
          m_totalMoveOffset = QVector2D(0, 0);
          setCursor(Qt::SizeAllCursor);
          return;
        }

        // New image: replace selection unless additive (Shift / Multi-select)
        if (!additive)
          clearSelection();
        clickedImage->setSelected(true);
        clickedImage->setMaskOverlayVisible(true);
        m_selectionManager->addToSelection(clickedImage);
        emit selectionChanged();
        emit smartHintChanged(
            additive
                ? QStringLiteral("%1 image(s) selected — click another to add")
                      .arg(static_cast<int>(selectedObjects().size()))
                : QStringLiteral("Image selected"));
        update();
        return;
      }

      // --- Non-images: original replace / modifier behavior ---
      // Mask picking N/A

      bool wasAlreadySelected = false;
      for (auto *obj : selectedObjects()) {
        if (obj == clickedObject) {
          wasAlreadySelected = true;
          break;
        }
      }

      if (!additive && !subtractive && wasAlreadySelected) {
        m_isMoving = true;
        m_isCopyAlongMove = false;
        m_moveStartPos = worldPos;
        m_totalMoveOffset = QVector2D(0, 0);
        setCursor(Qt::SizeAllCursor);
        const Qt::KeyboardModifiers dragMods =
            event->modifiers() | QApplication::keyboardModifiers();
        if (isCopyAlongModifier(dragMods)) {
          if (beginCopyAlongConnectedLineMove()) {
            emit smartHintChanged(
                QStringLiteral("Hold ⌘/Ctrl + drag to place copy along line"));
          }
        } else if (auto *line = dynamic_cast<LinePrimitive *>(clickedObject)) {
          if (resolveLineMoveConstraint(line).lengthSquared() > 1e-8f) {
            emit smartHintChanged(
                QStringLiteral("Slide along line · hold ⌘/Ctrl then drag to copy · Alt = free"));
          }
        }
        return;
      }

      if (subtractive) {
        if (wasAlreadySelected) {
          clickedObject->setSelected(false);
          m_selectionManager->removeFromSelection(clickedObject);
          emit selectionChanged();
          update();
        }
        return;
      }

      if (additive && wasAlreadySelected) {
        clickedObject->setSelected(false);
        m_selectionManager->removeFromSelection(clickedObject);
        emit selectionChanged();
        update();
        return;
      }

      if (!additive) {
        clearSelection();
      }
      clickedObject->setSelected(true);
      m_selectionManager->addToSelection(clickedObject);
      if (!additive && !subtractive)
        selectGroupMembers(clickedObject);
      emit selectionChanged();
      update();
    } else {
      if (!additive && !subtractive) {
        clearSelection();
        update();
      }
      m_selectionOperation = subtractive
                                 ? SelectionManager::SelectionOperation::Subtract
                                 : (additive ? SelectionManager::SelectionOperation::Add
                                             : SelectionManager::SelectionOperation::Replace);
      m_selectionManager->setIsSelecting(true);
      m_drawStartPos = worldPos;
      if (m_selectionMode == SelectionMode::Lasso) {
        m_lassoPoints.clear();
        m_lassoPoints << worldPos.toPointF();
        m_selectionManager->setSelectionRect(QRectF());
      } else {
        m_selectionManager->setSelectionRect(
            QRectF(worldPos.toPointF(), QSizeF(0, 0)));
      }
      update();
    }
  }
}

void DrawingCanvas::handleLineTool(QMouseEvent *event) {
  qDebug() << "Line tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  if (!m_isDrawing) {
    QVector2D worldPos = screenToWorld(event->pos());

    // Apply magnetic connection and grid snapping independently
    QVector2D snappedPos = snapToGrid(worldPos);
    m_drawStartPos = snapToLineEndpoint(snappedPos);
    m_drawCurrentPos = m_drawStartPos;

    // Create temporary line primitive
    m_currentPrimitive =
        std::make_unique<LinePrimitive>(m_drawStartPos, m_drawCurrentPos);
    m_currentPrimitive->setColor(m_defaultDrawingColor);
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    m_isDrawing = true;
  }
  // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::handleAngleLineTool(QMouseEvent *event) {
  QVector2D worldPos = snapToGrid(screenToWorld(event->pos()));

  if (event->button() == Qt::RightButton) {
    // Cancel angle line creation
    m_currentPrimitive.reset();
    m_isDrawing = false;
    m_angleLineStage = 0;
    m_angleBaselinePrimitiveId = QUuid();
    m_angleBaselineConstraintDir = QVector2D();
    emit smartHintChanged(
        QStringLiteral("Angle line: drag a baseline, then drag the angled segment"));
    update();
    return;
  }

  if (event->button() != Qt::LeftButton)
    return;

  if (m_angleLineStage == 0 || m_angleLineStage == 1) {
    // Start (or restart) baseline definition — press-drag-release
    m_angleBaselineStart = worldPos;
    m_angleBaselineEnd = worldPos;
    m_drawStartPos = worldPos;
    m_drawCurrentPos = worldPos;
    m_currentPrimitive.reset();
    m_isDrawing = true;
    m_angleLineStage = 1;
    emit smartHintChanged(
        QStringLiteral("Drag to set the reference baseline, then release"));
    update();
    return;
  }

  if (m_angleLineStage == 2) {
    // Start angled segment from the baseline end point
    m_drawStartPos = m_angleBaselineEnd;
    m_drawCurrentPos = worldPos;
    QVector2D end =
        snapAngleLineEndpoint(m_angleBaselineEnd, worldPos, event->modifiers());
    m_currentPrimitive =
        std::make_unique<LinePrimitive>(m_angleBaselineEnd, end);
    m_currentPrimitive->setColor(m_defaultDrawingColor);
    m_currentPrimitive->setLineStyle(m_defaultLineStyle);
    m_currentPrimitive->setLineWidth(m_defaultLineWidth);
    m_isDrawing = true;
    emit smartHintChanged(
        QStringLiteral("Drag angled line · snaps to 15° from baseline · "
                       "Shift=5° · Alt=free · Right-click cancels"));
    update();
  }
}

QVector2D DrawingCanvas::snapAngleLineEndpoint(
    const QVector2D &origin, const QVector2D &rawEnd,
    Qt::KeyboardModifiers mods) const {
  QVector2D delta = rawEnd - origin;
  const float len = delta.length();
  if (len < 0.001f)
    return origin;

  // Alt: free angle (no snap to baseline)
  if (mods.testFlag(Qt::AltModifier))
    return rawEnd;

  QVector2D base = m_angleBaselineEnd - m_angleBaselineStart;
  if (base.length() < 0.001f)
    return rawEnd;

  const float baseAng = std::atan2(base.y(), base.x());
  const float ang = std::atan2(delta.y(), delta.x());
  float rel = ang - baseAng;
  while (rel > static_cast<float>(M_PI))
    rel -= 2.0f * static_cast<float>(M_PI);
  while (rel < -static_cast<float>(M_PI))
    rel += 2.0f * static_cast<float>(M_PI);

  // Default 15° steps relative to baseline; Shift = finer 5°
  const float stepDeg = mods.testFlag(Qt::ShiftModifier) ? 5.0f : 15.0f;
  const float step = stepDeg * static_cast<float>(M_PI) / 180.0f;
  const float snappedRel = std::round(rel / step) * step;
  const float finalAng = baseAng + snappedRel;

  return origin + QVector2D(std::cos(finalAng), std::sin(finalAng)) * len;
}

DrawingPrimitive *DrawingCanvas::findPrimitiveById(const QUuid &id) const {
  if (id.isNull())
    return nullptr;
  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer)
        continue;
      for (const auto &prim : layer->primitives()) {
        if (prim && prim->id() == id)
          return prim.get();
      }
    }
  }
  for (const auto &prim : m_primitives) {
    if (prim && prim->id() == id)
      return prim.get();
  }
  return nullptr;
}

QVector2D DrawingCanvas::resolveLineMoveConstraint(
    const LinePrimitive *line) const {
  if (!line)
    return {};
  if (auto *prev = dynamic_cast<LinePrimitive *>(
          findPrimitiveById(line->connectedLineId()))) {
    QVector2D dir = prev->endPoint() - prev->startPoint();
    if (dir.lengthSquared() > 1e-8f) {
      dir.normalize();
      return dir;
    }
  }
  QVector2D dir = line->moveConstraintDirection();
  if (dir.lengthSquared() > 1e-8f) {
    dir.normalize();
    return dir;
  }
  return {};
}

QVector2D DrawingCanvas::constrainDeltaAlongConnectedLine(
    const QVector2D &delta, Qt::KeyboardModifiers mods) const {
  // Alt = free move
  if (mods.testFlag(Qt::AltModifier))
    return delta;

  QVector2D constraint;
  bool found = false;
  bool mixed = false;
  for (auto *obj : selectedObjects()) {
    auto *line = dynamic_cast<LinePrimitive *>(obj);
    if (!line)
      continue;
    const QVector2D dir = resolveLineMoveConstraint(line);
    if (dir.lengthSquared() < 1e-8f)
      continue;
    if (!found) {
      constraint = dir;
      found = true;
    } else if (std::abs(QVector2D::dotProduct(constraint, dir)) < 0.98f) {
      mixed = true;
      break;
    }
  }
  if (!found || mixed)
    return delta;

  const float t = QVector2D::dotProduct(delta, constraint);
  return constraint * t;
}

QVector2D DrawingCanvas::projectPointOntoLineSegment(const QVector2D &point,
                                                     const QVector2D &a,
                                                     const QVector2D &b) const {
  QVector2D ab = b - a;
  const float len2 = ab.lengthSquared();
  if (len2 < 1e-8f)
    return a;
  float t = QVector2D::dotProduct(point - a, ab) / len2;
  t = std::clamp(t, 0.0f, 1.0f);
  return a + ab * t;
}

void DrawingCanvas::handleArcTool(QMouseEvent *event) {
  qDebug() << "Arc tool called! stage:" << m_arcStage
           << "button:" << event->button();
  QVector2D pos = snapToLineEndpoint(snapToGrid(screenToWorld(event->pos())));

  if (event->button() == Qt::RightButton) {
    // Cancel arc creation
    m_currentPrimitive.reset();
    m_isDrawing = false;
    m_arcStage = 0;
    update();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    if (m_arcStage == 0) {
      // First click: set start point
      m_arcStart = pos;
      m_arcStage = 1; // Next set end point
      m_isDrawing = true;
      qDebug() << "Arc: start point set" << m_arcStart
               << "- move to set end point";
    } else if (m_arcStage == 1) {
      // Second click: set end point
      m_arcEnd = pos;
      // Create temporary arc primitive
      auto arc = std::make_unique<ArcPrimitive>();
      // Use final render styling immediately (no special preview color).
      arc->setColor(m_defaultDrawingColor);
      arc->setLineStyle(m_defaultLineStyle); // Apply selected line style
      arc->setLineWidth(m_defaultLineWidth); // Apply selected line width
      m_currentPrimitive = std::move(arc);
      m_arcStage = 2; // Now adjust height with mouse movement
      qDebug() << "Arc: end point set" << m_arcEnd
               << ", move mouse to adjust height";
    } else if (m_arcStage == 2) {
      // Finalization will occur on mouse release
    }
    update();
  }
}

bool DrawingCanvas::computeCircleThroughPoints(const QVector2D &p1,
                                               const QVector2D &p2,
                                               const QVector2D &p3,
                                               QVector2D &centerOut,
                                               float &radiusOut) {
  // Calculate perpendicular bisectors intersection
  float a = p2.x() - p1.x();
  float b = p2.y() - p1.y();
  float c = p3.x() - p1.x();
  float d = p3.y() - p1.y();
  float e = a * (p1.x() + p2.x()) + b * (p1.y() + p2.y());
  float f = c * (p1.x() + p3.x()) + d * (p1.y() + p3.y());
  float g = 2.0f * (a * (p3.y() - p2.y()) - b * (p3.x() - p2.x()));
  if (std::abs(g) < 1e-5f) {
    return false; // Points are collinear or too close
  }
  float cx = (d * e - b * f) / g;
  float cy = (a * f - c * e) / g;
  centerOut = QVector2D(cx, cy);
  radiusOut = (centerOut - p1).length();
  return true;
}

void DrawingCanvas::handleCurveTool(QMouseEvent *event) {
  qDebug() << "Curve tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  QVector2D worldPos = screenToWorld(event->pos());
  QVector2D snappedPos = snapToGrid(worldPos);
  QVector2D pos = snapToLineEndpoint(snappedPos);

  if (event->button() == Qt::RightButton && m_isDrawing) {
    // Finalize curve on right-click
    if (m_currentPrimitive) {
      m_currentPrimitive->setColor(
          m_defaultDrawingColor); // Use default drawing color
      m_currentPrimitive->setSelected(true);

      addPrimitiveWithCommand(std::move(m_currentPrimitive));
    }
    m_isDrawing = false;
    return;
  }

  if (!m_isDrawing && event->button() == Qt::LeftButton) {
    // Start new curve
    m_currentPrimitive = std::make_unique<CurvePrimitive>();
    static_cast<CurvePrimitive *>(m_currentPrimitive.get())
        ->addControlPoint(pos);
    // Use final render styling immediately (no special preview color).
    m_currentPrimitive->setColor(m_defaultDrawingColor);
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    m_isDrawing = true;
  } else if (m_isDrawing && event->button() == Qt::LeftButton) {
    // Check if clicking near the starting point to close the curve
    auto curvePrimitive =
        static_cast<CurvePrimitive *>(m_currentPrimitive.get());
    const auto &controlPoints = curvePrimitive->controlPoints();

    if (controlPoints.size() >= 3) { // Need at least 3 points to close
      QVector2D startPoint = controlPoints[0];
      float distanceToStart = (pos - startPoint).length();

      if (distanceToStart <= 20.0f) { // Close if within 20 pixels of start
        qDebug() << "Closing curve - clicked near starting point";
        curvePrimitive->setClosed(true);
        m_currentPrimitive->setColor(m_defaultDrawingColor);
        m_currentPrimitive->setSelected(true);
        addPrimitiveWithCommand(std::move(m_currentPrimitive));
        m_isDrawing = false;
        return;
      }
    }

    // Add control point
    qDebug() << "Adding control point to curve:" << pos;
    curvePrimitive->addControlPoint(pos);
  }
}

void DrawingCanvas::handleBezierTool(QMouseEvent *event) {
  qDebug() << "Bezier tool called! Stage:" << m_bezierCreationStage
           << "Button:" << event->button();
  // Apply grid snapping and optional magnetic endpoint snap
  QVector2D worldPos = screenToWorld(event->pos());
  QVector2D snappedPos = snapToGrid(worldPos);
  QVector2D pos = snapToLineEndpoint(snappedPos);

  if (event->button() == Qt::RightButton) {
    // Cancel current bezier creation on right-click
    if (m_isDrawing) {
      qDebug() << "Canceling bezier creation";
      m_currentPrimitive.reset();
      m_isDrawing = false;
      m_bezierCreationStage = 0;
      update();
    }
    return;
  }

  if (event->button() == Qt::LeftButton) {
    bool isShiftPressed = event->modifiers() & Qt::ShiftModifier;

    if (m_bezierCreationStage == 0) {
      // Start new bezier - use stored start point if available, otherwise use
      // click position
      QVector2D startPoint = pos;

      // Check if we should continue from the last bezier's end point
      if ((m_drawStartPos - pos).length() < 10.0f && !m_primitives.empty()) {
        // If clicked near the last stored point, continue from there
        startPoint = m_drawStartPos;
        qDebug() << "Continuing bezier chain from last point:" << startPoint
                 << "to" << pos;
      } else {
        qDebug() << "Starting new bezier curve at" << pos;
      }

      m_currentPrimitive = std::make_unique<BezierCurvePrimitive>();
      auto bezier =
          static_cast<BezierCurvePrimitive *>(m_currentPrimitive.get());

      // Initialize with 4 control points (typical cubic bezier)
      std::vector<QVector2D> points = {startPoint, startPoint, pos, pos};
      bezier->setControlPoints(points);
      m_currentPrimitive->setColor(
          QColor(100, 255, 100)); // Green during creation
      m_currentPrimitive->setLineStyle(
          m_defaultLineStyle); // Apply selected line style
      m_currentPrimitive->setLineWidth(
          m_defaultLineWidth); // Apply selected line width
      m_currentPrimitive->setSelected(
          true); // Show control points during creation

      m_isDrawing = true;
      m_bezierCreationStage = 1;
      m_drawStartPos = startPoint;
      qDebug() << "Bezier started, stage 1: drag to set end point (release to "
                  "finalize)";
    } else if (m_bezierCreationStage == 1 && isShiftPressed) {
      // Shift+click to adjust control points during creation
      qDebug() << "Shift+click: adjusting control points at" << pos;
      int controlPointIndex = findClosestControlPoint(pos);
      if (controlPointIndex >= 0) {
        startControlPointEdit(m_currentPrimitive.get(), controlPointIndex);
      }
    }
    // Note: Mouse release will handle finalization, second click will start a
    // new bezier
  }
}

// Helper method to find closest control point
int DrawingCanvas::findClosestControlPoint(const QVector2D &pos) {
  if (!m_currentPrimitive)
    return -1;

  auto bezier = dynamic_cast<BezierCurvePrimitive *>(m_currentPrimitive.get());
  if (!bezier)
    return -1;

  const auto &points = bezier->controlPoints();
  int closestIndex = -1;
  float minDistance = 15.0f; // Tolerance in world units

  for (int i = 0; i < points.size(); ++i) {
    float distance = (points[i] - pos).length();
    if (distance < minDistance) {
      minDistance = distance;
      closestIndex = i;
    }
  }

  return closestIndex;
}

void DrawingCanvas::handleSplineTool(QMouseEvent *event) {
  qDebug() << "Spline tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  QVector2D worldPos = screenToWorld(event->pos());
  QVector2D snappedPos = snapToGrid(worldPos);
  QVector2D pos = snapToLineEndpoint(snappedPos);

  if (event->button() == Qt::RightButton && m_isDrawing) {
    // Finalize spline on right-click
    if (m_currentPrimitive) {
      qDebug() << "Finalizing spline with"
               << static_cast<SplinePrimitive *>(m_currentPrimitive.get())
                      ->points()
                      .size()
               << "points";
      m_currentPrimitive->setColor(
          m_defaultDrawingColor); // Use default drawing color
      m_currentPrimitive->setSelected(true);

      addPrimitiveWithCommand(std::move(m_currentPrimitive));
    }
    m_isDrawing = false;
    return;
  }

  if (!m_isDrawing && event->button() == Qt::LeftButton) {
    // Start new spline
    qDebug() << "Starting new spline at position:" << pos;
    m_currentPrimitive = std::make_unique<SplinePrimitive>();
    static_cast<SplinePrimitive *>(m_currentPrimitive.get())->addPoint(pos);
    // Use final render styling immediately (no special preview color).
    m_currentPrimitive->setColor(m_defaultDrawingColor);
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    m_isDrawing = true;
  } else if (m_isDrawing && event->button() == Qt::LeftButton) {
    // Check if clicking near the starting point to close the spline
    auto splinePrimitive =
        static_cast<SplinePrimitive *>(m_currentPrimitive.get());
    const auto &points = splinePrimitive->points();

    if (points.size() >= 3) { // Need at least 3 points to close
      QVector2D startPoint = points[0];
      float distanceToStart = (pos - startPoint).length();

      if (distanceToStart <= 20.0f) { // Close if within 20 pixels of start
        qDebug() << "Closing spline - clicked near starting point";
        splinePrimitive->setClosed(true);
        m_currentPrimitive->setColor(m_defaultDrawingColor);
        m_currentPrimitive->setSelected(true);
        addPrimitiveWithCommand(std::move(m_currentPrimitive));
        m_isDrawing = false;
        return;
      }
    }

    // Add point to spline
    qDebug() << "Adding point to spline:" << pos;
    splinePrimitive->addPoint(pos);
  }
}

void DrawingCanvas::handlePolygonTool(QMouseEvent *event) {
  qDebug() << "Polygon tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  QVector2D worldPos = screenToWorld(event->pos());
  QVector2D snappedPos = snapToGrid(worldPos);
  QVector2D pos = snapToLineEndpoint(snappedPos);

  if (event->button() == Qt::RightButton && m_isDrawing) {
    // Finalize polygon on right-click
    if (m_currentPrimitive) {
      auto *poly = static_cast<PolygonPrimitive *>(m_currentPrimitive.get());
      const auto pts = poly->getControlPoints();
      qDebug() << "Finalizing polygon with" << pts.size() << "points";

      // Ignore trivial polygons
      if (pts.size() < 3) {
        m_currentPrimitive.reset();
        m_isDrawing = false;
        return;
      }
      float minX = pts[0].x(), maxX = pts[0].x();
      float minY = pts[0].y(), maxY = pts[0].y();
      for (const auto &p : pts) { minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x()); minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y()); }
      const float w = maxX - minX, h = maxY - minY;
      if (w < 2.0f || h < 2.0f) {
        qDebug() << "Discarding tiny polygon (" << w << "," << h << ")";
        m_currentPrimitive.reset();
        m_isDrawing = false;
        return;
      }

      m_currentPrimitive->setColor(m_defaultDrawingColor);
      m_currentPrimitive->setSelected(true);

      // Apply default fill (tool Fill settings) on finalize
      poly->setFilled(m_defaultFillEnabled);
      if (m_defaultFillEnabled) {
        poly->setFillColor(m_defaultFillColor);
      } else {
        poly->clearFillColor();
      }

      addPrimitiveWithCommand(std::move(m_currentPrimitive));
    }
    m_isDrawing = false;
    return;
  }

  if (!m_isDrawing && event->button() == Qt::LeftButton) {
    // Start new polygon
    qDebug() << "Starting new polygon at position:" << pos;
    m_currentPrimitive = std::make_unique<PolygonPrimitive>();
    static_cast<PolygonPrimitive *>(m_currentPrimitive.get())->addPoint(pos);
    m_currentPrimitive->setColor(
        QColor(0, 255, 255)); // Cyan for better visibility
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    m_isDrawing = true;
  } else if (m_isDrawing && event->button() == Qt::LeftButton) {
    // Check if clicking near the starting point to close the polygon
    auto polygonPrimitive =
        static_cast<PolygonPrimitive *>(m_currentPrimitive.get());
    const auto &points = polygonPrimitive->getControlPoints();

    if (points.size() >= 3) { // Need at least 3 points to close
      QVector2D startPoint = points[0];
      float distanceToStart = (pos - startPoint).length();

      if (distanceToStart <= 20.0f) { // Close if within 20 pixels of start
        qDebug() << "Closing polygon - clicked near starting point";
        // Ignore trivial polygons
        float minX = points[0].x(), maxX = points[0].x();
        float minY = points[0].y(), maxY = points[0].y();
        for (const auto &p : points) { minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x()); minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y()); }
        const float w = maxX - minX, h = maxY - minY;
        if (w < 2.0f || h < 2.0f) {
          qDebug() << "Discarding tiny polygon (" << w << "," << h << ")";
          m_currentPrimitive.reset();
          m_isDrawing = false;
          return;
        }

        polygonPrimitive->setClosed(true);
        m_currentPrimitive->setColor(m_defaultDrawingColor);
        m_currentPrimitive->setSelected(true);

        // Apply default fill (tool Fill settings) on finalize
        polygonPrimitive->setFilled(m_defaultFillEnabled);
        if (m_defaultFillEnabled) {
          polygonPrimitive->setFillColor(m_defaultFillColor);
        } else {
          polygonPrimitive->clearFillColor();
        }

        addPrimitiveWithCommand(std::move(m_currentPrimitive));
        m_isDrawing = false;
        return;
      }
    }

    // Add point to polygon
    qDebug() << "Adding point to polygon:" << pos;
    polygonPrimitive->addPoint(pos);
  }
}

void DrawingCanvas::handleRectangleTool(QMouseEvent *event) {
  qDebug() << "Rectangle tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  if (!m_isDrawing) {
    m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
    m_drawCurrentPos = m_drawStartPos;

    // Create temporary rectangle
    m_currentPrimitive =
        std::make_unique<RectanglePrimitive>(m_drawStartPos, m_drawCurrentPos);
    m_currentPrimitive->setColor(
        QColor(0, 255, 255)); // Cyan for better visibility
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    if (auto *rect = dynamic_cast<RectanglePrimitive *>(m_currentPrimitive.get())) {
      rect->setFilled(m_defaultFillEnabled);
      if (m_defaultFillEnabled) {
        rect->setFillColor(m_defaultFillColor);
      }
    }
    qDebug() << "Rectangle created with line style:"
             << static_cast<int>(m_defaultLineStyle)
             << "line width:" << m_defaultLineWidth;
    m_isDrawing = true;
  }
  // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::handleEllipseTool(QMouseEvent *event) {
  qDebug() << "Ellipse tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  if (!m_isDrawing) {
    m_drawStartPos = snapToGrid(screenToWorld(event->pos()));

    // Create temporary ellipse
    m_currentPrimitive =
        std::make_unique<EllipsePrimitive>(m_drawStartPos, 10.0f, 10.0f);
    m_currentPrimitive->setColor(
        QColor(0, 255, 255)); // Cyan for better visibility
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    if (auto *ellipse = dynamic_cast<EllipsePrimitive *>(m_currentPrimitive.get())) {
      ellipse->setFilled(m_defaultFillEnabled);
      if (m_defaultFillEnabled) {
        ellipse->setFillColor(m_defaultFillColor);
      }
    }
    m_isDrawing = true;
  }
  // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::handleCircleTool(QMouseEvent *event) {
  qDebug() << "Circle tool called! m_isDrawing:" << m_isDrawing
           << "button:" << event->button();
  if (!m_isDrawing) {
    m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
    m_drawCurrentPos = m_drawStartPos;
    // Create temporary circle
    m_currentPrimitive =
        std::make_unique<CirclePrimitive>(m_drawStartPos, 1.0f);
    m_currentPrimitive->setColor(QColor(0, 255, 255));
    m_currentPrimitive->setLineStyle(
        m_defaultLineStyle); // Apply selected line style
    m_currentPrimitive->setLineWidth(
        m_defaultLineWidth); // Apply selected line width
    if (auto *circle = dynamic_cast<CirclePrimitive *>(m_currentPrimitive.get())) {
      circle->setFilled(m_defaultFillEnabled);
      if (m_defaultFillEnabled) {
        circle->setFillColor(m_defaultFillColor);
      }
    }
    m_isDrawing = true;
  }
  // Finalization handled in mouseReleaseEvent as part of circle branch
}

void DrawingCanvas::selectObjectsInRect(
    const QRectF &rect,
    SelectionManager::SelectionOperation operation) {
  m_selectionManager->selectObjectsInRect(rect, m_primitives, operation);
}

void DrawingCanvas::selectObjectsInLasso(
    const QPolygonF &polygon,
    SelectionManager::SelectionOperation operation) {
  m_selectionManager->selectObjectsInLasso(polygon, m_primitives, operation);
}

// Primitive management methods
void DrawingCanvas::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive) {
  if (primitive && m_layerManager) {
    qDebug() << "Adding primitive of type:"
             << static_cast<int>(primitive->type()) << "to active layer";

    // Connect ImagePrimitive signals before moving ownership
    if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(primitive.get())) {
      connect(imgPrim, &ImagePrimitive::detectionComplete, this,
              [this, imgPrim]() {
                qDebug() << "DrawingCanvas: Detection complete signal "
                            "received, updating canvas";
                update();
                // Emit signal so MainWindow can update mask UI
                emit maskDetectionComplete(imgPrim);
              });
      connect(imgPrim, &ImagePrimitive::detectionFailed, this,
              [this](const QString &error) {
                qDebug() << "DrawingCanvas: Detection failed:" << error;
                emit maskDetectionFailed(error);
              });
      connect(imgPrim, &ImagePrimitive::detectionProgress, this,
              [this](int percentage, const QString &message) {
                emit maskDetectionProgress(percentage, message);
              });
    }

    // Determine target layer: respect an existing layerId if possible (e.g. redo/load)
    Layer *targetLayer = nullptr;
    if (!primitive->layerId().isNull()) {
      targetLayer = m_layerManager->getLayer(primitive->layerId());
    }
    if (!targetLayer) {
      targetLayer = m_layerManager->activeLayer();
      if (!targetLayer) {
        targetLayer = m_layerManager->createLayer("Background");
        m_layerManager->setActiveLayer(targetLayer);
      }
      if (targetLayer) {
        primitive->setLayerId(targetLayer->id());
      }
    }

    // Store pointer before moving ownership for selection
    DrawingPrimitive *primitivePtr = primitive.get();

    // Add to layer manager
    if (targetLayer) {
      if (m_layerManager->activeLayer() &&
          targetLayer->id() == m_layerManager->activeLayer()->id()) {
        m_layerManager->addPrimitiveToActiveLayer(std::move(primitive));
      } else {
        m_layerManager->addPrimitiveToLayer(targetLayer->id(),
                                            std::move(primitive));
      }
    }

    // Auto-select if requested by creator (selected flag), or for Text
    if (primitivePtr && (primitivePtr->isSelected() || primitivePtr->type() == PrimitiveType::Text)) {
      qDebug() << "*** AUTO-SELECTING NEWLY CREATED PRIMITIVE ***";
      clearSelection();
      addToSelection(primitivePtr);
      emit selectionChanged();
    }

    update();
  } else if (primitive) {
    // Fallback to old behavior if no layer manager
    qDebug() << "Adding primitive of type:"
             << static_cast<int>(primitive->type())
             << "Total primitives:" << m_primitives.size() + 1;
    m_primitives.push_back(std::move(primitive));
    update();
  }
}

void DrawingCanvas::addPrimitiveWithCommand(
    std::unique_ptr<DrawingPrimitive> primitive) {
  if (primitive) {
    qDebug() << "Creating AddPrimitiveCommand for type:"
             << static_cast<int>(primitive->type());

    // Create AddPrimitiveCommand and emit it as raw pointer
    auto command =
        std::make_unique<AddPrimitiveCommand>(this, std::move(primitive));
    emit commandRequested(command.release()); // Release ownership to signal
  }
}

void DrawingCanvas::addTextPrimitive(TextPrimitive *primitive) {
  if (primitive) {
    // Wrap the raw pointer in a unique_ptr and add with command support
    auto uniquePrimitive = std::unique_ptr<DrawingPrimitive>(primitive);
    addPrimitiveWithCommand(std::move(uniquePrimitive));
    update();
  }
}

void DrawingCanvas::deleteSelectedPrimitivesWithCommand() {
  const auto &selected = m_selectionManager->selectedObjects();
  if (!selected.empty()) {
    qDebug() << "Deleting" << selected.size() << "primitives";

    auto command = std::make_unique<DeletePrimitivesCommand>(this, selected);
    emit commandRequested(command.release()); // Ownership transferred
  }
}

void DrawingCanvas::deleteSelectedPrimitives() {
  const auto &selected = m_selectionManager->selectedObjects();
  if (selected.empty()) {
    return;
  }

  // Create a set of IDs to delete for faster lookup
  std::set<QUuid> idsToDelete;
  for (DrawingPrimitive *primitive : selected) {
    if (primitive) {
      idsToDelete.insert(primitive->id());
    }
  }

  bool changed = false;
  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible() || layer->isLocked()) {
        continue;
      }

      auto &layerPrimitives = layer->primitives();
      auto it = std::remove_if(
          layerPrimitives.begin(), layerPrimitives.end(),
          [&idsToDelete](const std::unique_ptr<DrawingPrimitive> &p) {
            return p && idsToDelete.count(p->id()) > 0;
          });
      if (it != layerPrimitives.end()) {
        changed = true;
        layerPrimitives.erase(it, layerPrimitives.end());
      }
    }
  } else {
    // Remove from legacy primitives list
    auto it = std::remove_if(
        m_primitives.begin(), m_primitives.end(),
        [&idsToDelete](const std::unique_ptr<DrawingPrimitive> &p) {
          return p && idsToDelete.count(p->id()) > 0;
        });

    if (it != m_primitives.end()) {
        changed = true;
        m_primitives.erase(it, m_primitives.end());
    }
  }

  // Clear selection
  m_selectionManager->clearSelection();

  if (changed) {
    update();
  }
}

void DrawingCanvas::clearPrimitives() {
  m_selectionManager->clearSelection();
  m_primitives.clear();
  update();
}

QImage DrawingCanvas::renderToImage(int w, int h) {
  // Use canvas size if not specified
  if (w <= 0)
    w = this->width();
  if (h <= 0)
    h = this->height();

  // Grab what's currently displayed
  QImage image = this->grab().toImage();

  // Resize if different size requested
  if (image.width() != w || image.height() != h) {
    image = image.scaled(w, h, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  }

  qDebug() << "Rendered canvas to image:" << image.width() << "x"
           << image.height();
  return image;
}

// OpenGL shader helpers removed - no longer needed with QPainter rendering

// Measurement system implementations
QString DrawingCanvas::getUnitsString() const {
  switch (m_units) {
  case Units::Millimeters:
    return "mm";
  case Units::Centimeters:
    return "cm";
  case Units::Inches:
    return "in";
  default:
    return "mm";
  }
}

void DrawingCanvas::setUnits(Units units) {
  m_units = units;
  const QString unitStr = getUnitsString();
  const float ppu = static_cast<float>(pixelsPerUnit());

  auto syncDims = [&](const std::vector<std::unique_ptr<DrawingPrimitive>> &list) {
    for (const auto &primitive : list) {
      if (auto *dim = dynamic_cast<DimensionPrimitive *>(primitive.get())) {
        dim->setUnitsString(unitStr);
        dim->setPixelsPerUnit(ppu);
        dim->recalculateMeasurement();
      }
    }
  };

  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (layer) {
        syncDims(layer->primitives());
      }
    }
  } else {
    syncDims(m_primitives);
  }

  updateStatusBar();
}

float DrawingCanvas::worldToUnits(float worldDistance) const {
  // World coordinates are in pixels, convert to real units
  float mm = worldDistance / m_pixelsPerMM;
  switch (m_units) {
  case Units::Millimeters:
    return mm;
  case Units::Centimeters:
    return mm / 10.0f;
  case Units::Inches:
    return mm / 25.4f;
  default:
    return mm;
  }
}

float DrawingCanvas::unitsToWorld(float unitDistance) const {
  float mm;
  switch (m_units) {
  case Units::Millimeters:
    mm = unitDistance;
    break;
  case Units::Centimeters:
    mm = unitDistance * 10.0f;
    break;
  case Units::Inches:
    mm = unitDistance * 25.4f;
    break;
  default:
    mm = unitDistance;
    break;
  }
  return mm * m_pixelsPerMM;
}

double DrawingCanvas::pixelsPerUnit() const {
  switch (m_units) {
  case Units::Millimeters:
    return m_pixelsPerMM;
  case Units::Centimeters:
    return m_pixelsPerMM * 10.0;
  case Units::Inches:
    return m_pixelsPerMM * 25.4;
  }
  return m_pixelsPerMM;
}

QImage DrawingCanvas::getImageFromPrimitive(ImagePrimitive *img) {
  if (!img) return QImage();
  return img->image();
}

void DrawingCanvas::updateStatusBar() {
  // This will be called by MainWindow to update coordinates display
  update();
}

// Tool handlers
void DrawingCanvas::handleEraserTool(QMouseEvent *event) {
  QVector2D pos = screenToWorld(event->pos());
  const float eraserRadius = qMax(2.0f, m_eraserSize * 0.5f);

  qDebug() << "Eraser tool: erasing at position" << pos;

  if (m_layerManager) {
    std::vector<DrawingPrimitive *> toErase;
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible() || layer->isLocked()) continue;

      for (const auto &primitive : layer->primitives()) {
        if (primitive && primitive->containsPoint(pos, eraserRadius)) {
          toErase.push_back(primitive.get());
        }
      }
    }

    if (!toErase.empty()) {
      auto command = std::make_unique<DeletePrimitivesCommand>(this, toErase);
      emit commandRequested(command.release());
    } else {
      QToolTip::showText(event->globalPosition().toPoint(),
                         "Nothing to erase here. Increase eraser size or move over a shape.",
                         this);
    }
  } else {
    // Fallback
    auto it = std::remove_if(m_primitives.begin(), m_primitives.end(),
                       [&pos, eraserRadius](const std::unique_ptr<DrawingPrimitive> &primitive) {
                         if (primitive) return primitive->containsPoint(pos, eraserRadius);
                         return false;
                       });

    if (it != m_primitives.end()) {
      m_primitives.erase(it, m_primitives.end());
      m_selectionManager->clearSelection();
      update();
    } else {
      QToolTip::showText(event->globalPosition().toPoint(),
                         "Nothing to erase here. Increase eraser size or move over a shape.",
                         this);
    }
  }
}

void DrawingCanvas::handleFillTool(QMouseEvent *event) {
  QVector2D pos = screenToWorld(event->pos());

  if (event->button() == Qt::LeftButton) {
    
    // --- STABLE ORGANIC SPLASH ---
    auto createSplash = [&](const QVector2D& center, float scale) {
        srand(QDateTime::currentMSecsSinceEpoch());
        
        // Main puddle (composed of multiple overlapping circles for organic shape)
        int blobCount = 3 + (rand() % 3);
        for(int i=0; i<blobCount; i++) {
            float offsetR = (rand() % 10) * scale;
            float angle = (rand() % 360) * M_PI / 180.0f;
            QVector2D blobPos = center + QVector2D(cos(angle)*offsetR, sin(angle)*offsetR);
            float r = (15.0f + (rand() % 10)) * scale;
            
            auto blob = std::make_unique<CirclePrimitive>(blobPos, r);
            blob->setFilled(true);
            blob->setColor(m_defaultDrawingColor);
            blob->setFillColor(m_defaultDrawingColor);
            addPrimitiveWithCommand(std::move(blob));
        }

        // Drips/Splatter around
        int dropCount = 8 + (rand() % 8);
        for(int k=0; k<dropCount; k++) {
            float angle = (rand() % 360) * M_PI / 180.0f;
            float dist = (30.0f + (rand() % 40)) * scale;
            QVector2D dropPos = center + QVector2D(cos(angle)*dist, sin(angle)*dist);
            
            // Random size for drops
            float r = (2.0f + (rand() % 4)) * scale;
            
            auto drop = std::make_unique<CirclePrimitive>(dropPos, r);
            drop->setFilled(true);
            drop->setColor(m_defaultDrawingColor);
            drop->setFillColor(m_defaultDrawingColor);
            addPrimitiveWithCommand(std::move(drop));
        }
    };

    const float fillTolerance = 5.0f; 
    bool foundObject = false;

    if (m_layerManager) {
      const auto &layers = m_layerManager->layers();
      for (auto layerIt = layers.rbegin(); layerIt != layers.rend(); ++layerIt) {
        const auto &layer = *layerIt;
        if (!layer || !layer->isVisible() || layer->isLocked()) continue;

        const auto &primitives = layer->primitives();
        for (auto primIt = primitives.rbegin(); primIt != primitives.rend(); ++primIt) {
          const auto &primitive = *primIt;
          if (primitive) {
            bool containsPoint = primitive->containsPoint(pos, fillTolerance);
            
            // Check bounding boxes for simpler hit testing on closed shapes
            if (!containsPoint) {
                if (primitive->type() == PrimitiveType::Rectangle || 
                    primitive->type() == PrimitiveType::Circle || 
                    primitive->type() == PrimitiveType::Ellipse) {
                    containsPoint = primitive->boundingRect().contains(pos.x(), pos.y());
                }
            }

            if (containsPoint) {
              foundObject = true;
              bool splash = (m_fillMode == FillMode::Splash);

              // Capture old state for undo
              QJsonObject oldState = primitive->toJson();

              // Fill the object (support common closed shapes)
              switch (primitive->type()) {
                case PrimitiveType::Rectangle: {
                  auto *p = static_cast<RectanglePrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  if (splash)
                    createSplash(QVector2D(p->boundingRect().bottomRight()), 0.6f);
                  break;
                }
                case PrimitiveType::Circle: {
                  auto *p = static_cast<CirclePrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  if (splash) createSplash(p->center(), 0.7f);
                  break;
                }
                case PrimitiveType::Ellipse: {
                  auto *p = static_cast<EllipsePrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  break;
                }
                case PrimitiveType::Polygon: {
                  auto *p = static_cast<PolygonPrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  break;
                }
                case PrimitiveType::Spline: {
                  auto *p = static_cast<SplinePrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  break;
                }
                case PrimitiveType::Curve: {
                  auto *p = static_cast<CurvePrimitive *>(primitive.get());
                  p->setFilled(true);
                  p->setFillColor(m_defaultDrawingColor);
                  break;
                }
                default:
                  break;
              }

              // Push undo command for the property change
              auto cmd = std::make_unique<ModifyPrimitiveCommand>(primitive.get(), "Fill");
              cmd->storePropertyChange(oldState, primitive->toJson());
              cmd->markAlreadyApplied();
              if (cmd->hasStateChange()) {
                emit commandRequested(cmd.release());
              }

              // Keep the filled object selected for follow-up edits
              clearSelection();
              primitive->setSelected(true);
              m_selectionManager->addToSelection(primitive.get());
              emit selectionChanged();

              break;
            }
}
        if (foundObject) break;
      }
    }

    // Floor Splash
    if (!foundObject) {
        if (m_fillMode == FillMode::Splash) {
            createSplash(pos, 1.0f);
        } else {
            QToolTip::showText(event->globalPosition().toPoint(),
                         "Fill works inside closed shapes.\nClick a shape or enable Splash Mode.",
                         this);
        }
    }

    update();
  }
}

}

// Removed leftover boolean operation code

void DrawingCanvas::handleMeasureTool(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  if (m_isDrawing) {
    return; // Drag/finalize handled in mouseMove/mouseRelease
  }

  m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
  m_drawCurrentPos = m_drawStartPos;

  auto dimension =
      std::make_unique<DimensionPrimitive>(m_drawStartPos, m_drawCurrentPos);
  dimension->setUnitsString(getUnitsString());
  dimension->setPixelsPerUnit(static_cast<float>(pixelsPerUnit()));
  dimension->recalculateMeasurement();
  dimension->setColor(QColor(255, 200, 0));
  dimension->setSelected(true);

  m_currentPrimitive = std::move(dimension);
  m_isDrawing = true;
  emit smartHintChanged(QStringLiteral("Measuring — drag to set length, release to place"));
}

void DrawingCanvas::renderDimensionTexts(QPainter& painter) {
  painter.save();
  painter.resetTransform();

  QFont font("Arial", 10);
  painter.setFont(font);
  painter.setPen(QColor(255, 200, 0));

  auto renderAll = [&](const std::vector<std::unique_ptr<DrawingPrimitive>> &list) {
    for (const auto &primitive : list) {
      if (auto *dimension = dynamic_cast<DimensionPrimitive *>(primitive.get())) {
        renderDimensionText(&painter, dimension);
      }
    }
  };

  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible()) {
        continue;
      }
      renderAll(layer->primitives());
    }
  } else {
    renderAll(m_primitives);
  }

  if (m_currentPrimitive) {
    if (auto *dimension = dynamic_cast<DimensionPrimitive *>(m_currentPrimitive.get())) {
      renderDimensionText(&painter, dimension);
    }
  }

  painter.restore();
}

void DrawingCanvas::renderTextPrimitives(QPainter& painter)
{
  painter.save();
  painter.resetTransform();
  painter.setRenderHint(QPainter::TextAntialiasing);

  auto renderOne = [&](TextPrimitive *textPrim) {
    if (!textPrim || !textPrim->isVisible()) {
      return;
    }

    if (textPrim->followsSpline()) {
      renderTextOnSpline(textPrim, painter);
      return;
    }

    const QVector2D worldPos = textPrim->position();
    const QPoint screenPos = worldToScreen(worldPos);

    const int scaledFontSize = std::max(
        1, static_cast<int>(textPrim->fontSize() * m_zoomLevel * textPrim->scale()));
    QFont font(textPrim->fontFamily(), scaledFontSize);
    font.setBold(textPrim->isBold());
    font.setItalic(textPrim->isItalic());

    // Never draw selection box/handles here — renderSelection() handles that
    // in world-space coordinates where hit-testing works correctly.
    if (textPrim->rotation() != 0.0f) {
      painter.save();
      painter.translate(screenPos);
      painter.rotate(textPrim->rotation() * 180.0f / M_PI);
      renderFormattedText(&painter, textPrim, QPoint(0, 0), font, false);
      painter.restore();
    } else {
      renderFormattedText(&painter, textPrim, screenPos, font, false);
    }
  };

  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible()) {
        continue;
      }
      for (const auto &primitive : layer->primitives()) {
        renderOne(dynamic_cast<TextPrimitive *>(primitive.get()));
      }
    }
  } else {
    for (const auto &primitive : m_primitives) {
      renderOne(dynamic_cast<TextPrimitive *>(primitive.get()));
    }
  }

  painter.restore();
}

// applyWorldMatrices and applyScreenMatrices removed - using setupWorldTransform() instead

void DrawingCanvas::handleMoveTool(QMouseEvent *event) {
  qDebug() << "Move tool called! button:" << event->button();
  QVector2D worldPos = screenToWorld(event->pos());

  if (event->button() == Qt::LeftButton) {
    qDebug() << "Move tool: Left button pressed, selected objects count:"
             << m_selectionManager->selectedObjects().size();

    // First check if we clicked on a control point of a selected object
    // BUT: For ImagePrimitives, we want to move them, not edit control points
    float controlPointTolerance = 8.0f / m_zoomLevel;
    bool clickedOnControlPoint = false;

    for (auto selectedObj : m_selectionManager->selectedObjects()) {
      if (selectedObj->isSelected()) {
        // Skip control point editing for ImagePrimitives - they should just
        // move
        if (dynamic_cast<ImagePrimitive *>(selectedObj)) {
          qDebug() << "Skipping control point check for ImagePrimitive";
          continue;
        }

        m_editingPrimitive = selectedObj;
        int controlPointIndex =
            findControlPointAt(worldPos, controlPointTolerance);
        if (controlPointIndex >= 0) {
          qDebug() << "Control point" << controlPointIndex
                   << "clicked for editing (from Move tool)";
          startControlPointEdit(selectedObj, controlPointIndex);
          clickedOnControlPoint = true;
          return;
        }
      }
    }

    if (clickedOnControlPoint) {
      return;
    }

    // Text resize/rotate is handled in handleSelectTool with 8 control points
    // This duplicate code removed to avoid conflicts

    // If we have selected objects, we should be able to move them
    // Check if we clicked on any selected object
    bool clickedOnSelected = false;
    DrawingPrimitive *clickedObject = nullptr;

    // First check if we clicked on any already-selected object
    for (auto *selectedObj : m_selectionManager->selectedObjects()) {
      if (selectedObj &&
          selectedObj->containsPoint(worldPos, 5.0f / m_zoomLevel)) {
        clickedOnSelected = true;
        clickedObject = selectedObj;
        qDebug() << "Clicked on already-selected object";
        break;
      }
    }

    // If we didn't click on a selected object, find any object at this position
    if (!clickedOnSelected) {
      qDebug() << "Searching for primitive at" << worldPos.x() << ","
               << worldPos.y();
      clickedObject = findPrimitiveAt(worldPos);
      qDebug() << "Found primitive:" << (clickedObject ? "YES" : "NO");
    }

    if (clickedObject) {
      qDebug() << "Clicked object type:"
               << static_cast<int>(clickedObject->type());
      qDebug() << "Is ImagePrimitive:"
               << (dynamic_cast<ImagePrimitive *>(clickedObject) ? "YES"
                                                                 : "NO");

      // Images always add to selection; other objects replace unless Shift/⌘
      if (!clickedObject->isSelected()) {
        const bool isImage = dynamic_cast<ImagePrimitive *>(clickedObject) != nullptr;
        const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
        const bool additive =
            isImage ||
            mods.testFlag(Qt::ShiftModifier) ||
            mods.testFlag(Qt::ControlModifier) ||
            mods.testFlag(Qt::MetaModifier);
        if (!additive) {
          clearSelection();
        }
        clickedObject->setSelected(true);
        if (auto *img = dynamic_cast<ImagePrimitive *>(clickedObject))
          img->setMaskOverlayVisible(true);
        m_selectionManager->addToSelection(clickedObject);
        emit selectionChanged();
        qDebug() << "Move tool: Selected object before moving (additive="
                 << additive << ")";
      }

      // Start move operation - THIS IS CRITICAL
      m_isMoving = true;
      m_moveStartPos = worldPos;
      m_totalMoveOffset = QVector2D(0, 0);

      qDebug() << "=== MOVE OPERATION STARTED ===";
      qDebug() << "m_isMoving =" << m_isMoving;
      qDebug() << "m_moveStartPos =" << m_moveStartPos.x() << ","
               << m_moveStartPos.y();
      qDebug() << "Selected objects count:"
               << m_selectionManager->selectedObjects().size();

      // Use appropriate cursor based on object type
      if (dynamic_cast<TextPrimitive *>(clickedObject)) {
        setCursor(Qt::IBeamCursor); // Text cursor for text objects
      } else {
        setCursor(Qt::SizeAllCursor); // Size cursor for other objects
      }
    } else {
      qDebug() << "No object found at click position - cannot start move";
    }
  }
}

void DrawingCanvas::handleMoveOperation(const QVector2D &worldPos) {
  qDebug() << "handleMoveOperation called, worldPos:" << worldPos.x() << ","
           << worldPos.y();

  // Constraint / free-move modifiers (copy must already be armed at press)
  const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();

  // Update alignment guides
  updateAlignmentGuides(worldPos);

  QVector2D currentOffset = worldPos - m_moveStartPos;
  // Angle-line segments slide only along their previously connected baseline
  currentOffset = constrainDeltaAlongConnectedLine(currentOffset, mods);
  QVector2D deltaOffset = currentOffset - m_totalMoveOffset;

  qDebug() << "deltaOffset:" << deltaOffset.x() << "," << deltaOffset.y();
  qDebug() << "selected objects:"
           << m_selectionManager->selectedObjects().size();

  if (!m_selectionManager->selectedObjects().empty()) {
    for (auto *obj : m_selectionManager->selectedObjects()) {
      if (obj) {
        qDebug() << "Calling translate on object";
        obj->translate(deltaOffset);
      }
    }
    m_totalMoveOffset = currentOffset;
    update();
    qDebug() << "Canvas updated";
  } else {
    qDebug() << "WARNING: No selected objects to move!";
  }
}

bool DrawingCanvas::isCopyAlongModifier(Qt::KeyboardModifiers mods) {
  return mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
}

bool DrawingCanvas::beginCopyAlongConnectedLineMove() {
  if (m_isCopyAlongMove)
    return true;

  std::vector<LinePrimitive *> sources;
  for (auto *obj : selectedObjects()) {
    auto *line = dynamic_cast<LinePrimitive *>(obj);
    if (line && resolveLineMoveConstraint(line).lengthSquared() > 1e-8f)
      sources.push_back(line);
  }
  if (sources.empty())
    return false;

  // Leave originals in place; drag clones along the constraint
  clearSelection();
  for (LinePrimitive *src : sources) {
    if (!src)
      continue;
    auto clone = src->clone();
    if (!clone)
      continue;
    clone->setSelected(true);
    // Keep connection + constraint so the copy also slides along the baseline
    DrawingPrimitive *raw = clone.get();
    addPrimitiveWithCommand(std::move(clone));
    if (raw)
      m_selectionManager->addToSelection(raw);
  }

  m_isCopyAlongMove = true;
  emit selectionChanged();
  emit smartHintChanged(
      QStringLiteral("Copying along connected line · release to place"));
  update();
  return true;
}

void DrawingCanvas::finishMoveOperation() {
  m_isMoving = false;

  // Set appropriate cursor based on current tool
  if (m_currentTool == DrawingTool::Move) {
    setCursor(Qt::SizeAllCursor);
  } else {
    setCursor(Qt::ArrowCursor);
  }

  if (m_totalMoveOffset.length() > 1.0f) {
    qDebug() << "Finished moving objects. Total offset:" << m_totalMoveOffset;

    if (!m_selectionManager->selectedObjects().empty()) {
      auto command = std::make_unique<MovePrimitivesCommand>(
          m_selectionManager->selectedObjects(), m_totalMoveOffset);
      emit commandRequested(command.release());
    }
    if (m_isCopyAlongMove) {
      emit smartHintChanged(
          QStringLiteral("Copied along connected line"));
    }
  } else if (m_isCopyAlongMove) {
    // No real drag — discard zero-offset duplicates
    deleteSelectedPrimitivesWithCommand();
    emit smartHintChanged(QString());
  }

  m_isCopyAlongMove = false;
  m_originalPositions.clear();
}

void DrawingCanvas::renderDimensionText(QPainter *painter,
                                        DimensionPrimitive *dimension) {
  if (!dimension || !dimension->isVisible())
    return;

  // Get dimension line endpoints in world coordinates
  QVector2D start = dimension->getStart();
  QVector2D end = dimension->getEnd();

  // Calculate text position (midpoint of dimension line with offset)
  QVector2D direction = (end - start).normalized();
  QVector2D perpendicular(-direction.y(), direction.x());
  float offset = 25.0f; // Text offset from measured line

  // Convert world coordinates to screen coordinates
  QPoint screenStart = worldToScreen(start);
  QPoint screenEnd = worldToScreen(end);
  QVector2D screenMidpoint((screenStart + screenEnd) / 2.0);
  QVector2D screenPerp(perpendicular * offset);

  QPoint textPos = (screenMidpoint + screenPerp).toPoint();

  // Format measurement text from live geometry (correct units)
  QString text = dimension->getDisplayText();

  // Draw text with background
  QFontMetrics fm(painter->font());
  QRect textRect = fm.boundingRect(text);
  textRect.moveCenter(textPos);

  // Draw background rectangle
  painter->fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 128));

  // Draw text
  painter->setPen(QColor(255, 255, 255));
  painter->drawText(textRect, Qt::AlignCenter, text);
}

void DrawingCanvas::renderRulerTexts(QPainter& painter) {
  if (!m_rulersVisible)
    return;

  painter.save();
  painter.resetTransform();

  // Get viewport bounds in world coordinates
  QRectF viewportWorld =
      QRectF(-1000, -1000, 2000, 2000); // Simple bounds for now

  // Classic ruler with fewer numbers - show only major divisions
  float majorSpacing = 100.0f; // Show numbers every 100 units for classic look

  // Calculate unit conversion for display
  float unitScale = 1.0f;
  QString unitSuffix;
  switch (m_units) {
  case Units::Millimeters:
    unitScale = 1.0f;
    unitSuffix = "mm";
    majorSpacing = 100.0f; // 100mm intervals
    break;
  case Units::Centimeters:
    unitScale = 0.1f;
    unitSuffix = "cm";
    majorSpacing = 10.0f; // 10cm intervals
    break;
  case Units::Inches:
    unitScale = 0.0393701f;
    unitSuffix = "in";
    majorSpacing = 5.0f; // 5 inch intervals
    break;
  }

  painter.setPen(QColor(80, 80, 80)); // Darker gray for better readability
  QFont font("Arial", 9, QFont::Bold);
  painter.setFont(font);

  // Draw horizontal ruler numbers (only major divisions)
  float startX = std::floor(viewportWorld.left() / majorSpacing) * majorSpacing;
  float endX = std::ceil(viewportWorld.right() / majorSpacing) * majorSpacing;

  for (float x = startX; x <= endX; x += majorSpacing) {
    QPoint screenPos = worldToScreen(QVector2D(x, viewportWorld.top()));
    if (screenPos.x() >= 25 && screenPos.x() <= width() - 10) {
      float displayValue = x * unitScale;
      QString text;
      if (displayValue == (int)displayValue) {
        text = QString::number((int)displayValue);
      } else {
        text = QString::number(displayValue, 'f', 1);
      }

      // Center the text below the tick mark
      QRect textRect = painter.fontMetrics().boundingRect(text);
      painter.drawText(screenPos.x() - textRect.width() / 2, 20, text);
    }
  }

  // Draw vertical ruler numbers (only major divisions)
  float startY = std::floor(viewportWorld.top() / majorSpacing) * majorSpacing;
  float endY = std::ceil(viewportWorld.bottom() / majorSpacing) * majorSpacing;

  for (float y = startY; y <= endY; y += majorSpacing) {
    QPoint screenPos = worldToScreen(QVector2D(viewportWorld.left(), y));
    if (screenPos.y() >= 25 && screenPos.y() <= height() - 10) {
      float displayValue = y * unitScale;
      QString text;
      if (displayValue == (int)displayValue) {
        text = QString::number((int)displayValue);
      } else {
        text = QString::number(displayValue, 'f', 1);
      }

      // Rotate and position text for vertical ruler
      painter.save();
      painter.translate(15, screenPos.y() + 5);
      painter.rotate(-90);
      painter.drawText(0, 0, text);
      painter.restore();
    }
  }

  // Draw origin indicator
  QPoint originScreen = worldToScreen(QVector2D(0, 0));
  if (originScreen.x() >= 25 && originScreen.x() <= width() - 10 &&
      originScreen.y() >= 25 && originScreen.y() <= height() - 10) {

    painter.setPen(QColor(200, 0, 0)); // Red for origin
    QFont originFont("Arial", 8, QFont::Bold);
    painter.setFont(originFont);
    painter.drawText(originScreen.x() - 3, 18, "0");
    painter.save();
    painter.translate(15, originScreen.y() + 3);
    painter.rotate(-90);
    painter.drawText(0, 0, "0");
    painter.restore();
  }

  painter.restore(); // restore from resetTransform
}

void DrawingCanvas::renderControlPoint(QPainter& painter, const QVector2D &point,
                                       bool highlighted) {
  const float r = (highlighted ? 4.2f : 3.4f) / m_zoomLevel;

  // Soft shadow
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(15, 23, 42, 40));
  painter.drawEllipse(QPointF(point.x(), point.y()), r + 0.7f / m_zoomLevel,
                      r + 0.7f / m_zoomLevel);

  // White disc + blue ring (matches selection handles)
  QPen ring(highlighted ? QColor(34, 197, 94) : QColor(37, 99, 235), 1.3);
  ring.setCosmetic(true);
  painter.setPen(ring);
  painter.setBrush(Qt::white);
  painter.drawEllipse(QPointF(point.x(), point.y()), r, r);

  // Inner accent
  painter.setPen(Qt::NoPen);
  painter.setBrush(highlighted ? QColor(34, 197, 94) : QColor(59, 130, 246));
  painter.drawEllipse(QPointF(point.x(), point.y()), r * 0.32f, r * 0.32f);
}

void DrawingCanvas::startControlPointEdit(DrawingPrimitive *primitive,
                                          int controlPointIndex) {
  m_editingPrimitive = primitive;
  m_selectedControlPoint = controlPointIndex;
  m_isEditingControlPoints = true;
  m_controlEditOrigState = QJsonObject();

  if (!primitive) {
    return;
  }

  m_controlEditOrigState = primitive->toJson();

  // Store original position for undo
  if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(primitive);
      imgPrim && imgPrim->isEditMode()) {
    const auto contour = imgPrim->getEditableContour();
    if (controlPointIndex >= 0 &&
        controlPointIndex < static_cast<int>(contour.size())) {
      const QPointF maskPoint = contour[controlPointIndex];
      m_originalControlPointPosition = QVector2D(maskPoint.x(), maskPoint.y());
    }
  } else {
    const auto controlPoints = primitive->getControlPoints();
    if (controlPointIndex >= 0 &&
        controlPointIndex < static_cast<int>(controlPoints.size())) {
      m_originalControlPointPosition = controlPoints[controlPointIndex];
    }
  }

  qDebug() << "Started editing control point" << controlPointIndex
           << "for primitive";
}

void DrawingCanvas::updateControlPoint(const QVector2D &newPos) {
  if (!m_isEditingControlPoints || !m_editingPrimitive ||
      m_selectedControlPoint < 0) {
    return;
  }

  // Handle ImagePrimitive mask control points specially
  if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(m_editingPrimitive)) {
    if (imgPrim->isEditMode()) {
      imgPrim->moveControlPoint(m_selectedControlPoint, newPos);
      update();
      return;
    }
  }

  // Use the generic control point interface for other primitives
  std::vector<QVector2D> controlPoints = m_editingPrimitive->getControlPoints();
  if (m_selectedControlPoint < static_cast<int>(controlPoints.size())) {
    QVector2D snappedPos = snapToGrid(newPos);

    // Angle-line endpoint constraints:
    // - connected end (start): slide along previous baseline
    // - free end: move along this segment (length only), Alt = free
    if (auto *line = dynamic_cast<LinePrimitive *>(m_editingPrimitive)) {
      const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
      if (m_selectedControlPoint == 0) {
        QVector2D constrained = snappedPos;
        if (auto *prev = dynamic_cast<LinePrimitive *>(
                findPrimitiveById(line->connectedLineId()))) {
          constrained = projectPointOntoLineSegment(
              snappedPos, prev->startPoint(), prev->endPoint());
        } else {
          const QVector2D dir = resolveLineMoveConstraint(line);
          if (dir.lengthSquared() > 1e-8f) {
            const QVector2D origin = m_originalControlPointPosition;
            const float t = QVector2D::dotProduct(snappedPos - origin, dir);
            constrained = origin + dir * t;
          }
        }
        const QVector2D delta = constrained - line->startPoint();
        line->setStartPoint(constrained);
        line->setEndPoint(line->endPoint() + delta);
        update();
        return;
      }
      if (m_selectedControlPoint == 1 && !mods.testFlag(Qt::AltModifier)) {
        QVector2D axis = line->endPoint() - line->startPoint();
        if (axis.lengthSquared() < 1e-8f)
          axis = QVector2D(1.0f, 0.0f);
        else
          axis.normalize();
        const float t =
            QVector2D::dotProduct(snappedPos - line->startPoint(), axis);
        snappedPos = line->startPoint() + axis * std::max(0.0f, t);
      }
    }

    m_editingPrimitive->setControlPointPosition(m_selectedControlPoint,
                                                snappedPos);
    update();
  }
}

void DrawingCanvas::finishControlPointEdit() {
  // Create command for control point modification if there was a change
  if (m_editingPrimitive && m_selectedControlPoint >= 0) {
    // Handle ImagePrimitive mask control points specially
    if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(m_editingPrimitive);
        imgPrim && imgPrim->isEditMode()) {
      const auto contour = imgPrim->getEditableContour();
      if (m_selectedControlPoint < static_cast<int>(contour.size())) {
        const QPointF currentPosition = contour[m_selectedControlPoint];
        const QPointF originalPosition(m_originalControlPointPosition.x(),
                                       m_originalControlPointPosition.y());

        // Only create command if position actually changed
        const QVector2D diff(currentPosition.x() - originalPosition.x(),
                             currentPosition.y() - originalPosition.y());
        if (diff.length() > 0.1f) {
          auto command = std::make_unique<ModifyMaskControlPointCommand>(
              imgPrim, m_selectedControlPoint, originalPosition, currentPosition);
          emit commandRequested(command.release());
        }
      }
    } else {
      // Handle regular primitive control points
      const auto controlPoints = m_editingPrimitive->getControlPoints();
      if (m_selectedControlPoint < static_cast<int>(controlPoints.size())) {
        const QVector2D currentPosition = controlPoints[m_selectedControlPoint];
        const bool moved =
            (currentPosition - m_originalControlPointPosition).length() > 0.1f;

        // Connected angle-line start slides the whole segment — need full-state undo
        auto *line = dynamic_cast<LinePrimitive *>(m_editingPrimitive);
        const bool slideAlongPrev =
            line && m_selectedControlPoint == 0 &&
            (!line->connectedLineId().isNull() ||
             line->moveConstraintDirection().lengthSquared() > 1e-8f);

        if (moved && slideAlongPrev && !m_controlEditOrigState.isEmpty()) {
          auto command = std::make_unique<TransformPrimitivesCommand>(
              std::vector<DrawingPrimitive *>{m_editingPrimitive},
              TransformPrimitivesCommand::Resize,
              QStringLiteral("Slide along line"));
          command->storeTransformation({m_controlEditOrigState},
                                       {m_editingPrimitive->toJson()});
          emit commandRequested(command.release());
        } else if (moved) {
          auto command = std::make_unique<ModifyControlPointCommand>(
              m_editingPrimitive, m_selectedControlPoint,
              m_originalControlPointPosition, currentPosition);
          emit commandRequested(command.release());
        }
      }
    }
  }

  m_isEditingControlPoints = false;
  m_selectedControlPoint = -1;
  m_editingPrimitive = nullptr;
  m_controlEditOrigState = QJsonObject();
  update();
}

// Removed all leftover boolean operation code

// Magnetic connection functionality
void DrawingCanvas::setMagneticConnectionEnabled(bool enabled) {
  m_magneticConnectionEnabled = enabled;
}

void DrawingCanvas::setMagneticConnectionTolerance(float tolerance) {
  m_magneticConnectionTolerance = tolerance;
}

QVector2D DrawingCanvas::findNearestLineEndpoint(const QVector2D &pos,
                                                 float tolerance) {
  QVector2D nearestPoint = pos;
  float minDistance = tolerance;

  // Search through layer manager if available
  if (m_layerManager) {
    const auto &layers = m_layerManager->layers();
    for (const auto &layer : layers) {
      if (!layer || !layer->isVisible())
        continue;

      const auto &primitives = layer->primitives();
      for (const auto &primitive : primitives) {
        if (!primitive || !primitive->isVisible())
          continue;

        if (auto line = dynamic_cast<LinePrimitive *>(primitive.get())) {
          // Check start point
          float distToStart = (line->startPoint() - pos).length();
          if (distToStart < minDistance) {
            nearestPoint = line->startPoint();
            minDistance = distToStart;
          }

          // Check end point
          float distToEnd = (line->endPoint() - pos).length();
          if (distToEnd < minDistance) {
            nearestPoint = line->endPoint();
            minDistance = distToEnd;
          }
        }
      }
    }
  } else {
    // Fallback: search in m_primitives (legacy mode)
    for (const auto &primitive : m_primitives) {
      if (auto line = dynamic_cast<LinePrimitive *>(primitive.get())) {
        // Check start point
        float distToStart = (line->startPoint() - pos).length();
        if (distToStart < minDistance) {
          nearestPoint = line->startPoint();
          minDistance = distToStart;
        }

        // Check end point
        float distToEnd = (line->endPoint() - pos).length();
        if (distToEnd < minDistance) {
          nearestPoint = line->endPoint();
          minDistance = distToEnd;
        }
      }
    }
  }

  return nearestPoint;
}

bool DrawingCanvas::pointInPolygon(
    const QVector2D &point, const std::vector<QVector2D> &polygon) const {
  if (polygon.size() < 3) {
    return false;
  }

  int crossings = 0;
  int n = polygon.size();

  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;

    QVector2D p1 = polygon[i];
    QVector2D p2 = polygon[j];

    // Check if the ray from point crosses the edge from p1 to p2
    if (((p1.y() <= point.y()) && (point.y() < p2.y())) ||
        ((p2.y() <= point.y()) && (point.y() < p1.y()))) {

      // Calculate x-coordinate of intersection
      float xIntersect =
          p1.x() + (point.y() - p1.y()) * (p2.x() - p1.x()) / (p2.y() - p1.y());

      if (point.x() < xIntersect) {
        crossings++;
      }
    }
  }

  // Odd number of crossings means point is inside
  return (crossings % 2) == 1;
}

QString DrawingCanvas::paperFormatName() const {
  switch (m_paperFormat) {
  case PaperFormat::A4:
    return "A4 (210×297mm)";
  case PaperFormat::A3:
    return "A3 (297×420mm)";
  case PaperFormat::A2:
    return "A2 (420×594mm)";
  case PaperFormat::A1:
    return "A1 (594×841mm)";
  case PaperFormat::A0:
    return "A0 (841×1189mm)";
  case PaperFormat::Letter:
    return "Letter (8.5×11in)";
  case PaperFormat::Legal:
    return "Legal (8.5×14in)";
  case PaperFormat::Tabloid:
    return "Tabloid (11×17in)";
  case PaperFormat::Custom:
    return "Custom";
  default:
    return "Unknown";
  }
}

void DrawingCanvas::renderPaper(QPainter& painter) {
  if (!m_renderer) {
    return;
  }

  painter.save();
  setupWorldTransform(painter);

  // Convert paper size from mm to pixels
  float paperWidth = m_paperSize.width() * m_pixelsPerMM;
  float paperHeight = m_paperSize.height() * m_pixelsPerMM;

  // Center the paper at origin
  float left = -paperWidth / 2.0f;
  float right = paperWidth / 2.0f;
  float bottom = -paperHeight / 2.0f;
  float top = paperHeight / 2.0f;

  // Draw shadow for paper effect FIRST (behind paper)
  float shadowOffset = 3.0f;
  {
    auto shadowBatch = m_renderer->begin(CanvasRenderer::Mode::TriangleFan);
    QVector4D shadowColor(0.5f, 0.5f, 0.5f, 0.3f);
    m_renderer->addVertex(shadowBatch,
                          QVector2D(left + shadowOffset, bottom - shadowOffset),
                          shadowColor);
    m_renderer->addVertex(
        shadowBatch, QVector2D(right + shadowOffset, bottom - shadowOffset),
        shadowColor);
    m_renderer->addVertex(shadowBatch,
                          QVector2D(right + shadowOffset, top - shadowOffset),
                          shadowColor);
    m_renderer->addVertex(shadowBatch,
                          QVector2D(left + shadowOffset, top - shadowOffset),
                          shadowColor);
    m_renderer->submit(shadowBatch);
  }

  // Draw paper background using configurable color
  {
    auto paperBatch = m_renderer->begin(CanvasRenderer::Mode::TriangleFan);
    QVector4D paperColor(m_paperColor.redF(), m_paperColor.greenF(),
                         m_paperColor.blueF(), 1.0f);
    m_renderer->addVertex(paperBatch, QVector2D(left, bottom), paperColor);
    m_renderer->addVertex(paperBatch, QVector2D(right, bottom), paperColor);
    m_renderer->addVertex(paperBatch, QVector2D(right, top), paperColor);
    m_renderer->addVertex(paperBatch, QVector2D(left, top), paperColor);
    m_renderer->submit(paperBatch);
  }

  // Draw paper border
  {
    auto borderBatch = m_renderer->begin(CanvasRenderer::Mode::LineLoop, 2.0f);
    QColor borderColor(179, 179, 179);
    m_renderer->addVertex(borderBatch, QVector2D(left, bottom), borderColor);
    m_renderer->addVertex(borderBatch, QVector2D(right, bottom), borderColor);
    m_renderer->addVertex(borderBatch, QVector2D(right, top), borderColor);
    m_renderer->addVertex(borderBatch, QVector2D(left, top), borderColor);
    m_renderer->submit(borderBatch);
  }

  painter.restore();
}

void DrawingCanvas::setPaperFormat(PaperFormat format) {
  m_paperFormat = format;

  // Set paper size based on format
  switch (format) {
  case PaperFormat::A4:
    m_paperSize = QSizeF(210.0f, 297.0f);
    break;
  case PaperFormat::A3:
    m_paperSize = QSizeF(297.0f, 420.0f);
    break;
  case PaperFormat::A2:
    m_paperSize = QSizeF(420.0f, 594.0f);
    break;
  case PaperFormat::A1:
    m_paperSize = QSizeF(594.0f, 841.0f);
    break;
  case PaperFormat::A0:
    m_paperSize = QSizeF(841.0f, 1189.0f);
    break;
  case PaperFormat::Letter:
    m_paperSize = QSizeF(216.0f, 279.0f);
    break;
  case PaperFormat::Legal:
    m_paperSize = QSizeF(216.0f, 356.0f);
    break;
  case PaperFormat::Tabloid:
    m_paperSize = QSizeF(279.0f, 432.0f);
    break;
  case PaperFormat::Custom:
  default:
    // Keep current size for custom format
    break;
  }

  update();
}

DrawingPrimitive *DrawingCanvas::findPrimitiveAt(const QVector2D &pos,
                                                 float tolerance) {
  if (m_layerManager) {
    // Search through layers (reverse order to get topmost first)
    const auto &layers = m_layerManager->layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
      const auto &layer = *it;
      if (!layer || !layer->isVisible() || layer->isLocked()) {
        continue;
      }

      const auto &primitives = layer->primitives();
      for (auto primIt = primitives.rbegin(); primIt != primitives.rend();
           ++primIt) {
        const auto &primitive = *primIt;
        if (primitive && primitive->isVisible() &&
            primitive->containsPoint(pos, tolerance)) {
          return primitive.get();
        }
      }
    }
  } else {
    // Fallback to legacy mode
    for (auto it = m_primitives.rbegin(); it != m_primitives.rend(); ++it) {
      const auto &primitive = *it;
      if (primitive && primitive->containsPoint(pos, tolerance)) {
        return primitive.get();
      }
    }
  }

  return nullptr;
}

void DrawingCanvas::handleImageTool(QMouseEvent *event) {
  // Only react to press — opening a native file dialog synchronously here
  // makes the subsequent mouse-up land on the dialog and dismiss it (macOS).
  if (event->type() != QEvent::MouseButtonPress ||
      event->button() != Qt::LeftButton) {
    return;
  }

  const QPoint clickPos = event->pos();
  QTimer::singleShot(0, this, [this, clickPos]() {
    QWidget *dialogParent = window() ? window() : static_cast<QWidget *>(this);
    const QString fileName = QFileDialog::getOpenFileName(
        dialogParent, QStringLiteral("Select Image File"), QString(),
        QStringLiteral("Image Files (*.png *.jpg *.jpeg *.bmp *.gif *.tiff "
                       "*.webp);;All Files (*)"));

    if (fileName.isEmpty())
      return;

    qDebug() << "Loading image from:" << fileName;

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);

    QSize originalSize = reader.size();
    const int MAX_LOAD_DIMENSION = 2048;
    if (originalSize.width() > MAX_LOAD_DIMENSION ||
        originalSize.height() > MAX_LOAD_DIMENSION) {
      QSize scaledSize = originalSize.scaled(
          MAX_LOAD_DIMENSION, MAX_LOAD_DIMENSION, Qt::KeepAspectRatio);
      reader.setScaledSize(scaledSize);
      qDebug() << "Large image detected, scaling from" << originalSize << "to"
               << scaledSize;
    }

    QImage image = reader.read();
    if (image.isNull()) {
      qDebug() << "Failed to load image:" << fileName;
      return;
    }

    QVector2D worldPos = snapToGrid(screenToWorld(clickPos));

    float maxSize = 200.0f;
    float aspectRatio = static_cast<float>(image.width()) /
                        static_cast<float>(image.height());
    QVector2D size;
    if (aspectRatio > 1.0f) {
      size = QVector2D(maxSize, maxSize / aspectRatio);
    } else {
      size = QVector2D(maxSize * aspectRatio, maxSize);
    }

    auto imagePrimitive =
        std::make_unique<ImagePrimitive>(image, worldPos, size);
    imagePrimitive->setColor(Qt::black);
    addPrimitiveWithCommand(std::move(imagePrimitive));

    qDebug() << "Image imported and placed at:" << worldPos << "size:" << size;
  });
}

void DrawingCanvas::handleTextTool(QMouseEvent *event) {
  // Get the MainWindow to access the ClassicTextTool
  MainWindow *mainWindow = qobject_cast<MainWindow *>(window());
  if (!mainWindow || !mainWindow->getClassicTextTool()) {
    return;
  }

  ClassicTextTool *textTool = mainWindow->getClassicTextTool();

  // Ensure text tool is active when canvas routes events to it
  if (!textTool->isActive()) {
    textTool->activate();
  }

  QVector2D worldPos = screenToWorld(event->pos());

  // Forward mouse events to the ClassicTextTool
  switch (event->type()) {
  case QEvent::MouseButtonPress:
    textTool->mousePress(worldPos);
    break;
  case QEvent::MouseMove:
    textTool->mouseMove(worldPos);
    break;
  case QEvent::MouseButtonRelease:
    textTool->mouseRelease(worldPos);
    break;
  case QEvent::MouseButtonDblClick:
    textTool->mouseDoubleClick(worldPos);
    break;
  default:
    // For single click, treat as press
    if (event->button() == Qt::LeftButton) {
      textTool->mousePress(worldPos);
    }
    break;
  }

  update();
}

int DrawingCanvas::findControlPointAt(const QVector2D &pos, float tolerance) {
  if (!m_editingPrimitive || !m_editingPrimitive->isSelected()) {
    return -1;
  }

  // Use the generic control point interface
  std::vector<QVector2D> controlPoints = m_editingPrimitive->getControlPoints();
  for (int i = 0; i < static_cast<int>(controlPoints.size()); ++i) {
    float distance = (controlPoints[i] - pos).length();
    if (distance <= tolerance) {
      return i;
    }
  }

  return -1;
}

QVector2D DrawingCanvas::snapToLineEndpoint(const QVector2D &pos) {
  if (!m_magneticConnectionEnabled) {
    return pos;
  }

  return findNearestLineEndpoint(pos, m_magneticConnectionTolerance);
}

QVector2D DrawingCanvas::applySmartDrawingConstraints(const QVector2D &rawPos,
                                                      Qt::KeyboardModifiers mods,
                                                      QString *hintOut)
{
  if (hintOut) {
    hintOut->clear();
  }

  const bool shift = mods & Qt::ShiftModifier;
  const bool alt = mods & Qt::AltModifier;
  QVector2D pos = rawPos;

  auto setHint = [&](const QString &text) {
    if (hintOut) {
      *hintOut = text;
    }
  };

  switch (m_currentTool) {
  case DrawingTool::Line:
  case DrawingTool::Measure: {
    QVector2D delta = pos - m_drawStartPos;
    if (shift) {
      if (std::abs(delta.x()) >= std::abs(delta.y())) {
        pos.setY(m_drawStartPos.y());
        setHint(QStringLiteral("Shift: horizontal lock"));
      } else {
        pos.setX(m_drawStartPos.x());
        setHint(QStringLiteral("Shift: vertical lock"));
      }
    } else {
      const float len = delta.length();
      if (len > 8.0f) {
        const float angle = std::atan2(delta.y(), delta.x()) * 180.0f / static_cast<float>(M_PI);
        const float snapped = std::round(angle / 45.0f) * 45.0f;
        if (std::abs(angle - snapped) < 6.0f) {
          setHint(QStringLiteral("Near %1° — hold Shift to lock axis")
                      .arg(static_cast<int>(snapped)));
        }
      }
    }
    break;
  }
  case DrawingTool::Rectangle: {
    float w = pos.x() - m_drawStartPos.x();
    float h = pos.y() - m_drawStartPos.y();
    const float aw = std::abs(w);
    const float ah = std::abs(h);
    if (aw < 1.0f && ah < 1.0f) {
      break;
    }
    const float ratio = aw > 0.0f ? ah / aw : 1.0f;
    const bool nearSquare = ratio > 0.88f && ratio < 1.12f;
    if (shift || (!alt && nearSquare)) {
      const float side = std::max(aw, ah);
      pos.setX(m_drawStartPos.x() + std::copysign(side, w == 0.0f ? 1.0f : w));
      pos.setY(m_drawStartPos.y() + std::copysign(side, h == 0.0f ? 1.0f : h));
      setHint(shift ? QStringLiteral("Shift: square locked")
                    : QStringLiteral("Predicted square — hold Alt to freeform"));
    } else if (nearSquare) {
      setHint(QStringLiteral("Almost square — hold Shift to lock"));
    }
    break;
  }
  case DrawingTool::Ellipse: {
    float w = pos.x() - m_drawStartPos.x();
    float h = pos.y() - m_drawStartPos.y();
    const float aw = std::abs(w);
    const float ah = std::abs(h);
    if (aw < 1.0f && ah < 1.0f) {
      break;
    }
    const float ratio = aw > 0.0f ? ah / aw : 1.0f;
    const bool nearCircle = ratio > 0.88f && ratio < 1.12f;
    if (shift || (!alt && nearCircle)) {
      const float side = std::max(aw, ah);
      pos.setX(m_drawStartPos.x() + std::copysign(side, w == 0.0f ? 1.0f : w));
      pos.setY(m_drawStartPos.y() + std::copysign(side, h == 0.0f ? 1.0f : h));
      setHint(shift ? QStringLiteral("Shift: circle locked")
                    : QStringLiteral("Predicted circle — hold Alt to freeform"));
    } else if (nearCircle) {
      setHint(QStringLiteral("Almost circle — hold Shift to lock"));
    }
    break;
  }
  default:
    break;
  }

  return pos;
}

void DrawingCanvas::groupSelected() {
  auto selected = selectedObjects();
  if (selected.size() < 2)
    return;

  std::vector<QJsonObject> oldStates;
  oldStates.reserve(selected.size());
  for (auto *obj : selected)
    oldStates.push_back(obj->toJson());

  const QUuid gid = QUuid::createUuid();
  for (auto *obj : selected) {
    if (obj)
      obj->setGroupId(gid);
  }

  std::vector<QJsonObject> newStates;
  newStates.reserve(selected.size());
  for (auto *obj : selected)
    newStates.push_back(obj->toJson());

  auto command = std::make_unique<TransformPrimitivesCommand>(
      selected, TransformPrimitivesCommand::Rotate, QStringLiteral("Group"));
  command->storeTransformation(oldStates, newStates);
  emit commandRequested(command.release());
  update();
}

void DrawingCanvas::ungroupSelected() {
  auto selected = selectedObjects();
  if (selected.empty())
    return;

  bool anyGrouped = false;
  for (auto *obj : selected) {
    if (obj && !obj->groupId().isNull()) {
      anyGrouped = true;
      break;
    }
  }
  if (!anyGrouped)
    return;

  std::vector<QJsonObject> oldStates;
  oldStates.reserve(selected.size());
  for (auto *obj : selected)
    oldStates.push_back(obj->toJson());

  for (auto *obj : selected) {
    if (obj)
      obj->setGroupId(QUuid());
  }

  std::vector<QJsonObject> newStates;
  newStates.reserve(selected.size());
  for (auto *obj : selected)
    newStates.push_back(obj->toJson());

  auto command = std::make_unique<TransformPrimitivesCommand>(
      selected, TransformPrimitivesCommand::Rotate, QStringLiteral("Ungroup"));
  command->storeTransformation(oldStates, newStates);
  emit commandRequested(command.release());
  update();
}

void DrawingCanvas::selectGroupMembers(DrawingPrimitive *seed) {
  if (!seed || seed->groupId().isNull())
    return;
  const QUuid gid = seed->groupId();

  auto selectIfMatch = [&](DrawingPrimitive *p) {
    if (p && p->isVisible() && p->groupId() == gid) {
      p->setSelected(true);
      m_selectionManager->addToSelection(p);
    }
  };

  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
      if (!layer || !layer->isVisible() || layer->isLocked())
        continue;
      for (const auto &prim : layer->primitives())
        selectIfMatch(prim.get());
    }
  } else {
    for (const auto &prim : m_primitives)
      selectIfMatch(prim.get());
  }
}

void DrawingCanvas::alignSelectedObjects(AlignmentType alignmentType) {
  if (m_selectionManager->selectedObjects().empty())
    return;

  // Get page bounds - paper is centered at origin
  float paperWidth = m_paperSize.width() * m_pixelsPerMM;
  float paperHeight = m_paperSize.height() * m_pixelsPerMM;
  QRectF pageRect =
      QRectF(-paperWidth / 2.0f, -paperHeight / 2.0f, paperWidth, paperHeight);

  // Calculate target position based on alignment type
  QVector2D targetPos;
  bool isHorizontal = false;
  bool isVertical = false;

  switch (alignmentType) {
  case AlignmentType::PageLeft:
    targetPos = QVector2D(pageRect.left(), 0);
    isHorizontal = true;
    break;
  case AlignmentType::PageRight:
    targetPos = QVector2D(pageRect.right(), 0);
    isHorizontal = true;
    break;
  case AlignmentType::PageCenterHorizontal:
    targetPos = QVector2D(pageRect.center().x(), 0);
    isHorizontal = true;
    break;
  case AlignmentType::PageTop:
    targetPos = QVector2D(0, pageRect.top());
    isVertical = true;
    break;
  case AlignmentType::PageBottom:
    targetPos = QVector2D(0, pageRect.bottom());
    isVertical = true;
    break;
  case AlignmentType::PageCenterVertical:
    targetPos = QVector2D(0, pageRect.center().y());
    isVertical = true;
    break;
  default:
    // For object-to-object alignment, find the reference object based on
    // alignment type
    if (!m_selectionManager->selectedObjects().empty()) {
      QRectF referenceBounds;
      bool found = false;

      switch (alignmentType) {
      case AlignmentType::Left:
        // Find the leftmost object
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (!found || bounds.left() < referenceBounds.left()) {
            referenceBounds = bounds;
            found = true;
          }
        }
        if (found) {
          targetPos = QVector2D(referenceBounds.left(), 0);
          isHorizontal = true;
        }
        break;

      case AlignmentType::Right:
        // Find the rightmost object
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (!found || bounds.right() > referenceBounds.right()) {
            referenceBounds = bounds;
            found = true;
          }
        }
        if (found) {
          targetPos = QVector2D(referenceBounds.right(), 0);
          isHorizontal = true;
        }
        break;

      case AlignmentType::CenterHorizontal: {
        // Calculate the center of all selected objects
        QRectF combinedBounds;
        bool first = true;
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (first) {
            combinedBounds = bounds;
            first = false;
          } else {
            combinedBounds = combinedBounds.united(bounds);
          }
        }
        targetPos = QVector2D(combinedBounds.center().x(), 0);
        isHorizontal = true;
        break;
      }

      case AlignmentType::Top:
        // Find the topmost object
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (!found || bounds.top() < referenceBounds.top()) {
            referenceBounds = bounds;
            found = true;
          }
        }
        if (found) {
          targetPos = QVector2D(0, referenceBounds.top());
          isVertical = true;
        }
        break;

      case AlignmentType::Bottom:
        // Find the bottommost object
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (!found || bounds.bottom() > referenceBounds.bottom()) {
            referenceBounds = bounds;
            found = true;
          }
        }
        if (found) {
          targetPos = QVector2D(0, referenceBounds.bottom());
          isVertical = true;
        }
        break;

      case AlignmentType::CenterVertical: {
        // Calculate the center of all selected objects
        QRectF combinedBoundsV;
        bool firstV = true;
        for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
          QRectF bounds = obj->boundingRect();
          if (firstV) {
            combinedBoundsV = bounds;
            firstV = false;
          } else {
            combinedBoundsV = combinedBoundsV.united(bounds);
          }
        }
        targetPos = QVector2D(0, combinedBoundsV.center().y());
        isVertical = true;
        break;
      }

      default:
        return;
      }
    }
    break;
  }

  // Apply alignment to all selected objects
  for (DrawingPrimitive *obj : m_selectionManager->selectedObjects()) {
    QRectF objBounds = obj->boundingRect();
    QVector2D currentCenter =
        QVector2D(objBounds.center().x(), objBounds.center().y());
    QVector2D offset(0, 0);

    if (isHorizontal) {
      // Calculate horizontal offset to align to target
      float targetX = targetPos.x();
      float currentX = currentCenter.x();

      // For left alignment, align left edges
      if (alignmentType == AlignmentType::Left ||
          alignmentType == AlignmentType::PageLeft) {
        offset.setX(targetX - objBounds.left());
      }
      // For right alignment, align right edges
      else if (alignmentType == AlignmentType::Right ||
               alignmentType == AlignmentType::PageRight) {
        offset.setX(targetX - objBounds.right());
      }
      // For center alignment, align centers
      else if (alignmentType == AlignmentType::CenterHorizontal ||
               alignmentType == AlignmentType::PageCenterHorizontal) {
        offset.setX(targetX - currentX);
      }
    }

    if (isVertical) {
      // Calculate vertical offset to align to target
      float targetY = targetPos.y();
      float currentY = currentCenter.y();

      // For top alignment, align top edges
      if (alignmentType == AlignmentType::Top ||
          alignmentType == AlignmentType::PageTop) {
        offset.setY(targetY - objBounds.top());
      }
      // For bottom alignment, align bottom edges
      else if (alignmentType == AlignmentType::Bottom ||
               alignmentType == AlignmentType::PageBottom) {
        offset.setY(targetY - objBounds.bottom());
      }
      // For center alignment, align centers
      else if (alignmentType == AlignmentType::CenterVertical ||
               alignmentType == AlignmentType::PageCenterVertical) {
        offset.setY(targetY - currentY);
      }
    }

    // Apply the offset
    obj->translate(offset);
  }

  update();
  qDebug() << "Aligned" << m_selectionManager->selectedObjects().size()
           << "objects to" << static_cast<int>(alignmentType);
}

void DrawingCanvas::distributeSelectedObjects(bool horizontal) {
  const auto &selected = m_selectionManager->selectedObjects();
  if (selected.size() < 3) {
    return;
  }

  struct Item {
    DrawingPrimitive *primitive;
    QRectF bounds;
  };

  std::vector<Item> items;
  items.reserve(selected.size());
  for (auto *obj : selected) {
    if (obj) {
      items.push_back({obj, obj->boundingRect()});
    }
  }

  if (items.size() < 3) {
    return;
  }

  if (horizontal) {
    std::sort(items.begin(), items.end(),
              [](const Item &a, const Item &b) {
                return a.bounds.left() < b.bounds.left();
              });
    float minX = items.front().bounds.left();
    float maxX = items.back().bounds.right();
    float totalWidth = 0.0f;
    for (const auto &item : items) {
      totalWidth += item.bounds.width();
    }
    float spacing = (maxX - minX - totalWidth) /
                    static_cast<float>(items.size() - 1);
    float cursor = minX;
    for (auto &item : items) {
      float offsetX = cursor - item.bounds.left();
      item.primitive->translate(QVector2D(offsetX, 0.0f));
      cursor += item.bounds.width() + spacing;
    }
  } else {
    std::sort(items.begin(), items.end(),
              [](const Item &a, const Item &b) {
                return a.bounds.top() < b.bounds.top();
              });
    float minY = items.front().bounds.top();
    float maxY = items.back().bounds.bottom();
    float totalHeight = 0.0f;
    for (const auto &item : items) {
      totalHeight += item.bounds.height();
    }
    float spacing = (maxY - minY - totalHeight) /
                    static_cast<float>(items.size() - 1);
    float cursor = minY;
    for (auto &item : items) {
      float offsetY = cursor - item.bounds.top();
      item.primitive->translate(QVector2D(0.0f, offsetY));
      cursor += item.bounds.height() + spacing;
    }
  }

  update();
}

void DrawingCanvas::renderAlignmentGuides(QPainter& painter) {
  if (m_alignmentGuides.empty())
    return;

  painter.save();
  setupWorldTransform(painter);

  for (const auto &guide : m_alignmentGuides) {
    if (!guide.isActive)
      continue;

    QColor guideColor;
    // Different colors for different alignment types
    switch (guide.type) {
    case AlignmentType::PageLeft:
    case AlignmentType::PageRight:
    case AlignmentType::PageCenterHorizontal:
    case AlignmentType::PageTop:
    case AlignmentType::PageBottom:
    case AlignmentType::PageCenterVertical:
      guideColor = QColor(255, 128, 0, 204); // Orange for page alignment
      break;
    case AlignmentType::Left:
    case AlignmentType::Right:
    case AlignmentType::CenterHorizontal:
    case AlignmentType::Top:
    case AlignmentType::Bottom:
    case AlignmentType::CenterVertical:
      guideColor = QColor(0, 204, 255, 204); // Cyan for object alignment
      break;
    default:
      guideColor = QColor(204, 204, 204, 153); // Gray for others
      break;
    }

    QPen guidePen(guideColor, 2.0);
    guidePen.setStyle(Qt::DashLine);
    guidePen.setCosmetic(true);
    painter.setPen(guidePen);

    switch (guide.type) {
    case AlignmentType::Left:
    case AlignmentType::Right:
    case AlignmentType::CenterHorizontal:
    case AlignmentType::PageLeft:
    case AlignmentType::PageRight:
    case AlignmentType::PageCenterHorizontal:
      // Vertical line
      painter.drawLine(QPointF(guide.position.x(), -10000.0),
                       QPointF(guide.position.x(), 10000.0));
      break;

    case AlignmentType::Top:
    case AlignmentType::Bottom:
    case AlignmentType::CenterVertical:
    case AlignmentType::PageTop:
    case AlignmentType::PageBottom:
    case AlignmentType::PageCenterVertical:
      // Horizontal line
      painter.drawLine(QPointF(-10000.0, guide.position.y()),
                       QPointF(10000.0, guide.position.y()));
      break;

    default:
      break;
    }
  }

  painter.restore();
}

void DrawingCanvas::updateAlignmentGuides(const QVector2D &mousePos) {
  if (!m_showAlignmentGuides) {
    m_alignmentGuides.clear();
    m_snapIndicatorActive = false;
    return;
  }

  m_alignmentGuides.clear();
  m_snapIndicatorActive = false;
  float snapTolerance = 10.0f / m_zoomLevel;
  float bestDistance = std::numeric_limits<float>::max();
  QVector2D bestSnapPos = mousePos;

  // Collect candidate guide lines from existing objects
  std::vector<float> horizontalGuides;
  std::vector<float> verticalGuides;

  // Add center/edge guides
  if (m_layerManager) {
    for (const auto &layer : m_layerManager->layers()) {
        if (!layer->isVisible()) continue;
        for (const auto &prim : layer->primitives()) {
            if (prim->isSelected()) continue; // Don't snap to self if moving
            QRectF bounds = prim->boundingRect();
            horizontalGuides.push_back(bounds.top());
            horizontalGuides.push_back(bounds.bottom());
            horizontalGuides.push_back(bounds.center().y());
            verticalGuides.push_back(bounds.left());
            verticalGuides.push_back(bounds.right());
            verticalGuides.push_back(bounds.center().x());
        }
    }
  }

  // Find closest horizontal guide
  float bestHDist = std::numeric_limits<float>::max();
  float bestHPos = 0;
  bool foundH = false;
  for (float guideY : horizontalGuides) {
    float dist = std::abs(mousePos.y() - guideY);
    if (dist < snapTolerance && dist < bestHDist) {
      bestHDist = dist;
      bestHPos = guideY;
      foundH = true;
    }
  }
  if (foundH) {
    m_alignmentGuides.push_back({QVector2D(0, bestHPos), AlignmentType::Top, true});
    if (bestHDist < bestDistance) {
      bestDistance = bestHDist;
      bestSnapPos = QVector2D(mousePos.x(), bestHPos);
    }
  }

  // Find closest vertical guide
  float bestVDist = std::numeric_limits<float>::max();
  float bestVPos = 0;
  bool foundV = false;
  for (float guideX : verticalGuides) {
    float dist = std::abs(mousePos.x() - guideX);
    if (dist < snapTolerance && dist < bestVDist) {
      bestVDist = dist;
      bestVPos = guideX;
      foundV = true;
    }
  }
  if (foundV) {
    m_alignmentGuides.push_back({QVector2D(bestVPos, 0), AlignmentType::Left, true});
    if (bestVDist < bestDistance) {
      bestDistance = bestVDist;
      bestSnapPos = QVector2D(bestVPos, mousePos.y());
    }
  }

  if (!m_alignmentGuides.empty()) {
    m_snapIndicatorActive = true;
    m_snapIndicatorPos = bestSnapPos;
  }
}

bool DrawingCanvas::isObjectAtAlignmentPosition(DrawingPrimitive *obj, float tolerance) {
  if (!obj) return false;
  
  QRectF bounds = obj->boundingRect();
  
  for (const auto& guide : m_alignmentGuides) {
      if (!guide.isActive) continue;
      
      if (guide.type == AlignmentType::Top || guide.type == AlignmentType::Bottom || guide.type == AlignmentType::CenterHorizontal) {
          // Horizontal guide (y-coord)
          if (std::abs(bounds.top() - guide.position.y()) < tolerance ||
              std::abs(bounds.bottom() - guide.position.y()) < tolerance ||
              std::abs(bounds.center().y() - guide.position.y()) < tolerance) {
              return true;
          }
      } else {
          // Vertical guide (x-coord)
          if (std::abs(bounds.left() - guide.position.x()) < tolerance ||
              std::abs(bounds.right() - guide.position.x()) < tolerance ||
              std::abs(bounds.center().x() - guide.position.x()) < tolerance) {
              return true;
          }
      }
  }
  return false;
}

QColor DrawingCanvas::getColorAtPosition(const QVector2D &pos) {
  // Render to an image and read the pixel
  QImage image = grab().toImage();
  QPoint screenPos = worldToScreen(pos);

  if (screenPos.x() >= 0 && screenPos.x() < image.width() &&
      screenPos.y() >= 0 && screenPos.y() < image.height()) {
    return image.pixelColor(screenPos.x(), screenPos.y());
  }

  return QColor(255, 255, 255);
}

void DrawingCanvas::setSplineSelectionMode(bool enabled,
                                           TextPrimitive *textPrim) {
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
                                               bool visible) {
  if (!textPrim || !textPrim->followsSpline())
    return;

  // Find the spline primitive
  DrawingPrimitive *spline = nullptr;

  if (m_layerManager) {
    const auto &layers = m_layerManager->layers();
    for (const auto &layer : layers) {
      for (const auto &prim : layer->primitives()) {
        if (prim->id() == textPrim->splineId()) {
          spline = prim.get();
          break;
        }
      }
      if (spline)
        break;
    }
  } else {
    for (const auto &prim : m_primitives) {
      if (prim->id() == textPrim->splineId()) {
        spline = prim.get();
        break;
      }
    }
  }

  if (spline) {
    spline->setVisible(visible);
    update();
  }
}

bool DrawingCanvas::getSplineVisibilityForText(TextPrimitive *textPrim) const {
  if (!textPrim || !textPrim->followsSpline())
    return true;

  // Find the spline primitive
  const DrawingPrimitive *spline = nullptr;

  if (m_layerManager) {
    const auto &layers = m_layerManager->layers();
    for (const auto &layer : layers) {
      for (const auto &prim : layer->primitives()) {
        if (prim->id() == textPrim->splineId()) {
          spline = prim.get();
          break;
        }
      }
      if (spline)
        break;
    }
  } else {
    for (const auto &prim : m_primitives) {
      if (prim->id() == textPrim->splineId()) {
        spline = prim.get();
        break;
      }
    }
  }

  return spline ? spline->isVisible() : true;
}

void DrawingCanvas::renderTextOnSpline(const TextPrimitive *textPrim,
                                       QPainter &painter) {
  if (!textPrim || !textPrim->followsSpline())
    return;

  // Find the spline primitive
  SplinePrimitive *spline = nullptr;

  if (m_layerManager) {
    const auto &layers = m_layerManager->layers();
    for (const auto &layer : layers) {
      for (const auto &prim : layer->primitives()) {
        if (prim->id() == textPrim->splineId()) {
          spline = dynamic_cast<SplinePrimitive *>(prim.get());
          break;
        }
      }
      if (spline)
        break;
    }
  } else {
    for (const auto &prim : m_primitives) {
      if (prim->id() == textPrim->splineId()) {
        spline = dynamic_cast<SplinePrimitive *>(prim.get());
        break;
      }
    }
  }

  if (!spline)
    return;

  // Get spline points
  const auto &splinePoints = spline->points();
  if (splinePoints.size() < 2)
    return;

  // Generate smooth curve points
  std::vector<QVector2D> curvePoints;
  int segments =
      (splinePoints.size() - 1) * 20; // 20 segments per spline section

  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments;
    int segmentIndex = qMin((int)(t * (splinePoints.size() - 1)),
                            (int)splinePoints.size() - 2);
    float localT = t * (splinePoints.size() - 1) - segmentIndex;

    // Catmull-Rom spline interpolation
    QVector2D p0 = splinePoints[qMax(0, segmentIndex - 1)];
    QVector2D p1 = splinePoints[segmentIndex];
    QVector2D p2 =
        splinePoints[qMin(segmentIndex + 1, (int)splinePoints.size() - 1)];
    QVector2D p3 =
        splinePoints[qMin(segmentIndex + 2, (int)splinePoints.size() - 1)];

    float t2 = localT * localT;
    float t3 = t2 * localT;

    QVector2D point = 0.5f * ((2.0f * p1) + (-p0 + p2) * localT +
                              (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                              (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

    curvePoints.push_back(point);
  }

  if (curvePoints.empty())
    return;

  // Set up font with zoom for rendering
  int scaledFontSize =
      static_cast<int>(textPrim->fontSize() * m_zoomLevel * textPrim->scale());
  QFont font(textPrim->fontFamily(), scaledFontSize);
  font.setBold(textPrim->isBold());
  font.setItalic(textPrim->isItalic());
  painter.setFont(font);

  QFontMetrics fm(font);
  QString text = textPrim->text();

  // Also need font metrics in world space for spacing calculations
  QFont worldFont(textPrim->fontFamily(),
                  static_cast<int>(textPrim->fontSize() * textPrim->scale()));
  worldFont.setBold(textPrim->isBold());
  worldFont.setItalic(textPrim->isItalic());
  QFontMetrics worldFm(worldFont);

  // Calculate total path length
  float totalLength = 0.0f;
  std::vector<float> segmentLengths;
  for (size_t i = 1; i < curvePoints.size(); ++i) {
    float len = (curvePoints[i] - curvePoints[i - 1]).length();
    segmentLengths.push_back(len);
    totalLength += len;
  }

  // Apply path offset
  float startOffset = textPrim->pathOffset() * totalLength;

  // Draw each character along the path
  float currentDistance = startOffset;

  for (int i = 0; i < text.length(); ++i) {
    QChar ch = text[i];
    int charWidthScreen = fm.horizontalAdvance(ch);
    int charWidthWorld = worldFm.horizontalAdvance(ch);

    // Find position on curve for this character (use world space width)
    float targetDist = currentDistance + charWidthWorld * 0.5f;
    float accumulatedDist = 0.0f;

    QVector2D charPos;
    QVector2D tangent;

    for (size_t j = 0; j < segmentLengths.size(); ++j) {
      if (accumulatedDist + segmentLengths[j] >= targetDist) {
        float t = (targetDist - accumulatedDist) / segmentLengths[j];
        charPos = curvePoints[j] * (1.0f - t) + curvePoints[j + 1] * t;
        tangent = (curvePoints[j + 1] - curvePoints[j]).normalized();
        break;
      }
      accumulatedDist += segmentLengths[j];
    }

    if (tangent.length() > 0.0f) {
      // Calculate angle from tangent
      float angle = atan2f(tangent.y(), tangent.x()) * 180.0f / M_PI;

      // Convert to screen coordinates
      QPoint screenPos = worldToScreen(charPos);

      // Draw character rotated along path
      painter.save();
      painter.translate(screenPos);
      painter.rotate(angle);
      painter.setPen(textPrim->color());
      painter.drawText(QPoint(-charWidthScreen / 2, fm.ascent() / 2),
                       QString(ch));
      painter.restore();
    }

    currentDistance += charWidthWorld;

    // Stop if we've gone past the end of the path
    if (currentDistance > totalLength)
      break;
  }
}

void DrawingCanvas::renderFormattedText(QPainter *painter,
                                        const TextPrimitive *textPrim,
                                        const QPoint &pos, const QFont &font,
                                        bool showBox) {
  if (!textPrim || !painter)
    return;

  // Always draw the real text color so Format / ATE edits are visible
  // while selected. Selection is indicated by handles, not recoloring.
  if (!textPrim->gradientEnabled()) {
    painter->setPen(textPrim->color());
  }

  QString text = textPrim->text();

  QFont effectiveFont(font);
  effectiveFont.setUnderline(textPrim->isUnderline());

  constexpr qreal kSubSuperscriptScale = 0.7;
  const auto baselineShift = textPrim->baselineShift();
  if (baselineShift != TextPrimitive::BaselineShift::Normal) {
    if (effectiveFont.pointSizeF() > 0) {
      effectiveFont.setPointSizeF(
          std::max(1.0, effectiveFont.pointSizeF() * kSubSuperscriptScale));
    } else if (effectiveFont.pixelSize() > 0) {
      int scaled = std::max(1, static_cast<int>(effectiveFont.pixelSize() *
                                                kSubSuperscriptScale));
      effectiveFont.setPixelSize(scaled);
    }
  }

  QFontMetrics fm(effectiveFont);
  QFont previousFont = painter->font();
  painter->setFont(effectiveFont);

  int baselineOffset = 0;
  if (baselineShift == TextPrimitive::BaselineShift::Subscript) {
    baselineOffset = static_cast<int>(fm.height() * 0.25f);
  } else if (baselineShift == TextPrimitive::BaselineShift::Superscript) {
    baselineOffset = static_cast<int>(-fm.height() * 0.35f);
  }

  float letterSpacing = textPrim->letterSpacing() * m_zoomLevel;
  float lineSpacing = textPrim->lineSpacing();
  int lineHeight = fm.height() * lineSpacing;

  // Get text box dimensions in screen space
  float boxWidth = textPrim->textBoxWidth() * m_zoomLevel;
  float boxHeight = textPrim->textBoxHeight() * m_zoomLevel;

  // If no box dimensions, calculate actual text bounds for gradient
  // Check world-space dimensions (not screen-space) to avoid zoom-related
  // issues
  bool hasBox = (textPrim->textBoxWidth() > 0 && textPrim->textBoxHeight() > 0);
  float actualTextWidth = 0;
  float actualTextHeight = 0;

  if (!hasBox) {
    // Calculate actual text dimensions for gradient purposes
    QStringList lines = text.split('\n');
    for (const QString &line : lines) {
      actualTextWidth =
          qMax(actualTextWidth, (float)fm.horizontalAdvance(line));
    }
    actualTextHeight = fm.height() * lines.size() * textPrim->lineSpacing() +
                       std::abs(baselineOffset);

    // Use large values for wrapping, but keep actual dimensions for gradient
    boxWidth = 10000; // Very large for no wrapping
    boxHeight = 10000;
  } else {
    actualTextWidth = boxWidth;
    actualTextHeight = boxHeight;

    // IMPORTANT: Set clipping region to prevent text from rendering outside box
    painter->save();
    painter->setClipRect(QRectF(pos.x(), pos.y(), boxWidth, boxHeight));
  }

  // Word wrap text to fit within box width
  QStringList wrappedLines;
  QStringList paragraphs = text.split('\n');

  for (const QString &paragraph : paragraphs) {
    if (paragraph.isEmpty()) {
      wrappedLines.append("");
      continue;
    }

    QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
    QString currentLine;

    for (const QString &word : words) {
      QString testLine =
          currentLine.isEmpty() ? word : currentLine + " " + word;

      // Calculate width with letter spacing
      int testWidth = 0;
      for (int i = 0; i < testLine.length(); ++i) {
        testWidth += fm.horizontalAdvance(testLine[i]);
        if (i < testLine.length() - 1) {
          testWidth += letterSpacing;
        }
      }

      if (testWidth <= boxWidth) {
        currentLine = testLine;
      } else {
        if (!currentLine.isEmpty()) {
          wrappedLines.append(currentLine);
        }
        currentLine = word;
      }
    }

    if (!currentLine.isEmpty()) {
      wrappedLines.append(currentLine);
    }
  }

  // Alignment must be relative to the text area:
  // - explicit text box → use box width
  // - auto-sized text → use the widest content line (not the wrap sentinel)
  float maxContentWidth = 0.0f;
  for (const QString &line : wrappedLines) {
    if (line.isEmpty()) {
      continue;
    }
    float lineWidth = 0.0f;
    for (int i = 0; i < line.length(); ++i) {
      lineWidth += fm.horizontalAdvance(line[i]);
      if (i < line.length() - 1) {
        lineWidth += letterSpacing;
      }
    }
    maxContentWidth = qMax(maxContentWidth, lineWidth);
  }
  const float alignWidth = hasBox ? boxWidth : maxContentWidth;

  // Draw wrapped lines
  int y = pos.y() + fm.ascent() + baselineOffset;

  for (int lineIdx = 0; lineIdx < wrappedLines.size(); ++lineIdx) {
    const QString &line = wrappedLines[lineIdx];

    // Check if we're exceeding box height
    if (y - pos.y() > boxHeight)
      break;

    if (line.isEmpty()) {
      y += lineHeight;
      continue;
    }

    // Calculate line width with letter spacing
    int lineWidth = 0;
    for (int i = 0; i < line.length(); ++i) {
      lineWidth += fm.horizontalAdvance(line[i]);
      if (i < line.length() - 1) {
        lineWidth += letterSpacing;
      }
    }

    // Calculate x position based on alignment within the text area
    int x = pos.x();
    switch (textPrim->alignment()) {
    case TextPrimitive::TextAlignment::Left:
      break;
    case TextPrimitive::TextAlignment::Center:
      x = pos.x() + static_cast<int>((alignWidth - lineWidth) / 2);
      break;
    case TextPrimitive::TextAlignment::Right:
      x = pos.x() + static_cast<int>(alignWidth - lineWidth);
      break;
    case TextPrimitive::TextAlignment::Justify:
      // Justify only if not the last line
      if (lineIdx < wrappedLines.size() - 1 && line.contains(' ')) {
        int totalCharWidth = 0;
        for (QChar ch : line) {
          totalCharWidth += fm.horizontalAdvance(ch);
        }

        int spaceCount = line.count(' ');
        if (spaceCount > 0) {
          float extraSpacing =
              (alignWidth - totalCharWidth) / (float)(line.length() - 1);

          int currentX = x;
          for (QChar ch : line) {
            QString charStr(ch);

            // Draw shadow only when explicitly enabled
            if (textPrim->shadowEnabled()) {
              QPoint shadowOffset(textPrim->shadowOffsetX() * m_zoomLevel,
                                  textPrim->shadowOffsetY() * m_zoomLevel);
              float blur = textPrim->shadowBlur() * m_zoomLevel;
              QColor shadowColor = textPrim->shadowColor();

              painter->save();

              // Ensure shadow color has proper alpha
              if (shadowColor.alpha() == 0) {
                shadowColor.setAlpha(
                    180); // Default to ~70% opacity if fully transparent
              }

              // Apply blur effect by drawing multiple passes with reduced
              // opacity
              if (blur > 0.5f) {
                int blurPasses = qMin(15, qMax(3, (int)(blur / 1.5f)));
                float baseAlpha = shadowColor.alphaF();

                for (int i = 0; i < blurPasses; ++i) {
                  // Gaussian-like distribution for better blur
                  float t = (float)i / (float)(blurPasses - 1);
                  float gaussianWeight = expf(-2.5f * t * t);
                  float alpha =
                      baseAlpha * gaussianWeight / (float)blurPasses * 3.0f;

                  QColor blurColor = shadowColor;
                  blurColor.setAlphaF(qMax(0.05f, qMin(1.0f, alpha)));
                  painter->setPen(blurColor);

                  float spread = t * blur;
                  QPoint blurOffset = shadowOffset + QPoint(spread, spread);
                  painter->drawText(currentX + blurOffset.x(),
                                    y + blurOffset.y(), charStr);
                }
              } else {
                // No blur - simple shadow with proper alpha
                painter->setPen(shadowColor);
                painter->drawText(currentX + shadowOffset.x(),
                                  y + shadowOffset.y(), charStr);
              }

              painter->restore();
            }

            // Draw stroke if enabled
            if (textPrim->strokeEnabled()) {
              QPainterPath charPath;
              charPath.addText(currentX, y, effectiveFont, charStr);
              painter->save();
              QPen strokePen(textPrim->strokeColor(),
                             textPrim->strokeWidth() * m_zoomLevel);
              strokePen.setJoinStyle(Qt::RoundJoin);
              painter->setPen(strokePen);
              painter->setBrush(Qt::NoBrush);
              painter->drawPath(charPath);
              painter->restore();
            }

            // Draw character with gradient or normal color
            if (textPrim->gradientEnabled()) {
              // Create gradient across the actual text dimensions
              float angleRad = textPrim->gradientAngle() * M_PI / 180.0f;

              // Calculate gradient start and end points using actual text size
              QPointF gradStart(pos.x(), pos.y());
              QPointF gradEnd;

              if (textPrim->gradientAngle() == 0) {
                // Horizontal gradient
                gradEnd = QPointF(pos.x() + actualTextWidth, pos.y());
              } else if (textPrim->gradientAngle() == 90) {
                // Vertical gradient
                gradEnd = QPointF(pos.x(), pos.y() + actualTextHeight);
              } else {
                // Angled gradient - use actual text dimensions
                gradEnd = QPointF(pos.x() + cos(angleRad) * actualTextWidth,
                                  pos.y() + sin(angleRad) * actualTextHeight);
              }

              QLinearGradient gradient(gradStart, gradEnd);
              gradient.setColorAt(0, textPrim->gradientStartColor());
              gradient.setColorAt(1, textPrim->gradientEndColor());

              // Use QPainterPath for reliable gradient rendering
              QPainterPath textPath;
              textPath.addText(currentX, y, effectiveFont, charStr);

              painter->save();
              painter->setPen(Qt::NoPen);
              painter->setBrush(QBrush(gradient));
              painter->drawPath(textPath);
              painter->restore();
            } else {
              // Use current painter pen (which has the text color set)
              painter->drawText(currentX, y, charStr);
            }

            currentX += fm.horizontalAdvance(ch) + extraSpacing;
          }
          y += lineHeight;
          continue;
        }
      }
      break;
    }

    // Draw characters with letter spacing, shadow, and stroke
    int currentX = x;
    for (int i = 0; i < line.length(); ++i) {
      QChar ch = line[i];
      QString charStr(ch);

      // Draw shadow only when explicitly enabled
      if (textPrim->shadowEnabled()) {
        QPoint shadowOffset(textPrim->shadowOffsetX() * m_zoomLevel,
                            textPrim->shadowOffsetY() * m_zoomLevel);
        float blur = textPrim->shadowBlur() * m_zoomLevel;
        QColor shadowColor = textPrim->shadowColor();

        painter->save();

        // Ensure shadow color has proper alpha
        if (shadowColor.alpha() == 0) {
          shadowColor.setAlpha(
              180); // Default to ~70% opacity if fully transparent
        }

        // Apply blur effect by drawing multiple passes with reduced opacity
        if (blur > 0.5f) {
          int blurPasses = qMin(15, qMax(3, (int)(blur / 1.5f)));
          float baseAlpha = shadowColor.alphaF();

          for (int i = 0; i < blurPasses; ++i) {
            // Gaussian-like distribution for better blur
            float t = (float)i / (float)(blurPasses - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float alpha = baseAlpha * gaussianWeight / (float)blurPasses * 3.0f;

            QColor blurColor = shadowColor;
            blurColor.setAlphaF(qMax(0.05f, qMin(1.0f, alpha)));
            painter->setPen(blurColor);

            float spread = t * blur;
            QPoint blurOffset = shadowOffset + QPoint(spread, spread);
            painter->drawText(currentX + blurOffset.x(), y + blurOffset.y(),
                              charStr);
          }
        } else {
          // No blur - simple shadow with proper alpha
          painter->setPen(shadowColor);
          painter->drawText(currentX + shadowOffset.x(), y + shadowOffset.y(),
                            charStr);
        }

        painter->restore();
      }

      // Draw stroke if enabled
      if (textPrim->strokeEnabled()) {
        QPainterPath charPath;
        charPath.addText(currentX, y, effectiveFont, charStr);
        painter->save();
        QPen strokePen(textPrim->strokeColor(),
                       textPrim->strokeWidth() * m_zoomLevel);
        strokePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(strokePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(charPath);
        painter->restore();
      }

      // Draw the character with gradient or normal color
      if (textPrim->gradientEnabled()) {
        // Create gradient across the actual text dimensions
        float angleRad = textPrim->gradientAngle() * M_PI / 180.0f;

        // Calculate gradient start and end points using actual text size
        QPointF gradStart(pos.x(), pos.y());
        QPointF gradEnd;

        if (textPrim->gradientAngle() == 0) {
          // Horizontal gradient
          gradEnd = QPointF(pos.x() + actualTextWidth, pos.y());
        } else if (textPrim->gradientAngle() == 90) {
          // Vertical gradient
          gradEnd = QPointF(pos.x(), pos.y() + actualTextHeight);
        } else {
          // Angled gradient - use actual text dimensions
          gradEnd = QPointF(pos.x() + cos(angleRad) * actualTextWidth,
                            pos.y() + sin(angleRad) * actualTextHeight);
        }

        QLinearGradient gradient(gradStart, gradEnd);
        gradient.setColorAt(0, textPrim->gradientStartColor());
        gradient.setColorAt(1, textPrim->gradientEndColor());

        // Use QPainterPath for reliable gradient rendering
        QPainterPath textPath;
        textPath.addText(currentX, y, effectiveFont, charStr);

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(gradient));
        painter->drawPath(textPath);
        painter->restore();
      } else {
        // Use current painter pen (which has the text color set)
        painter->drawText(currentX, y, charStr);
      }

      currentX += fm.horizontalAdvance(ch);
      if (i < line.length() - 1) {
        currentX += letterSpacing;
      }
    }

    y += lineHeight;
  }

  // Calculate actual text dimensions for bounding box if no explicit box size
  if (!hasBox) {
    // Update box dimensions to actual text size with small padding
    float padding = 4.0f; // Small padding around text
    boxWidth = maxContentWidth + padding * 2;
    boxHeight = wrappedLines.size() * lineHeight + padding * 2 +
                std::abs(baselineOffset);
  }

  // Draw corner handles for resize/rotate ONLY if showBox is true (when
  // selected)
  if (showBox) {
    painter->save();

    // Draw bounding box outline with constant dash pattern
    QPen boxPen(QColor(100, 149, 237), 1.5f, Qt::CustomDashLine);
    // Set dash pattern with constant pixel sizes
    QVector<qreal> dashPattern;
    dashPattern << 5.0f << 3.0f; // 5 pixel dash, 3 pixel gap
    boxPen.setDashPattern(dashPattern);
    painter->setPen(boxPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(QRectF(pos.x(), pos.y(), boxWidth, boxHeight));

    // Draw all 8 resize handles (4 corners + 4 edges)
    float handleSize = 8.0f; // Constant 8-pixel visual size at all zoom levels
    QColor handleColor(100, 149, 237);
    painter->setPen(QPen(Qt::white, 1.5f)); // Constant pen width
    painter->setBrush(handleColor);

    // All 8 control points: 4 corners + 4 edge midpoints
    QPointF controlPoints[8] = {
        QPointF(pos.x(), pos.y()),                // 0: Top-left corner
        QPointF(pos.x() + boxWidth / 2, pos.y()), // 1: Top edge
        QPointF(pos.x() + boxWidth, pos.y()),     // 2: Top-right corner
        QPointF(pos.x() + boxWidth, pos.y() + boxHeight / 2), // 3: Right edge
        QPointF(pos.x() + boxWidth,
                pos.y() + boxHeight), // 4: Bottom-right corner
        QPointF(pos.x() + boxWidth / 2, pos.y() + boxHeight), // 5: Bottom edge
        QPointF(pos.x(), pos.y() + boxHeight),    // 6: Bottom-left corner
        QPointF(pos.x(), pos.y() + boxHeight / 2) // 7: Left edge
    };

    for (const QPointF &point : controlPoints) {
      // Draw square handles centered on control points
      QRectF handleRect(point.x() - handleSize / 2, point.y() - handleSize / 2,
                        handleSize, handleSize);
      painter->drawRect(handleRect);
    }

    // Draw rotation handle (green circle at top center)
    float rotateHandleY = pos.y() - 20.0f;           // 20 pixels above top edge
    float rotateHandleX = pos.x() + boxWidth / 2.0f; // Center horizontally
    QPointF rotateHandlePos(rotateHandleX, rotateHandleY);

    // Draw line connecting rotation handle to top edge
    painter->setPen(QPen(QColor(100, 149, 237), 1.5f));
    painter->drawLine(QPointF(rotateHandleX, pos.y()), rotateHandlePos);

    // Draw rotation handle as green circle
    painter->setPen(QPen(Qt::white, 1.5f));
    painter->setBrush(QColor(0, 255, 0)); // Green
    QRectF rotateHandleRect(rotateHandlePos.x() - handleSize / 2,
                            rotateHandlePos.y() - handleSize / 2, handleSize,
                            handleSize);
    painter->drawEllipse(rotateHandleRect);

    painter->restore();
  }

  // Restore painter state if clipping was applied
  if (hasBox) {
    painter->restore();
  }

  painter->setFont(previousFont);
}

void DrawingCanvas::onAdvancedTextEditorRequested() {
  if (auto *mw = qobject_cast<MainWindow *>(window())) {
    QMetaObject::invokeMethod(mw, "showAdvancedTextEditor", Qt::DirectConnection);
    return;
  }
}
