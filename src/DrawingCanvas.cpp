// Silence OpenGL deprecation warnings on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "GuitarProject.h"
#include "ComponentLibrary.h"
#include "GuitarComponent.h"
#include "ComponentDatabase.h"
#include "BlueprintManager.h"
#include "BlueprintParser.h"
#include <QPainter>
#include "GuitarProject.h"
#include "ComponentDatabase.h"
#include "shaders.h"
#include <QOpenGLShaderProgram>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QImageReader>
#include <cmath>
#include <algorithm>

#ifdef HAVE_QT_PDF
#include <QPdfDocument>
#endif

DrawingCanvas::DrawingCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_viewCenter(0.0f, 0.0f)
    , m_zoomLevel(1.0f)
    , m_gridVisible(true)
    , m_snapEnabled(true)
    , m_gridSize(7.5591f) // 2mm grid (2mm * 3.77953 pixels/mm) - better for blueprint work
    , m_magneticConnectionEnabled(false)
    , m_magneticConnectionTolerance(15.0f) // 15 pixels tolerance
    , m_currentTool(DrawingTool::Select)
    , m_isDrawing(false)
    , m_isPanning(false)
    , m_bezierCreationStage(0)
    , m_isSelecting(false)
    , m_isEditingControlPoints(false)
    , m_selectedControlPoint(-1)
    , m_editingPrimitive(nullptr)
    , m_contextMenu(nullptr)
    , m_componentsMenu(nullptr)
    , m_project(nullptr)
    , m_units(Units::Millimeters)
    , m_pixelsPerMM(3.77953f) // Standard 96 DPI: 1mm = 3.77953 pixels
    , m_gridVBO(0)
    , m_gridVAO(0)
    , m_mainShaderProgram(0)
    , m_gridShaderProgram(0)
    , m_blueprintManager(std::make_unique<BlueprintManager>(this))
    , m_blueprintParser(std::make_unique<BlueprintParser>(this))
{
    setFocusPolicy(Qt::WheelFocus); // Ensure wheel events are received
    setAttribute(Qt::WA_AcceptTouchEvents, false); // Disable touch to avoid conflicts
    setupContextMenu();
    
    // Connect blueprint signals
    connect(m_blueprintManager.get(), &BlueprintManager::blueprintLoaded,
            this, &DrawingCanvas::blueprintLoaded);
    connect(m_blueprintParser.get(), &BlueprintParser::parseComplete,
            this, [this](const GuitarOutline& outline) {
                qDebug() << "Guitar outline generated with" << outline.bodyOutline.size() << "body points";
                createGuitarOutlinePrimitives(outline);
                emit blueprintOutlineGenerated(outline.bodyOutline.size());
            });
}

DrawingCanvas::~DrawingCanvas()
{
    makeCurrent();
    // Clean up OpenGL resources
    if (m_gridVBO) glDeleteBuffers(1, &m_gridVBO);
    if (m_gridVAO) glDeleteVertexArrays(1, &m_gridVAO);
    if (m_mainShaderProgram) glDeleteProgram(m_mainShaderProgram);
    if (m_gridShaderProgram) glDeleteProgram(m_gridShaderProgram);
    doneCurrent();
}

void DrawingCanvas::initializeGL()
{
    initializeOpenGLFunctions();
    
    // Set clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Enable blending for smooth lines
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable line smoothing
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    // Try to create shader programs - use 0 if they fail
    m_mainShaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    m_gridShaderProgram = createShaderProgram(gridVertexShaderSource, gridFragmentShaderSource);
    
    if (m_mainShaderProgram == 0) {
        qDebug() << "Warning: Main shader compilation failed, using legacy OpenGL";
    }
    if (m_gridShaderProgram == 0) {
        qDebug() << "Warning: Grid shader compilation failed, using legacy OpenGL";
    }
    
    // Generate grid geometry
    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);
    
    updateProjection();
    
    // Initialize blueprint manager
    m_blueprintManager->initializeGL();
}

void DrawingCanvas::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render blueprint first (behind everything)
    m_blueprintManager->render(m_projectionMatrix, m_viewMatrix);
    
    if (m_gridVisible) {
        renderGrid();
    }
    
    renderObjects();
    renderSelection();
    renderTool();
    renderCrosshair();
    
    // Render dimension text overlays using QPainter
    renderDimensionTexts();
}

void DrawingCanvas::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
    updateProjection();
}

void DrawingCanvas::updateProjection()
{
    // Projection is now handled directly in OpenGL calls
    // This function kept for compatibility
}

void DrawingCanvas::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    QVector2D worldPos = screenToWorld(event->pos());
    
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    switch (m_currentTool) {
        case DrawingTool::Select:
            handleSelectTool(event);
            break;
        case DrawingTool::Line:
            handleLineTool(event);
            break;
        case DrawingTool::Curve:
            handleCurveTool(event);
            break;
        case DrawingTool::BezierCurve:
            handleBezierTool(event);
            break;
        case DrawingTool::Spline:
            handleSplineTool(event);
            break;
        case DrawingTool::Rectangle:
            handleRectangleTool(event);
            break;
        case DrawingTool::Ellipse:
            handleEllipseTool(event);
            break;
        case DrawingTool::Eraser:
            handleEraserTool(event);
            break;
        case DrawingTool::Measure:
            handleMeasureTool(event);
            break;
    }
    
    emit coordinatesChanged(worldPos);
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent *event)
{
    QVector2D worldPos = screenToWorld(event->pos());
    emit coordinatesChanged(worldPos);
    
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        QVector2D worldDelta(-delta.x() / m_zoomLevel, delta.y() / m_zoomLevel);
        m_viewCenter += worldDelta;
        m_lastMousePos = event->pos();
        update();
        return;
    }
    
    if (m_isSelecting) {
        // Update selection rectangle
        m_drawCurrentPos = worldPos;
        update();
        return;
    }
    
    // Handle control point dragging
    if (m_isEditingControlPoints) {
        updateControlPoint(worldPos);
        return;
    }
    
    if (m_isDrawing && m_currentPrimitive) {
        m_drawCurrentPos = snapToGrid(worldPos);
        
        // Update current primitive for preview
        switch (m_currentTool) {
            case DrawingTool::Line:
                if (auto line = dynamic_cast<LinePrimitive*>(m_currentPrimitive.get())) {
                    // Apply magnetic connection to end point as well
                    QVector2D endPoint = snapToLineEndpoint(m_drawCurrentPos);
                    line->setEndPoint(endPoint);
                }
                break;
            case DrawingTool::Rectangle:
                if (auto rect = dynamic_cast<RectanglePrimitive*>(m_currentPrimitive.get())) {
                    rect->setBottomRight(m_drawCurrentPos);
                }
                break;
            case DrawingTool::Ellipse:
                if (auto ellipse = dynamic_cast<EllipsePrimitive*>(m_currentPrimitive.get())) {
                    QVector2D center = (m_drawStartPos + m_drawCurrentPos) * 0.5f;
                    float radiusX = std::abs(m_drawCurrentPos.x() - m_drawStartPos.x()) * 0.5f;
                    float radiusY = std::abs(m_drawCurrentPos.y() - m_drawStartPos.y()) * 0.5f;
                    ellipse->setCenter(center);
                    ellipse->setRadiusX(radiusX);
                    ellipse->setRadiusY(radiusY);
                }
                break;
            case DrawingTool::Measure:
                if (auto dimension = dynamic_cast<DimensionPrimitive*>(m_currentPrimitive.get())) {
                    dimension->setEndPoint(m_drawCurrentPos);
                    float distance = (m_drawCurrentPos - m_drawStartPos).length();
                    float realDistance = worldToUnits(distance);
                    dimension->setMeasurementValue(realDistance);
                }
                break;
            case DrawingTool::BezierCurve:
                if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(m_currentPrimitive.get())) {
                    auto currentPoints = bezier->controlPoints();
                    if (currentPoints.size() >= 4 && m_bezierCreationStage == 1) {
                        // Update end point and auto-generate control points during drag
                        QVector2D direction = m_drawCurrentPos - m_drawStartPos;
                        currentPoints[1] = m_drawStartPos + direction * 0.33f; 
                        currentPoints[2] = m_drawCurrentPos - direction * 0.33f; 
                        currentPoints[3] = m_drawCurrentPos; 
                        bezier->setControlPoints(currentPoints);
                    }
                }
                break;
            case DrawingTool::Spline:
                // Spline tools don't need preview updates during mouse move
                // since they work with discrete point additions
                break;
            default:
                break;
        }
        update();
    }
    
    m_lastMousePos = event->pos();
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    // Handle control point editing release
    if (m_isEditingControlPoints && event->button() == Qt::LeftButton) {
        finishControlPointEdit();
        return;
    }
    
    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    if (m_isSelecting && event->button() == Qt::LeftButton) {
        // Finalize selection rectangle
        QRectF selectionRect(m_drawStartPos.x(), m_drawStartPos.y(),
                           m_drawCurrentPos.x() - m_drawStartPos.x(),
                           m_drawCurrentPos.y() - m_drawStartPos.y());
        selectionRect = selectionRect.normalized(); // Handle negative width/height
        
        selectObjectsInRect(selectionRect);
        m_isSelecting = false;
        update();
        return;
    }
    
    if (m_isDrawing && event->button() == Qt::LeftButton) {
        // Finalize the current drawing operation based on the active tool
        switch (m_currentTool) {
            case DrawingTool::Line:
            case DrawingTool::Rectangle:
            case DrawingTool::Ellipse:
            case DrawingTool::Measure:
                // These tools complete on mouse release
                if (m_currentPrimitive) {
                    m_currentPrimitive->setColor(QColor(255, 255, 255)); // Set final color
                    
                    // Log dimension creation before moving the primitive
                    if (m_currentTool == DrawingTool::Measure) {
                        QVector2D worldPos = screenToWorld(event->pos());
                        if (auto dimension = dynamic_cast<DimensionPrimitive*>(m_currentPrimitive.get())) {
                            float distance = (worldPos - m_drawStartPos).length();
                            float realDistance = worldToUnits(distance);
                            qDebug() << "Measure tool: Created dimension with length" << realDistance << getUnitsString();
                        }
                    }
                    
                    addPrimitive(std::move(m_currentPrimitive));
                }
                m_isDrawing = false;
                break;
                
            case DrawingTool::BezierCurve:
                // Bezier tool: finalize current bezier on mouse release, ready for next click
                if (m_currentPrimitive && m_bezierCreationStage == 1) {
                    qDebug() << "Mouse release: finalizing bezier curve";
                    
                    // Store the end point for potential continuation
                    auto bezier = static_cast<BezierCurvePrimitive*>(m_currentPrimitive.get());
                    auto points = bezier->controlPoints();
                    QVector2D endPoint = points.size() >= 4 ? points[3] : m_drawStartPos;
                    
                    m_currentPrimitive->setColor(QColor(255, 255, 255));
                    m_currentPrimitive->setSelected(false);
                    addPrimitive(std::move(m_currentPrimitive));
                    qDebug() << "Bezier finalized on mouse release. Total primitives:" << m_primitives.size();
                    
                    // Store the last end point for potential chaining
                    m_drawStartPos = endPoint;
                    m_bezierCreationStage = 0;
                    m_isDrawing = false;
                }
                break;
                
            case DrawingTool::Curve:
            case DrawingTool::Spline:
                // These tools continue until right-click or escape
                // Don't finalize on left mouse release
                break;
                
            default:
                m_isDrawing = false;
                break;
        }
        update();
    }
}

void DrawingCanvas::wheelEvent(QWheelEvent *event)
{
    const float zoomFactor = 1.15f;
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
    
    qDebug() << "Wheel event - deltaY:" << deltaY << "pixelDelta:" << numPixels << "angleDelta:" << event->angleDelta();
    
    // Apply zoom based on delta direction
    if (deltaY > 0) {
        m_zoomLevel = std::min(maxZoom, m_zoomLevel * zoomFactor);
        qDebug() << "Zooming IN, new level:" << m_zoomLevel;
    } else if (deltaY < 0) {
        m_zoomLevel = std::max(minZoom, m_zoomLevel / zoomFactor);
        qDebug() << "Zooming OUT, new level:" << m_zoomLevel;
    }
    
    // Keep zoom centered on mouse cursor
    QVector2D mousePos = QVector2D(event->position().toPoint().x() - width() / 2.0f, 
                                   height() / 2.0f - event->position().toPoint().y());
    
    // Adjust view center to keep the point under mouse stable
    QVector2D worldDelta = mousePos * (1.0f/oldZoom - 1.0f/m_zoomLevel);
    m_viewCenter += worldDelta;
    
    emit zoomChanged(m_zoomLevel);
    update();
    
    event->accept();
}

void DrawingCanvas::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Space:
            if (!m_isPanning) {
                m_isPanning = true;
                setCursor(Qt::OpenHandCursor);
            }
            break;
        case Qt::Key_Escape:
            m_isDrawing = false;
            m_isSelecting = false;
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
            clearSelection();
            update();
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
        default:
            QOpenGLWidget::keyPressEvent(event);
            break;
    }
}

void DrawingCanvas::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Space:
            if (m_isPanning) {
                m_isPanning = false;
                setCursor(Qt::ArrowCursor);
            }
            break;
        default:
            QOpenGLWidget::keyReleaseEvent(event);
            break;
    }
}

void DrawingCanvas::contextMenuEvent(QContextMenuEvent *event)
{
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

void DrawingCanvas::enterEvent(QEnterEvent *event)
{
    // Ensure the widget has focus when mouse enters to receive wheel events
    setFocus(Qt::MouseFocusReason);
    QOpenGLWidget::enterEvent(event);
}

void DrawingCanvas::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    // Check if there's a selected primitive
    bool hasSelectedPrimitive = false;
    for (const auto& primitive : m_primitives) {
        if (primitive && primitive->isSelected()) {
            hasSelectedPrimitive = true;
            break;
        }
    }
    
    // Add promote menu only if a primitive is selected
    if (hasSelectedPrimitive) {
        QMenu *promoteMenu = m_contextMenu->addMenu("🎸 Promote to Guitar Component");
        
        // Main guitar parts
        QMenu *mainPartsMenu = promoteMenu->addMenu("Main Parts");
        mainPartsMenu->addAction("Body", [this]() { promoteSelectedToComponent(ComponentType::Body, "Body"); });
        mainPartsMenu->addAction("Neck", [this]() { promoteSelectedToComponent(ComponentType::Neck, "Neck"); });
        mainPartsMenu->addAction("Headstock", [this]() { promoteSelectedToComponent(ComponentType::Headstock, "Headstock"); });
        
        promoteMenu->addSeparator();
        
        // Pickups submenu
        QMenu *pickupsMenu = promoteMenu->addMenu("Pickups");
        pickupsMenu->addAction("Single Coil", [this]() { promoteSelectedToComponent(ComponentType::Pickup, "Single Coil"); });
        pickupsMenu->addAction("Humbucker", [this]() { promoteSelectedToComponent(ComponentType::Pickup, "Humbucker"); });
        pickupsMenu->addAction("P90", [this]() { promoteSelectedToComponent(ComponentType::Pickup, "P90"); });
        pickupsMenu->addAction("Mini Humbucker", [this]() { promoteSelectedToComponent(ComponentType::Pickup, "Mini Humbucker"); });
        
        // Hardware submenu
        QMenu *hardwareMenu = promoteMenu->addMenu("Hardware");
        
        // Bridges
        QMenu *bridgeMenu = hardwareMenu->addMenu("Bridges");
        bridgeMenu->addAction("Tune-o-matic", [this]() { promoteSelectedToComponent(ComponentType::Bridge, "Tune-o-matic"); });
        bridgeMenu->addAction("Vintage Tremolo", [this]() { promoteSelectedToComponent(ComponentType::Bridge, "Tremolo"); });
        bridgeMenu->addAction("Floyd Rose", [this]() { promoteSelectedToComponent(ComponentType::Bridge, "Floyd Rose"); });
        bridgeMenu->addAction("Hardtail", [this]() { promoteSelectedToComponent(ComponentType::Bridge, "Hardtail"); });
        
        // Tuners
        QMenu *tunersMenu = hardwareMenu->addMenu("Tuners");
        tunersMenu->addAction("6-in-line", [this]() { promoteSelectedToComponent(ComponentType::Tuner, "6-in-line"); });
        tunersMenu->addAction("3+3", [this]() { promoteSelectedToComponent(ComponentType::Tuner, "3+3"); });
        tunersMenu->addAction("Locking", [this]() { promoteSelectedToComponent(ComponentType::Tuner, "Locking"); });
        tunersMenu->addAction("Vintage", [this]() { promoteSelectedToComponent(ComponentType::Tuner, "Vintage"); });
        
        // Other Hardware
        hardwareMenu->addAction("Nut", [this]() { promoteSelectedToComponent(ComponentType::Nut, "Standard"); });
        hardwareMenu->addAction("Output Jack", [this]() { promoteSelectedToComponent(ComponentType::OutputJack, "1/4 Mono"); });
        hardwareMenu->addAction("Strap Button", [this]() { promoteSelectedToComponent(ComponentType::StrapButton, "Standard"); });
        
        // Electronics submenu
        QMenu *electronicsMenu = promoteMenu->addMenu("Electronics");
        
        QMenu *knobsMenu = electronicsMenu->addMenu("Knobs");
        knobsMenu->addAction("Volume Knob", [this]() { promoteSelectedToComponent(ComponentType::VolumeKnob, "Standard"); });
        knobsMenu->addAction("Tone Knob", [this]() { promoteSelectedToComponent(ComponentType::ToneKnob, "Standard"); });
        
        QMenu *switchesMenu = electronicsMenu->addMenu("Switches");
        switchesMenu->addAction("3-Way Toggle", [this]() { promoteSelectedToComponent(ComponentType::PickupSwitch, "3-Way"); });
        switchesMenu->addAction("5-Way Selector", [this]() { promoteSelectedToComponent(ComponentType::PickupSwitch, "5-Way"); });
        
        electronicsMenu->addAction("Potentiometer", [this]() { promoteSelectedToComponent(ComponentType::Potentiometer, "500K"); });
        electronicsMenu->addAction("Capacitor", [this]() { promoteSelectedToComponent(ComponentType::Capacitor, "0.022uF"); });
    }
}

void DrawingCanvas::addComponentAtPosition(const QString &componentType, const QString &subType, const QVector2D &position)
{
    if (!m_project) {
        qDebug() << "No project set, cannot add component";
        return;
    }
    
    QVector2D snappedPos = m_snapEnabled ? snapToGrid(position) : position;
    
    std::unique_ptr<GuitarComponent> component;
    
    if (componentType == "Pickup") {
        component = ComponentLibrary::createPickup(snappedPos, subType);
    } else if (componentType == "Bridge") {
        component = ComponentLibrary::createBridge(snappedPos, subType);
    } else if (componentType == "Tuner") {
        component = ComponentLibrary::createTuner(snappedPos, subType);
    }
    
    if (component) {
        qDebug() << "Adding" << componentType << "(" << subType << ") at:" << snappedPos;
        m_project->addComponent(std::move(component));
        emit componentAdded(componentType + " (" + subType + ")", snappedPos);
        update();
    }
}

void DrawingCanvas::zoomIn()
{
    m_zoomLevel *= 1.2f;
    m_zoomLevel = std::min(20.0f, m_zoomLevel);
    qDebug() << "ZoomIn - new level:" << m_zoomLevel;
    emit zoomChanged(m_zoomLevel);
    update();
}

void DrawingCanvas::zoomOut()
{
    m_zoomLevel /= 1.2f;
    m_zoomLevel = std::max(0.05f, m_zoomLevel);
    qDebug() << "ZoomOut - new level:" << m_zoomLevel;
    emit zoomChanged(m_zoomLevel);
    update();
}

void DrawingCanvas::zoomFit()
{
    // Reset to origin and normal zoom
    m_zoomLevel = 1.0f;
    m_viewCenter = QVector2D(0.0f, 0.0f);
    emit zoomChanged(m_zoomLevel);
    update();
}

void DrawingCanvas::zoomActual()
{
    // Reset to origin and 100% zoom
    m_zoomLevel = 1.0f;
    m_viewCenter = QVector2D(0.0f, 0.0f);
    emit zoomChanged(m_zoomLevel);
    update();
}

void DrawingCanvas::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

void DrawingCanvas::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
}

void DrawingCanvas::setGridSize(float size)
{
    m_gridSize = size;
    if (m_gridVisible) {
        update();
    }
}

void DrawingCanvas::setCurrentTool(DrawingTool tool)
{
    // Only reset drawing state if switching to a different tool
    if (m_currentTool != tool) {
        qDebug() << "Tool changed from" << static_cast<int>(m_currentTool) << "to" << static_cast<int>(tool) << "- resetting drawing state";
        
        // Finalize any incomplete primitives before switching tools
        if (m_currentPrimitive && m_isDrawing) {
            qDebug() << "Finalizing incomplete primitive before tool switch";
            
            // For curve-based tools, make sure they have enough points to be valid
            bool shouldFinalize = true;
            
            if (auto curve = dynamic_cast<CurvePrimitive*>(m_currentPrimitive.get())) {
                shouldFinalize = curve->controlPoints().size() >= 2;
            } else if (auto spline = dynamic_cast<SplinePrimitive*>(m_currentPrimitive.get())) {
                shouldFinalize = spline->points().size() >= 2;
            } else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(m_currentPrimitive.get())) {
                shouldFinalize = bezier->controlPoints().size() >= 4;
            }
            
            if (shouldFinalize) {
                m_currentPrimitive->setColor(QColor(255, 255, 255)); // Set final color
                m_currentPrimitive->setSelected(false); // Deselect
                addPrimitive(std::move(m_currentPrimitive));
                qDebug() << "Primitive finalized and added to canvas";
            } else {
                qDebug() << "Discarding incomplete primitive (not enough points)";
                m_currentPrimitive.reset();
            }
        } else {
            m_currentPrimitive.reset();
        }
        
        m_isDrawing = false;
        m_isSelecting = false;
        m_bezierCreationStage = 0;
    }
    m_currentTool = tool;
    
    switch (tool) {
        case DrawingTool::Select:
            setCursor(Qt::ArrowCursor);
            break;
        case DrawingTool::Eraser:
            setCursor(Qt::PointingHandCursor);
            break;
        default:
            setCursor(Qt::CrossCursor);
            break;
    }
}

void DrawingCanvas::setProject(GuitarProject *project)
{
    m_project = project;
    update();
}

QVector2D DrawingCanvas::screenToWorld(const QPoint &screenPos) const
{
    // Simple screen to world conversion - origin at center, Y up
    float x = (screenPos.x() - width() / 2.0f) / m_zoomLevel + m_viewCenter.x();
    float y = (height() / 2.0f - screenPos.y()) / m_zoomLevel + m_viewCenter.y();
    return QVector2D(x, y);
}

QPoint DrawingCanvas::worldToScreen(const QVector2D &worldPos) const
{
    // Simple world to screen conversion
    float x = (worldPos.x() - m_viewCenter.x()) * m_zoomLevel + width() / 2.0f;
    float y = height() / 2.0f - (worldPos.y() - m_viewCenter.y()) * m_zoomLevel;
    return QPoint(static_cast<int>(x), static_cast<int>(y));
}

QVector2D DrawingCanvas::snapToGrid(const QVector2D &pos) const
{
    if (!m_snapEnabled) {
        return pos;
    }
    
    float snappedX = std::round(pos.x() / m_gridSize) * m_gridSize;
    float snappedY = std::round(pos.y() / m_gridSize) * m_gridSize;
    return QVector2D(snappedX, snappedY);
}

void DrawingCanvas::renderGrid()
{
    if (!m_gridVisible) return;
    
    // Set up proper coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-width()/2.0, width()/2.0, -height()/2.0, height()/2.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Apply view transformation
    glScalef(m_zoomLevel, m_zoomLevel, 1.0f);
    glTranslatef(-m_viewCenter.x(), -m_viewCenter.y(), 0.0f);
    
    // Set grid color - more visible at higher zoom
    float opacity = std::min(0.5f, 0.1f + m_zoomLevel * 0.1f);
    glColor4f(0.3f, 0.3f, 0.3f, opacity);
    glLineWidth(1.0f);
    
    // Calculate visible world area
    float halfWidth = (width() / 2.0f) / m_zoomLevel;
    float halfHeight = (height() / 2.0f) / m_zoomLevel;
    
    float left = m_viewCenter.x() - halfWidth;
    float right = m_viewCenter.x() + halfWidth;
    float bottom = m_viewCenter.y() - halfHeight;
    float top = m_viewCenter.y() + halfHeight;
    
    // Align grid to fixed positions
    float gridLeft = floor(left / m_gridSize) * m_gridSize;
    float gridRight = ceil(right / m_gridSize) * m_gridSize;
    float gridBottom = floor(bottom / m_gridSize) * m_gridSize;
    float gridTop = ceil(top / m_gridSize) * m_gridSize;
    
    // Draw grid lines
    glBegin(GL_LINES);
    
    // Vertical lines
    for (float x = gridLeft; x <= gridRight; x += m_gridSize) {
        glVertex2f(x, gridBottom);
        glVertex2f(x, gridTop);
    }
    
    // Horizontal lines  
    for (float y = gridBottom; y <= gridTop; y += m_gridSize) {
        glVertex2f(gridLeft, y);
        glVertex2f(gridRight, y);
    }
    
    glEnd();
}

void DrawingCanvas::renderObjects()
{
    // Set up same coordinate system as grid
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-width()/2.0, width()/2.0, -height()/2.0, height()/2.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Apply same view transformation as grid
    glScalef(m_zoomLevel, m_zoomLevel, 1.0f);
    glTranslatef(-m_viewCenter.x(), -m_viewCenter.y(), 0.0f);
    
    // Render all primitives
    for (const auto& primitive : m_primitives) {
        if (primitive) {
            primitive->render();
        }
    }
    
    // Render current primitive being drawn
    if (m_currentPrimitive) {
        m_currentPrimitive->render();
    }
    
    // Render guitar components from project
    if (m_project) {
        for (const auto& component : m_project->components()) {
            if (component) {
                component->render();
            }
        }
    }
    
    // Render control points for selected objects
    for (auto* selectedObj : m_selectedObjects) {
        if (selectedObj && selectedObj->isSelected()) {
            selectedObj->renderControlPoints();
        }
    }
}

void DrawingCanvas::renderSelection()
{
    // Set up coordinate system like other rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-width()/2.0, width()/2.0, -height()/2.0, height()/2.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(m_zoomLevel, m_zoomLevel, 1.0f);
    glTranslatef(-m_viewCenter.x(), -m_viewCenter.y(), 0.0f);
    
    // Render selection rectangle if selecting
    if (m_isSelecting) {
        glColor4f(0.0f, 0.5f, 1.0f, 0.3f); // Semi-transparent blue
        glLineWidth(1.0f);
        
        float left = m_drawStartPos.x();
        float right = m_drawCurrentPos.x();
        float top = m_drawStartPos.y();
        float bottom = m_drawCurrentPos.y();
        
        // Draw filled selection rectangle
        glBegin(GL_QUADS);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();
        
        // Draw selection rectangle border
        glColor3f(0.0f, 0.5f, 1.0f); // Solid blue border
        glBegin(GL_LINE_LOOP);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();
    }
    
    // Note: Selected object highlighting is handled in the primitives' render() method
    // by checking their selected state and changing color accordingly
}

void DrawingCanvas::renderCrosshair()
{
    // TODO: Render crosshair at mouse position
}

void DrawingCanvas::renderTool()
{
    // TODO: Render tool-specific feedback (e.g., rubber band for rectangle)
}

// Tool handlers (simplified implementations)
void DrawingCanvas::handleSelectTool(QMouseEvent *event) {
    QVector2D worldPos = screenToWorld(event->pos());
    
    if (event->button() == Qt::LeftButton) {
        // First check if we clicked on a control point of a selected object
        for (auto selectedObj : m_selectedObjects) {
            if (selectedObj->isSelected()) {
                m_editingPrimitive = selectedObj;
                int controlPointIndex = findControlPointAt(worldPos, 8.0f);
                if (controlPointIndex >= 0) {
                    qDebug() << "Control point" << controlPointIndex << "clicked for editing";
                    startControlPointEdit(selectedObj, controlPointIndex);
                    return;
                }
            }
        }
        
        // Check if clicked on an existing object
        DrawingPrimitive* clickedObject = nullptr;
        for (auto& primitive : m_primitives) {
            if (primitive && primitive->containsPoint(worldPos, 5.0f)) {
                clickedObject = primitive.get();
                break; // Take the first hit (topmost)
            }
        }
        
        if (clickedObject) {
            // Toggle selection of clicked object
            auto it = std::find(m_selectedObjects.begin(), m_selectedObjects.end(), clickedObject);
            if (it != m_selectedObjects.end()) {
                // Object was selected, deselect it
                clickedObject->setSelected(false);
                m_selectedObjects.erase(it);
            } else {
                // Object was not selected, select it
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    // Clear previous selection if Ctrl not held
                    clearSelection();
                }
                clickedObject->setSelected(true);
                m_selectedObjects.push_back(clickedObject);
            }
            emit selectionChanged();
            update();
        } else {
            // Clicked on empty space - start selection rectangle or clear selection
            if (!(event->modifiers() & Qt::ControlModifier)) {
                clearSelection();
                update();
            }
            
            // Start selection rectangle for drag selection
            m_isSelecting = true;
            m_drawStartPos = worldPos;
            m_drawCurrentPos = worldPos;
        }
    }
}

void DrawingCanvas::handleLineTool(QMouseEvent *event) {
    if (!m_isDrawing) {
        QVector2D worldPos = screenToWorld(event->pos());
        
        // Apply magnetic connection if enabled
        m_drawStartPos = snapToLineEndpoint(snapToGrid(worldPos));
        m_drawCurrentPos = m_drawStartPos;
        
        // Create temporary line primitive
        m_currentPrimitive = std::make_unique<LinePrimitive>(m_drawStartPos, m_drawCurrentPos);
        m_currentPrimitive->setColor(QColor(200, 200, 200));
        m_isDrawing = true;
    }
    // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::handleCurveTool(QMouseEvent *event) {
    QVector2D worldPos = screenToWorld(event->pos());
    QVector2D pos = snapToLineEndpoint(snapToGrid(worldPos));
    
    if (event->button() == Qt::RightButton && m_isDrawing) {
        // Finalize curve on right-click
        if (m_currentPrimitive) {
            m_currentPrimitive->setColor(QColor(255, 255, 255));
            addPrimitive(std::move(m_currentPrimitive));
        }
        m_isDrawing = false;
        return;
    }
    
    if (!m_isDrawing && event->button() == Qt::LeftButton) {
        // Start new curve
        m_currentPrimitive = std::make_unique<CurvePrimitive>();
        static_cast<CurvePrimitive*>(m_currentPrimitive.get())->addControlPoint(pos);
        m_currentPrimitive->setColor(QColor(200, 200, 200));
        m_isDrawing = true;
    } else if (m_isDrawing && event->button() == Qt::LeftButton) {
        // Add control point
        static_cast<CurvePrimitive*>(m_currentPrimitive.get())->addControlPoint(pos);
    }
}

void DrawingCanvas::handleBezierTool(QMouseEvent *event) {
    qDebug() << "Bezier tool called! Stage:" << m_bezierCreationStage << "Button:" << event->button();
    QVector2D pos = snapToGrid(screenToWorld(event->pos()));
    
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
            // Start new bezier - use stored start point if available, otherwise use click position
            QVector2D startPoint = pos;
            
            // Check if we should continue from the last bezier's end point
            if ((m_drawStartPos - pos).length() < 10.0f && !m_primitives.empty()) {
                // If clicked near the last stored point, continue from there
                startPoint = m_drawStartPos;
                qDebug() << "Continuing bezier chain from last point:" << startPoint << "to" << pos;
            } else {
                qDebug() << "Starting new bezier curve at" << pos;
            }
            
            m_currentPrimitive = std::make_unique<BezierCurvePrimitive>();
            auto bezier = static_cast<BezierCurvePrimitive*>(m_currentPrimitive.get());
            
            // Initialize with 4 control points (typical cubic bezier)
            std::vector<QVector2D> points = {startPoint, startPoint, pos, pos};
            bezier->setControlPoints(points);
            m_currentPrimitive->setColor(QColor(100, 255, 100)); // Green during creation
            m_currentPrimitive->setSelected(true); // Show control points during creation
            
            m_isDrawing = true;
            m_bezierCreationStage = 1;
            m_drawStartPos = startPoint;
            qDebug() << "Bezier started, stage 1: drag to set end point (release to finalize)";
        } else if (m_bezierCreationStage == 1 && isShiftPressed) {
            // Shift+click to adjust control points during creation
            qDebug() << "Shift+click: adjusting control points at" << pos;
            int controlPointIndex = findClosestControlPoint(pos);
            if (controlPointIndex >= 0) {
                startControlPointEdit(m_currentPrimitive.get(), controlPointIndex);
            }
        }
        // Note: Mouse release will handle finalization, second click will start a new bezier
    }
}

// Helper method to find closest control point
int DrawingCanvas::findClosestControlPoint(const QVector2D &pos) {
    if (!m_currentPrimitive) return -1;
    
    auto bezier = dynamic_cast<BezierCurvePrimitive*>(m_currentPrimitive.get());
    if (!bezier) return -1;
    
    const auto& points = bezier->controlPoints();
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
    qDebug() << "Spline tool called! m_isDrawing:" << m_isDrawing << "button:" << event->button();
    QVector2D worldPos = screenToWorld(event->pos());
    QVector2D pos = snapToLineEndpoint(snapToGrid(worldPos));
    
    if (event->button() == Qt::RightButton && m_isDrawing) {
        // Finalize spline on right-click
        if (m_currentPrimitive) {
            qDebug() << "Finalizing spline with" << static_cast<SplinePrimitive*>(m_currentPrimitive.get())->points().size() << "points";
            m_currentPrimitive->setColor(QColor(255, 255, 255));
            addPrimitive(std::move(m_currentPrimitive));
        }
        m_isDrawing = false;
        return;
    }
    
    if (!m_isDrawing && event->button() == Qt::LeftButton) {
        // Start new spline
        qDebug() << "Starting new spline at position:" << pos;
        m_currentPrimitive = std::make_unique<SplinePrimitive>();
        static_cast<SplinePrimitive*>(m_currentPrimitive.get())->addPoint(pos);
        m_currentPrimitive->setColor(QColor(200, 200, 200));
        m_isDrawing = true;
    } else if (m_isDrawing && event->button() == Qt::LeftButton) {
        // Add point to spline
        qDebug() << "Adding point to spline:" << pos;
        static_cast<SplinePrimitive*>(m_currentPrimitive.get())->addPoint(pos);
    }
}

void DrawingCanvas::handleRectangleTool(QMouseEvent *event) {
    if (!m_isDrawing) {
        m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
        m_drawCurrentPos = m_drawStartPos;
        
        // Create temporary rectangle
        m_currentPrimitive = std::make_unique<RectanglePrimitive>(m_drawStartPos, m_drawCurrentPos);
        m_currentPrimitive->setColor(QColor(200, 200, 200));
        m_isDrawing = true;
    }
    // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::handleEllipseTool(QMouseEvent *event) {
    if (!m_isDrawing) {
        m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
        
        // Create temporary ellipse
        m_currentPrimitive = std::make_unique<EllipsePrimitive>(m_drawStartPos, 10.0f, 10.0f);
        m_currentPrimitive->setColor(QColor(200, 200, 200));
        m_isDrawing = true;
    }
    // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::selectObjectsInRect(const QRectF &rect) {
    bool selectionChanged = false;
    
    for (auto& primitive : m_primitives) {
        if (primitive) {
            QRectF primitiveBounds = primitive->boundingRect();
            
            // Check if primitive intersects with selection rectangle
            if (rect.intersects(primitiveBounds)) {
                // Add to selection if not already selected
                if (std::find(m_selectedObjects.begin(), m_selectedObjects.end(), primitive.get()) == m_selectedObjects.end()) {
                    primitive->setSelected(true);
                    m_selectedObjects.push_back(primitive.get());
                    selectionChanged = true;
                }
            }
        }
    }
    
    if (selectionChanged) {
        emit this->selectionChanged();
    }
}

void DrawingCanvas::selectObjectAt(const QVector2D &pos) {
    // TODO: Implement single object selection
}

void DrawingCanvas::clearSelection() {
    // Deselect all currently selected objects
    for (auto* primitive : m_selectedObjects) {
        if (primitive) {
            primitive->setSelected(false);
        }
    }
    m_selectedObjects.clear();
    emit selectionChanged();
}

// Component addition slots
void DrawingCanvas::addPickup() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    addComponentAtPosition("Pickup", "Single", pos);
}

void DrawingCanvas::addBridge() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    addComponentAtPosition("Bridge", "Fixed", pos);
}

void DrawingCanvas::addTuners() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    addComponentAtPosition("Tuner", "Standard", pos);
}

void DrawingCanvas::addNut() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    qDebug() << "Adding nut at:" << pos;
    
    auto nut = std::make_unique<RectanglePrimitive>(QVector2D(pos.x()-20, pos.y()-3), QVector2D(pos.x()+20, pos.y()+3));
    nut->setColor(QColor(255, 255, 100)); // Yellow color
    addPrimitive(std::move(nut));
    
    emit componentAdded("Nut", pos);
}

void DrawingCanvas::addFrets() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    qDebug() << "Adding frets at:" << pos;
    
    // Add multiple fret lines
    for (int i = 0; i < 12; ++i) {
        float fretPos = pos.y() + i * 30;
        auto fret = std::make_unique<LinePrimitive>(QVector2D(pos.x()-30, fretPos), QVector2D(pos.x()+30, fretPos));
        fret->setColor(QColor(200, 200, 200)); // Light gray
        addPrimitive(std::move(fret));
    }
    
    emit componentAdded("Frets", pos);
}

void DrawingCanvas::addInlay() {
    QVector2D pos = screenToWorld(m_lastMousePos);
    qDebug() << "Adding inlay at:" << pos;
    
    auto inlay = std::make_unique<EllipsePrimitive>(pos, 8.0f, 8.0f);
    inlay->setColor(QColor(255, 200, 100)); // Orange color
    addPrimitive(std::move(inlay));
    
    emit componentAdded("Inlay", pos);
}

// Primitive management methods
void DrawingCanvas::addPrimitive(std::unique_ptr<DrawingPrimitive> primitive) {
    if (primitive) {
        qDebug() << "Adding primitive of type:" << static_cast<int>(primitive->type()) << "Total primitives:" << m_primitives.size() + 1;
        m_primitives.push_back(std::move(primitive));
        update();
    }
}

void DrawingCanvas::clearPrimitives() {
    m_primitives.clear();
    m_promotedComponents.clear(); // Clear component tracking too
    m_componentNameToPrimitive.clear(); // Clear name mapping too
    m_currentPrimitive.reset();
    update();
}

// Helper methods for OpenGL
unsigned int DrawingCanvas::compileShader(const char* source, unsigned int type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        qDebug() << "Shader compilation failed:" << infoLog;
        glDeleteShader(shader);
        return 0; // Return 0 to indicate failure
    }
    
    return shader;
}

unsigned int DrawingCanvas::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    // Check if shaders compiled successfully
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) glDeleteShader(vertexShader);
        if (fragmentShader != 0) glDeleteShader(fragmentShader);
        return 0; // Return 0 to indicate failure
    }
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        qDebug() << "Shader program linking failed:" << infoLog;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0; // Return 0 to indicate failure
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void DrawingCanvas::generateGridVertices(std::vector<float>& vertices) {
    vertices.clear();
    
    // Calculate visible area
    float halfWidth = width() / (2.0f * m_zoomLevel);
    float halfHeight = height() / (2.0f * m_zoomLevel);
    
    float left = m_viewCenter.x() - halfWidth;
    float right = m_viewCenter.x() + halfWidth;
    float bottom = m_viewCenter.y() - halfHeight;
    float top = m_viewCenter.y() + halfHeight;
    
    // Snap to grid boundaries
    float gridLeft = floor(left / m_gridSize) * m_gridSize;
    float gridRight = ceil(right / m_gridSize) * m_gridSize;
    float gridBottom = floor(bottom / m_gridSize) * m_gridSize;
    float gridTop = ceil(top / m_gridSize) * m_gridSize;
    
    // Vertical lines
    for (float x = gridLeft; x <= gridRight; x += m_gridSize) {
        vertices.push_back(x);
        vertices.push_back(gridBottom);
        vertices.push_back(x);
        vertices.push_back(gridTop);
    }
    
    // Horizontal lines
    for (float y = gridBottom; y <= gridTop; y += m_gridSize) {
        vertices.push_back(gridLeft);
        vertices.push_back(y);
        vertices.push_back(gridRight);
        vertices.push_back(y);
    }
}

// Measurement system implementations
QString DrawingCanvas::getUnitsString() const {
    switch (m_units) {
        case Units::Millimeters: return "mm";
        case Units::Centimeters: return "cm";
        case Units::Inches: return "in";
        default: return "mm";
    }
}

float DrawingCanvas::worldToUnits(float worldDistance) const {
    // World coordinates are in pixels, convert to real units
    float mm = worldDistance / m_pixelsPerMM;
    switch (m_units) {
        case Units::Millimeters: return mm;
        case Units::Centimeters: return mm / 10.0f;
        case Units::Inches: return mm / 25.4f;
        default: return mm;
    }
}

float DrawingCanvas::unitsToWorld(float unitDistance) const {
    float mm;
    switch (m_units) {
        case Units::Millimeters: mm = unitDistance; break;
        case Units::Centimeters: mm = unitDistance * 10.0f; break;
        case Units::Inches: mm = unitDistance * 25.4f; break;
        default: mm = unitDistance; break;
    }
    return mm * m_pixelsPerMM;
}

void DrawingCanvas::updateStatusBar() {
    // This will be called by MainWindow to update coordinates display
    update();
}

// Tool handlers
void DrawingCanvas::handleEraserTool(QMouseEvent *event) {
    QVector2D pos = screenToWorld(event->pos());
    
    // Find and remove primitive at click position
    const float eraserRadius = 10.0f; // Erase anything within 10 pixels
    
    auto it = std::remove_if(m_primitives.begin(), m_primitives.end(),
        [&pos, eraserRadius](const std::unique_ptr<DrawingPrimitive>& primitive) {
            if (primitive) {
                return primitive->containsPoint(pos, eraserRadius);
            }
            return false;
        });
    
    if (it != m_primitives.end()) {
        m_primitives.erase(it, m_primitives.end());
        qDebug() << "Erased primitive(s) at position:" << pos;
        update();
    }
}

void DrawingCanvas::handleMeasureTool(QMouseEvent *event) {
    if (!m_isDrawing) {
        // Start measuring
        m_drawStartPos = snapToGrid(screenToWorld(event->pos()));
        m_drawCurrentPos = m_drawStartPos;
        
        // Create temporary dimension primitive
        auto dimension = std::make_unique<DimensionPrimitive>(m_drawStartPos, m_drawCurrentPos);
        dimension->setUnitsString(getUnitsString());
        dimension->setMeasurementValue(0.0f);
        dimension->setColor(QColor(255, 200, 0));
        
        m_currentPrimitive = std::move(dimension);
        m_isDrawing = true;
        qDebug() << "Started measuring at:" << m_drawStartPos;
    }
    // Finalization handled in mouseReleaseEvent
}

void DrawingCanvas::renderDimensionTexts()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QFont font("Arial", 10);
    painter.setFont(font);
    painter.setPen(QColor(255, 200, 0)); // Yellow text color
    
    // Render text for all dimension primitives
    for (const auto& primitive : m_primitives) {
        if (auto dimension = dynamic_cast<DimensionPrimitive*>(primitive.get())) {
            renderDimensionText(&painter, dimension);
        }
    }
    
    // Render text for current dimension being drawn
    if (m_currentPrimitive) {
        if (auto dimension = dynamic_cast<DimensionPrimitive*>(m_currentPrimitive.get())) {
            renderDimensionText(&painter, dimension);
        }
    }
}

void DrawingCanvas::renderDimensionText(QPainter *painter, DimensionPrimitive *dimension)
{
    if (!dimension || !dimension->isVisible()) return;
    
    // Get dimension line endpoints in world coordinates
    QVector2D start = dimension->getStart();
    QVector2D end = dimension->getEnd();
    
    // Calculate text position (midpoint of dimension line with offset)
    QVector2D direction = (end - start).normalized();
    QVector2D perpendicular(-direction.y(), direction.x());
    float offset = 25.0f; // Text offset from measured line
    
    QVector2D textPosWorld = (start + end) * 0.5f + perpendicular * offset;
    
    // Convert world position to screen coordinates
    QPoint textPosScreen = worldToScreen(textPosWorld);
    
    // Get measurement text
    QString text = QString("%1%2").arg(dimension->getMeasurementValue(), 0, 'f', 1)
                                   .arg(dimension->getUnitsString());
    
    // Draw text with background
    QFontMetrics fm(painter->font());
    QRect textRect = fm.boundingRect(text);
    textRect.moveCenter(textPosScreen);
    
    // Semi-transparent background
    painter->fillRect(textRect.adjusted(-2, -1, 2, 1), QColor(0, 0, 0, 128));
    
    // Draw the text
    painter->drawText(textRect, Qt::AlignCenter, text);
}


// Blueprint functionality implementation
bool DrawingCanvas::loadBlueprint(const QString &filePath)
{
    return m_blueprintManager->loadBlueprint(filePath);
}

void DrawingCanvas::setBlueprintVisible(bool visible)
{
    m_blueprintManager->setVisible(visible);
    update();
}

void DrawingCanvas::setBlueprintOpacity(float opacity)
{
    m_blueprintManager->setOpacity(opacity);
    update();
}

void DrawingCanvas::setBlueprintScale(float scale)
{
    m_blueprintManager->setScale(scale);
    update();
}

void DrawingCanvas::setBlueprintPosition(const QVector2D &position)
{
    m_blueprintManager->setPosition(QPointF(position.x(), position.y()));
    update();
}

void DrawingCanvas::clearBlueprint()
{
    m_blueprintManager->clearBlueprint();
    update();
}

void DrawingCanvas::generateGuitarOutlineFromBlueprint()
{
    if (!m_blueprintManager->hasBlueprint()) {
        qDebug() << "No blueprint loaded";
        return;
    }
    
    // Get the processed image from blueprint manager
    QImage blueprintImage;
    
    // For now, we'll need to access the image through the current file
    QString currentFile = m_blueprintManager->getCurrentFile();
    if (currentFile.isEmpty()) {
        qDebug() << "No current blueprint file";
        return;
    }
    
    QImageReader reader(currentFile);
    blueprintImage = reader.read();
    
    if (blueprintImage.isNull()) {
        qDebug() << "Failed to read blueprint image for parsing";
        return;
    }
    
    // Parse the blueprint to generate guitar outline
    qDebug() << "Starting blueprint parsing...";
    m_blueprintParser->parseBlueprint(blueprintImage);
}

bool DrawingCanvas::hasBlueprint() const
{
    return m_blueprintManager->hasBlueprint();
}

bool DrawingCanvas::isBlueprintVisible() const
{
    return m_blueprintManager->isVisible();
}

float DrawingCanvas::getBlueprintOpacity() const
{
    return m_blueprintManager->getOpacity();
}

float DrawingCanvas::getBlueprintScale() const
{
    return m_blueprintManager->getScale();
}

QVector2D DrawingCanvas::getBlueprintPosition() const
{
    QPointF pos = m_blueprintManager->getPosition();
    return QVector2D(pos.x(), pos.y());
}

void DrawingCanvas::createGuitarOutlinePrimitives(const GuitarOutline& outline)
{
    qDebug() << "Creating guitar outline primitives...";
    
    // Calculate appropriate scale and offset based on bounding rect
    QRectF bounds = outline.boundingRect;
    qDebug() << "Outline bounding rect:" << bounds;
    
    // Target size for the guitar outline (in world coordinates)
    float targetWidth = 800.0f;  // Make it visible on screen
    float targetHeight = 400.0f;
    
    // Calculate scale to fit the target size
    float scaleX = targetWidth / bounds.width();
    float scaleY = targetHeight / bounds.height();
    float scale = qMin(scaleX, scaleY); // Use smaller scale to maintain aspect ratio
    
    // Center the outline at origin
    QVector2D offset(-bounds.center().x() * scale, -bounds.center().y() * scale);
    
    qDebug() << "Using scale:" << scale << "offset:" << offset;
    
    // Create body outline
    if (!outline.bodyOutline.isEmpty()) {
        auto bodyPrimitive = std::make_unique<PolygonPrimitive>();
        bodyPrimitive->setColor(QColor(0, 255, 0)); // Green for body
        bodyPrimitive->setLineWidth(3.0f);
        bodyPrimitive->setClosed(true);
        bodyPrimitive->setFilled(false);
        
        // Convert contour points to QVector2D and scale them
        for (const auto& contourPoint : outline.bodyOutline) {
            QVector2D scaledPoint = QVector2D(contourPoint.position.x() * scale + offset.x(),
                                            contourPoint.position.y() * scale + offset.y());
            bodyPrimitive->addPoint(scaledPoint);
        }
        
        qDebug() << "Created body outline with" << bodyPrimitive->points().size() << "points";
        qDebug() << "Body outline bounds:" << bodyPrimitive->boundingRect();
        addPrimitive(std::move(bodyPrimitive));
    }
    
    // Create neck outline
    if (!outline.neckOutline.isEmpty()) {
        auto neckPrimitive = std::make_unique<PolygonPrimitive>();
        neckPrimitive->setColor(QColor(255, 255, 0)); // Yellow for neck
        neckPrimitive->setLineWidth(3.0f);
        neckPrimitive->setClosed(true);
        neckPrimitive->setFilled(false);
        
        // Convert contour points to QVector2D and scale them
        for (const auto& contourPoint : outline.neckOutline) {
            QVector2D scaledPoint = QVector2D(contourPoint.position.x() * scale + offset.x(),
                                            contourPoint.position.y() * scale + offset.y());
            neckPrimitive->addPoint(scaledPoint);
        }
        
        qDebug() << "Created neck outline with" << neckPrimitive->points().size() << "points";
        qDebug() << "Neck outline bounds:" << neckPrimitive->boundingRect();
        addPrimitive(std::move(neckPrimitive));
    }
    
    // Create headstock outline
    if (!outline.headstockOutline.isEmpty()) {
        auto headstockPrimitive = std::make_unique<PolygonPrimitive>();
        headstockPrimitive->setColor(QColor(0, 255, 255)); // Cyan for headstock
        headstockPrimitive->setLineWidth(3.0f);
        headstockPrimitive->setClosed(true);
        headstockPrimitive->setFilled(false);
        
        // Convert contour points to QVector2D and scale them
        for (const auto& contourPoint : outline.headstockOutline) {
            QVector2D scaledPoint = QVector2D(contourPoint.position.x() * scale + offset.x(),
                                            contourPoint.position.y() * scale + offset.y());
            headstockPrimitive->addPoint(scaledPoint);
        }
        
        qDebug() << "Created headstock outline with" << headstockPrimitive->points().size() << "points";
        qDebug() << "Headstock outline bounds:" << headstockPrimitive->boundingRect();
        addPrimitive(std::move(headstockPrimitive));
    }
    
    // Add a simple test rectangle to verify polygon rendering works
    auto testRect = std::make_unique<PolygonPrimitive>();
    testRect->setColor(QColor(255, 0, 255)); // Magenta test rectangle
    testRect->setLineWidth(5.0f);
    testRect->setClosed(true);
    testRect->setFilled(false);
    
    // Create a simple 200x100 rectangle at origin
    testRect->addPoint(QVector2D(-100, -50));
    testRect->addPoint(QVector2D(100, -50));
    testRect->addPoint(QVector2D(100, 50));
    testRect->addPoint(QVector2D(-100, 50));
    
    qDebug() << "Created test rectangle with bounds:" << testRect->boundingRect();
    addPrimitive(std::move(testRect));
    
    // Trigger a repaint to show the new outlines
    update();
    
    qDebug() << "Guitar outline primitives creation complete!";
    qDebug() << "Total primitives in canvas:" << m_primitives.size();
}

// Control point interaction methods
int DrawingCanvas::findControlPointAt(const QVector2D &pos, float tolerance)
{
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

bool DrawingCanvas::isControlPointVisible(DrawingPrimitive* primitive, int index)
{
    if (!primitive || !primitive->isSelected()) {
        return false;
    }
    
    if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive)) {
        return index >= 0 && index < bezier->controlPoints().size();
    }
    
    return false;
}

void DrawingCanvas::renderControlPoint(const QVector2D &point, bool highlighted)
{
    float size = highlighted ? 8.0f : 6.0f;
    QColor color = highlighted ? QColor(255, 255, 0) : QColor(255, 0, 0); // Yellow if highlighted, red otherwise
    
    glColor3f(color.redF(), color.greenF(), color.blueF());
    glPointSize(size);
    glBegin(GL_POINTS);
    glVertex2f(point.x(), point.y());
    glEnd();
    
    // Draw a small square around the point for better visibility
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    float halfSize = size * 0.7f;
    glVertex2f(point.x() - halfSize, point.y() - halfSize);
    glVertex2f(point.x() + halfSize, point.y() - halfSize);
    glVertex2f(point.x() + halfSize, point.y() + halfSize);
    glVertex2f(point.x() - halfSize, point.y() + halfSize);
    glEnd();
}

void DrawingCanvas::startControlPointEdit(DrawingPrimitive* primitive, int controlPointIndex)
{
    m_editingPrimitive = primitive;
    m_selectedControlPoint = controlPointIndex;
    m_isEditingControlPoints = true;
    update();
}

void DrawingCanvas::updateControlPoint(const QVector2D &newPos)
{
    if (!m_isEditingControlPoints || !m_editingPrimitive || m_selectedControlPoint < 0) {
        return;
    }
    
    // Use the generic control point interface
    std::vector<QVector2D> controlPoints = m_editingPrimitive->getControlPoints();
    if (m_selectedControlPoint < static_cast<int>(controlPoints.size())) {
        QVector2D snappedPos = snapToGrid(newPos);
        m_editingPrimitive->setControlPointPosition(m_selectedControlPoint, snappedPos);
        update();
    }
}

void DrawingCanvas::finishControlPointEdit()
{
    m_isEditingControlPoints = false;
    m_selectedControlPoint = -1;
    m_editingPrimitive = nullptr;
    update();
}

// Magnetic connection functionality
void DrawingCanvas::setMagneticConnectionEnabled(bool enabled)
{
    m_magneticConnectionEnabled = enabled;
}

void DrawingCanvas::setMagneticConnectionTolerance(float tolerance)
{
    m_magneticConnectionTolerance = tolerance;
}

QVector2D DrawingCanvas::findNearestLineEndpoint(const QVector2D &pos, float tolerance)
{
    QVector2D nearestPoint = pos;
    float minDistance = tolerance;
    
    for (const auto& primitive : m_primitives) {
        if (auto line = dynamic_cast<LinePrimitive*>(primitive.get())) {
            // Check start point
            float distToStart = (line->startPoint() - pos).length();
            if (distToStart < minDistance) {
                minDistance = distToStart;
                nearestPoint = line->startPoint();
            }
            
            // Check end point
            float distToEnd = (line->endPoint() - pos).length();
            if (distToEnd < minDistance) {
                minDistance = distToEnd;
                nearestPoint = line->endPoint();
            }
        }
        else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive.get())) {
            const auto& points = bezier->controlPoints();
            if (points.size() >= 2) {
                // Check start point (first control point)
                float distToStart = (points[0] - pos).length();
                if (distToStart < minDistance) {
                    minDistance = distToStart;
                    nearestPoint = points[0];
                }
                
                // Check end point (last control point)
                float distToEnd = (points.back() - pos).length();
                if (distToEnd < minDistance) {
                    minDistance = distToEnd;
                    nearestPoint = points.back();
                }
            }
        }
        else if (auto spline = dynamic_cast<SplinePrimitive*>(primitive.get())) {
            const auto& points = spline->points();
            if (!points.empty()) {
                // Check first point
                float distToStart = (points.front() - pos).length();
                if (distToStart < minDistance) {
                    minDistance = distToStart;
                    nearestPoint = points.front();
                }
                
                // Check last point
                float distToEnd = (points.back() - pos).length();
                if (distToEnd < minDistance) {
                    minDistance = distToEnd;
                    nearestPoint = points.back();
                }
            }
        }
        else if (auto curve = dynamic_cast<CurvePrimitive*>(primitive.get())) {
            const auto& points = curve->controlPoints();
            if (!points.empty()) {
                // Check first point
                float distToStart = (points.front() - pos).length();
                if (distToStart < minDistance) {
                    minDistance = distToStart;
                    nearestPoint = points.front();
                }
                
                // Check last point
                float distToEnd = (points.back() - pos).length();
                if (distToEnd < minDistance) {
                    minDistance = distToEnd;
                    nearestPoint = points.back();
                }
            }
        }
    }
    
    return nearestPoint;
}

QVector2D DrawingCanvas::snapToLineEndpoint(const QVector2D &pos)
{
    if (!m_magneticConnectionEnabled) {
        return pos;
    }
    
    return findNearestLineEndpoint(pos, m_magneticConnectionTolerance);
}

// New component system methods
void DrawingCanvas::addComponentByType(ComponentType type, const QString &subType)
{
    QVector2D pos = screenToWorld(m_lastMousePos);
    
    if (!m_project) {
        qDebug() << "No project set, cannot add component";
        return;
    }
    
    QVector2D snappedPos = m_snapEnabled ? snapToGrid(pos) : pos;
    
    // Get component specification from database
    ComponentDatabase& db = ComponentDatabase::instance();
    ComponentSpec spec = db.getDefaultComponentSpec(type);
    
    std::unique_ptr<GuitarComponent> component;
    
    // Create component based on type
    QString typeString = db.componentTypeToString(type);
    
    if (type == ComponentType::Pickup) {
        component = ComponentLibrary::createPickup(snappedPos, subType);
    } else if (type == ComponentType::Bridge) {
        component = ComponentLibrary::createBridge(snappedPos, subType);
    } else if (type == ComponentType::Tuner) {
        component = ComponentLibrary::createTuner(snappedPos, subType);
    } else {
        // Create component primitive based on type with proper size and color
        std::unique_ptr<DrawingPrimitive> primitive;
        QColor componentColor;
        
        // Set component-specific properties
        if (typeString == "Body" || (typeString == "Custom" && subType == "Body")) {
            // Large guitar body shape
            auto ellipse = std::make_unique<EllipsePrimitive>(snappedPos, 200.0f, 130.0f);
            ellipse->setFilled(false);
            componentColor = QColor(139, 69, 19); // Brown for body
            primitive = std::move(ellipse);
        } else if (typeString == "Neck" || (typeString == "Custom" && subType == "Neck")) {
            // Long rectangle for neck
            auto rect = std::make_unique<RectanglePrimitive>(
                QVector2D(snappedPos.x() - 120, snappedPos.y() - 8),
                QVector2D(snappedPos.x() + 120, snappedPos.y() + 8)
            );
            componentColor = QColor(160, 82, 45); // Saddle brown for neck
            primitive = std::move(rect);
        } else if (typeString == "Headstock" || (typeString == "Custom" && subType == "Headstock")) {
            // Headstock shape - wider at the top
            auto rect = std::make_unique<RectanglePrimitive>(
                QVector2D(snappedPos.x() - 30, snappedPos.y() - 20),
                QVector2D(snappedPos.x() + 30, snappedPos.y() + 20)
            );
            componentColor = QColor(160, 82, 45); // Same as neck
            primitive = std::move(rect);
        } else if (typeString == "Pickup") {
            // Pickup shape based on subtype
            if (subType == "Humbucker") {
                auto rect = std::make_unique<RectanglePrimitive>(
                    QVector2D(snappedPos.x() - 45, snappedPos.y() - 20),
                    QVector2D(snappedPos.x() + 45, snappedPos.y() + 20)
                );
                componentColor = QColor(40, 40, 40); // Dark for humbuckers
                primitive = std::move(rect);
            } else { // Single coil, P90, etc.
                auto rect = std::make_unique<RectanglePrimitive>(
                    QVector2D(snappedPos.x() - 44, snappedPos.y() - 9),
                    QVector2D(snappedPos.x() + 44, snappedPos.y() + 9)
                );
                componentColor = QColor(245, 245, 220); // Cream for single coils
                primitive = std::move(rect);
            }
        } else if (typeString == "Bridge") {
            // Bridge shape based on type
            if (subType.contains("Tremolo") || subType.contains("Floyd")) {
                auto rect = std::make_unique<RectanglePrimitive>(
                    QVector2D(snappedPos.x() - 42, snappedPos.y() - 25),
                    QVector2D(snappedPos.x() + 42, snappedPos.y() + 25)
                );
                componentColor = QColor(192, 192, 192); // Silver for tremolo
                primitive = std::move(rect);
            } else { // Tune-o-matic, hardtail
                auto rect = std::make_unique<RectanglePrimitive>(
                    QVector2D(snappedPos.x() - 40, snappedPos.y() - 6),
                    QVector2D(snappedPos.x() + 40, snappedPos.y() + 6)
                );
                componentColor = QColor(218, 165, 32); // Gold/brass for tune-o-matic
                primitive = std::move(rect);
            }
        } else if (typeString == "Tuner") {
            // Tuner shape - circular
            auto ellipse = std::make_unique<EllipsePrimitive>(snappedPos, 12.0f, 12.0f);
            ellipse->setFilled(true);
            componentColor = QColor(192, 192, 192); // Silver for tuners
            primitive = std::move(ellipse);
        } else if (typeString == "Nut") {
            // Nut shape - small rectangle
            auto rect = std::make_unique<RectanglePrimitive>(
                QVector2D(snappedPos.x() - 22, snappedPos.y() - 3),
                QVector2D(snappedPos.x() + 22, snappedPos.y() + 3)
            );
            componentColor = QColor(255, 228, 196); // Bone color
            primitive = std::move(rect);
        } else {
            // For other component types, create a basic rectangular component with database dimensions
            float width = spec.dimensions.x() > 0 ? spec.dimensions.x() : 20.0f;
            float height = spec.dimensions.y() > 0 ? spec.dimensions.y() : 20.0f;
            
            auto rect = std::make_unique<RectanglePrimitive>(
                QVector2D(snappedPos.x() - width/2, snappedPos.y() - height/2),
                QVector2D(snappedPos.x() + width/2, snappedPos.y() + height/2)
            );
            
            // Set colors based on category
            if (typeString.contains("Knob")) {
                componentColor = QColor(50, 50, 50); // Dark for knobs
            } else if (typeString.contains("Switch")) {
                componentColor = QColor(100, 100, 100); // Gray for switches
            } else if (typeString.contains("Output Jack")) {
                componentColor = QColor(200, 200, 200); // Light gray for jacks
            } else if (typeString.contains("Strap Button")) {
                componentColor = QColor(169, 169, 169); // Dark gray for buttons
            } else {
                componentColor = QColor(150, 150, 150); // Default gray
            }
            
            primitive = std::move(rect);
        }
        
        if (primitive) {
            primitive->setColor(componentColor);
            addPrimitive(std::move(primitive));
            
            qDebug() << "Adding" << typeString << "(" << subType << ") at:" << snappedPos;
            emit componentAdded(typeString + " (" + subType + ")", snappedPos);
            update();
            return;
        }
    }
    
    if (component) {
        qDebug() << "Adding" << typeString << "(" << subType << ") at:" << snappedPos;
        component->setName(typeString + " (" + subType + ")");
        
        m_project->addComponent(std::move(component));
        emit componentAdded(typeString + " (" + subType + ")", snappedPos);
        update();
    }
}

bool DrawingCanvas::isPromotedComponent(DrawingPrimitive* primitive) const
{
    return primitive && m_promotedComponents.find(primitive) != m_promotedComponents.end();
}

DrawingCanvas::ComponentInfo DrawingCanvas::getComponentInfo(DrawingPrimitive* primitive) const
{
    auto it = m_promotedComponents.find(primitive);
    if (it != m_promotedComponents.end()) {
        return it->second;
    }
    return ComponentInfo{};
}

DrawingPrimitive* DrawingCanvas::findPrimitiveByComponentName(const QString& componentName) const
{
    auto it = m_componentNameToPrimitive.find(componentName);
    if (it != m_componentNameToPrimitive.end()) {
        return it->second;
    }
    return nullptr;
}

void DrawingCanvas::selectComponentByName(const QString& componentName)
{
    qDebug() << "Selecting component by name:" << componentName;
    
    // Clear current selection
    clearSelection();
    
    // Find the primitive by component name
    DrawingPrimitive* primitive = findPrimitiveByComponentName(componentName);
    if (primitive) {
        // Select the primitive
        primitive->setSelected(true);
        m_selectedObjects.push_back(primitive);
        
        qDebug() << "Selected primitive for component:" << componentName;
        emit selectionChanged();
        update();
    } else {
        qDebug() << "Component not found:" << componentName;
    }
}

void DrawingCanvas::promoteSelectedToComponent(ComponentType type, const QString &subType)
{
    qDebug() << "Promoting selected primitive to component:" << subType;
    
    // Find the first selected primitive
    DrawingPrimitive* selectedPrimitive = nullptr;
    for (const auto& primitive : m_primitives) {
        if (primitive && primitive->isSelected()) {
            selectedPrimitive = primitive.get();
            break;
        }
    }
    
    if (!selectedPrimitive) {
        qDebug() << "No primitive selected for promotion";
        return;
    }
    
    if (!m_project) {
        qDebug() << "No project set, cannot promote to component";
        return;
    }
    
    // Get the primitive's center position
    QRectF bounds = selectedPrimitive->boundingRect();
    QVector2D componentPos(bounds.center().x(), bounds.center().y());
    
    qDebug() << "Promoting primitive at position:" << componentPos;
    
    // Get component specification from database
    ComponentDatabase& db = ComponentDatabase::instance();
    ComponentSpec spec = db.getDefaultComponentSpec(type);
    QString typeString = db.componentTypeToString(type);
    
    // Mark the primitive as a guitar component by changing its color and adding metadata
    selectedPrimitive->setColor(getComponentColor(type, subType));
    
    // Store component information
    ComponentInfo info;
    info.type = type;
    info.subType = subType;
    info.displayName = typeString + " (" + subType + ")";
    m_promotedComponents[selectedPrimitive] = info;
    m_componentNameToPrimitive[info.displayName] = selectedPrimitive;
    
    // Emit signal to register it as a component
    emit componentAdded(info.displayName, componentPos);
    
    // Automatically show properties for the promoted component
    emit componentPromoted(selectedPrimitive, info.displayName);
    
    qDebug() << "Promoted primitive to" << typeString << "(" << subType << ") at:" << componentPos;
    update();
}

QColor DrawingCanvas::getComponentColor(ComponentType type, const QString &subType)
{
    // Set component-specific colors
    switch (type) {
        case ComponentType::Body:
            return QColor(139, 69, 19); // Brown for body
        case ComponentType::Neck:
            return QColor(160, 82, 45); // Saddle brown for neck
        case ComponentType::Headstock:
            return QColor(160, 82, 45); // Same as neck
        case ComponentType::Pickup:
            if (subType == "Humbucker") {
                return QColor(40, 40, 40); // Dark for humbuckers
            } else {
                return QColor(245, 245, 220); // Cream for single coils
            }
        case ComponentType::Bridge:
            if (subType.contains("Tremolo") || subType.contains("Floyd")) {
                return QColor(192, 192, 192); // Silver for tremolo
            } else {
                return QColor(218, 165, 32); // Gold/brass for tune-o-matic
            }
        case ComponentType::Tuner:
            return QColor(192, 192, 192); // Silver for tuners
        case ComponentType::Nut:
            return QColor(255, 228, 196); // Bone color
        case ComponentType::VolumeKnob:
        case ComponentType::ToneKnob:
            return QColor(50, 50, 50); // Dark for knobs
        case ComponentType::PickupSwitch:
            return QColor(100, 100, 100); // Gray for switches
        case ComponentType::OutputJack:
            return QColor(200, 200, 200); // Light gray for jacks
        case ComponentType::StrapButton:
            return QColor(169, 169, 169); // Dark gray for buttons
        case ComponentType::Potentiometer:
            return QColor(80, 80, 80); // Dark gray for pots
        case ComponentType::Capacitor:
            return QColor(255, 165, 0); // Orange for capacitors
        default:
            return QColor(150, 150, 150); // Default gray
    }
}

void DrawingCanvas::addMeasurement(ComponentType measurementType)
{
    QVector2D pos = screenToWorld(m_lastMousePos);
    QVector2D snappedPos = m_snapEnabled ? snapToGrid(pos) : pos;
    
    ComponentDatabase& db = ComponentDatabase::instance();
    QString typeName = db.componentTypeToString(measurementType);
    
    // Create measurement primitive based on type
    std::unique_ptr<DrawingPrimitive> measurementPrimitive;
    
    switch (measurementType) {
        case ComponentType::DimensionLine: {
            // Create a dimension line that user can drag to set length
            auto dimension = std::make_unique<DimensionPrimitive>(snappedPos, snappedPos + QVector2D(100, 0));
            dimension->setUnitsString(getUnitsString());
            dimension->setColor(QColor(255, 200, 0)); // Yellow
            measurementPrimitive = std::move(dimension);
            break;
        }
        case ComponentType::AngleMeasure: {
            // Create angle measurement - for now, create as a simple line with angle
            auto line = std::make_unique<LinePrimitive>(snappedPos, snappedPos + QVector2D(50, 50));
            line->setColor(QColor(200, 255, 0)); // Yellow-green
            measurementPrimitive = std::move(line);
            break;
        }
        case ComponentType::RadiusMeasure: {
            // Create radius measurement - as a circle
            auto circle = std::make_unique<EllipsePrimitive>(snappedPos, 50.0f, 50.0f);
            circle->setColor(QColor(0, 255, 200)); // Cyan
            circle->setFilled(false);
            measurementPrimitive = std::move(circle);
            break;
        }
        default:
            qDebug() << "Unknown measurement type:" << static_cast<int>(measurementType);
            return;
    }
    
    if (measurementPrimitive) {
        qDebug() << "Adding" << typeName << "measurement at:" << snappedPos;
        addPrimitive(std::move(measurementPrimitive));
    }
}

void DrawingCanvas::addPresetMeasurement(const QString &presetName)
{
    ComponentDatabase& db = ComponentDatabase::instance();
    auto presets = db.getMeasurementPresets();
    
    // Find the preset
    ComponentDatabase::MeasurementPreset preset;
    bool found = false;
    for (const auto& p : presets) {
        if (p.name == presetName) {
            preset = p;
            found = true;
            break;
        }
    }
    
    if (!found) {
        qDebug() << "Preset measurement not found:" << presetName;
        return;
    }
    
    QVector2D pos = screenToWorld(m_lastMousePos);
    QVector2D snappedPos = m_snapEnabled ? snapToGrid(pos) : pos;
    
    // Create dimension line with preset value
    float lengthInPixels = preset.defaultValue * m_pixelsPerMM; // Convert mm to pixels
    QVector2D endPos = snappedPos + QVector2D(lengthInPixels, 0);
    
    auto dimension = std::make_unique<DimensionPrimitive>(snappedPos, endPos);
    dimension->setUnitsString(preset.units);
    dimension->setMeasurementValue(preset.defaultValue);
    dimension->setColor(QColor(255, 150, 0)); // Orange for preset measurements
    
    qDebug() << "Adding preset measurement" << presetName << "with value" << preset.defaultValue << preset.units;
    addPrimitive(std::move(dimension));
}
