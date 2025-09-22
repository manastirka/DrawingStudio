#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "PropertyPanel.h"
#include "DrawingPrimitive.h"
#include "GuitarProject.h"
#include "GuitarComponent.h"
#include "GuitarSpecsBrowserDialog.h"
#include "SimpleComponentPropertiesDialog.h"
#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_canvas(nullptr)
    , m_centralSplitter(nullptr)
    , m_componentsDock(nullptr)
    , m_propertiesDock(nullptr)
    , m_layersDock(nullptr)
    , m_componentsTree(nullptr)
    , m_propertyPanel(nullptr)
    , m_layersTree(nullptr)
    , m_isModified(false)
    , m_separatorAction(nullptr)
    , m_statusLabel(nullptr)
    , m_coordsLabel(nullptr)
    , m_zoomLabel(nullptr)
    , m_toolActionGroup(nullptr)
{
    setupUI();
    setupMenus();
    setupToolbars();
    setupStatusBar();
    setupDockWidgets();
    connectSignals();
    
    // Initialize recent files
    for (int i = 0; i < MaxRecentFiles; ++i) {
        m_recentFileActions[i] = new QAction(this);
        m_recentFileActions[i]->setVisible(false);
    }
    
    updateWindowTitle();
    updateRecentFileActions();
    
    // Set initial window size
    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    // Create central widget
    m_centralSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_centralSplitter);
    
    // Create drawing canvas
    m_canvas = new DrawingCanvas(this);
    m_centralSplitter->addWidget(m_canvas);
    
    // Create and set project
    m_project = std::make_unique<GuitarProject>();
    m_canvas->setProject(m_project.get());
    
    // Set splitter proportions
    m_centralSplitter->setStretchFactor(0, 1);
}

void MainWindow::setupMenus()
{
    // File Menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    QAction *newAction = fileMenu->addAction("&New", this, &MainWindow::newProject);
    newAction->setShortcut(QKeySequence::New);
    newAction->setStatusTip("Create a new guitar project");
    
    QAction *openAction = fileMenu->addAction("&Open...", this, &MainWindow::openProject);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open an existing guitar project");
    
    fileMenu->addSeparator();
    
    QAction *saveAction = fileMenu->addAction("&Save", this, &MainWindow::saveProject);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setStatusTip("Save the current project");
    
    QAction *saveAsAction = fileMenu->addAction("Save &As...", this, &MainWindow::saveProjectAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    saveAsAction->setStatusTip("Save the project with a new name");
    
    fileMenu->addSeparator();
    
    // Recent files will be added here (placeholder for now)
    m_separatorAction = fileMenu->addSeparator();
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("E&xit", this, &MainWindow::exitApplication);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    
    // Edit Menu
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    
    QAction *undoAction = editMenu->addAction("&Undo", this, &MainWindow::undo);
    undoAction->setShortcut(QKeySequence::Undo);
    
    QAction *redoAction = editMenu->addAction("&Redo", this, &MainWindow::redo);
    redoAction->setShortcut(QKeySequence::Redo);
    
    editMenu->addSeparator();
    
    QAction *cutAction = editMenu->addAction("Cu&t", this, &MainWindow::cut);
    cutAction->setShortcut(QKeySequence::Cut);
    
    QAction *copyAction = editMenu->addAction("&Copy", this, &MainWindow::copy);
    copyAction->setShortcut(QKeySequence::Copy);
    
    QAction *pasteAction = editMenu->addAction("&Paste", this, &MainWindow::paste);
    pasteAction->setShortcut(QKeySequence::Paste);
    
    editMenu->addSeparator();
    
    QAction *selectAllAction = editMenu->addAction("Select &All", this, &MainWindow::selectAll);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    
    QAction *deleteAction = editMenu->addAction("&Delete", this, &MainWindow::deleteSelected);
    deleteAction->setShortcut(QKeySequence::Delete);
    
    // View Menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    
    QAction *zoomInAction = viewMenu->addAction("Zoom &In", this, &MainWindow::zoomIn);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    
    QAction *zoomOutAction = viewMenu->addAction("Zoom &Out", this, &MainWindow::zoomOut);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    
    QAction *zoomFitAction = viewMenu->addAction("Zoom to &Fit", this, &MainWindow::zoomFit);
    zoomFitAction->setShortcut(QKeySequence("Ctrl+0"));
    
    QAction *zoomActualAction = viewMenu->addAction("&Actual Size", this, &MainWindow::zoomActual);
    zoomActualAction->setShortcut(QKeySequence("Ctrl+1"));
    
    viewMenu->addSeparator();
    
    QAction *toggleGridAction = viewMenu->addAction("Show &Grid", this, &MainWindow::toggleGrid);
    toggleGridAction->setCheckable(true);
    toggleGridAction->setChecked(true);
    
    QAction *toggleSnapAction = viewMenu->addAction("&Snap to Grid", this, &MainWindow::toggleSnap);
    toggleSnapAction->setCheckable(true);
    toggleSnapAction->setChecked(true);
    
    QAction *toggleMagneticAction = viewMenu->addAction("&Magnetic Connection", this, &MainWindow::toggleMagneticConnection);
    toggleMagneticAction->setCheckable(true);
    toggleMagneticAction->setChecked(false);
    toggleMagneticAction->setStatusTip("Automatically connect line endpoints to nearby line endpoints");
    
    QMenu *gridSizeMenu = viewMenu->addMenu("Grid &Size");
    
    QAction *fineGridAction = gridSizeMenu->addAction("&Fine (1mm)", this, &MainWindow::setGridSizeFine);
    fineGridAction->setCheckable(true);
    
    QAction *mediumGridAction = gridSizeMenu->addAction("&Medium (2mm)", this, &MainWindow::setGridSizeMedium);
    mediumGridAction->setCheckable(true);
    mediumGridAction->setChecked(true); // Default
    
    QAction *coarseGridAction = gridSizeMenu->addAction("&Coarse (10mm)", this, &MainWindow::setGridSizeCoarse);
    coarseGridAction->setCheckable(true);
    
    // Create action group for grid sizes
    QActionGroup *gridSizeActionGroup = new QActionGroup(this);
    gridSizeActionGroup->addAction(fineGridAction);
    gridSizeActionGroup->addAction(mediumGridAction);
    gridSizeActionGroup->addAction(coarseGridAction);
    
    viewMenu->addSeparator();
    
    QMenu *unitsMenu = viewMenu->addMenu("&Units");
    
    QAction *mmAction = unitsMenu->addAction("&Millimeters (mm)", this, &MainWindow::setUnitsMillimeters);
    mmAction->setCheckable(true);
    mmAction->setChecked(true); // Default
    
    QAction *cmAction = unitsMenu->addAction("&Centimeters (cm)", this, &MainWindow::setUnitsCentimeters);
    cmAction->setCheckable(true);
    
    QAction *inAction = unitsMenu->addAction("&Inches (in)", this, &MainWindow::setUnitsInches);
    inAction->setCheckable(true);
    
    // Blueprint background functionality
    viewMenu->addSeparator();
    QMenu *blueprintMenu = viewMenu->addMenu("&Blueprint Background");
    
    QAction *loadBlueprintAction = blueprintMenu->addAction("&Load Blueprint...", this, &MainWindow::loadBlueprint);
    loadBlueprintAction->setShortcut(QKeySequence("Ctrl+B"));
    loadBlueprintAction->setStatusTip("Load a blueprint image or PDF as background");
    
    QAction *toggleBlueprintAction = blueprintMenu->addAction("&Show/Hide Blueprint", this, &MainWindow::toggleBlueprint);
    toggleBlueprintAction->setShortcut(QKeySequence("Ctrl+Shift+B"));
    toggleBlueprintAction->setCheckable(true);
    toggleBlueprintAction->setChecked(true);
    
    QAction *clearBlueprintAction = blueprintMenu->addAction("&Clear Blueprint", this, &MainWindow::clearBlueprint);
    clearBlueprintAction->setStatusTip("Remove the current blueprint background");
    
    blueprintMenu->addSeparator();
    QAction *generateOutlineAction = blueprintMenu->addAction("&Generate Guitar Outline", this, [this]() {
        if (m_canvas) {
            m_canvas->generateGuitarOutlineFromBlueprint();
        }
    });
    generateOutlineAction->setShortcut(QKeySequence("Ctrl+G"));
    generateOutlineAction->setStatusTip("Parse blueprint and generate realistic guitar outline");
    
    blueprintMenu->addSeparator();
    QAction *blueprintOpacityAction = blueprintMenu->addAction("Adjust &Opacity...", this, &MainWindow::adjustBlueprintOpacity);
    QAction *blueprintScaleAction = blueprintMenu->addAction("Adjust &Scale...", this, &MainWindow::adjustBlueprintScale);
    
    // Tools Menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    
    // Create action group for mutually exclusive tool selection
    m_toolActionGroup = new QActionGroup(this);
    
    QAction *selectAction = toolsMenu->addAction("&Select", this, &MainWindow::selectTool);
    selectAction->setShortcut(QKeySequence("S"));
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    m_toolActionGroup->addAction(selectAction);
    
    QAction *lineAction = toolsMenu->addAction("&Line", this, &MainWindow::lineTool);
    lineAction->setShortcut(QKeySequence("L"));
    lineAction->setCheckable(true);
    m_toolActionGroup->addAction(lineAction);
    
    QAction *curveAction = toolsMenu->addAction("&Curve", this, &MainWindow::curveTool);
    curveAction->setShortcut(QKeySequence("C"));
    curveAction->setCheckable(true);
    m_toolActionGroup->addAction(curveAction);
    
    QAction *bezierAction = toolsMenu->addAction("&Bezier", this, &MainWindow::bezierTool);
    bezierAction->setShortcut(QKeySequence("B"));
    bezierAction->setCheckable(true);
    m_toolActionGroup->addAction(bezierAction);
    
    QAction *splineAction = toolsMenu->addAction("&Spline", this, &MainWindow::splineTool);
    splineAction->setShortcut(QKeySequence("P")); // Changed from S to P to avoid conflict
    splineAction->setCheckable(true);
    m_toolActionGroup->addAction(splineAction);
    
    QAction *rectangleAction = toolsMenu->addAction("&Rectangle", this, &MainWindow::rectangleTool);
    rectangleAction->setShortcut(QKeySequence("R"));
    rectangleAction->setCheckable(true);
    m_toolActionGroup->addAction(rectangleAction);
    
    QAction *ellipseAction = toolsMenu->addAction("&Ellipse", this, &MainWindow::ellipseTool);
    ellipseAction->setShortcut(QKeySequence("E"));
    ellipseAction->setCheckable(true);
    m_toolActionGroup->addAction(ellipseAction);
    
    toolsMenu->addSeparator();
    
    QAction *eraserAction = toolsMenu->addAction("E&raser", this, &MainWindow::eraserTool);
    eraserAction->setShortcut(QKeySequence("X"));
    eraserAction->setCheckable(true);
    m_toolActionGroup->addAction(eraserAction);
    
    QAction *measureAction = toolsMenu->addAction("&Measure", this, &MainWindow::measureTool);
    measureAction->setShortcut(QKeySequence("M"));
    measureAction->setCheckable(true);
    m_toolActionGroup->addAction(measureAction);
    
    toolsMenu->addSeparator();
    
    QAction *guitarSpecsAction = toolsMenu->addAction("Guitar &Specs Database", this, &MainWindow::showGuitarSpecsDatabase);
    guitarSpecsAction->setStatusTip("Browse guitar specifications from famous manufacturers");
    
    // Help Menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *helpAction = helpMenu->addAction("&Help", this, &MainWindow::showHelp);
    helpAction->setShortcut(QKeySequence::HelpContents);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction("&About", this, &MainWindow::showAbout);
    aboutAction->setStatusTip("About Guitar Builder");
}

void MainWindow::setupToolbars()
{
    // Main toolbar
    QToolBar *mainToolbar = addToolBar("Main");
    mainToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    
    // Add common actions to toolbar
    // TODO: Add icons and implement toolbar actions
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_coordsLabel = new QLabel("X: 0.00, Y: 0.00", this);
    statusBar()->addPermanentWidget(m_coordsLabel);
    
    statusBar()->addPermanentWidget(new QLabel(" | "));
    
    m_zoomLabel = new QLabel("Zoom: 100%", this);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupDockWidgets()
{
    // Components dock
    m_componentsDock = new QDockWidget("Components", this);
    m_componentsTree = new QTreeWidget();
    m_componentsTree->setHeaderLabel("Components");
    m_componentsDock->setWidget(m_componentsTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_componentsDock);
    
    // Connect component tree selection to properties
    connect(m_componentsTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::onComponentSelectionChanged);
    
    // Properties dock
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertyPanel = new PropertyPanel();
    m_propertiesDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
    // Connect property changes
    connect(m_propertyPanel, &PropertyPanel::propertyChanged,
            this, &MainWindow::onPropertyChanged);
    
    // Layers dock (keeping tree widget for now)
    m_layersDock = new QDockWidget("Layers", this);
    m_layersTree = new QTreeWidget();
    m_layersTree->setHeaderLabel("Layers");
    m_layersDock->setWidget(m_layersTree);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    
    // Stack right docks
    tabifyDockWidget(m_propertiesDock, m_layersDock);
    m_propertiesDock->raise();
}

void MainWindow::connectSignals()
{
    // Connect canvas signals
    if (m_canvas) {
        connect(m_canvas, &DrawingCanvas::coordinatesChanged,
                this, [this](const QVector2D &coords) {
                    if (m_canvas) {
                        float x = m_canvas->worldToUnits(coords.x());
                        float y = m_canvas->worldToUnits(coords.y());
                        QString units = m_canvas->getUnitsString();
                        m_coordsLabel->setText(QString("X: %1%3, Y: %2%3")
                                              .arg(x, 0, 'f', 1)
                                              .arg(y, 0, 'f', 1)
                                              .arg(units));
                    }
                });
        
        connect(m_canvas, &DrawingCanvas::zoomChanged,
                this, [this](float zoom) {
                    m_zoomLabel->setText(QString("Zoom: %1%")
                                        .arg(zoom * 100, 0, 'f', 0));
                });
        
        connect(m_canvas, &DrawingCanvas::selectionChanged,
                this, [this]() {
                    // Update property panel when selection changes
                    updatePropertyPanel();
                });
        
        connect(m_canvas, &DrawingCanvas::componentAdded,
                this, [this](const QString &componentType, const QVector2D &position) {
                    addComponentToPanel(componentType, position);
                });
        
        connect(m_canvas, &DrawingCanvas::componentPromoted,
                this, [this](DrawingPrimitive* primitive, const QString &componentName) {
                    onComponentPromoted(primitive, componentName);
                });
    }
    
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        // Save window settings
        QSettings settings;
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState());
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_isModified) {
        return true;
    }
    
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, "Guitar Builder",
                              "The document has been modified.\n"
                              "Do you want to save your changes?",
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    if (ret == QMessageBox::Save) {
        return saveProject();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::updateWindowTitle()
{
    QString title = "Guitar Builder";
    if (!m_currentFile.isEmpty()) {
        QFileInfo fileInfo(m_currentFile);
        title = fileInfo.baseName() + " - " + title;
    }
    if (m_isModified) {
        title = "*" + title;
    }
    setWindowTitle(title);
}

void MainWindow::updateRecentFileActions()
{
    // TODO: Implement recent files functionality
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    m_currentFile = fileName;
    m_isModified = false;
    updateWindowTitle();
    updateRecentFileActions();
}

// Slot implementations
void MainWindow::newProject()
{
    if (maybeSave()) {
        // TODO: Create new project
        setCurrentFile("");
        m_statusLabel->setText("New project created");
    }
}

void MainWindow::openProject()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Open Guitar Project", "", "Guitar Files (*.guitar)");
        if (!fileName.isEmpty()) {
            // TODO: Load project
            setCurrentFile(fileName);
            m_statusLabel->setText("Project opened");
        }
    }
}

bool MainWindow::saveProject()
{
    if (m_currentFile.isEmpty()) {
        return saveProjectAs();
    } else {
        // TODO: Save project
        m_isModified = false;
        updateWindowTitle();
        m_statusLabel->setText("Project saved");
        return true;
    }
}

bool MainWindow::saveProjectAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Guitar Project", "", "Guitar Files (*.guitar)");
    if (fileName.isEmpty()) {
        return false;
    }
    
    // TODO: Save project
    setCurrentFile(fileName);
    m_statusLabel->setText("Project saved as " + fileName);
    return true;
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::undo() { /* TODO: Implement undo */ }
void MainWindow::redo() { /* TODO: Implement redo */ }
void MainWindow::cut() { /* TODO: Implement cut */ }
void MainWindow::copy() { /* TODO: Implement copy */ }
void MainWindow::paste() { /* TODO: Implement paste */ }
void MainWindow::selectAll() {
    // TODO: Implement select all
}

void MainWindow::deleteSelected() {
    // For testing, clear all primitives
    if (m_canvas) {
        m_canvas->clearPrimitives();
    }
}

void MainWindow::zoomIn() { if (m_canvas) m_canvas->zoomIn(); }
void MainWindow::zoomOut() { if (m_canvas) m_canvas->zoomOut(); }
void MainWindow::zoomFit() { if (m_canvas) m_canvas->zoomFit(); }
void MainWindow::zoomActual() { if (m_canvas) m_canvas->zoomActual(); }

void MainWindow::toggleGrid()
{
    if (m_canvas) {
        // Toggle grid visibility
        // TODO: Get current state and toggle
        m_canvas->setGridVisible(true);
    }
}

void MainWindow::toggleSnap()
{
    if (m_canvas) {
        // Toggle snap to grid
        bool currentState = m_canvas->isSnapEnabled();
        m_canvas->setSnapEnabled(!currentState);
    }
}

void MainWindow::toggleMagneticConnection()
{
    if (m_canvas) {
        // Toggle magnetic connection
        bool currentState = m_canvas->isMagneticConnectionEnabled();
        m_canvas->setMagneticConnectionEnabled(!currentState);
    }
}

void MainWindow::setGridSizeFine()
{
    if (m_canvas) {
        m_canvas->setGridSize(3.77953f); // 1mm grid (1mm * 3.77953 pixels/mm)
    }
}

void MainWindow::setGridSizeMedium()
{
    if (m_canvas) {
        m_canvas->setGridSize(7.5591f); // 2mm grid (2mm * 3.77953 pixels/mm)
    }
}

void MainWindow::setGridSizeCoarse()
{
    if (m_canvas) {
        m_canvas->setGridSize(37.7953f); // 10mm grid (10mm * 3.77953 pixels/mm)
    }
}

void MainWindow::selectTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Select); }
void MainWindow::lineTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Line); }
void MainWindow::curveTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Curve); }
void MainWindow::bezierTool() { 
    qDebug() << "Bezier tool selected!";
    if (m_canvas) m_canvas->setCurrentTool(DrawingTool::BezierCurve); 
}
void MainWindow::splineTool() { 
    qDebug() << "Spline tool selected!";
    if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Spline); 
}
void MainWindow::rectangleTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Rectangle); }
void MainWindow::ellipseTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Ellipse); }
void MainWindow::eraserTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Eraser); }
void MainWindow::measureTool() { if (m_canvas) m_canvas->setCurrentTool(DrawingTool::Measure); }

void MainWindow::setUnitsMillimeters() {
    if (m_canvas) {
        m_canvas->setUnits(DrawingCanvas::Units::Millimeters);
        m_statusLabel->setText("Units: Millimeters (mm)");
    }
}

void MainWindow::setUnitsCentimeters() {
    if (m_canvas) {
        m_canvas->setUnits(DrawingCanvas::Units::Centimeters);
        m_statusLabel->setText("Units: Centimeters (cm)");
    }
}

void MainWindow::setUnitsInches() {
    if (m_canvas) {
        m_canvas->setUnits(DrawingCanvas::Units::Inches);
        m_statusLabel->setText("Units: Inches (in)");
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Guitar Builder",
                      "Guitar Builder v1.0\n\n"
                      "A professional CAD application for guitar design.\n\n"
                      "Built with Qt6 and C++20");
}

void MainWindow::showHelp()
{
    QMessageBox::information(this, "Help",
                           "Guitar Builder Help\n\n"
                           "Shortcuts:\n"
                           "S - Select tool\n"
                           "L - Line tool\n"
                           "C - Curve tool\n"
                           "R - Rectangle tool\n"
                           "E - Ellipse tool\n"
                           "M - Measure tool\n\n"
                           "Right-click on canvas to add components.");
}

void MainWindow::updatePropertyPanel()
{
    if (!m_propertyPanel) return;
    
    // Check if canvas has selected objects
    if (m_canvas) {
        auto selectedObjects = m_canvas->selectedObjects();
        
        if (selectedObjects.empty()) {
            // No selection - clear properties
            m_propertyPanel->clearProperties();
        } else if (selectedObjects.size() == 1) {
            // Single object selected - check if it's a promoted component
            DrawingPrimitive* primitive = selectedObjects.front();
            if (primitive) {
                // Check if this primitive is a promoted guitar component
                if (m_canvas->isPromotedComponent(primitive)) {
                    auto componentInfo = m_canvas->getComponentInfo(primitive);
                    m_propertyPanel->showPromotedComponentProperties(primitive, componentInfo.subType, componentInfo.displayName);
                } else {
                    // Regular primitive properties
                    m_propertyPanel->showPrimitiveProperties(primitive);
                }
            }
        } else {
            // Multiple objects selected - show common properties
            m_propertyPanel->showMultiplePrimitiveProperties(selectedObjects);
        }
    } else {
        m_propertyPanel->clearProperties();
    }
}

void MainWindow::onPropertyChanged(const QString& objectName, const QString& propertyName, const QVariant& value)
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    if (selectedObjects.empty()) return;
    
    qDebug() << "Applying property change to" << selectedObjects.size() << "objects:" << propertyName << "=" << value;
    
    // Apply property changes to all selected objects
    for (DrawingPrimitive* primitive : selectedObjects) {
        if (!primitive) continue;
        
        applyPropertyToPrimitive(primitive, propertyName, value);
    }
    
    // Force redraw
    if (m_canvas) {
        m_canvas->update();
    }
}

void MainWindow::applyPropertyToPrimitive(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value)
{
    // Handle readonly/calculated properties that shouldn't be modified
    if (propertyName == "area" || propertyName == "circumference" || propertyName == "perimeter" || 
        propertyName == "actualLength" || propertyName == "arcLength" || propertyName == "aspectRatio" ||
        propertyName == "controlPointCount" || propertyName == "pointCount" || propertyName == "vertices" ||
        propertyName == "type") {
        qDebug() << "Ignoring readonly property:" << propertyName << "(calculated value)";
        return;
    }
    
    // Handle basic properties
    if (propertyName == "visible") {
        primitive->setVisible(value.toBool());
    }
    else if (propertyName == "lineWidth") {
        primitive->setLineWidth(value.toFloat());
    }
    else if (propertyName == "color") {
        primitive->setColor(value.value<QColor>());
    }
    // Handle line properties
    else if (auto line = dynamic_cast<LinePrimitive*>(primitive)) {
        if (propertyName == "length") {
            // Adjust line length while maintaining angle
            QVector2D start = line->startPoint();
            QVector2D end = line->endPoint();
            QVector2D direction = (end - start).normalized();
            float newLength = value.toFloat();
            line->setEndPoint(start + direction * newLength);
        }
        else if (propertyName == "angle") {
            // Adjust line angle while maintaining length
            QVector2D start = line->startPoint();
            QVector2D end = line->endPoint();
            float length = (end - start).length();
            float angleRad = value.toFloat() * M_PI / 180.0f;
            QVector2D newEnd = start + QVector2D(cos(angleRad), sin(angleRad)) * length;
            line->setEndPoint(newEnd);
        }
        // Line tools (stored as metadata for future implementation)
        else if (propertyName == "snapToGrid" || propertyName == "constrainAngle" || propertyName == "showDimensions") {
            qDebug() << "Line tool property changed:" << propertyName << "=" << value;
            // These would be implemented with additional line properties in the future
        }
    }
    // Handle rectangle properties
    else if (auto rect = dynamic_cast<RectanglePrimitive*>(primitive)) {
        if (propertyName == "width") {
            QVector2D tl = rect->topLeft();
            QVector2D br = rect->bottomRight();
            QVector2D center = (tl + br) * 0.5f;
            float newWidth = value.toFloat();
            float height = abs(br.y() - tl.y());
            rect->setTopLeft(QVector2D(center.x() - newWidth/2, center.y() - height/2));
            rect->setBottomRight(QVector2D(center.x() + newWidth/2, center.y() + height/2));
        }
        else if (propertyName == "height") {
            QVector2D tl = rect->topLeft();
            QVector2D br = rect->bottomRight();
            QVector2D center = (tl + br) * 0.5f;
            float width = abs(br.x() - tl.x());
            float newHeight = value.toFloat();
            rect->setTopLeft(QVector2D(center.x() - width/2, center.y() - newHeight/2));
            rect->setBottomRight(QVector2D(center.x() + width/2, center.y() + newHeight/2));
        }
        else if (propertyName == "filled") {
            rect->setFilled(value.toBool());
        }
        // Rectangle tools - corner radius now implemented!
        else if (propertyName == "cornerRadius") {
            float radius = value.toFloat();
            qDebug() << "Setting corner radius to:" << radius;
            rect->setCornerRadius(radius);
        }
        // Other rectangle tools - some now implemented!
        else if (propertyName == "maintainAspectRatio") {
            rect->setMaintainAspectRatio(value.toBool());
        }
        else if (propertyName == "centerOnResize") {
            rect->setCenterOnResize(value.toBool());
        }
        else if (propertyName == "convertToRoundedRect") {
            if (value.toBool()) {
                // Convert to rounded rectangle by setting a default corner radius
                float cornerRadius = std::min(abs(rect->bottomRight().x() - rect->topLeft().x()),
                                               abs(rect->bottomRight().y() - rect->topLeft().y())) * 0.1f;
                qDebug() << "Converting to rounded rect with radius:" << cornerRadius;
                rect->setCornerRadius(cornerRadius);
            } else {
                // Remove rounding
                qDebug() << "Removing rounded corners";
                rect->setCornerRadius(0.0f);
            }
        }
    }
    // Handle ellipse properties
    else if (auto ellipse = dynamic_cast<EllipsePrimitive*>(primitive)) {
        if (propertyName == "radiusX") {
            ellipse->setRadiusX(value.toFloat());
        }
        else if (propertyName == "radiusY") {
            ellipse->setRadiusY(value.toFloat());
        }
        else if (propertyName == "filled") {
            ellipse->setFilled(value.toBool());
        }
        else if (propertyName == "makeCircle") {
            if (value.toBool()) {
                float avgRadius = (ellipse->radiusX() + ellipse->radiusY()) / 2.0f;
                ellipse->setRadiusX(avgRadius);
                ellipse->setRadiusY(avgRadius);
            }
        }
        // Ellipse tools - now implemented!
        else if (propertyName == "subdivisions") {
            ellipse->setSubdivisions(value.toInt());
        }
        else if (propertyName == "showAxes") {
            ellipse->setShowAxes(value.toBool());
        }
        else if (propertyName == "lockAspectRatio") {
            ellipse->setLockAspectRatio(value.toBool());
        }
    }
    // Handle bezier curve properties
    else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive)) {
        if (propertyName == "directDistance") {
            // Modify the distance between start and end points
            const auto& controlPoints = bezier->controlPoints();
            if (controlPoints.size() >= 2) {
                QVector2D start = controlPoints.front();
                QVector2D end = controlPoints.back();
                QVector2D direction = (end - start).normalized();
                float newDistance = value.toFloat();
                
                // Create new control points with updated distance
                std::vector<QVector2D> newPoints = controlPoints;
                newPoints.back() = start + direction * newDistance;
                
                // Update intermediate control points proportionally
                if (controlPoints.size() == 4) {
                    float ratio = newDistance / (end - start).length();
                    newPoints[1] = start + (controlPoints[1] - start) * ratio;
                    newPoints[2] = start + (controlPoints[2] - start) * ratio;
                }
                
                bezier->setControlPoints(newPoints);
            }
        }
        // Bezier visual tools - now implemented!
        else if (propertyName == "showControlLines") {
            bezier->setShowControlLines(value.toBool());
        }
        else if (propertyName == "subdivisionLevel") {
            bezier->setSubdivisionLevel(value.toInt());
        }
        else if (propertyName == "autoTangents") {
            bezier->setAutoTangents(value.toBool());
        }
        else if (propertyName == "symmetricHandles") {
            bezier->setSymmetricHandles(value.toBool());
        }
        // curveSmoothing is not yet implemented - would require rendering changes
        else if (propertyName == "curveSmoothing") {
            qDebug() << "Bézier curve smoothing not yet implemented in renderer";
        }
    }
    // Handle spline properties
    else if (auto spline = dynamic_cast<SplinePrimitive*>(primitive)) {
        if (propertyName == "closed") {
            spline->setClosed(value.toBool());
        }
        else if (propertyName == "filled") {
            spline->setFilled(value.toBool());
        }
        else if (propertyName == "smoothness") {
            spline->setSmoothness(value.toFloat());
        }
        // Spline tools - interpolation type now implemented!
        else if (propertyName == "interpolationType") {
            spline->setInterpolationType(value.toInt());
        }
        // Other spline tools - now implemented!
        else if (propertyName == "tension") {
            spline->setTension(value.toFloat());
        }
        else if (propertyName == "autoSmooth") {
            spline->setAutoSmooth(value.toBool());
        }
        else if (propertyName == "showPoints") {
            spline->setShowPoints(value.toBool());
        }
    }
    // Handle arc properties
    else if (auto arc = dynamic_cast<ArcPrimitive*>(primitive)) {
        if (propertyName == "radius") {
            arc->setRadius(value.toFloat());
        }
        else if (propertyName == "startAngle") {
            arc->setStartAngle(value.toFloat());
        }
        else if (propertyName == "endAngle") {
            arc->setEndAngle(value.toFloat());
        }
        else if (propertyName == "makeFullCircle") {
            if (value.toBool()) {
                arc->setStartAngle(0.0f);
                arc->setEndAngle(360.0f);
            }
        }
        else if (propertyName == "reverseDirection") {
            if (value.toBool()) {
                float temp = arc->startAngle();
                arc->setStartAngle(arc->endAngle());
                arc->setEndAngle(temp);
                qDebug() << "Arc direction reversed: start=" << arc->startAngle() << "end=" << arc->endAngle();
            }
        }
        // Arc tools (future implementation)
        else if (propertyName == "snapAngles" || propertyName == "showCenterlines" || propertyName == "arcQuality") {
            qDebug() << "Arc tool property changed:" << propertyName << "=" << value;
        }
    }
    // Handle circle properties
    else if (auto circle = dynamic_cast<CirclePrimitive*>(primitive)) {
        if (propertyName == "radius") {
            circle->setRadius(value.toFloat());
        }
        else if (propertyName == "diameter") {
            circle->setRadius(value.toFloat() / 2.0f);
        }
        else if (propertyName == "filled") {
            circle->setFilled(value.toBool());
        }
        // Circle visual tools - now implemented!
        else if (propertyName == "showCenterPoint") {
            circle->setShowCenterPoint(value.toBool());
        }
        else if (propertyName == "showQuadrants") {
            circle->setShowQuadrants(value.toBool());
        }
        // Other circle tools (future implementation)
        else if (propertyName == "subdivideToPolygon" || propertyName == "circleQuality") {
            qDebug() << "Circle tool property changed:" << propertyName << "=" << value << "(not yet implemented)";
        }
    }
    // Handle polygon properties
    else if (auto polygon = dynamic_cast<PolygonPrimitive*>(primitive)) {
        if (propertyName == "vertices") {
            // Changing vertex count is complex - log for now
            qDebug() << "Changing polygon vertex count to" << value.toInt() << "not yet implemented";
        }
        else if (propertyName == "filled") {
            polygon->setFilled(value.toBool());
        }
        else if (propertyName == "closed") {
            polygon->setClosed(value.toBool());
        }
        // Polygon tools (future implementation - methods don't exist yet)
        else if (propertyName == "regular" || propertyName == "rotation" || 
                 propertyName == "snapVertices" || propertyName == "showAngles" || propertyName == "cornerRadius") {
            qDebug() << "Polygon tool property changed:" << propertyName << "=" << value;
        }
    }
    // Handle curve properties (similar to spline but with different characteristics)
    else if (auto curve = dynamic_cast<CurvePrimitive*>(primitive)) {
        if (propertyName == "closed") {
            curve->setClosed(value.toBool());
        }
        // Curve type selection - now implemented!
        else if (propertyName == "curveType") {
            curve->setCurveType(value.toInt());
        }
        // Show control polygon - now implemented!
        else if (propertyName == "showControlPolygon") {
            curve->setShowControlPolygon(value.toBool());
        }
        // Other curve properties that still need implementation
        else if (propertyName == "simplifyTolerance" || propertyName == "smoothPasses") {
            qDebug() << "Curve property" << propertyName << "not yet implemented";
        }
    }
    // Handle dimension properties
    else if (auto dimension = dynamic_cast<DimensionPrimitive*>(primitive)) {
        if (propertyName == "unitsString") {
            dimension->setUnitsString(value.toString());
        }
        else if (propertyName == "measurementValue") {
            dimension->setMeasurementValue(value.toFloat());
        }
        // DimensionPrimitive doesn't have these methods yet
        else if (propertyName == "precision" || propertyName == "units" || 
                 propertyName == "showArrows" || propertyName == "offset") {
            qDebug() << "Dimension property" << propertyName << "not yet implemented";
        }
        // Dimension tools (future implementation)
        else if (propertyName == "dimensionStyle" || propertyName == "textSize" || 
                 propertyName == "autoPosition" || propertyName == "displayText" ||
                 propertyName == "extensionLineOffset" || propertyName == "arrowSize" ||
                 propertyName == "textPosition" || propertyName == "showUnits") {
            qDebug() << "Dimension tool property changed:" << propertyName << "=" << value;
        }
    }
    
    // Update the canvas
    m_canvas->update();
}

void MainWindow::clearBlueprint()
{
    if (m_canvas) {
        m_canvas->clearBlueprint();
        m_statusLabel->setText("Blueprint cleared");
    }
}

void MainWindow::adjustBlueprintOpacity()
{
    if (!m_canvas || !m_canvas->hasBlueprint()) {
        QMessageBox::information(this, "Info", "No blueprint image loaded");
        return;
    }
    
    bool ok;
    float currentOpacity = m_canvas->getBlueprintOpacity();
    double opacity = QInputDialog::getDouble(this, "Blueprint Opacity", 
        "Enter opacity (0.0 = transparent, 1.0 = opaque):", 
        currentOpacity, 0.0, 1.0, 2, &ok);
    
    if (ok) {
        m_canvas->setBlueprintOpacity(static_cast<float>(opacity));
        m_statusLabel->setText(QString("Blueprint opacity set to %1").arg(opacity, 0, 'f', 2));
    }
}

void MainWindow::adjustBlueprintScale()
{
    if (!m_canvas || !m_canvas->hasBlueprint()) {
        QMessageBox::information(this, "Info", "No blueprint image loaded");
        return;
    }
    
    bool ok;
    float currentScale = m_canvas->getBlueprintScale();
    double scale = QInputDialog::getDouble(this, "Blueprint Scale", 
        "Enter scale factor (1.0 = original size):", 
        currentScale, 0.1, 10.0, 2, &ok);
    
    if (ok) {
        m_canvas->setBlueprintScale(static_cast<float>(scale));
        m_statusLabel->setText(QString("Blueprint scale set to %1").arg(scale, 0, 'f', 2));
    }
}

void MainWindow::loadBlueprint()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Blueprint", "",
        "Images (*.png *.jpg *.jpeg *.bmp *.tiff);;PDF Files (*.pdf);;All Files (*)");
    
    if (!fileName.isEmpty() && m_canvas) {
        if (m_canvas->loadBlueprint(fileName)) {
            m_statusLabel->setText(QString("Blueprint loaded: %1").arg(QFileInfo(fileName).baseName()));
        } else {
            QMessageBox::warning(this, "Error", "Failed to load blueprint image");
        }
    }
}

void MainWindow::toggleBlueprint()
{
    if (m_canvas && m_canvas->hasBlueprint()) {
        bool isVisible = m_canvas->isBlueprintVisible();
        m_canvas->setBlueprintVisible(!isVisible);
        m_statusLabel->setText(QString("Blueprint %1").arg(!isVisible ? "shown" : "hidden"));
    }
}

void MainWindow::showGuitarDimensions()
{
    // Placeholder for guitar dimensions dialog
    QMessageBox::information(this, "Guitar Dimensions", "Guitar dimensions dialog placeholder");
}

void MainWindow::showGuitarSpecsDatabase()
{
    GuitarSpecsBrowserDialog dialog(this);
    dialog.exec();
}

void MainWindow::createParametricGuitar()
{
    // Placeholder for parametric guitar creation
    QMessageBox::information(this, "Parametric Guitar", "Parametric guitar creation placeholder");
}

void MainWindow::createParametricGuitar(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarBody(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarNeck(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarHeadstock(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarPickups(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarBridge(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::generateGuitarControls(const GuitarDimensions& dimensions)
{
    // Placeholder implementation
}

void MainWindow::addComponentToPanel(const QString& componentType, const QVector2D& position)
{
    if (!m_componentsTree) {
        return;
    }
    
    // Parse component type to get category and name
    QStringList parts = componentType.split(" (");
    QString name = parts[0];
    QString subtype = parts.size() > 1 ? parts[1].chopped(1) : "Standard"; // Remove closing parenthesis
    
    // Determine category based on component name
    QString category;
    if ((name == "Custom" && (subtype == "Body" || subtype == "Neck" || subtype == "Headstock")) ||
        name == "Body" || name == "Neck" || name == "Headstock") {
        category = "Main Parts";
        name = (name == "Custom") ? subtype : name; // Use the subtype as the display name for main parts when Custom
    } else if (name == "Pickup" || name.contains("Pickup")) {
        category = "Pickups";
    } else if (name == "Bridge" || name == "Tuner" || name == "Nut" || name == "Fret" ||
               name == "Output Jack" || name == "Strap Button" || name == "Tailpiece" ||
               name == "String Guide" || name == "Pickup Ring" || name == "Scratch Plate" ||
               name == "Control Cavity Cover" || name == "Neck Plate" || name == "Truss Rod Cover" ||
               name == "Fret Marker" || name.contains("Nut")) {
        category = "Hardware";
    } else if (name == "Volume Knob" || name == "Tone Knob" || name == "Pickup Switch" ||
               name == "Potentiometer" || name == "Capacitor") {
        category = "Electronics";
    } else if (name == "Dimension Line" || name == "Angle Measure" || name == "Radius Measure") {
        category = "Measurements";
    } else {
        category = "Other";
    }
    
    // Find or create category item
    QTreeWidgetItem* categoryItem = nullptr;
    for (int i = 0; i < m_componentsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_componentsTree->topLevelItem(i);
        if (item->text(0) == category) {
            categoryItem = item;
            break;
        }
    }
    
    if (!categoryItem) {
        categoryItem = new QTreeWidgetItem(m_componentsTree);
        categoryItem->setText(0, category);
        categoryItem->setExpanded(true);
        
        // Set category icons
        if (category == "Main Parts") {
            categoryItem->setText(0, "🎸 " + category);
        } else if (category == "Pickups") {
            categoryItem->setText(0, "🎸 " + category);
        } else if (category == "Hardware") {
            categoryItem->setText(0, "🔧 " + category);
        } else if (category == "Electronics") {
            categoryItem->setText(0, "⚡ " + category);
        } else if (category == "Measurements") {
            categoryItem->setText(0, "📏 " + category);
        }
    }
    
    // Add component item
    QTreeWidgetItem* componentItem = new QTreeWidgetItem(categoryItem);
    componentItem->setText(0, name + " (" + subtype + ")");
    componentItem->setText(1, QString("(%1, %2)").arg(position.x(), 0, 'f', 1).arg(position.y(), 0, 'f', 1));
    componentItem->setData(0, Qt::UserRole, componentType); // Store full component type
    
    // Expand the category and select the new component
    categoryItem->setExpanded(true);
    m_componentsTree->setCurrentItem(componentItem);
    m_componentsTree->scrollToItem(componentItem);
    
    qDebug() << "Added" << componentType << "to components panel under category" << category;
}

void MainWindow::updateComponentsTree()
{
    if (!m_componentsTree || !m_project) {
        return;
    }
    
    m_componentsTree->clear();
    
    // Get all components from the project
    const auto& components = m_project->components();
    
    for (const auto& component : components) {
        if (component) {
            QString componentType = component->name();
            QVector2D position = component->position();
            addComponentToPanel(componentType, position);
        }
    }
}

void MainWindow::onComponentSelectionChanged()
{
    auto selectedItems = m_componentsTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    QTreeWidgetItem* item = selectedItems.first();
    
    // Check if it's a component item (not a category)
    if (item->parent()) {
        QString componentType = item->data(0, Qt::UserRole).toString();
        QString componentName = item->text(0);
        
        qDebug() << "Component selected in tree:" << componentName << "Type:" << componentType;
        
        // Select the corresponding primitive on the canvas
        if (m_canvas) {
            m_canvas->selectComponentByName(componentName);
        }
        
        // Also show the component properties dialog (optional - can be removed if not wanted)
        // showComponentProperties(componentType, componentName);
    }
}

void MainWindow::onComponentPromoted(DrawingPrimitive* primitive, const QString& componentName)
{
    qDebug() << "Component promoted:" << componentName << "- showing properties in panel";
    
    if (m_propertyPanel && primitive) {
        // Parse the component name to extract type (e.g., "Body (Body)" -> "Body")
        QString componentType = componentName;
        if (componentName.contains("(") && componentName.contains(")")) {
            int startIdx = componentName.indexOf("(");
            int endIdx = componentName.lastIndexOf(")");
            componentType = componentName.mid(startIdx + 1, endIdx - startIdx - 1);
        }
        
        // Show promoted component properties in the right panel
        m_propertyPanel->showPromotedComponentProperties(primitive, componentType, componentName);
        
        // Make sure the properties dock is visible
        if (m_propertiesDock) {
            m_propertiesDock->setVisible(true);
            m_propertiesDock->raise();
        }
    }
}

void MainWindow::showComponentProperties(const QString& componentType, const QString& componentName)
{
    qDebug() << "Showing properties for component:" << componentName << "of type:" << componentType;
    
    SimpleComponentPropertiesDialog dialog(componentType, componentName, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        qDebug() << "Component properties dialog accepted";
        // Properties have been saved in the dialog
        // Here we could update the actual component in the project if needed
    }
}
