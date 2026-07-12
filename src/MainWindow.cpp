


#include "MainWindow.h"
#include "IconFactory.h"
#include "DrawingCanvas.h"
#include "PropertyPanel.h"
#include "DrawingProject.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "LayerPanel.h"
#include "ColorPalette.h"
#include "CommandManager.h"
#include "Commands.h"
#include "ClassicTextTool.h"
#include "AdvancedTextEditor.h"
#include "RemoteSDHelper.h"
#include "DiffusionHelper.h"
#include "CommandServer.h"
#include "SAM2ServiceManager.h"
#include "AISettingsDialog.h"
#include "AIImageClient.h"
#include "AIGenerateDialog.h"
#include "AICompositeDialog.h"
#include "CompositeHelper.h"
#include "EdgeSelectionTool.h"
#include "LineExtractor.h"
#include "DXFExporter.h"
#include "LineExtractionDialog.h"
#include "ToolSuggestionService.h"
// #include "AISettingsDialog.h"
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QSettings>
#include <QEventLoop>
#include <QToolButton>
#include <QMenu>
#include <QCloseEvent>
#include <QDebug>
#include <QDockWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QScrollArea>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QPainterPath>
#include <QCheckBox>
#include <QSlider>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QProgressDialog>
#include <QWidgetAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QAbstractItemView>
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>
#include <queue>
#include <cmath>
#include "SpinnerDialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_canvas(nullptr)
    , m_propertiesDock(nullptr)
    , m_layersDock(nullptr)
    , m_propertyPanel(nullptr)
    , m_layerPanel(nullptr)
    , m_colorPalette(nullptr)
    , m_project(nullptr)
    , m_layerManager(new LayerManager(this))
    , m_commandManager(new CommandManager(this))
    , m_statusLabel(nullptr)
    , m_coordsLabel(nullptr)
    , m_zoomLabel(nullptr)
    , m_toolActionGroup(nullptr)
{
    for (int i = 0; i < MaxRecentFiles; ++i) {
        m_recentFileActions[i] = new QAction(this);
        m_recentFileActions[i]->setVisible(false);
        connect(m_recentFileActions[i], &QAction::triggered,
                this, &MainWindow::openRecentFile);
    }

    setupUI();
    setupMenus();
    setupToolbars();
    setupStatusBar();
    setupDockWidgets();
    connectSignals();
    
    updateWindowTitle();
    updateRecentFileActions();
    
    // Restore saved window geometry and dock layout; fall back to defaults.
    // DOCK_LAYOUT_VERSION is bumped when the default dock arrangement changes
    // so stale saved layouts don't override the improved defaults.
    QSettings settings;
    if (!restoreGeometry(settings.value("geometry").toByteArray())) {
        resize(1200, 800);
    }
    restoreState(settings.value("windowState").toByteArray(), DOCK_LAYOUT_VERSION);

    if (m_commandManager) {
        connect(m_commandManager, &CommandManager::historyChanged,
                this, &MainWindow::updateUndoHistoryPanel);
        connect(m_commandManager, &CommandManager::cleanChanged,
                this, [this](bool clean) {
                    m_isModified = !clean;
                    updateWindowTitle();
                });
    }

    // Start command server for bot integration
    m_commandServer = new CommandServer(this);
    m_commandServer->setCanvas(m_canvas);
    m_commandServer->setMainWindow(this);
    m_commandServer->start(19100);
    connect(m_commandServer, &CommandServer::commandReceived,
            this, &MainWindow::executeDrawingCommand);

    m_sam2Service = new SAM2ServiceManager(this);
    connect(m_sam2Service, &SAM2ServiceManager::statusChanged, this,
            [this](bool /*healthy*/, const QString &message) {
                if (m_sam2StatusLabel)
                    m_sam2StatusLabel->setText(message);
            });
    m_sam2Service->start();
}

MainWindow::~MainWindow()
{
    if (m_sam2Service)
        m_sam2Service->stop();
}

void MainWindow::saveUndoState(const QString& description)
{
    if (!m_commandManager || !m_layerManager) {
        return;
    }
    // Captures the "before" state now. Callers mutate the document
    // synchronously after this call, so the "after" state is captured on the
    // next event-loop iteration and the whole operation lands on the same
    // undo stack as regular commands.
    auto* cmd = new SnapshotCommand(m_layerManager, description);
    QTimer::singleShot(0, this, [this, cmd]() {
        std::unique_ptr<SnapshotCommand> command(cmd);
        command->captureAfterState();
        m_commandManager->addCommandWithoutExecuting(std::move(command));
    });
}

void MainWindow::setupUI()
{
    // Create central widget
    m_centralSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_centralSplitter);
    
    // Create drawing canvas
    m_canvas = new DrawingCanvas(this);
    
    m_centralSplitter->addWidget(m_canvas);

    // Create floating mask panel (shown when image with masks is selected)
    createFloatingMaskPanel();
    
    // Connect layer manager to canvas
    m_canvas->setLayerManager(m_layerManager);
    
    // Create and set project
    m_project = std::make_unique<DrawingProject>();
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
    newAction->setStatusTip("Create a new drawing project");
    
    QAction *openAction = fileMenu->addAction("&Open...", this, &MainWindow::openProject);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open an existing drawing project");

    m_recentFilesSeparator = fileMenu->addSeparator();
    for (int i = 0; i < MaxRecentFiles; ++i)
        fileMenu->addAction(m_recentFileActions[i]);
    updateRecentFileActions();
    
    fileMenu->addSeparator();
    
    QAction *saveAction = fileMenu->addAction("&Save", this, &MainWindow::saveProject);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setStatusTip("Save the current project");
    
    QAction *saveAsAction = fileMenu->addAction("Save &As...", this, &MainWindow::saveProjectAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    saveAsAction->setStatusTip("Save the project with a new name");
    
    fileMenu->addSeparator();

    QMenu *exportMenu = fileMenu->addMenu("&Export");
    exportMenu->addAction("Export &Image As...", this, &MainWindow::exportImageAs);
    exportMenu->addSeparator();
    {
        const QList<QByteArray> writers = QImageWriter::supportedImageFormats();
        auto canWrite = [&](const char *fmt) {
            return writers.contains(QByteArray(fmt));
        };
        if (canWrite("png"))
            exportMenu->addAction("PNG...", this, &MainWindow::exportAsPNG);
        if (canWrite("jpg") || canWrite("jpeg"))
            exportMenu->addAction("JPEG...", this, &MainWindow::exportAsJPG);
        if (canWrite("bmp"))
            exportMenu->addAction("BMP...", this, &MainWindow::exportAsBMP);
        if (canWrite("tif") || canWrite("tiff"))
            exportMenu->addAction("TIFF...", this, &MainWindow::exportAsTIFF);
        if (canWrite("webp"))
            exportMenu->addAction("WebP...", this, &MainWindow::exportAsWebP);
        if (canWrite("gif"))
            exportMenu->addAction("GIF...", this, &MainWindow::exportAsGIF);
        if (canWrite("ppm"))
            exportMenu->addAction("PPM...", this, &MainWindow::exportAsPPM);
        if (canWrite("jp2"))
            exportMenu->addAction("JPEG 2000...", this, [this]() {
                exportImageWithDialog(QStringLiteral("JP2"));
            });
        if (canWrite("ico"))
            exportMenu->addAction("ICO...", this, [this]() {
                exportImageWithDialog(QStringLiteral("ICO"));
            });
        if (canWrite("heic"))
            exportMenu->addAction("HEIC...", this, [this]() {
                exportImageWithDialog(QStringLiteral("HEIC"));
            });
    }
    exportMenu->addSeparator();
    exportMenu->addAction("DXF...", this, &MainWindow::exportAsDXF);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("E&xit", this, &MainWindow::exitApplication);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    
    // Edit Menu
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    
    QAction *undoAction = editMenu->addAction("&Undo", this, &MainWindow::undo);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);
    
    QAction *redoAction = editMenu->addAction("&Redo", this, &MainWindow::redo);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);

    if (m_commandManager) {
        connect(m_commandManager, &CommandManager::canUndoChanged,
                undoAction, &QAction::setEnabled);
        connect(m_commandManager, &CommandManager::canRedoChanged,
                redoAction, &QAction::setEnabled);
        connect(m_commandManager, &CommandManager::undoTextChanged,
                undoAction, &QAction::setText);
        connect(m_commandManager, &CommandManager::redoTextChanged,
                redoAction, &QAction::setText);
    }
    
    editMenu->addSeparator();

    QAction *cutAction = editMenu->addAction("Cu&t", this, &MainWindow::cutSelected);
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setStatusTip("Cut selected objects to the clipboard");

    QAction *copyAction = editMenu->addAction("&Copy", this, &MainWindow::copySelected);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setStatusTip("Copy selected objects to the clipboard");

    QAction *pasteAction = editMenu->addAction("&Paste", this, &MainWindow::pasteClipboard);
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setStatusTip("Paste objects from the clipboard");

    QAction *duplicateAction = editMenu->addAction("D&uplicate", this, &MainWindow::duplicateSelected);
    duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+J")));
    duplicateAction->setStatusTip("Duplicate selected objects");

    QAction *groupAction = editMenu->addAction("&Group", this, &MainWindow::groupSelected);
    groupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    groupAction->setStatusTip("Group selected objects");

    QAction *ungroupAction = editMenu->addAction("&Ungroup", this, &MainWindow::ungroupSelected);
    ungroupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    ungroupAction->setStatusTip("Ungroup selected objects");

    editMenu->addSeparator();
    
    QAction *selectAllAction = editMenu->addAction("Select &All", this, &MainWindow::selectAll);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    
    QAction *deleteAction = editMenu->addAction("&Delete", this, &MainWindow::deleteSelected);
    deleteAction->setShortcut(QKeySequence::Delete);
    
    // Removed Align submenu from Edit menu per request

    QAction *shortcutsAction = editMenu->addAction("Customize &Shortcuts...", this, &MainWindow::showShortcutEditor);
    shortcutsAction->setStatusTip("Edit keyboard shortcuts for tools");
    
    // Format Menu
    QMenu *formatMenu = menuBar()->addMenu("F&ormat");

    QMenu *textAlignMenu = formatMenu->addMenu("Text &Alignment");
    textAlignMenu->addAction("Align &Left", this, &MainWindow::setTextAlignLeft);
    textAlignMenu->addAction("Align &Center", this, &MainWindow::setTextAlignCenter);
    textAlignMenu->addAction("Align &Right", this, &MainWindow::setTextAlignRight);
    textAlignMenu->addAction("&Justify", this, &MainWindow::setTextAlignJustify);

    QMenu *typographyMenu = formatMenu->addMenu("&Typography");
    typographyMenu->addAction("&Font Family...", this, &MainWindow::showFontFamilyDialog);
    typographyMenu->addAction("&Letter Spacing...", this, &MainWindow::showLetterSpacingDialog);
    typographyMenu->addAction("Line &Spacing...", this, &MainWindow::showLineSpacingDialog);
    typographyMenu->addAction("&Tracking...", this, &MainWindow::showTrackingDialog);

    QMenu *textEffectsMenu = formatMenu->addMenu("Text &Effects");
    textEffectsMenu->addAction("&Shadow...", this, &MainWindow::showShadowDialog);
    textEffectsMenu->addAction("S&troke...", this, &MainWindow::showStrokeDialog);
    textEffectsMenu->addAction("&Gradient...", this, &MainWindow::showGradientDialog);

    formatMenu->addAction("Text &Box Settings...", this, &MainWindow::showTextBoxDialog);
    formatMenu->addSeparator();
    formatMenu->addAction("&Advanced Text Editor...", this, &MainWindow::showAdvancedTextEditor);

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
    
    // Create action group for mutually exclusive unit selection
    QActionGroup *unitsActionGroup = new QActionGroup(this);
    unitsActionGroup->addAction(mmAction);
    unitsActionGroup->addAction(cmAction);
    unitsActionGroup->addAction(inAction);
    
    viewMenu->addSeparator();
    
    QAction *toggleRulersAction = viewMenu->addAction("Show &Rulers", this, &MainWindow::toggleRulers);
    toggleRulersAction->setCheckable(true);
    toggleRulersAction->setChecked(true); // Default to on
    toggleRulersAction->setShortcut(QKeySequence("Ctrl+R"));
    toggleRulersAction->setStatusTip("Show/hide measurement rulers around the canvas");
    
    // Colors submenu
    viewMenu->addSeparator();
    QMenu *colorsMenu = viewMenu->addMenu("&Colors");
    
    QAction *backgroundColorAction = colorsMenu->addAction("&Background Color...", this, &MainWindow::changeBackgroundColor);
    backgroundColorAction->setStatusTip("Change the canvas background color");
    
    QAction *paperColorAction = colorsMenu->addAction("&Paper Color...", this, &MainWindow::changePaperColor);
    paperColorAction->setStatusTip("Change the A4 paper area color");
    
    
    // Canvas settings
    viewMenu->addSeparator();
    QAction *canvasSettingsAction = viewMenu->addAction("Canvas &Settings", this, [this]() {
        if (m_canvas && m_propertyPanel) {
            m_propertyPanel->showCanvasProperties(m_canvas);
        }
    });
    canvasSettingsAction->setStatusTip("Configure canvas appearance and behavior");
    
    // Tools Menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");

    toolsMenu->addAction("&Select", this, &MainWindow::selectTool);
    toolsMenu->addAction("&Hand/Pan", this, &MainWindow::handTool);

    toolsMenu->addSeparator();
    toolsMenu->addAction("&Line", this, &MainWindow::lineTool);
    toolsMenu->addAction("Angle &Line", this, &MainWindow::angleLineTool);
    toolsMenu->addAction("&Curve", this, &MainWindow::curveTool);
    toolsMenu->addAction("&Bezier", this, &MainWindow::bezierTool);
    toolsMenu->addAction("&Spline", this, &MainWindow::splineTool);

    toolsMenu->addSeparator();
    toolsMenu->addAction("&Rectangle", this, &MainWindow::rectangleTool);
    toolsMenu->addAction("&Ellipse", this, &MainWindow::ellipseTool);
    toolsMenu->addAction("C&ircle", this, &MainWindow::circleTool);
    toolsMenu->addAction("&Arc", this, &MainWindow::arcTool);
    toolsMenu->addAction("&Polygon", this, &MainWindow::polygonTool);

    toolsMenu->addSeparator();
    toolsMenu->addAction("&Brush", this, &MainWindow::brushTool);
    toolsMenu->addAction("E&raser", this, &MainWindow::eraserTool);
    toolsMenu->addAction("&Fill", this, &MainWindow::fillTool);
    toolsMenu->addAction("Bl&ur", this, &MainWindow::blurTool);

    toolsMenu->addSeparator();
    toolsMenu->addAction("&Measure", this, &MainWindow::measureTool);
    toolsMenu->addAction("&Image", this, &MainWindow::imageTool);
    toolsMenu->addAction("&Text", this, &MainWindow::textTool);

    // Image Menu
    QMenu *imageMenu = menuBar()->addMenu("&Image");

    imageMenu->addAction("&Import Image...", this, &MainWindow::imageTool);

    imageMenu->addSeparator();
    QMenu *imageExportMenu = imageMenu->addMenu("&Export");
    imageExportMenu->addAction("Export &Image As...", this, &MainWindow::exportImageAs);
    imageExportMenu->addSeparator();
    {
        const QList<QByteArray> writers = QImageWriter::supportedImageFormats();
        auto canWrite = [&](const char *fmt) {
            return writers.contains(QByteArray(fmt));
        };
        if (canWrite("png"))
            imageExportMenu->addAction("PNG...", this, &MainWindow::exportAsPNG);
        if (canWrite("jpg") || canWrite("jpeg"))
            imageExportMenu->addAction("JPEG...", this, &MainWindow::exportAsJPG);
        if (canWrite("bmp"))
            imageExportMenu->addAction("BMP...", this, &MainWindow::exportAsBMP);
        if (canWrite("tif") || canWrite("tiff"))
            imageExportMenu->addAction("TIFF...", this, &MainWindow::exportAsTIFF);
        if (canWrite("webp"))
            imageExportMenu->addAction("WebP...", this, &MainWindow::exportAsWebP);
        if (canWrite("gif"))
            imageExportMenu->addAction("GIF...", this, &MainWindow::exportAsGIF);
        if (canWrite("ppm"))
            imageExportMenu->addAction("PPM...", this, &MainWindow::exportAsPPM);
        if (canWrite("jp2"))
            imageExportMenu->addAction("JPEG 2000...", this, [this]() {
                exportImageWithDialog(QStringLiteral("JP2"));
            });
        if (canWrite("ico"))
            imageExportMenu->addAction("ICO...", this, [this]() {
                exportImageWithDialog(QStringLiteral("ICO"));
            });
        if (canWrite("heic"))
            imageExportMenu->addAction("HEIC...", this, [this]() {
                exportImageWithDialog(QStringLiteral("HEIC"));
            });
    }
    imageExportMenu->addSeparator();
    imageExportMenu->addAction("DXF...", this, &MainWindow::exportAsDXF);

    imageMenu->addSeparator();

    imageMenu->addAction("Extract Lines from Building &Plan...", this, &MainWindow::extractLinesFromImage);

    imageMenu->addSeparator();

    // Adjustments submenu
    QMenu *adjustMenu = imageMenu->addMenu("&Adjustments");
    adjustMenu->addAction("&Brightness / Contrast...", this, &MainWindow::showBrightnessContrast);
    adjustMenu->addAction("&Hue / Saturation...", this, &MainWindow::showHueSaturation);
    adjustMenu->addAction("&Levels...", this, &MainWindow::showLevelsAdjustment);
    adjustMenu->addAction("&Curves...", this, &MainWindow::showCurvesAdjustment);
    adjustMenu->addSeparator();
    adjustMenu->addAction("&Shadows...", this, &MainWindow::showShadowsAdjustment);
    adjustMenu->addAction("H&ighlights...", this, &MainWindow::showHighlightsAdjustment);

    // Effects submenu
    QMenu *effectsMenu = imageMenu->addMenu("&Effects");

    QMenu *blurMenu = effectsMenu->addMenu("&Blur");
    blurMenu->addAction("&Gaussian Blur...", this, &MainWindow::showGaussianBlur);
    blurMenu->addAction("&Motion Blur...", this, &MainWindow::showMotionBlur);
    blurMenu->addAction("&Radial Blur...", this, &MainWindow::showRadialBlur);
    blurMenu->addAction("&Bokeh Blur...", this, &MainWindow::showBokehBlur);
    blurMenu->addAction("&Surface Blur...", this, &MainWindow::showSurfaceBlur);

    effectsMenu->addSeparator();
    effectsMenu->addAction("Add &Shadow...", this, &MainWindow::addAutoShadow);

    imageMenu->addSeparator();
    imageMenu->addAction("&Auto Enhance", this, &MainWindow::autoEnhanceImage);

    imageMenu->addSeparator();

    // OCR submenu
    // ── Single SAM2 subject-detection flow ──
    // One clean set of entry points; both funnel to the two ImagePrimitive
    // endpoints (startSubjectDetection / startHumanDetection).
    imageMenu->addSeparator();

    QAction *detectSubjectsAction = imageMenu->addAction("&Detect Subjects", this, [this]() {
        if (!m_canvas) return;
        ImagePrimitive *imgPrim = imageForDetection();
        if (!imgPrim) {
            m_statusLabel->setText("Select an image first, then run detection.");
            return;
        }
        selectImageForMaskUI(imgPrim);
        imgPrim->startSubjectDetection();
        m_statusLabel->setText("Detecting subjects…");
        m_canvas->update();
    });
    detectSubjectsAction->setShortcut(QKeySequence("Ctrl+D"));

    QAction *detectHumansAction = imageMenu->addAction("Detect &Humans", this, [this]() {
        if (!m_canvas) return;
        ImagePrimitive *imgPrim = imageForDetection();
        if (!imgPrim) {
            m_statusLabel->setText("Select an image first, then run detection.");
            return;
        }
        selectImageForMaskUI(imgPrim);
        imgPrim->startHumanDetection();
        m_statusLabel->setText("Detecting humans…");
        m_canvas->update();
    });
    detectHumansAction->setShortcut(QKeySequence("Ctrl+Shift+D"));

    imageMenu->addAction("&Mask Settings…", this, &MainWindow::showMaskSettingsPopup);

    QAction *prevMaskAction = imageMenu->addAction("Previous &Mask", this, [this]() {
        selectPreviousMask();
    });
    prevMaskAction->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    prevMaskAction->setToolTip("Cycle to the previous detected mask ([)");

    QAction *nextMaskAction = imageMenu->addAction("Next Ma&sk", this, [this]() {
        selectNextMask();
    });
    nextMaskAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    nextMaskAction->setToolTip("Cycle to the next detected mask (])");

    QAction *invertMaskAction = imageMenu->addAction("&Invert Mask", this, [this]() {
        invertSelectedMask();
    });
    invertMaskAction->setShortcut(QKeySequence("Ctrl+I"));
    invertMaskAction->setToolTip("Invert the current mask selection (Ctrl+I)");

    imageMenu->addSeparator();

    imageMenu->addAction("&Extract Selected Subject(s)", this,
                         &MainWindow::extractSelectedSubjects);

    imageMenu->addAction("Remove &Background", this, [this]() {
        if (!m_canvas) return;
        for (auto* obj : m_canvas->selectedObjects()) {
            if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
                if (imgPrim->getEditableContour().empty()) {
                    m_statusLabel->setText("No mask to remove. Run detection first.");
                    return;
                }
                auto compound = std::make_unique<CompoundCommand>("Remove Background");
                compound->addCommand(std::make_unique<ExtractSubjectCommand>(m_canvas, imgPrim));
                std::vector<DrawingPrimitive*> toDelete = {imgPrim};
                compound->addCommand(std::make_unique<DeletePrimitivesCommand>(m_canvas, toDelete));
                if (m_commandManager) {
                    m_commandManager->executeCommand(std::move(compound));
                }
                m_statusLabel->setText("Background removed.");
                m_canvas->update();
                return;
            }
        }
        m_statusLabel->setText("Select an image first.");
    });

    // AI Menu
    QMenu *aiMenu = menuBar()->addMenu("&AI");

    aiMenu->addAction("API &Settings...", this, &MainWindow::showAISettings);
    aiMenu->addSeparator();
    aiMenu->addAction("&Generate Image...", this, &MainWindow::generateAIImage);
    aiMenu->addAction("&Edit Selected Image...", this, &MainWindow::editSelectedWithAI);
    aiMenu->addAction("Place &Subject in Scene...", this, &MainWindow::placeSubjectInScene);
    
    // Help Menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *helpAction = helpMenu->addAction("&Help", this, &MainWindow::showHelp);
    helpAction->setShortcut(QKeySequence::HelpContents);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction("&About", this, &MainWindow::showAbout);
    aboutAction->setStatusTip("About Drawing Studio");
}

void MainWindow::setupToolbars()
{
    // Elegant Modern Toolbar Styling
    QString modernToolbarStyle = R"(
        QToolBar {
            background: #26282b;
            border: none;
            border-bottom: 1px solid rgba(255,255,255,0.07);
            border-radius: 0px;
            spacing: 4px;
            padding: 2px 6px;
            margin: 0px;
        }
        QToolBar::separator {
            background: rgba(255,255,255,0.09);
            width: 1px;
            margin: 5px 4px;
            border-radius: 1px;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 4px 6px;
            margin: 1px;
            color: #e7eaf0;
            min-width: 34px;
            max-width: 34px;
            min-height: 34px;
            max-height: 34px;
            width: 34px;
            height: 34px;
        }
        QToolButton:hover {
            background: rgba(255,255,255,0.06);
            border: 1px solid rgba(255,255,255,0.10);
        }
        QToolButton:checked {
            background: rgba(138, 180, 255, 0.20);
            border: 1px solid rgba(138, 180, 255, 0.55);
            color: #ffffff;
        }
        QToolButton:pressed {
            background: rgba(138, 180, 255, 0.26);
        }
        QToolButton:checked:hover {
            background: rgba(138, 180, 255, 0.26);
        }
    )";

    // Photoshop-style flat tool strip: darker than the window, hairline group
    // separators, compact square buttons with a clearly visible accent-tinted
    // checked state.
    QString leftToolbarStyle = R"(
        QToolBar#LeftToolbar, QToolBar#FavoritesToolbar {
            background: #1e2023;
            border: none;
            border-right: 1px solid rgba(255,255,255,0.06);
            spacing: 2px;
            padding: 6px 3px;
        }
        QToolBar#LeftToolbar QToolButton,
        QToolBar#FavoritesToolbar QToolButton {
            background: transparent;
            border: none;
            border-radius: 5px;
            padding: 0px;
            margin: 0px;
            min-width: 34px;
            min-height: 34px;
            max-width: 34px;
            max-height: 34px;
        }
        QToolBar#LeftToolbar QToolButton:hover,
        QToolBar#FavoritesToolbar QToolButton:hover {
            background: rgba(255, 255, 255, 0.08);
        }
        QToolBar#LeftToolbar QToolButton:checked,
        QToolBar#FavoritesToolbar QToolButton:checked {
            background: #2f6fed;
        }
        QToolBar#LeftToolbar QToolButton:checked:hover,
        QToolBar#FavoritesToolbar QToolButton:checked:hover {
            background: #3a7af5;
        }
        QToolBar#LeftToolbar::separator,
        QToolBar#FavoritesToolbar::separator {
            height: 1px;
            background: rgba(255, 255, 255, 0.10);
            margin: 5px 8px;
        }
    )";
    
    // Global tooltip styling with transparency
    QString tooltipStyle = R"(
        QToolTip {
            background-color: rgba(30, 30, 30, 0.8);
            color: #e8e8e8;
            border: 1px solid rgba(100, 149, 237, 0.3);
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
            font-weight: 500;
        }
    )";
    
    // Global context menu styling with transparency
    QString contextMenuStyle = R"(
        QMenu {
            background-color: rgba(30, 30, 30, 0.8);
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
    )";
    
    // Main toolbar (top)
    if (!m_mainToolbar) {
        m_mainToolbar = new QToolBar("Main", this);
        m_mainToolbar->setObjectName("MainToolbar");
        addToolBar(Qt::TopToolBarArea, m_mainToolbar);
    }
    m_mainToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_mainToolbar->setMovable(false);
    m_mainToolbar->setFloatable(false);
    m_mainToolbar->setAllowedAreas(Qt::TopToolBarArea);
    m_mainToolbar->setStyleSheet(modernToolbarStyle);
    
    // Tool Settings Panel (in main toolbar) - Dynamic based on active tool
    m_toolSettingsWidget = new QWidget();
    // Allow the widget to use available toolbar space naturally.
    m_toolSettingsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolSettingsWidget->setMinimumWidth(0);
    m_toolSettingsWidget->setMaximumWidth(1600);
    m_toolSettingsLayout = new QHBoxLayout(m_toolSettingsWidget);
    m_toolSettingsLayout->setContentsMargins(10, 3, 10, 3);
    m_toolSettingsLayout->setSpacing(10);
    
    // Photoshop-like options bar: flat, compact, accent #2a82da / #8ab4ff.
    QString sliderStyle = R"(
        QSlider { min-height: 20px; }
        QSlider::groove:horizontal {
            background: #1f2124;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #2a82da;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #8ab4ff;
            width: 12px;
            height: 12px;
            margin: -5px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #ffffff;
        }
    )";
    
    QString labelStyle = "color: #c8ccd4; font-size: 12px;";
    QString valueLabelStyle = "color: #8ab4ff; font-size: 12px;";
    QString checkboxStyle = R"(
        QCheckBox {
            color: #c8ccd4;
            font-size: 12px;
            spacing: 5px;
        }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid #4a4f57;
            border-radius: 3px;
            background: #1f2124;
        }
        QCheckBox::indicator:checked {
            border: 1px solid #2a82da;
            background: #2a82da;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #8ab4ff;
        }
    )";

    // Helper to build a fixed-width, right-aligned numeric readout label.
    auto makeValueLabel = [&](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setStyleSheet(valueLabelStyle);
        lbl->setFixedWidth(44);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return lbl;
    };

    // Setting 1 (e.g., Size, Line Width, etc.)
    m_setting1Label = new QLabel("Setting 1:");
    m_setting1Label->setStyleSheet(labelStyle);
    m_setting1Slider = new QSlider(Qt::Horizontal);
    m_setting1Slider->setRange(1, 100);
    m_setting1Slider->setValue(10);
    m_setting1Slider->setFixedWidth(120);
    m_setting1Slider->setStyleSheet(sliderStyle);
    m_setting1ValueLabel = makeValueLabel("10");
    
    // Setting 2 (e.g., Hardness, Opacity, etc.)
    m_setting2Label = new QLabel("Setting 2:");
    m_setting2Label->setStyleSheet(labelStyle);
    m_setting2Slider = new QSlider(Qt::Horizontal);
    m_setting2Slider->setRange(0, 100);
    m_setting2Slider->setValue(50);
    m_setting2Slider->setFixedWidth(120);
    m_setting2Slider->setStyleSheet(sliderStyle);
    m_setting2ValueLabel = makeValueLabel("50");
    
    // Setting 3 (optional third slider)
    m_setting3Label = new QLabel("Setting 3:");
    m_setting3Label->setStyleSheet(labelStyle);
    m_setting3Slider = new QSlider(Qt::Horizontal);
    m_setting3Slider->setRange(0, 100);
    m_setting3Slider->setValue(50);
    m_setting3Slider->setFixedWidth(120);
    m_setting3Slider->setStyleSheet(sliderStyle);
    m_setting3ValueLabel = makeValueLabel("50");
    
    // Boolean settings (checkboxes)
    m_boolSetting1 = new QCheckBox("Option 1");
    m_boolSetting1->setStyleSheet(checkboxStyle);
    m_boolSetting2 = new QCheckBox("Option 2");
    m_boolSetting2->setStyleSheet(checkboxStyle);

    // Hairline vertical group separators.
    QString sepStyle = "QFrame { color: rgba(255,255,255,0.10); }";
    m_optSep1 = new QFrame();
    m_optSep1->setFrameShape(QFrame::VLine);
    m_optSep1->setFixedHeight(22);
    m_optSep1->setStyleSheet(sepStyle);
    m_optSep2 = new QFrame();
    m_optSep2->setFrameShape(QFrame::VLine);
    m_optSep2->setFixedHeight(22);
    m_optSep2->setStyleSheet(sepStyle);

    // Shared control styles for the flat options bar.
    QString optComboStyle = R"(
        QComboBox {
            background: #1f2124;
            color: #dfe3ea;
            border: 1px solid #3a3f47;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 12px;
            min-height: 20px;
        }
        QComboBox:hover { border: 1px solid #2a82da; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #8ab4ff;
            margin-right: 5px;
        }
        QComboBox QAbstractItemView {
            background: #26282b;
            color: #dfe3ea;
            border: 1px solid #3a3f47;
            selection-background-color: #2a82da;
        }
    )";
    QString optButtonStyle = R"(
        QPushButton {
            background: #2f3237;
            color: #dfe3ea;
            border: 1px solid #3a3f47;
            border-radius: 4px;
            padding: 4px 10px;
            font-size: 12px;
            min-height: 20px;
        }
        QPushButton:hover { background: #3a3f47; border-color: #2a82da; }
        QPushButton:pressed { background: #2a82da; }
    )";

    // Text tool specific widgets
    m_textFontCombo = new QFontComboBox();
    m_textFontCombo->setFixedWidth(170);
    m_textFontCombo->setToolTip("Font family");
    m_textFontCombo->setStyleSheet(optComboStyle);
    m_textFontCombo->hide();

    // Icon tool-buttons for the text options (underline toggle + colour picker),
    // rendered from the header-only IconFactory to match the flat dark look.
    QString iconBtnStyle = R"(
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px;
            min-width: 26px;
            max-width: 26px;
            min-height: 22px;
            max-height: 22px;
        }
        QToolButton:hover {
            background: rgba(255,255,255,0.08);
            border-color: rgba(255,255,255,0.12);
        }
        QToolButton:checked {
            background: rgba(42,130,218,0.30);
            border-color: #2a82da;
        }
    )";

    m_textUnderlineBtn = new QToolButton();
    m_textUnderlineBtn->setCheckable(true);
    m_textUnderlineBtn->setIcon(IconFactory::underline());
    m_textUnderlineBtn->setIconSize(QSize(18, 18));
    m_textUnderlineBtn->setStyleSheet(iconBtnStyle);
    m_textUnderlineBtn->setToolTip("Underline");
    m_textUnderlineBtn->hide();

    m_textColorBtn = new QToolButton();
    m_textColorBtn->setIcon(IconFactory::textColor(Qt::white));
    m_textColorBtn->setIconSize(QSize(18, 18));
    m_textColorBtn->setStyleSheet(iconBtnStyle);
    m_textColorBtn->setToolTip("Text Color");
    m_textColorBtn->hide();

    // SAM2 specific widgets
    m_extractButton = new QPushButton("Extract Subject");
    m_extractButton->setToolTip("Extract the detected subject as a new, movable object");
    m_extractButton->setStyleSheet(R"(
        QPushButton {
            background: #2a82da;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 4px 10px;
            font-size: 12px;
            min-height: 20px;
        }
        QPushButton:hover { background: #3a92ea; }
        QPushButton:pressed { background: #1f6fbf; }
    )");
    m_extractButton->hide(); // Hidden by default

    m_maskSettingsButton = new QPushButton("Mask Settings…");
    m_maskSettingsButton->setToolTip("Refine the detected mask: feather, blur, expand, invert, preview");
    m_maskSettingsButton->setStyleSheet(optButtonStyle);
    m_maskSettingsButton->hide(); // Hidden by default

    // Line style selector
    m_lineStyleLabel = new QLabel("Style:");
    m_lineStyleLabel->setStyleSheet(labelStyle);
    m_lineStyleCombo = new QComboBox();
    m_lineStyleCombo->setFixedWidth(130);
    m_lineStyleCombo->setToolTip("Stroke line style");
    m_lineStyleCombo->setStyleSheet(optComboStyle);
    
    // Add line style options with visual preview
    m_lineStyleCombo->addItem("─────  Solid", static_cast<int>(Qt::SolidLine));
    m_lineStyleCombo->addItem("- - - -  Dashed", static_cast<int>(Qt::DashLine));
    m_lineStyleCombo->addItem("· · · · ·  Dotted", static_cast<int>(Qt::DotLine));
    m_lineStyleCombo->addItem("─ · ─  Dash-Dot", static_cast<int>(Qt::DashDotLine));
    m_lineStyleCombo->addItem("─ · · ─  Dash-Dot-Dot", static_cast<int>(Qt::DashDotDotLine));
    
    // Select by color widgets
    m_selectColorLabel = new QLabel("Color:");
    m_selectColorLabel->setStyleSheet(labelStyle);
    
    m_selectColorButton = new QPushButton();
    m_selectColorButton->setFixedSize(34, 22);
    m_selectColorButton->setToolTip("Choose a color, then select all objects matching it");
    m_selectColorButton->setStyleSheet(R"(
        QPushButton {
            background: #ffffff;
            border: 1px solid #3a3f47;
            border-radius: 4px;
        }
        QPushButton:hover { border: 1px solid #2a82da; }
    )");
    m_selectByColor = Qt::black; // Default color
    
    m_pipetteButton = new QPushButton("⦿");
    m_pipetteButton->setFixedSize(30, 22);
    m_pipetteButton->setToolTip("Pick a color from the canvas");
    m_pipetteButton->setStyleSheet(optButtonStyle);

    // Selection mode widgets
    m_selectionModeLabel = new QLabel("Mode:");
    m_selectionModeLabel->setStyleSheet(labelStyle);
    m_selectionModeCombo = new QComboBox();
    m_selectionModeCombo->addItem("Rectangle", static_cast<int>(DrawingCanvas::SelectionMode::Rectangle));
    m_selectionModeCombo->addItem("Lasso", static_cast<int>(DrawingCanvas::SelectionMode::Lasso));
    m_selectionModeCombo->setFixedWidth(110);
    m_selectionModeCombo->setToolTip("Marquee shape: rectangle or freehand lasso");
    m_selectionModeCombo->setStyleSheet(optComboStyle);

    m_selectSimilarButton = new QPushButton("Select Similar");
    m_selectSimilarButton->setToolTip("Select all objects with a color similar to the current selection");
    m_selectSimilarButton->setStyleSheet(optButtonStyle);

    // Presets widgets
    m_presetLabel = new QLabel("Preset:");
    m_presetLabel->setStyleSheet(labelStyle);
    m_presetCombo = new QComboBox();
    m_presetCombo->setFixedWidth(140);
    m_presetCombo->setToolTip("Apply a saved tool preset");
    m_presetCombo->setStyleSheet(optComboStyle);

    m_savePresetButton = new QToolButton();
    m_savePresetButton->setText("+");
    m_savePresetButton->setToolTip("Save current settings as a preset");
    m_savePresetButton->setAutoRaise(true);
    m_savePresetButton->setStyleSheet(
        "QToolButton { color: #dfe3ea; font-size: 15px; border-radius: 4px; min-width: 22px; min-height: 22px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.08); }");

    // Mask refinement widgets (Image tool)
    m_maskInvertCheck = new QCheckBox("Invert Mask");
    m_maskInvertCheck->setStyleSheet(checkboxStyle);
    m_maskOverlayCheck = new QCheckBox("Preview Mask");
    m_maskOverlayCheck->setStyleSheet(checkboxStyle);

    m_maskFeatherLabel = new QLabel("Feather:");
    m_maskFeatherLabel->setStyleSheet(labelStyle);
    m_maskFeatherSlider = new QSlider(Qt::Horizontal);
    m_maskFeatherSlider->setRange(0, 30);
    m_maskFeatherSlider->setValue(0);
    m_maskFeatherSlider->setFixedWidth(120);
    m_maskFeatherSlider->setStyleSheet(sliderStyle);
    m_maskFeatherValue = new QLabel("0");
    m_maskFeatherValue->setStyleSheet(valueLabelStyle);

    m_maskBlurLabel = new QLabel("Blur:");
    m_maskBlurLabel->setStyleSheet(labelStyle);
    m_maskBlurSlider = new QSlider(Qt::Horizontal);
    m_maskBlurSlider->setRange(0, 20);
    m_maskBlurSlider->setValue(0);
    m_maskBlurSlider->setFixedWidth(120);
    m_maskBlurSlider->setStyleSheet(sliderStyle);
    m_maskBlurValue = new QLabel("0");
    m_maskBlurValue->setStyleSheet(valueLabelStyle);

    m_maskExpandLabel = new QLabel("Expand:");
    m_maskExpandLabel->setStyleSheet(labelStyle);
    m_maskExpandSlider = new QSlider(Qt::Horizontal);
    m_maskExpandSlider->setRange(-20, 20);
    m_maskExpandSlider->setValue(0);
    m_maskExpandSlider->setFixedWidth(120);
    m_maskExpandSlider->setStyleSheet(sliderStyle);
    m_maskExpandValue = new QLabel("0");
    m_maskExpandValue->setStyleSheet(valueLabelStyle);

    // Add all widgets to layout (initially hidden, will be shown based on tool).
    // Order defines left-to-right grouping; hidden widgets collapse to zero width.
    m_toolSettingsLayout->addWidget(m_setting1Label);
    m_toolSettingsLayout->addWidget(m_setting1Slider);
    m_toolSettingsLayout->addWidget(m_setting1ValueLabel);
    m_toolSettingsLayout->addWidget(m_setting2Label);
    m_toolSettingsLayout->addWidget(m_setting2Slider);
    m_toolSettingsLayout->addWidget(m_setting2ValueLabel);
    m_toolSettingsLayout->addWidget(m_setting3Label);
    m_toolSettingsLayout->addWidget(m_setting3Slider);
    m_toolSettingsLayout->addWidget(m_setting3ValueLabel);
    m_toolSettingsLayout->addWidget(m_lineStyleLabel);
    m_toolSettingsLayout->addWidget(m_lineStyleCombo);
    m_toolSettingsLayout->addWidget(m_selectColorLabel);
    m_toolSettingsLayout->addWidget(m_selectColorButton);
    m_toolSettingsLayout->addWidget(m_pipetteButton);
    m_toolSettingsLayout->addWidget(m_selectionModeLabel);
    m_toolSettingsLayout->addWidget(m_selectionModeCombo);
    m_toolSettingsLayout->addWidget(m_selectSimilarButton);
    m_toolSettingsLayout->addWidget(m_textFontCombo);
    m_toolSettingsLayout->addWidget(m_optSep1);
    m_toolSettingsLayout->addWidget(m_boolSetting1);
    m_toolSettingsLayout->addWidget(m_boolSetting2);
    m_toolSettingsLayout->addWidget(m_textUnderlineBtn);
    m_toolSettingsLayout->addWidget(m_textColorBtn);
    m_toolSettingsLayout->addWidget(m_extractButton);
    m_toolSettingsLayout->addWidget(m_maskSettingsButton);
    m_toolSettingsLayout->addWidget(m_optSep2);
    m_toolSettingsLayout->addWidget(m_presetLabel);
    m_toolSettingsLayout->addWidget(m_presetCombo);
    m_toolSettingsLayout->addWidget(m_savePresetButton);
    m_toolSettingsLayout->addStretch();

    m_suggestionStrip = new QWidget(m_toolSettingsWidget);
    m_suggestionStripLayout = new QHBoxLayout(m_suggestionStrip);
    m_suggestionStripLayout->setContentsMargins(8, 0, 0, 0);
    m_suggestionStripLayout->setSpacing(4);
    m_suggestionStrip->setVisible(false);
    m_toolSettingsLayout->addWidget(m_suggestionStrip);

    m_suggestionRestoreTimer = new QTimer(this);
    m_suggestionRestoreTimer->setSingleShot(true);
    connect(m_suggestionRestoreTimer, &QTimer::timeout, this, &MainWindow::refreshSmartSuggestions);
    
    m_mainToolbar->addWidget(m_toolSettingsWidget);
    
    // Initialize with Select tool settings
    updateToolSettings(DrawingTool::Select);
    
     setupFavoritesToolbar();
     if (m_favoritesToolbar) {
         m_favoritesToolbar->setStyleSheet(leftToolbarStyle);
     }

     // Create left toolbar with modern icons
     m_leftToolbar = new QToolBar("Tools", this);
     addToolBar(Qt::LeftToolBarArea, m_leftToolbar);
     m_leftToolbar->setObjectName("LeftToolbar");
     m_leftToolbar->setStyleSheet(leftToolbarStyle);
     m_leftToolbar->setOrientation(Qt::Vertical);
     m_leftToolbar->setMovable(false);
     m_leftToolbar->setFloatable(false);
     m_leftToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
     m_leftToolbar->setIconSize(QSize(22, 22));
     m_leftToolbar->setFixedWidth(44);

     // Create action group for tool selection (one checked slot per flyout family)
     m_toolActionGroup = new QActionGroup(this);
     m_toolActionGroup->setExclusive(true);

     // Classic long-press flyouts: short click = current tool, hold = related tools.
     // Families mirror Photoshop / Affinity / CorelDRAW tool stacks.
     setupToolFlyoutSlot({DrawingTool::Select, DrawingTool::Move}, DrawingTool::Select);
     m_leftToolbar->addSeparator();
     setupToolFlyoutSlot({DrawingTool::Line, DrawingTool::AngleLine}, DrawingTool::Line);
     setupToolFlyoutSlot({DrawingTool::BezierCurve, DrawingTool::Curve, DrawingTool::Spline}, DrawingTool::BezierCurve);
     m_leftToolbar->addSeparator();
     setupToolFlyoutSlot(
         {DrawingTool::Rectangle, DrawingTool::Ellipse, DrawingTool::Circle,
          DrawingTool::Polygon, DrawingTool::Arc},
         DrawingTool::Rectangle);
     m_leftToolbar->addSeparator();
     setupToolFlyoutSlot(
         {DrawingTool::Brush, DrawingTool::Eraser, DrawingTool::Fill, DrawingTool::Blur},
         DrawingTool::Brush);
     m_leftToolbar->addSeparator();
     setupToolFlyoutSlot({DrawingTool::Measure}, DrawingTool::Measure);
     setupToolFlyoutSlot({DrawingTool::Image}, DrawingTool::Image);
     setupToolFlyoutSlot({DrawingTool::Text}, DrawingTool::Text);

     // Style flyout indicators (corner mark on tools that have variants)
     for (QAction *action : m_toolActionGroup->actions()) {
         if (QToolButton *btn = qobject_cast<QToolButton *>(m_leftToolbar->widgetForAction(action))) {
             if (btn->menu() && btn->menu()->actions().size() > 1) {
                 btn->setStyleSheet(btn->styleSheet() + R"(
                     QToolButton::menu-indicator {
                         subcontrol-position: right bottom;
                         subcontrol-origin: padding;
                         width: 6px;
                         height: 6px;
                         margin: 1px;
                     }
                 )");
             }
         }
     }

     updateFavoritesToolbar();
     loadToolShortcuts();
     updateToolTooltips();
}

void MainWindow::setupFavoritesToolbar()
{
    if (m_favoritesToolbar) {
        return;
    }

    m_favoritesToolbar = new QToolBar("Favorites", this);
    addToolBar(Qt::LeftToolBarArea, m_favoritesToolbar);
    m_favoritesToolbar->setObjectName("FavoritesToolbar");
    m_favoritesToolbar->setOrientation(Qt::Vertical);
    m_favoritesToolbar->setMovable(false);
    m_favoritesToolbar->setFloatable(false);
    m_favoritesToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_favoritesToolbar->setIconSize(QSize(22, 22));
    m_favoritesToolbar->setFixedWidth(58);

    QSettings settings;
    m_favoriteTools = settings.value("Favorites/Tools").toStringList();
}


void MainWindow::setupUndoHistoryDock()
{
    if (m_undoHistoryDock) {
        return;
    }

    m_undoHistoryDock = new QDockWidget("History", this);
    m_undoHistoryDock->setObjectName("HistoryDock");
    if (m_propertiesDock) {
        m_undoHistoryDock->setStyleSheet(m_propertiesDock->styleSheet());
    }
    m_undoHistoryDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable |
                                   QDockWidget::DockWidgetClosable);

    m_undoHistoryList = new QListWidget();
    m_undoHistoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_undoHistoryList->setStyleSheet(R"(
        QListWidget {
            background: #1e1e1e;
            color: #e8e8e8;
            border: none;
            padding: 6px;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-radius: 4px;
        }
        QListWidget::item:selected {
            background: rgba(74, 144, 226, 0.25);
        }
    )");
    m_undoHistoryDock->setWidget(m_undoHistoryList);
    addDockWidget(Qt::RightDockWidgetArea, m_undoHistoryDock);

    // Tab History with Properties/Layers so it doesn't consume half the
    // right dock's height and leave the Properties panel unreadably short.
    if (m_layersDock) {
        tabifyDockWidget(m_layersDock, m_undoHistoryDock);
    }
    if (m_propertiesDock) {
        m_propertiesDock->raise();
    }

    connect(m_undoHistoryList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) {
                if (!item || !m_commandManager) {
                    return;
                }
                int steps = item->data(Qt::UserRole).toInt();
                for (int i = 0; i < steps; ++i) {
                    m_commandManager->undo();
                }
            });
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

    statusBar()->addPermanentWidget(new QLabel(" | "));
    m_sam2StatusLabel = new QLabel(QStringLiteral("SAM2…"), this);
    statusBar()->addPermanentWidget(m_sam2StatusLabel);
}

void MainWindow::setupDockWidgets()
{
    // Modern Dark Theme - Professional Creative Software Style
    QString unifiedDockStyle = R"(
        QDockWidget {
            background: #1e1e1e;
            border: 1px solid #2d2d2d;
            border-radius: 12px;
            margin: 4px;
        }
        QDockWidget::title {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a2a2a, stop:1 #1e1e1e);
            color: #e8e8e8;
            font-weight: 600;
            font-size: 13px;
            padding: 12px 16px;
            border-bottom: 1px solid #2d2d2d;
            border-radius: 12px 12px 0 0;
        }
        QDockWidget::close-button, QDockWidget::float-button {
            background: transparent;
            border: none;
            padding: 6px;
            border-radius: 6px;
            margin: 2px;
        }
        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background: rgba(100, 149, 237, 0.15);
        }
        QDockWidget::close-button:pressed, QDockWidget::float-button:pressed {
            background: rgba(100, 149, 237, 0.25);
        }
    )";
    
    // Properties dock
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setStyleSheet(unifiedDockStyle);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | 
                                   QDockWidget::DockWidgetFloatable | 
                                   QDockWidget::DockWidgetClosable);
    m_propertyPanel = new PropertyPanel();
    m_propertiesDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
        // Apply global tooltip and context menu styling with 50% opacity and feather effect
        QString globalStyle = R"(
            QToolTip {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                    stop:0 rgba(0, 0, 0, 0.5), 
                    stop:1 rgba(0, 0, 0, 0.5));
                color: #ffffff;
                border: 1px solid rgba(100, 149, 237, 0.5);
                border-radius: 8px;
                padding: 4px 8px;
                font-size: 10px;
                font-weight: 700;
            }
            QMenu {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                    stop:0 rgba(0, 0, 0, 0.5), 
                    stop:1 rgba(0, 0, 0, 0.5));
                color: #ffffff;
                border: 1px solid rgba(100, 149, 237, 0.5);
                border-radius: 8px;
                padding: 3px;
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
    )";
    setStyleSheet(globalStyle);
    
    // Connect property changes
    connect(m_propertyPanel, &PropertyPanel::propertyChanged,
            this, &MainWindow::onPropertyChanged);
    
    // Layers dock - place it on the right side, tabbed with properties
    m_layersDock = new QDockWidget("Layers", this);
    m_layersDock->setObjectName("LayersDock");
    m_layersDock->setStyleSheet(unifiedDockStyle);
    m_layersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layersDock->setFeatures(QDockWidget::DockWidgetMovable | 
                              QDockWidget::DockWidgetFloatable | 
                              QDockWidget::DockWidgetClosable);
    m_layerPanel = new LayerPanel();
    m_layerPanel->setLayerManager(m_layerManager);
    m_layersDock->setWidget(m_layerPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    
    // Tab the layers dock with properties dock
    tabifyDockWidget(m_propertiesDock, m_layersDock);
    m_propertiesDock->raise();

    // Make the auto-created dock tab bar expand evenly across the panel width and give it a modern look
    const auto tabBars = findChildren<QTabBar *>();
    for (auto *tb : tabBars) {
        tb->setExpanding(true);
        tb->setUsesScrollButtons(false);
        tb->setElideMode(Qt::ElideNone);
        tb->setDocumentMode(true);
        tb->setStyleSheet(R"(
            QTabBar {
                background: #17191c;
                qproperty-drawBase: 0;
            }
            QTabBar::tab {
                background: transparent;
                color: rgba(231,234,240,0.55);
                padding: 8px 12px;
                border: none;
                border-bottom: 2px solid transparent;
                font-size: 11px;
                font-weight: 600;
                min-width: 60px;
            }
            QTabBar::tab:hover {
                color: rgba(231,234,240,0.85);
            }
            QTabBar::tab:selected {
                color: rgba(231,234,240,0.95);
                border-bottom: 2px solid rgba(138, 180, 255, 0.95);
            }
        )");
    }
    
    // Create classic text tool
    m_classicTextTool = new ClassicTextTool(this);
    m_classicTextTool->setCanvas(m_canvas);
    if (m_propertyPanel) {
        m_propertyPanel->setTextTool(m_classicTextTool);
    }

    // Connect text tool signals
    connect(m_classicTextTool, &ClassicTextTool::textCreated, this,
            [this](TextPrimitive *text) {
                if (m_canvas && text) {
                    m_canvas->addTextPrimitive(text);
                }
            });

    // Connect text tool undo support
    connect(m_classicTextTool, &ClassicTextTool::commandCreated,
            this, &MainWindow::executeCommand);

    // Make sure both docks are visible and show properties by default
    m_propertiesDock->show();
    m_layersDock->show();
    m_propertiesDock->raise(); // Show Properties tab by default
    // Give the properties panel a comfortable width; allow the user to
    // widen it further if needed.
    m_propertiesDock->setMinimumWidth(320);
    m_propertiesDock->setMaximumWidth(520);
    resizeDocks({m_propertiesDock}, {340}, Qt::Horizontal);
    
    // Create color palette in bottom dock
    QDockWidget *colorDock = new QDockWidget("Color Palette", this);
    colorDock->setObjectName("ColorPaletteDock");
    colorDock->setStyleSheet(unifiedDockStyle);
    m_colorPalette = new ColorPalette();
    colorDock->setWidget(m_colorPalette);
    addDockWidget(Qt::BottomDockWidgetArea, colorDock);

    setupUndoHistoryDock();
    updateUndoHistoryPanel();
    
    // Connect color palette signals
    connect(m_colorPalette, &ColorPalette::colorChanged, this, &MainWindow::onColorChanged);
}

void MainWindow::connectSignals()
{
    connect(m_extractButton, &QPushButton::clicked, this,
            &MainWindow::extractSelectedSubjects);
    
    // Connect canvas signals
    if (m_canvas) {
        connect(m_canvas, &DrawingCanvas::commandRequested,
                this, &MainWindow::executeCommand);

        connect(m_canvas, &DrawingCanvas::maskDetectionComplete, this,
                [this](ImagePrimitive *imgPrim) {
                    if (m_canvas && imgPrim) {
                        selectImageForMaskUI(imgPrim);
                    }
                    updateMaskSelectionUI();
                    updateFloatingMaskPanel();
                    showFloatingMaskPanel();
                    if (m_statusLabel && imgPrim &&
                        imgPrim->getMaskCandidateCount() > 0) {
                        m_statusLabel->setText(
                            QString("Detected %1 masks")
                                .arg(imgPrim->getMaskCandidateCount()));
                    }
                    if (m_suggestionRestoreTimer) {
                        m_suggestionRestoreTimer->start(1800);
                    } else {
                        refreshSmartSuggestions();
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskDetectionFailed, this,
                [this](const QString &error) {
                    if (m_statusLabel) {
                        m_statusLabel->setText("Mask detection failed: " + error);
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskDetectionProgress, this,
                [this](int progress, const QString &message) {
                    if (m_statusLabel) {
                        m_statusLabel->setText(
                            QString("SAM2: %1 %2%").arg(message).arg(progress));
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskNavigationRequested, this,
                [this](int direction) {
                    if (direction < 0) {
                        selectPreviousMask();
                    } else {
                        selectNextMask();
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskInvertRequested, this,
                &MainWindow::invertSelectedMask);

        connect(m_canvas, &DrawingCanvas::smartHintChanged, this,
                [this](const QString &hint) {
                    m_liveSmartHint = hint;
                    refreshSmartSuggestions();
                });

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
                    updatePropertyPanel();
                    refreshSmartSuggestions();

                    const int selCount =
                        m_canvas ? static_cast<int>(m_canvas->selectedObjects().size())
                                 : 0;
                    if (selCount > 1) {
                        statusBar()->showMessage(
                            QStringLiteral("%1 objects selected — Shift/⌘-click to add/remove")
                                .arg(selCount),
                            4000);
                    }

                    bool hasSelection = selCount > 0;

                    // Check for ImagePrimitive with detected masks
                    bool showMask = false;
                    if (hasSelection) {
                        for (auto *obj : m_canvas->selectedObjects()) {
                            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                                if (imgPrim->getMaskCandidateCount() > 0) {
                                    showMask = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (showMask) {
                        updateFloatingMaskPanel();
                        showFloatingMaskPanel();
                    } else {
                        hideFloatingMaskPanel();
                    }

                });
    }
    
    // Connect layer panel signals
    if (m_layerPanel) {
        connect(m_layerPanel, &LayerPanel::commandRequested,
                this, &MainWindow::executeCommand);
        connect(m_layerPanel, &LayerPanel::layerPropertyChanged,
                this, [this]() {
                    if (m_canvas) {
                        m_canvas->update();
                    }
                });
        connect(m_layerPanel, &LayerPanel::documentModified,
                this, [this]() {
                    if (m_commandManager)
                        m_commandManager->invalidateClean();
                    m_isModified = true;
                    updateWindowTitle();
                });
    }
    
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        // Save window settings
        QSettings settings;
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState(DOCK_LAYOUT_VERSION));
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
    ret = QMessageBox::warning(this, "Drawing Studio",
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
    QString title = "Drawing Studio";
    if (!m_currentFile.isEmpty()) {
        QFileInfo fileInfo(m_currentFile);
        title = fileInfo.baseName() + " - " + title;
    }
    if (m_isModified) {
        title = "*" + title;
    }
    setWindowTitle(title);
}


void MainWindow::setCurrentFile(const QString &fileName)
{
    m_currentFile = fileName;
    m_isModified = false;
    if (m_commandManager)
        m_commandManager->markClean();
    updateWindowTitle();
}

void MainWindow::syncModifiedFlag()
{
    m_isModified = !(m_commandManager && m_commandManager->isClean());
    updateWindowTitle();
}

void MainWindow::addToRecentFiles(const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    QSettings settings;
    QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue(QStringLiteral("recentFileList"), files);
    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    const QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
    const int count = qMin(files.size(), static_cast<int>(MaxRecentFiles));
    for (int i = 0; i < count; ++i) {
        const QString text = QStringLiteral("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        m_recentFileActions[i]->setText(text);
        m_recentFileActions[i]->setData(files[i]);
        m_recentFileActions[i]->setVisible(true);
    }
    for (int i = count; i < MaxRecentFiles; ++i)
        m_recentFileActions[i]->setVisible(false);
    if (m_recentFilesSeparator)
        m_recentFilesSeparator->setVisible(count > 0);
}

void MainWindow::openRecentFile()
{
    if (auto *action = qobject_cast<QAction *>(sender())) {
        const QString fileName = action->data().toString();
        if (fileName.isEmpty() || !maybeSave())
            return;
        if (!loadProjectFromFile(fileName, true)) {
            QMessageBox::warning(this, QStringLiteral("Drawing Studio"),
                                 QStringLiteral("Could not open \"%1\".").arg(fileName));
            QSettings settings;
            QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
            files.removeAll(fileName);
            settings.setValue(QStringLiteral("recentFileList"), files);
            updateRecentFileActions();
        }
    }
}

// Slot implementations
void MainWindow::newProject()
{
    if (!maybeSave()) {
        return;
    }

    if (m_classicTextTool) {
        m_classicTextTool->deactivate();
    }

    if (m_layerManager) {
        m_layerManager->clearLayers();
    }

    if (m_canvas) {
        m_canvas->clearSelection();
        m_canvas->clearPrimitives();
        if (m_project) {
            m_project->clearSelection();
            m_project->clearPrimitives();
        }
        m_canvas->zoomActual();
        m_canvas->update();
    }

    if (m_commandManager) {
        m_commandManager->clear();
    }

    hideFloatingMaskPanel();
    updatePropertyPanel();
    if (m_layerPanel) {
        m_layerPanel->refresh();
    }
    updateUndoHistoryPanel();

    setCurrentFile(QString());
    if (m_statusLabel) {
        m_statusLabel->setText("New project created");
    }
}

void MainWindow::openProject()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Open Drawing Project", m_currentFile,
            "Drawing Project (*.drawing);;All Files (*)");
        if (!fileName.isEmpty()) {
            if (!loadProjectFromFile(fileName, true)) {
                QMessageBox::warning(this, "Open Project",
                                     "Failed to open project file:\n" + fileName);
            }
        }
    }
}

bool MainWindow::saveProject()
{
    if (m_currentFile.isEmpty()) {
        return saveProjectAs();
    }
    return saveProjectToFile(m_currentFile);
}

bool MainWindow::saveProjectAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Drawing Project", m_currentFile,
        "Drawing Project (*.drawing);;All Files (*)");
    if (fileName.isEmpty()) {
        return false;
    }

    if (!fileName.endsWith(".drawing", Qt::CaseInsensitive)) {
        fileName += ".drawing";
    }

    return saveProjectToFile(fileName);
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::undo()
{
    if (m_commandManager && m_commandManager->canUndo()) {
        const QString undoText = m_commandManager->undoText();
        m_commandManager->undo();
        if (m_statusLabel) {
            QString status = undoText;
            status.replace("Undo", "Undid");
            m_statusLabel->setText(status);
        }
    }

    if (m_canvas) {
        m_canvas->update();
    }

    syncModifiedFlag();
}

void MainWindow::redo()
{
    if (m_commandManager && m_commandManager->canRedo()) {
        const QString redoText = m_commandManager->redoText();
        m_commandManager->redo();
        if (m_statusLabel) {
            QString status = redoText;
            status.replace("Redo", "Redid");
            m_statusLabel->setText(status);
        }
    }

    if (m_canvas) {
        m_canvas->update();
    }

    syncModifiedFlag();
}
void MainWindow::selectAll() {
    if (!m_canvas || !m_layerManager) return;
    m_canvas->clearSelection();
    for (auto *prim : m_layerManager->getAllPrimitives()) {
        if (prim && prim->isVisible()) {
            m_canvas->addToSelection(prim);
        }
    }
    m_canvas->update();
    updatePropertyPanel();
}

void MainWindow::deleteSelected() {
    if (m_canvas) {
        m_canvas->deleteSelectedPrimitivesWithCommand();
    }
}

void MainWindow::copySelected() {
    m_clipboard.clear();
    if (!m_canvas)
        return;
    for (auto *obj : m_canvas->selectedObjects()) {
        if (obj)
            m_clipboard.push_back(obj->toJson());
    }
    if (m_statusLabel) {
        m_statusLabel->setText(
            m_clipboard.empty()
                ? QStringLiteral("Nothing to copy")
                : QStringLiteral("Copied %1 object(s)").arg(m_clipboard.size()));
    }
}

void MainWindow::cutSelected() {
    copySelected();
    if (!m_clipboard.empty())
        deleteSelected();
}

void MainWindow::pasteClipboard() {
    if (!m_canvas || m_clipboard.empty() || !m_commandManager)
        return;

    m_canvas->clearSelection();
    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("Paste"));
    std::vector<DrawingPrimitive *> pasted;
    pasted.reserve(m_clipboard.size());

    for (const QJsonObject &json : m_clipboard) {
        QJsonObject copy = json;
        copy.remove(QStringLiteral("id")); // fresh identity for the paste
        auto prim = DrawingPrimitive::createFromJson(copy);
        if (!prim)
            continue;
        prim->translate(QVector2D(20.0f, -20.0f));
        prim->setSelected(true);
        DrawingPrimitive *raw = prim.get();
        compound->addCommand(
            std::make_unique<AddPrimitiveCommand>(m_canvas, std::move(prim)));
        pasted.push_back(raw);
    }

    if (compound->isEmpty())
        return;

    m_commandManager->executeCommand(std::move(compound));
    for (auto *p : pasted) {
        if (p)
            m_canvas->addToSelection(p);
    }
    syncModifiedFlag();
    m_canvas->update();
    updatePropertyPanel();
    if (m_statusLabel) {
        m_statusLabel->setText(
            QStringLiteral("Pasted %1 object(s)").arg(pasted.size()));
    }
}

void MainWindow::duplicateSelected() {
    if (!m_canvas || m_canvas->selectedObjects().empty())
        return;
    copySelected();
    pasteClipboard();
}

void MainWindow::groupSelected() {
    if (m_canvas)
        m_canvas->groupSelected();
}

void MainWindow::ungroupSelected() {
    if (m_canvas)
        m_canvas->ungroupSelected();
}

void MainWindow::zoomIn() { if (m_canvas) m_canvas->zoomIn(); }
void MainWindow::zoomOut() { if (m_canvas) m_canvas->zoomOut(); }
void MainWindow::zoomFit() { if (m_canvas) m_canvas->zoomFit(); }
void MainWindow::zoomActual() { if (m_canvas) m_canvas->zoomActual(); }

void MainWindow::toggleGrid()
{
    if (m_canvas) {
        // Toggle grid visibility
        bool currentState = m_canvas->isGridVisible();
        m_canvas->setGridVisible(!currentState);
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
        m_canvas->setGridSize(DrawingCanvas::FINE_GRID_SIZE);
    }
}

void MainWindow::setGridSizeMedium()
{
    if (m_canvas) {
        m_canvas->setGridSize(DrawingCanvas::DEFAULT_GRID_SIZE);
    }
}

void MainWindow::setGridSizeCoarse()
{
    if (m_canvas) {
        m_canvas->setGridSize(DrawingCanvas::COARSE_GRID_SIZE);
    }
}

void MainWindow::selectTool() { activateDrawingTool(DrawingTool::Select); }
void MainWindow::lineTool() { activateDrawingTool(DrawingTool::Line); }
void MainWindow::curveTool() { activateDrawingTool(DrawingTool::Curve); }
void MainWindow::bezierTool() { activateDrawingTool(DrawingTool::BezierCurve); }
void MainWindow::splineTool() { activateDrawingTool(DrawingTool::Spline); }
void MainWindow::polygonTool() { activateDrawingTool(DrawingTool::Polygon); }
void MainWindow::rectangleTool() { activateDrawingTool(DrawingTool::Rectangle); }
void MainWindow::ellipseTool() { activateDrawingTool(DrawingTool::Ellipse); }
void MainWindow::circleTool() { activateDrawingTool(DrawingTool::Circle); }
void MainWindow::arcTool() { activateDrawingTool(DrawingTool::Arc); }
void MainWindow::angleLineTool() { activateDrawingTool(DrawingTool::AngleLine); }
void MainWindow::eraserTool() { activateDrawingTool(DrawingTool::Eraser); }
void MainWindow::fillTool() { activateDrawingTool(DrawingTool::Fill); }
void MainWindow::brushTool() { activateDrawingTool(DrawingTool::Brush); }
void MainWindow::blurTool() { activateDrawingTool(DrawingTool::Blur); }
void MainWindow::measureTool() { activateDrawingTool(DrawingTool::Measure); }
void MainWindow::imageTool() { activateDrawingTool(DrawingTool::Image); }
void MainWindow::handTool() { activateDrawingTool(DrawingTool::Move); }

void MainWindow::activateDrawingTool(DrawingTool tool)
{
    if (!m_canvas) {
        return;
    }

    if (m_classicTextTool && tool != DrawingTool::Text) {
        m_classicTextTool->deactivate();
    }

    m_canvas->setCurrentTool(tool);

    if (tool == DrawingTool::Text && m_classicTextTool) {
        m_classicTextTool->activate();
    }

    syncToolSlotAppearance(tool);
    persistToolSlotChoice(tool);
    updateToolSettings(tool);
    refreshSmartSuggestions();
}

QString MainWindow::displayNameForTool(DrawingTool tool) const
{
    switch (tool) {
    case DrawingTool::Select: return QStringLiteral("Select");
    case DrawingTool::Move: return QStringLiteral("Hand");
    case DrawingTool::Line: return QStringLiteral("Line");
    case DrawingTool::AngleLine: return QStringLiteral("Angle Line");
    case DrawingTool::Curve: return QStringLiteral("Curve");
    case DrawingTool::BezierCurve: return QStringLiteral("Bezier");
    case DrawingTool::Spline: return QStringLiteral("Spline");
    case DrawingTool::Polygon: return QStringLiteral("Polygon");
    case DrawingTool::Rectangle: return QStringLiteral("Rectangle");
    case DrawingTool::Ellipse: return QStringLiteral("Ellipse");
    case DrawingTool::Circle: return QStringLiteral("Circle");
    case DrawingTool::Arc: return QStringLiteral("Arc");
    case DrawingTool::Eraser: return QStringLiteral("Eraser");
    case DrawingTool::Fill: return QStringLiteral("Fill");
    case DrawingTool::Brush: return QStringLiteral("Brush");
    case DrawingTool::Blur: return QStringLiteral("Blur");
    case DrawingTool::Measure: return QStringLiteral("Measure");
    case DrawingTool::Image: return QStringLiteral("Image");
    case DrawingTool::Text: return QStringLiteral("Text");
    }
    return QStringLiteral("Tool");
}

QKeySequence MainWindow::defaultShortcutForTool(DrawingTool tool) const
{
    switch (tool) {
    case DrawingTool::Select: return QKeySequence(QStringLiteral("V"));
    case DrawingTool::Move: return QKeySequence(QStringLiteral("H"));
    case DrawingTool::Line: return QKeySequence(QStringLiteral("L"));
    case DrawingTool::AngleLine: return QKeySequence(QStringLiteral("Shift+L"));
    case DrawingTool::Curve: return QKeySequence(QStringLiteral("C"));
    case DrawingTool::BezierCurve: return QKeySequence(QStringLiteral("B"));
    case DrawingTool::Spline: return QKeySequence(QStringLiteral("P"));
    case DrawingTool::Polygon: return QKeySequence(QStringLiteral("G"));
    case DrawingTool::Rectangle: return QKeySequence(QStringLiteral("R"));
    case DrawingTool::Ellipse: return QKeySequence(QStringLiteral("E"));
    case DrawingTool::Circle: return QKeySequence(QStringLiteral("O"));
    case DrawingTool::Arc: return QKeySequence(QStringLiteral("A"));
    case DrawingTool::Eraser: return QKeySequence(QStringLiteral("X"));
    case DrawingTool::Fill: return QKeySequence(QStringLiteral("F"));
    case DrawingTool::Brush: return QKeySequence(QStringLiteral("D"));
    case DrawingTool::Blur: return QKeySequence(QStringLiteral("U"));
    case DrawingTool::Measure: return QKeySequence(QStringLiteral("M"));
    case DrawingTool::Image: return QKeySequence(QStringLiteral("I"));
    case DrawingTool::Text: return QKeySequence(QStringLiteral("T"));
    }
    return {};
}

QIcon MainWindow::iconForTool(DrawingTool tool)
{
    switch (tool) {
    case DrawingTool::Select: return createSelectIcon();
    case DrawingTool::Move: return createHandIcon();
    case DrawingTool::Line: return createLineIcon();
    case DrawingTool::AngleLine: return createAngleLineIcon();
    case DrawingTool::Curve: return createCurveIcon();
    case DrawingTool::BezierCurve: return createBezierIcon();
    case DrawingTool::Spline: return createSplineIcon();
    case DrawingTool::Polygon: return createPolygonIcon();
    case DrawingTool::Rectangle: return createRectangleIcon();
    case DrawingTool::Ellipse: return createEllipseIcon();
    case DrawingTool::Circle: return createCircleIcon();
    case DrawingTool::Arc: return createArcIcon();
    case DrawingTool::Eraser: return createEraserIcon();
    case DrawingTool::Fill: return createFillIcon();
    case DrawingTool::Brush: return createBrushIcon();
    case DrawingTool::Blur: return createBlurIcon();
    case DrawingTool::Measure: return createMeasureIcon();
    case DrawingTool::Image: return createImageIcon();
    case DrawingTool::Text: return createTextIcon();
    }
    return createSelectIcon();
}

void MainWindow::setupToolFlyoutSlot(const QList<DrawingTool> &tools, DrawingTool defaultTool)
{
    if (tools.isEmpty() || !m_leftToolbar) {
        return;
    }

    QSettings settings;
    DrawingTool current = defaultTool;
    const QString saved = settings.value(
        QStringLiteral("ToolFlyouts/%1").arg(toolKey(defaultTool))).toString();
    for (DrawingTool t : tools) {
        if (toolKey(t) == saved) {
            current = t;
            break;
        }
    }

    auto *slotAction = new QAction(iconForTool(current), displayNameForTool(current), this);
    slotAction->setCheckable(true);
    slotAction->setToolTip(QStringLiteral("%1 — hold for related tools")
                               .arg(displayNameForTool(current)));
    slotAction->setProperty("currentTool", static_cast<int>(current));
    if (defaultTool == DrawingTool::Select) {
        slotAction->setChecked(true);
    }
    m_toolActionGroup->addAction(slotAction);
    m_leftToolbar->addAction(slotAction);

    auto *menu = new QMenu(m_leftToolbar);
    menu->setToolTipsVisible(true);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background:#2a2d32; border:1px solid rgba(255,255,255,0.12); padding:4px; }"
        "QMenu::item { padding:6px 18px 6px 8px; color:#e7eaf0; border-radius:4px; }"
        "QMenu::item:selected { background:rgba(138,180,255,0.22); }"));

    for (DrawingTool t : tools) {
        QAction *item = menu->addAction(iconForTool(t), displayNameForTool(t));
        item->setData(static_cast<int>(t));
        const QKeySequence seq = defaultShortcutForTool(t);
        if (!seq.isEmpty()) {
            item->setShortcut(seq);
            item->setShortcutVisibleInContextMenu(true);

            auto *shortcutAction = new QAction(this);
            shortcutAction->setShortcut(seq);
            shortcutAction->setShortcutContext(Qt::ApplicationShortcut);
            connect(shortcutAction, &QAction::triggered, this, [this, t]() {
                activateDrawingTool(t);
            });
            addAction(shortcutAction);
        }
        connect(item, &QAction::triggered, this, [this, t]() {
            activateDrawingTool(t);
        });

        m_toolActions.insert(t, item);
        m_toolSlotActions.insert(t, slotAction);
        m_toolSlotLeader.insert(t, current);
    }

    if (QToolButton *btn = qobject_cast<QToolButton *>(m_leftToolbar->widgetForAction(slotAction))) {
        if (tools.size() > 1) {
            btn->setPopupMode(QToolButton::DelayedPopup);
            btn->setMenu(menu);
        } else {
            // Single-tool slots: no flyout menu
            menu->deleteLater();
        }
        btn->setAutoRaise(true);
    }

    connect(slotAction, &QAction::triggered, this, [this, slotAction]() {
        const QVariant v = slotAction->property("currentTool");
        const int stored = v.isValid() ? v.toInt() : static_cast<int>(DrawingTool::Select);
        activateDrawingTool(static_cast<DrawingTool>(stored));
    });
}

void MainWindow::syncToolSlotAppearance(DrawingTool tool)
{
    QAction *slot = m_toolSlotActions.value(tool, nullptr);
    if (!slot) {
        return;
    }

    slot->setIcon(iconForTool(tool));
    slot->setText(displayNameForTool(tool));
    slot->setToolTip(QStringLiteral("%1 — hold for related tools").arg(displayNameForTool(tool)));
    slot->setProperty("currentTool", static_cast<int>(tool));
    slot->setChecked(true);

    for (auto it = m_toolSlotActions.begin(); it != m_toolSlotActions.end(); ++it) {
        if (it.value() == slot) {
            m_toolSlotLeader[it.key()] = tool;
        }
    }
}

void MainWindow::persistToolSlotChoice(DrawingTool tool)
{
    QAction *slot = m_toolSlotActions.value(tool, nullptr);
    if (!slot || !m_leftToolbar) {
        return;
    }

    DrawingTool familyKey = tool;
    if (QToolButton *btn = qobject_cast<QToolButton *>(m_leftToolbar->widgetForAction(slot))) {
        if (QMenu *menu = btn->menu()) {
            const auto acts = menu->actions();
            if (!acts.isEmpty()) {
                familyKey = static_cast<DrawingTool>(acts.first()->data().toInt());
            }
        }
    }

    QSettings settings;
    settings.setValue(QStringLiteral("ToolFlyouts/%1").arg(toolKey(familyKey)), toolKey(tool));
}

void MainWindow::refreshSmartSuggestions()
{
    if (!m_canvas || !m_suggestionStrip || !m_suggestionStripLayout) {
        return;
    }

    ToolSuggestionContext ctx;
    ctx.tool = m_canvas->currentTool();
    ctx.snapEnabled = m_canvas->isSnapEnabled();
    ctx.magneticEnabled = m_canvas->isMagneticConnectionEnabled();
    ctx.gridVisible = m_canvas->isGridVisible();
    ctx.liveDrawHint = m_liveSmartHint;

    const auto &selected = m_canvas->selectedObjects();
    ctx.selectionCount = static_cast<int>(selected.size());
    for (auto *obj : selected) {
        if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
            ctx.hasImage = true;
            if (img->getMaskCandidateCount() > 0) {
                ctx.imageHasMasks = true;
            }
        }
        if (dynamic_cast<TextPrimitive *>(obj)) {
            ctx.hasText = true;
        }
    }

    int primitiveCount = 0;
    if (m_layerManager) {
        for (const auto &layer : m_layerManager->layers()) {
            if (layer) {
                primitiveCount += static_cast<int>(layer->primitives().size());
            }
        }
    } else {
        primitiveCount = static_cast<int>(m_canvas->primitives().size());
    }
    ctx.canvasEmpty = (primitiveCount == 0);

    const ToolSuggestionResult result = ToolSuggestionService::resolve(ctx);

    if (m_statusLabel && !result.statusHint.isEmpty()) {
        m_statusLabel->setText(result.statusHint);
    }

    while (QLayoutItem *item = m_suggestionStripLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QString chipStyle = QStringLiteral(
        "QToolButton {"
        "  background: rgba(138, 180, 255, 0.14);"
        "  border: 1px solid rgba(138, 180, 255, 0.45);"
        "  border-radius: 11px;"
        "  padding: 2px 10px;"
        "  color: #dce6ff;"
        "  font-size: 11px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(138, 180, 255, 0.28);"
        "}");

    for (const auto &action : result.chips) {
        auto *btn = new QToolButton(m_suggestionStrip);
        btn->setText(action.label);
        btn->setToolTip(action.tip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(chipStyle);
        btn->setAutoRaise(true);
        const QString actionId = action.id;
        connect(btn, &QToolButton::clicked, this, [this, actionId]() {
            applySmartSuggestion(actionId);
        });
        m_suggestionStripLayout->addWidget(btn);
    }

    m_suggestionStrip->setVisible(!result.chips.empty());
}

void MainWindow::applySmartSuggestion(const QString &actionId)
{
    if (!m_canvas) {
        return;
    }

    if (actionId == QLatin1String("detect_subjects")) {
        if (ImagePrimitive *img = imageForDetection()) {
            selectImageForMaskUI(img);
            img->startSubjectDetection();
            if (m_statusLabel) {
                m_statusLabel->setText(QStringLiteral("Detecting subjects…"));
            }
            if (m_suggestionRestoreTimer) {
                m_suggestionRestoreTimer->start(2500);
            }
            m_canvas->update();
        }
        return;
    }

    if (actionId == QLatin1String("extract_subject")) {
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                auto extracted = imgPrim->extractDetectedSubject();
                if (extracted) {
                    m_canvas->addPrimitiveWithCommand(std::move(extracted));
                    if (m_statusLabel) {
                        m_statusLabel->setText(QStringLiteral("Subject extracted."));
                    }
                }
                m_canvas->update();
                refreshSmartSuggestions();
                return;
            }
        }
        return;
    }

    if (actionId == QLatin1String("remove_background")) {
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                if (imgPrim->getEditableContour().empty()) {
                    return;
                }
                auto compound = std::make_unique<CompoundCommand>("Remove Background");
                compound->addCommand(std::make_unique<ExtractSubjectCommand>(m_canvas, imgPrim));
                std::vector<DrawingPrimitive *> toDelete = {imgPrim};
                compound->addCommand(std::make_unique<DeletePrimitivesCommand>(m_canvas, toDelete));
                if (m_commandManager) {
                    m_commandManager->executeCommand(std::move(compound));
                }
                refreshSmartSuggestions();
                return;
            }
        }
        return;
    }

    if (actionId == QLatin1String("mask_settings")) {
        showMaskSettingsPopup();
        return;
    }

    if (actionId == QLatin1String("enable_snap")) {
        m_canvas->setSnapEnabled(true);
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("enable_magnetic")) {
        m_canvas->setMagneticConnectionEnabled(true);
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("show_grid")) {
        if (!m_canvas->isGridVisible()) {
            toggleGrid();
        }
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("import_image") || actionId == QLatin1String("switch_image")) {
        imageTool();
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("Image tool — click the canvas to choose a file"));
        }
        return;
    }

    if (actionId == QLatin1String("switch_text")) {
        textTool();
        return;
    }

    if (actionId == QLatin1String("switch_rectangle")) {
        rectangleTool();
        return;
    }

    if (actionId == QLatin1String("select_all")) {
        selectAll();
        refreshSmartSuggestions();
        return;
    }

    // Informational chips — just reinforce the hint
    if (actionId.startsWith(QLatin1String("hint_"))) {
        refreshSmartSuggestions();
    }
}

void MainWindow::textTool() { activateDrawingTool(DrawingTool::Text); }

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

void MainWindow::changeBackgroundColor()
{
    if (!m_canvas) return;
    
    QColor currentColor = m_canvas->backgroundColor();
    QColor newColor = QColorDialog::getColor(currentColor, this, "Select Background Color");
    
    if (newColor.isValid()) {
        m_canvas->setBackgroundColor(newColor);
        m_statusLabel->setText(QString("Background color changed to %1").arg(newColor.name()));
    }
}

void MainWindow::changePaperColor()
{
    if (!m_canvas) return;
    
    QColor currentColor = m_canvas->paperColor();
    QColor newColor = QColorDialog::getColor(currentColor, this, "Select Paper Color");
    
    if (newColor.isValid()) {
        m_canvas->setPaperColor(newColor);
        m_statusLabel->setText(QString("Paper color changed to %1").arg(newColor.name()));
    }
}

void MainWindow::onColorChanged(const QColor &color)
{
    if (!m_canvas) return;
    
    // Set the default drawing color for new primitives
    m_canvas->setDefaultDrawingColor(color);

    // Apply to currently selected primitives
    m_canvas->setColorForSelection(color);
    m_statusLabel->setText(QString("Default color changed to %1").arg(color.name()));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Drawing Studio",
                      "Drawing Studio v1.0\n\n"
                      "A professional drawing and design application.\n\n"
                      "Built with Qt6 and C++20");
}

void MainWindow::showHelp()
{
    QMessageBox::information(this, "Help",
                           "Drawing Studio Help\n\n"
                           "Shortcuts:\n"
                           "V - Select tool\n"
                           "L - Line tool\n"
                           "C - Curve tool\n"
                           "B - Bezier tool\n"
                           "P - Spline tool\n"
                           "G - Polygon tool\n"
                           "R - Rectangle tool\n"
                           "E - Ellipse tool\n"
                           "F - Fill tool\n"
                           "M - Measure tool\n\n"
                           "Use drawing tools to create shapes and drawings.");
}

void MainWindow::updatePropertyPanel()
{
    if (!m_propertyPanel) return;
    
    if (m_canvas) {
        auto selectedObjects = m_canvas->selectedObjects();
        
        if (selectedObjects.empty()) {
            const DrawingTool tool = m_canvas->currentTool();
            if (tool == DrawingTool::Text) {
                m_propertyPanel->showTextControls();
            } else {
                m_propertyPanel->showToolProperties(tool, m_canvas);
            }
        } else if (selectedObjects.size() == 1) {
            DrawingPrimitive* primitive = selectedObjects.front();
            if (primitive) {
                m_propertyPanel->showPrimitiveProperties(primitive);
            }
        } else {
            m_propertyPanel->showMultiplePrimitiveProperties(selectedObjects);
        }
    } else {
        m_propertyPanel->clearProperties();
    }
}

void MainWindow::updateUndoHistoryPanel()
{
    if (!m_undoHistoryList || !m_commandManager) {
        return;
    }

    m_undoHistoryList->clear();
    const QStringList undoList = m_commandManager->undoHistory();
    if (undoList.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("No history");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_undoHistoryList->addItem(item);
        return;
    }

    for (int i = 0; i < undoList.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(undoList.at(i));
        item->setData(Qt::UserRole, i + 1);
        m_undoHistoryList->addItem(item);
    }
}

QString MainWindow::toolKey(DrawingTool tool) const
{
    switch (tool) {
        case DrawingTool::Select: return "select";
        case DrawingTool::Move: return "hand";
        case DrawingTool::Line: return "line";
        case DrawingTool::Curve: return "curve";
        case DrawingTool::BezierCurve: return "bezier";
        case DrawingTool::Spline: return "spline";
        case DrawingTool::Polygon: return "polygon";
        case DrawingTool::Rectangle: return "rectangle";
        case DrawingTool::Ellipse: return "ellipse";
        case DrawingTool::Circle: return "circle";
        case DrawingTool::Arc: return "arc";
        case DrawingTool::AngleLine: return "angleline";
        case DrawingTool::Eraser: return "eraser";
        case DrawingTool::Fill: return "fill";
        case DrawingTool::Brush: return "brush";
        case DrawingTool::Blur: return "blur";
        case DrawingTool::Measure: return "measure";
        case DrawingTool::Image: return "image";
        case DrawingTool::Text: return "text";
        default: return "tool";
    }
}

void MainWindow::updateFavoritesToolbar()
{
    if (!m_favoritesToolbar) {
        return;
    }

    m_favoritesToolbar->clear();

    if (m_favoriteTools.isEmpty()) {
        m_favoritesToolbar->hide();
        return;
    }

    m_favoritesToolbar->show();
    QLabel *label = new QLabel("★");
    label->setAlignment(Qt::AlignCenter);
    QWidgetAction *labelAction = new QWidgetAction(m_favoritesToolbar);
    labelAction->setDefaultWidget(label);
    m_favoritesToolbar->addAction(labelAction);

    QMap<QString, QAction*> byKey;
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        byKey.insert(toolKey(it.key()), it.value());
    }

    for (const auto &key : m_favoriteTools) {
        if (!byKey.contains(key)) {
            continue;
        }
        QAction *source = byKey.value(key);
        QAction *fav = m_favoritesToolbar->addAction(source->icon(), source->text());
        fav->setToolTip(source->toolTip());
        connect(fav, &QAction::triggered, source, &QAction::trigger);
    }
}

void MainWindow::toggleFavoriteTool(DrawingTool tool)
{
    QString key = toolKey(tool);
    if (m_favoriteTools.contains(key)) {
        m_favoriteTools.removeAll(key);
    } else {
        m_favoriteTools.append(key);
    }

    QSettings settings;
    settings.setValue("Favorites/Tools", m_favoriteTools);
    updateFavoritesToolbar();
}

void MainWindow::updatePresetList(DrawingTool tool)
{
    if (!m_presetCombo) {
        return;
    }

    m_presetCombo->blockSignals(true);
    m_presetCombo->clear();
    m_presetCombo->addItem("Default");

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(tool));
    const QStringList names = settings.childKeys();
    for (const auto &name : names) {
        m_presetCombo->addItem(name);
    }
    settings.endGroup();
    settings.endGroup();
    m_presetCombo->blockSignals(false);
}

void MainWindow::applyPreset(const QString& presetName)
{
    if (!m_presetCombo || presetName == "Default") {
        return;
    }

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(m_canvas ? m_canvas->currentTool() : DrawingTool::Select));
    QVariantMap map = settings.value(presetName).toMap();
    settings.endGroup();
    settings.endGroup();

    if (map.contains("setting1") && m_setting1Slider) {
        m_setting1Slider->setValue(map.value("setting1").toInt());
    }
    if (map.contains("setting2") && m_setting2Slider) {
        m_setting2Slider->setValue(map.value("setting2").toInt());
    }
    if (map.contains("setting3") && m_setting3Slider) {
        m_setting3Slider->setValue(map.value("setting3").toInt());
    }
    if (map.contains("bool1") && m_boolSetting1) {
        m_boolSetting1->setChecked(map.value("bool1").toBool());
    }
    if (map.contains("bool2") && m_boolSetting2) {
        m_boolSetting2->setChecked(map.value("bool2").toBool());
    }
    if (map.contains("lineStyle") && m_lineStyleCombo) {
        int style = map.value("lineStyle").toInt();
        for (int i = 0; i < m_lineStyleCombo->count(); ++i) {
            if (m_lineStyleCombo->itemData(i).toInt() == style) {
                m_lineStyleCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    if (map.contains("maskFeather") && m_maskFeatherSlider) {
        m_maskFeatherSlider->setValue(map.value("maskFeather").toInt());
    }
    if (map.contains("maskBlur") && m_maskBlurSlider) {
        m_maskBlurSlider->setValue(map.value("maskBlur").toInt());
    }
    if (map.contains("maskExpand") && m_maskExpandSlider) {
        m_maskExpandSlider->setValue(map.value("maskExpand").toInt());
    }
    if (map.contains("maskInvert") && m_maskInvertCheck) {
        m_maskInvertCheck->setChecked(map.value("maskInvert").toBool());
    }
    if (map.contains("maskOverlay") && m_maskOverlayCheck) {
        m_maskOverlayCheck->setChecked(map.value("maskOverlay").toBool());
    }
}

void MainWindow::saveCurrentPreset()
{
    if (!m_canvas) {
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "Save Preset",
                                         "Preset name:", QLineEdit::Normal,
                                         "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    QVariantMap map;
    if (m_setting1Slider && m_setting1Slider->isVisible()) {
        map["setting1"] = m_setting1Slider->value();
    }
    if (m_setting2Slider && m_setting2Slider->isVisible()) {
        map["setting2"] = m_setting2Slider->value();
    }
    if (m_setting3Slider && m_setting3Slider->isVisible()) {
        map["setting3"] = m_setting3Slider->value();
    }
    if (m_boolSetting1 && m_boolSetting1->isVisible()) {
        map["bool1"] = m_boolSetting1->isChecked();
    }
    if (m_boolSetting2 && m_boolSetting2->isVisible()) {
        map["bool2"] = m_boolSetting2->isChecked();
    }
    if (m_lineStyleCombo && m_lineStyleCombo->isVisible()) {
        map["lineStyle"] = m_lineStyleCombo->currentData().toInt();
    }
    if (m_maskFeatherSlider && m_maskFeatherSlider->isVisible()) {
        map["maskFeather"] = m_maskFeatherSlider->value();
    }
    if (m_maskBlurSlider && m_maskBlurSlider->isVisible()) {
        map["maskBlur"] = m_maskBlurSlider->value();
    }
    if (m_maskExpandSlider && m_maskExpandSlider->isVisible()) {
        map["maskExpand"] = m_maskExpandSlider->value();
    }
    if (m_maskInvertCheck && m_maskInvertCheck->isVisible()) {
        map["maskInvert"] = m_maskInvertCheck->isChecked();
    }
    if (m_maskOverlayCheck && m_maskOverlayCheck->isVisible()) {
        map["maskOverlay"] = m_maskOverlayCheck->isChecked();
    }

    QSettings settings;
    settings.beginGroup("ToolPresets");
    settings.beginGroup(toolKey(m_canvas->currentTool()));
    settings.setValue(name, map);
    settings.endGroup();
    settings.endGroup();

    updatePresetList(m_canvas->currentTool());
    m_presetCombo->setCurrentText(name);
}

void MainWindow::updateToolTooltips()
{
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        if (!action) {
            continue;
        }
        QString tip = action->text();
        if (!action->shortcut().isEmpty()) {
            tip += QString(" (%1)").arg(action->shortcut().toString(QKeySequence::NativeText));
        }
        action->setToolTip(tip);
    }
}

void MainWindow::loadToolShortcuts()
{
    QSettings settings;
    settings.beginGroup("Shortcuts");
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        QString key = toolKey(it.key());
        QString stored = settings.value(key).toString();
        if (!stored.isEmpty()) {
            action->setShortcut(QKeySequence(stored));
            action->setShortcutContext(Qt::ApplicationShortcut);
            addAction(action);
        }
    }
    settings.endGroup();
}

void MainWindow::saveToolShortcuts(const QMap<QString, QKeySequence>& shortcuts)
{
    QSettings settings;
    settings.beginGroup("Shortcuts");
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        settings.setValue(it.key(), it.value().toString(QKeySequence::NativeText));
    }
    settings.endGroup();
}

void MainWindow::showShortcutEditor()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Customize Shortcuts");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QWidget *container = new QWidget();
    QFormLayout *form = new QFormLayout(container);
    QMap<QString, QKeySequenceEdit*> edits;

    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        QString key = toolKey(it.key());
        QKeySequenceEdit *edit = new QKeySequenceEdit(action->shortcut());
        edit->setClearButtonEnabled(true);
        form->addRow(action->text(), edit);
        edits.insert(key, edit);
    }

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QMap<QString, QKeySequence> shortcuts;
        for (auto it = edits.begin(); it != edits.end(); ++it) {
            shortcuts.insert(it.key(), it.value()->keySequence());
        }
        saveToolShortcuts(shortcuts);
        for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
            QString key = toolKey(it.key());
            if (shortcuts.contains(key)) {
                it.value()->setShortcut(shortcuts.value(key));
                it.value()->setShortcutContext(Qt::ApplicationShortcut);
                addAction(it.value());
            }
        }
        updateToolTooltips();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::onPropertyChanged(const QString& objectName, const QString& propertyName, const QVariant& value)
{
    if (!m_canvas) {
        return;
    }
    
    // Handle canvas properties
    if (objectName == "Canvas") {
        if (propertyName == "backgroundColor") {
            m_canvas->setBackgroundColor(value.value<QColor>());
        }
        else if (propertyName == "gridVisible") {
            m_canvas->setGridVisible(value.toBool());
        }
        else if (propertyName == "gridColor") {
            m_canvas->setGridColor(value.value<QColor>());
        }
        else if (propertyName == "gridSize") {
            m_canvas->setGridSize(value.toFloat());
        }
        else if (propertyName == "snapEnabled") {
            m_canvas->setSnapEnabled(value.toBool());
        }
        else if (propertyName == "zoomSensitivity") {
            m_canvas->setZoomSensitivity(value.toFloat());
        }
        else if (propertyName == "rulersVisible") {
            m_canvas->setRulersVisible(value.toBool());
        }
        else if (propertyName == "paperFormat") {
            m_canvas->setPaperFormat(static_cast<DrawingCanvas::PaperFormat>(value.toInt()));
        }
        
        m_canvas->update();
        return;
    }
    
    auto selectedObjects = m_canvas->selectedObjects();
    if (selectedObjects.empty()) return;

    // Make primitive property edits undoable via CommandManager.
    std::unique_ptr<CompoundCommand> compound;
    if (m_commandManager) {
        compound = std::make_unique<CompoundCommand>(QString("Modify %1").arg(propertyName));
    }

    // Apply property changes to all selected objects
    for (DrawingPrimitive *primitive : selectedObjects) {
        if (!primitive) {
            continue;
        }

        std::unique_ptr<ModifyPrimitiveCommand> modify;
        if (compound) {
            modify = std::make_unique<ModifyPrimitiveCommand>(primitive, propertyName);
        }

        applyPropertyToPrimitive(primitive, propertyName, value);

        if (modify) {
            modify->captureNewState();
            if (modify->hasStateChange()) {
                // Property already applied; avoid re-applying on initial push.
                modify->markAlreadyApplied();
                compound->addCommand(std::move(modify));
            }
        }
    }

    if (compound && !compound->isEmpty() && m_commandManager) {
        m_commandManager->executeCommand(std::move(compound));
    }

    // Force redraw
    m_canvas->update();

    syncModifiedFlag();
}

void MainWindow::updateToolSettings(DrawingTool tool)
{
    // Leaving Select tool: turn off sticky multi-select so normal clicks replace
    if (tool != DrawingTool::Select && m_canvas)
        m_canvas->setAdditiveSelection(false);

    // Disconnect all previous connections to avoid conflicts
    disconnect(m_setting1Slider, nullptr, this, nullptr);
    disconnect(m_setting2Slider, nullptr, this, nullptr);
    disconnect(m_setting3Slider, nullptr, this, nullptr);
    disconnect(m_boolSetting1, nullptr, this, nullptr);
    disconnect(m_boolSetting2, nullptr, this, nullptr);
    disconnect(m_lineStyleCombo, nullptr, this, nullptr);
    disconnect(m_selectColorButton, nullptr, this, nullptr);
    disconnect(m_pipetteButton, nullptr, this, nullptr);
    disconnect(m_selectionModeCombo, nullptr, this, nullptr);
    disconnect(m_selectSimilarButton, nullptr, this, nullptr);
    disconnect(m_presetCombo, nullptr, this, nullptr);
    disconnect(m_savePresetButton, nullptr, this, nullptr);
    disconnect(m_maskSettingsButton, nullptr, this, nullptr);
    disconnect(m_textFontCombo, nullptr, this, nullptr);
    disconnect(m_textUnderlineBtn, nullptr, this, nullptr);
    disconnect(m_textColorBtn, nullptr, this, nullptr);

    // Block signals while resetting widget state to prevent stale handlers
    m_setting1Slider->blockSignals(true);
    m_setting2Slider->blockSignals(true);
    m_setting3Slider->blockSignals(true);
    m_boolSetting1->blockSignals(true);
    m_boolSetting2->blockSignals(true);
    m_lineStyleCombo->blockSignals(true);
    m_selectionModeCombo->blockSignals(true);
    m_presetCombo->blockSignals(true);

    // Hide all settings initially
    m_setting1Label->hide();
    m_setting1Slider->hide();
    m_setting1ValueLabel->hide();
    m_setting2Label->hide();
    m_setting2Slider->hide();
    m_setting2ValueLabel->hide();
    m_setting3Label->hide();
    m_setting3Slider->hide();
    m_setting3ValueLabel->hide();
    m_boolSetting1->hide();
    m_boolSetting2->hide();
    m_textFontCombo->hide();
    m_textUnderlineBtn->hide();
    m_textColorBtn->hide();
    m_extractButton->hide();
    m_lineStyleLabel->hide();
    m_lineStyleCombo->hide();
    m_selectColorLabel->hide();
    m_selectColorButton->hide();
    m_pipetteButton->hide();
    m_selectionModeLabel->hide();
    m_selectionModeCombo->hide();
    m_selectSimilarButton->hide();
    m_presetLabel->hide();
    m_presetCombo->hide();
    m_savePresetButton->hide();
    m_maskSettingsButton->hide();
    m_optSep1->hide();
    m_optSep2->hide();

    // NOTE: signals stay blocked through the switch statement and preset setup
    // to prevent setValue/setChecked from triggering handlers during reconfiguration.
    // They are unblocked at the very end of this function.

    // Configure settings based on active tool
    switch (tool) {
        case DrawingTool::Select:
            // Select by color widgets
            m_selectColorLabel->show();
            m_selectColorButton->show();
            m_pipetteButton->show();
            m_selectionModeLabel->show();
            m_selectionModeCombo->show();
            m_selectSimilarButton->show();

            // Multi-select: Shift/⌘ still work; checkbox enables sticky add-mode.
            // Default OFF so the AI composite isn't glued to the original photo.
            m_boolSetting1->setText(QStringLiteral("Multi-select mode"));
            m_boolSetting1->setToolTip(
                QStringLiteral(
                    "When on, clicks add images to the selection (no Shift needed). "
                    "Green mask clicks always add subjects on the same photo."));
            m_boolSetting1->setChecked(false);
            if (m_canvas)
                m_canvas->setAdditiveSelection(false);
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool on) {
                if (m_canvas)
                    m_canvas->setAdditiveSelection(on);
            });

            // Color range tolerance slider — value feeds selectByColor / pipette.
            m_setting1Label->setText("Tolerance:");
            m_setting1Label->show();
            m_setting1Slider->setRange(0, 100);
            m_setting1Slider->setValue(10); // Default tolerance
            m_setting1Slider->setToolTip("Color match tolerance for Select-by-Color and pipette");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText("10");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value));
                // Tolerance is stored and used when selecting by color
            });

            // Update color button swatch appearance
            m_selectColorButton->setStyleSheet(QString(
                "QPushButton { background: %1; border: 1px solid #3a3f47; border-radius: 4px; }"
                "QPushButton:hover { border: 1px solid #2a82da; }").arg(m_selectByColor.name()));

            // Color picker button - use Qt::QueuedConnection to avoid double-trigger
            connect(m_selectColorButton, &QPushButton::clicked, this, [this]() {
                m_selectColorButton->setEnabled(false);
                QColor color = QColorDialog::getColor(m_selectByColor, this, "Select Color to Find");
                m_selectColorButton->setEnabled(true);
                if (color.isValid()) {
                    m_selectByColor = color;
                    m_selectColorButton->setStyleSheet(QString(
                        "QPushButton { background: %1; border: 1px solid #3a3f47; border-radius: 4px; }"
                        "QPushButton:hover { border: 1px solid #2a82da; }").arg(color.name()));
                    if (m_canvas) {
                        m_canvas->selectByColor(color, m_setting1Slider->value());
                    }
                }
            }, Qt::QueuedConnection);

            // Pipette button
            connect(m_pipetteButton, &QPushButton::clicked, this, [this]() {
                if (m_canvas) {
                    m_canvas->enablePipetteMode(m_setting1Slider->value());
                }
            });

            if (m_canvas) {
                m_selectionModeCombo->setCurrentIndex(
                    m_canvas->selectionMode() == DrawingCanvas::SelectionMode::Lasso ? 1 : 0);
            }
            connect(m_selectionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int index) {
                        if (m_canvas) {
                            auto mode = index == 1 ? DrawingCanvas::SelectionMode::Lasso
                                                   : DrawingCanvas::SelectionMode::Rectangle;
                            m_canvas->setSelectionMode(mode);
                        }
                    });

            connect(m_selectSimilarButton, &QPushButton::clicked, this, [this]() {
                if (!m_canvas) {
                    return;
                }
                auto selected = m_canvas->selectedObjects();
                if (selected.empty()) {
                    return;
                }
                QColor color = selected.front()->color();
                m_canvas->selectByColor(color, m_setting1Slider->value());
            });
            break;
            
        case DrawingTool::Move:
            // No specific settings for move tool
            break;
            
        case DrawingTool::Line:
        case DrawingTool::AngleLine:
        case DrawingTool::Curve:
        case DrawingTool::BezierCurve:
        case DrawingTool::Spline:
        case DrawingTool::Polygon:
            // Line Width
            m_setting1Label->setText("Width:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 50);
            m_setting1Slider->setValue(m_canvas ? static_cast<int>(m_canvas->defaultLineWidth()) : 2);
            m_setting1Slider->setToolTip("Stroke width in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_canvas) {
                    m_canvas->setDefaultLineWidth(static_cast<float>(value));
                }
            });

            // Line Style selector — preserve current selection, re-apply to canvas.
            m_lineStyleLabel->show();
            m_lineStyleCombo->show();
            if (m_canvas) {
                int currentIndex = m_lineStyleCombo->currentIndex();
                Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(currentIndex).toInt());
                m_canvas->setDefaultLineStyle(style);
            }
            connect(m_lineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                if (m_canvas) {
                    Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(index).toInt());
                    m_canvas->setDefaultLineStyle(style);
                }
            });

            // Snap to Grid checkbox
            m_boolSetting1->setText("Snap to Grid");
            m_boolSetting1->setChecked(m_canvas ? m_canvas->isSnapEnabled() : true);
            m_boolSetting1->setToolTip("Snap new points to the grid");
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_canvas) {
                    m_canvas->setSnapEnabled(checked);
                }
            });
            break;
            
        case DrawingTool::Rectangle:
        case DrawingTool::Ellipse:
        case DrawingTool::Circle:
        case DrawingTool::Arc:
            // Line Width
            m_setting1Label->setText("Width:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 50);
            m_setting1Slider->setValue(m_canvas ? static_cast<int>(m_canvas->defaultLineWidth()) : 2);
            m_setting1Slider->setToolTip("Stroke width in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_canvas) {
                    m_canvas->setDefaultLineWidth(static_cast<float>(value));
                }
            });

            // Line Style selector — preserve current selection, re-apply to canvas.
            m_lineStyleLabel->show();
            m_lineStyleCombo->show();
            if (m_canvas) {
                int currentIndex = m_lineStyleCombo->currentIndex();
                Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(currentIndex).toInt());
                m_canvas->setDefaultLineStyle(style);
            }
            connect(m_lineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                if (m_canvas) {
                    Qt::PenStyle style = static_cast<Qt::PenStyle>(m_lineStyleCombo->itemData(index).toInt());
                    m_canvas->setDefaultLineStyle(style);
                }
            });
            break;
            
        case DrawingTool::Brush:
            // Brush Size
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_canvas ? static_cast<int>(m_canvas->brushSize()) : 10);
            m_setting1Slider->setToolTip("Brush diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_canvas) {
                    m_canvas->setBrushSize(value);
                }
            });

            // Hardness
            m_setting2Label->setText("Hardness:");
            m_setting2Label->show();
            m_setting2Slider->setRange(0, 100);
            m_setting2Slider->setValue(m_canvas ? static_cast<int>(m_canvas->brushHardness() * 100.0f) : 50);
            m_setting2Slider->setToolTip("Edge softness of the brush");
            m_setting2Slider->show();
            m_setting2ValueLabel->setText(QString::number(m_setting2Slider->value()) + "%");
            m_setting2ValueLabel->show();
            connect(m_setting2Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting2ValueLabel->setText(QString::number(value) + "%");
                if (m_canvas) {
                    m_canvas->setBrushHardness(value / 100.0f);
                }
            });
            break;
            
        case DrawingTool::Blur:
            // Blur Size — reuses the canvas brush size for the blur stroke radius.
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_canvas ? static_cast<int>(m_canvas->brushSize()) : 20);
            m_setting1Slider->setToolTip("Blur brush diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_canvas) {
                    m_canvas->setBrushSize(value);
                }
            });

            // Blur Strength — reuses the canvas brush hardness for blur intensity.
            m_setting2Label->setText("Strength:");
            m_setting2Label->show();
            m_setting2Slider->setRange(0, 100);
            m_setting2Slider->setValue(m_canvas ? static_cast<int>(m_canvas->brushHardness() * 100.0f) : 50);
            m_setting2Slider->setToolTip("Blur intensity");
            m_setting2Slider->show();
            m_setting2ValueLabel->setText(QString::number(m_setting2Slider->value()) + "%");
            m_setting2ValueLabel->show();
            connect(m_setting2Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting2ValueLabel->setText(QString::number(value) + "%");
                if (m_canvas) {
                    m_canvas->setBrushHardness(value / 100.0f);
                }
            });
            break;

        case DrawingTool::Eraser:
            // Eraser Size — wired to the canvas eraser radius.
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(1, 100);
            m_setting1Slider->setValue(m_canvas ? static_cast<int>(m_canvas->eraserSize()) : 20);
            m_setting1Slider->setToolTip("Eraser diameter in pixels");
            m_setting1Slider->show();
            m_setting1ValueLabel->setText(QString::number(m_setting1Slider->value()) + "px");
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "px");
                if (m_canvas) {
                    m_canvas->setEraserSize(static_cast<float>(value));
                }
            });
            break;

        case DrawingTool::Fill:
            // Fill has no numeric options that reach the flood-fill implementation;
            // only the preset picker is shown for this tool.
            break;

        case DrawingTool::Measure:
            // Measure has no wired options; measurements render directly on canvas.
            break;

        case DrawingTool::Text:
        {
            // --- Font Family ---
            m_textFontCombo->show();
            if (m_classicTextTool) {
                m_textFontCombo->setCurrentFont(QFont(m_classicTextTool->fontFamily()));
            }
            connect(m_textFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
                if (m_classicTextTool) {
                    m_classicTextTool->setFontFamily(f.family());
                }
            });

            // --- Font Size ---
            m_setting1Label->setText("Size:");
            m_setting1Label->show();
            m_setting1Slider->setRange(8, 144);
            m_setting1Slider->setToolTip("Font size in points");
            {
                int initSize = m_classicTextTool ? m_classicTextTool->fontSize() : 24;
                m_setting1Slider->setValue(initSize);
                m_setting1ValueLabel->setText(QString::number(initSize) + "pt");
            }
            m_setting1Slider->show();
            m_setting1ValueLabel->show();
            connect(m_setting1Slider, &QSlider::valueChanged, this, [this](int value) {
                m_setting1ValueLabel->setText(QString::number(value) + "pt");
                if (m_classicTextTool) {
                    m_classicTextTool->setFontSize(value);
                }
            });

            // --- Bold ---
            m_boolSetting1->setText("Bold");
            m_boolSetting1->setChecked(m_classicTextTool ? m_classicTextTool->isBold() : false);
            m_boolSetting1->setToolTip("Bold");
            m_boolSetting1->show();
            connect(m_boolSetting1, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_classicTextTool) {
                    m_classicTextTool->setBold(checked);
                }
            });

            // --- Italic ---
            m_boolSetting2->setText("Italic");
            m_boolSetting2->setChecked(m_classicTextTool ? m_classicTextTool->isItalic() : false);
            m_boolSetting2->setToolTip("Italic");
            m_boolSetting2->show();
            connect(m_boolSetting2, &QCheckBox::toggled, this, [this](bool checked) {
                if (m_classicTextTool) {
                    m_classicTextTool->setItalic(checked);
                }
            });

            // --- Underline (IconFactory icon button) ---
            m_textUnderlineBtn->setChecked(m_classicTextTool ? m_classicTextTool->isUnderline() : false);
            m_textUnderlineBtn->show();
            connect(m_textUnderlineBtn, &QToolButton::toggled, this, [this](bool checked) {
                if (m_classicTextTool) {
                    m_classicTextTool->setUnderline(checked);
                }
            });

            // --- Text Color (IconFactory swatch icon) ---
            {
                QColor initColor = m_classicTextTool ? m_classicTextTool->textColor() : Qt::white;
                m_textColorBtn->setIcon(IconFactory::textColor(initColor));
            }
            m_textColorBtn->show();
            connect(m_textColorBtn, &QToolButton::clicked, this, [this]() {
                QColor current = m_classicTextTool ? m_classicTextTool->textColor() : Qt::white;
                QColor color = QColorDialog::getColor(current, this, "Text Color");
                if (color.isValid()) {
                    if (m_classicTextTool) {
                        m_classicTextTool->setTextColor(color);
                    }
                    m_textColorBtn->setIcon(IconFactory::textColor(color));
                }
            });

            // --- Sync toolbar when ClassicTextTool properties change (e.g. after selecting existing text) ---
            if (m_classicTextTool) {
                connect(m_classicTextTool, &ClassicTextTool::propertyChanged, this, [this]() {
                    if (!m_classicTextTool) return;
                    m_textFontCombo->blockSignals(true);
                    m_textFontCombo->setCurrentFont(QFont(m_classicTextTool->fontFamily()));
                    m_textFontCombo->blockSignals(false);

                    m_setting1Slider->blockSignals(true);
                    m_setting1Slider->setValue(m_classicTextTool->fontSize());
                    m_setting1ValueLabel->setText(QString::number(m_classicTextTool->fontSize()) + "pt");
                    m_setting1Slider->blockSignals(false);

                    m_boolSetting1->blockSignals(true);
                    m_boolSetting1->setChecked(m_classicTextTool->isBold());
                    m_boolSetting1->blockSignals(false);

                    m_boolSetting2->blockSignals(true);
                    m_boolSetting2->setChecked(m_classicTextTool->isItalic());
                    m_boolSetting2->blockSignals(false);

                    m_textUnderlineBtn->blockSignals(true);
                    m_textUnderlineBtn->setChecked(m_classicTextTool->isUnderline());
                    m_textUnderlineBtn->blockSignals(false);

                    m_textColorBtn->setIcon(IconFactory::textColor(m_classicTextTool->textColor()));
                });
            }
            break;
        }
            
        case DrawingTool::Image:
        {
            // AI subject detection workflow (all controls reach ImagePrimitive).
            m_extractButton->show();

            // Enable Object Detection
            m_boolSetting2->setText("Detect Subjects");
            m_boolSetting2->setChecked(false);
            m_boolSetting2->setToolTip("Automatically detect the main subject in the selected image");
            m_boolSetting2->show();
            connect(m_boolSetting2, &QCheckBox::toggled, this, [this](bool checked) {
                if (!m_canvas) return;

                ImagePrimitive *imgPrim = imageForDetection();
                if (!imgPrim) {
                    if (checked) {
                        m_boolSetting2->setChecked(false);
                        QMessageBox::information(this, "No Image Selected",
                            "Please select an image first to detect objects.");
                    }
                    return;
                }

                if (checked) {
                    selectImageForMaskUI(imgPrim);
                    // Detection is asynchronous; success/failure
                    // status arrives via maskDetectionComplete /
                    // maskDetectionFailed. Don't read the candidate
                    // count here (always 0 right after starting).
                    imgPrim->startSubjectDetection();
                    m_statusLabel->setText("Detecting subjects…");
                    m_canvas->update();
                } else {
                    m_canvas->update();
                    m_statusLabel->setText("Object detection disabled");
                }
            });

            // Mask refinement controls — moved to popup dialog
            m_maskSettingsButton->show();
            connect(m_maskSettingsButton, &QPushButton::clicked, this, &MainWindow::showMaskSettingsPopup);
            break;
        }
            
        default:
            break;
    }

    // Preset controls — only for drawing tools that benefit from presets.
    // (Pin-to-favorites and recent-size/color widgets were removed.)
    bool showPresets = (tool == DrawingTool::Brush || tool == DrawingTool::Eraser ||
                        tool == DrawingTool::Blur || tool == DrawingTool::Line ||
                        tool == DrawingTool::Curve || tool == DrawingTool::BezierCurve ||
                        tool == DrawingTool::Spline || tool == DrawingTool::Polygon ||
                        tool == DrawingTool::Rectangle || tool == DrawingTool::Ellipse ||
                        tool == DrawingTool::Circle || tool == DrawingTool::Arc);
    if (showPresets) {
        m_presetLabel->show();
        m_presetCombo->show();
        m_savePresetButton->show();

        updatePresetList(tool);
        connect(m_presetCombo, &QComboBox::currentTextChanged, this, &MainWindow::applyPreset);
        connect(m_savePresetButton, &QToolButton::clicked, this, &MainWindow::saveCurrentPreset);
    }

    // Position hairline group separators: only shown when they actually divide
    // two visible groups (avoids orphan/leading separators when space collapses).
    bool leftGroup = m_setting1Slider->isVisible() || m_setting2Slider->isVisible() ||
                     m_lineStyleCombo->isVisible() || m_selectColorButton->isVisible() ||
                     m_selectionModeCombo->isVisible() || m_selectSimilarButton->isVisible() ||
                     m_textFontCombo->isVisible();
    bool rightGroup = m_boolSetting1->isVisible() || m_boolSetting2->isVisible() ||
                      m_textUnderlineBtn->isVisible() || m_textColorBtn->isVisible() ||
                      m_extractButton->isVisible() ||
                      m_maskSettingsButton->isVisible();
    m_optSep1->setVisible(leftGroup && rightGroup);
    m_optSep2->setVisible(showPresets && (leftGroup || rightGroup));

    refreshSmartSuggestions();

    // Unblock signals now that all widgets are configured and connections are set up
    m_setting1Slider->blockSignals(false);
    m_setting2Slider->blockSignals(false);
    m_setting3Slider->blockSignals(false);
    m_boolSetting1->blockSignals(false);
    m_boolSetting2->blockSignals(false);
    m_lineStyleCombo->blockSignals(false);
    m_selectionModeCombo->blockSignals(false);
    m_presetCombo->blockSignals(false);

    updatePropertyPanel();
}

void MainWindow::showMaskSettingsPopup()
{
    // Sync slider values from the currently selected image (if any)
    ImagePrimitive *firstImage = nullptr;
    if (m_canvas) {
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
                firstImage = imgPrim;
                break;
            }
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Mask Settings");
    dlg.setMinimumWidth(320);

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(8);

    QString sliderStyle = R"(
        QSlider::groove:horizontal { background: #34495e; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #4a90e2; width: 16px; margin: -5px 0; border-radius: 8px; }
        QSlider::handle:horizontal:hover { background: #5ba0f2; }
    )";

    // Invert Mask
    QCheckBox *invertCheck = new QCheckBox("Invert Mask");
    invertCheck->setChecked(firstImage ? firstImage->isMaskInverted() : m_maskInvertCheck->isChecked());
    form->addRow(invertCheck);

    // Preview Mask
    QCheckBox *overlayCheck = new QCheckBox("Preview Mask");
    overlayCheck->setChecked(firstImage ? firstImage->isMaskOverlayVisible() : m_maskOverlayCheck->isChecked());
    form->addRow(overlayCheck);

    // Feather
    QSlider *featherSlider = new QSlider(Qt::Horizontal);
    featherSlider->setRange(0, 30);
    featherSlider->setValue(firstImage ? firstImage->getMaskFeather() : m_maskFeatherSlider->value());
    featherSlider->setStyleSheet(sliderStyle);
    QLabel *featherVal = new QLabel(QString::number(featherSlider->value()));
    QHBoxLayout *featherRow = new QHBoxLayout();
    featherRow->addWidget(featherSlider);
    featherRow->addWidget(featherVal);
    connect(featherSlider, &QSlider::valueChanged, [featherVal](int v) { featherVal->setText(QString::number(v)); });
    form->addRow("Feather:", featherRow);

    // Blur
    QSlider *blurSlider = new QSlider(Qt::Horizontal);
    blurSlider->setRange(0, 20);
    blurSlider->setValue(firstImage ? firstImage->getMaskBlur() : m_maskBlurSlider->value());
    blurSlider->setStyleSheet(sliderStyle);
    QLabel *blurVal = new QLabel(QString::number(blurSlider->value()));
    QHBoxLayout *blurRow = new QHBoxLayout();
    blurRow->addWidget(blurSlider);
    blurRow->addWidget(blurVal);
    connect(blurSlider, &QSlider::valueChanged, [blurVal](int v) { blurVal->setText(QString::number(v)); });
    form->addRow("Blur:", blurRow);

    // Expand
    QSlider *expandSlider = new QSlider(Qt::Horizontal);
    expandSlider->setRange(-20, 20);
    expandSlider->setValue(firstImage ? firstImage->getMaskExpand() : m_maskExpandSlider->value());
    expandSlider->setStyleSheet(sliderStyle);
    QLabel *expandVal = new QLabel(QString::number(expandSlider->value()));
    QHBoxLayout *expandRow = new QHBoxLayout();
    expandRow->addWidget(expandSlider);
    expandRow->addWidget(expandVal);
    connect(expandSlider, &QSlider::valueChanged, [expandVal](int v) { expandVal->setText(QString::number(v)); });
    form->addRow("Expand:", expandRow);

    // Apply live while dialog is open
    auto applyToSelected = [&]() {
        if (!m_canvas) return;
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
                imgPrim->setMaskFeather(featherSlider->value());
                imgPrim->setMaskBlur(blurSlider->value());
                imgPrim->setMaskExpand(expandSlider->value());
                if (imgPrim->isMaskInverted() != invertCheck->isChecked()) {
                    imgPrim->invertMask();
                }
                imgPrim->setMaskOverlayVisible(overlayCheck->isChecked());
            }
        }
        m_canvas->update();
    };

    connect(featherSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(blurSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(expandSlider, &QSlider::valueChanged, [&]() { applyToSelected(); });
    connect(invertCheck, &QCheckBox::toggled, [&]() { applyToSelected(); });
    connect(overlayCheck, &QCheckBox::toggled, [&]() { applyToSelected(); });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    form->addRow(buttons);

    // Keep internal slider state in sync
    dlg.exec();
    m_maskFeatherSlider->setValue(featherSlider->value());
    m_maskBlurSlider->setValue(blurSlider->value());
    m_maskExpandSlider->setValue(expandSlider->value());
    m_maskInvertCheck->setChecked(invertCheck->isChecked());
    m_maskOverlayCheck->setChecked(overlayCheck->isChecked());
}

void MainWindow::onToolChanged()
{
    if (m_canvas) {
        updateToolSettings(m_canvas->currentTool());
    }
}

void MainWindow::applyPropertyToPrimitive(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value)
{
    // Handle readonly/calculated properties that shouldn't be modified
    if (propertyName == "area" || propertyName == "circumference" || propertyName == "perimeter" || 
        propertyName == "actualLength" || propertyName == "arcLength" || propertyName == "aspectRatio" ||
        propertyName == "controlPointCount" || propertyName == "pointCount" || propertyName == "vertices" ||
        propertyName == "type") {
        return; // Readonly property
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
    else if (propertyName == "lineStyle") {
        primitive->setLineStyle(static_cast<Qt::PenStyle>(value.toInt()));
    }
    else if (propertyName == "fillColor") {
        primitive->setFillColor(value.value<QColor>());
        if (auto *rect = dynamic_cast<RectanglePrimitive *>(primitive)) {
            rect->setFilled(true);
        } else if (auto *ellipse = dynamic_cast<EllipsePrimitive *>(primitive)) {
            ellipse->setFilled(true);
        } else if (auto *circle = dynamic_cast<CirclePrimitive *>(primitive)) {
            circle->setFilled(true);
        } else if (auto *polygon = dynamic_cast<PolygonPrimitive *>(primitive)) {
            polygon->setFilled(true);
        } else if (auto *spline = dynamic_cast<SplinePrimitive *>(primitive)) {
            spline->setFilled(true);
        }
    }
    else if (propertyName == "shadowEnabled") {
        primitive->setShadowEnabled(value.toBool());
    }
    else if (propertyName == "shadowColor") {
        primitive->setShadowColor(value.value<QColor>());
    }
    else if (propertyName == "shadowBlur") {
        primitive->setShadowBlur(value.toFloat());
    }
    else if (propertyName == "shadowOffsetX") {
        primitive->setShadowOffset(value.toFloat(), primitive->shadowOffsetY());
    }
    else if (propertyName == "shadowOffsetY") {
        primitive->setShadowOffset(primitive->shadowOffsetX(), value.toFloat());
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
            bool filled = value.toBool();
            rect->setFilled(filled);
            if (filled) {
                if (!rect->hasFillColor()) {
                    rect->setFillColor(rect->color());
                }
            } else {
                rect->clearFillColor();
            }
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
            bool filled = value.toBool();
            ellipse->setFilled(filled);
            if (filled) {
                if (!ellipse->hasFillColor()) {
                    ellipse->setFillColor(ellipse->color());
                }
            } else {
                ellipse->clearFillColor();
            }
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
            bool filled = value.toBool();
            spline->setFilled(filled);
            if (filled) {
                if (!spline->hasFillColor()) {
                    spline->setFillColor(spline->color());
                }
            } else {
                spline->clearFillColor();
            }
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
            bool filled = value.toBool();
            circle->setFilled(filled);
            if (filled) {
                if (!circle->hasFillColor()) {
                    circle->setFillColor(circle->color());
                }
            } else {
                circle->clearFillColor();
            }
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
            bool filled = value.toBool();
            polygon->setFilled(filled);
            if (filled) {
                if (!polygon->hasFillColor()) {
                    polygon->setFillColor(polygon->color());
                }
            } else {
                polygon->clearFillColor();
            }
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
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            curve->setFilled(filled);
            if (filled) {
                if (!curve->hasFillColor()) {
                    curve->setFillColor(curve->color());
                }
            } else {
                curve->clearFillColor();
            }
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
    // Handle image properties (from redesigned Property panel)
    else if (auto imagePrim = dynamic_cast<ImagePrimitive*>(primitive)) {
        if (propertyName == "imagePositionX") {
            QVector2D pos = imagePrim->position();
            imagePrim->setPosition(QVector2D(value.toFloat(), pos.y()));
        }
        else if (propertyName == "imagePositionY") {
            QVector2D pos = imagePrim->position();
            imagePrim->setPosition(QVector2D(pos.x(), value.toFloat()));
        }
        else if (propertyName == "imageWidth") {
            QVector2D sz = imagePrim->size();
            imagePrim->setSize(QVector2D(value.toFloat(), sz.y()));
        }
        else if (propertyName == "imageHeight") {
            QVector2D sz = imagePrim->size();
            imagePrim->setSize(QVector2D(sz.x(), value.toFloat()));
        }
        else if (propertyName == "imageRotation") {
            imagePrim->setRotation(value.toFloat());
        }
        else if (propertyName == "maintainAspectRatio") {
            imagePrim->setMaintainAspectRatio(value.toBool());
        }
        else if (propertyName == "opacity") {
            imagePrim->setOpacityMultiplier(value.toFloat());
        }
        else if (propertyName == "maskOverlayVisible") {
            imagePrim->setMaskOverlayVisible(value.toBool());
        }
        else if (propertyName == "maskInverted") {
            if (value.toBool() != imagePrim->isMaskInverted()) {
                imagePrim->invertMask();
            }
        }
        if (m_canvas) m_canvas->update();
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
    // Handle text properties
    else if (auto textPrim = dynamic_cast<TextPrimitive*>(primitive)) {
        if (propertyName == "text") {
            textPrim->setText(value.toString());
        }
        else if (propertyName == "fontFamily") {
            textPrim->setFontFamily(value.toString());
        }
        else if (propertyName == "fontSize") {
            textPrim->setFontSize(value.toInt());
        }
        else if (propertyName == "bold") {
            textPrim->setBold(value.toBool());
        }
        else if (propertyName == "italic") {
            textPrim->setItalic(value.toBool());
        }
        else if (propertyName == "rotation") {
            // Convert degrees to radians
            float radians = value.toFloat() * M_PI / 180.0f;
            textPrim->setRotation(radians);
        }
        else if (propertyName == "scale") {
            // Convert percentage to scale factor
            float scaleFactor = value.toFloat() / 100.0f;
            textPrim->setScale(scaleFactor);
        }
        else if (propertyName == "posX") {
            QVector2D pos = textPrim->position();
            textPrim->setPosition(QVector2D(value.toFloat(), pos.y()));
        }
        else if (propertyName == "posY") {
            QVector2D pos = textPrim->position();
            textPrim->setPosition(QVector2D(pos.x(), value.toFloat()));
        }
    }
}


// Alignment methods


// Icon creation methods
QIcon MainWindow::createMonoIcon(
    const std::function<void(QPainter&, const QRectF&)> &draw)
{
    // Affinity/Photoshop style: bright glyph; solid white when tool is checked
    // (button chrome turns blue).
    const QColor offColor(220, 224, 230);
    const QColor onColor(255, 255, 255);
    const QColor disabledColor(220, 224, 230, 70);

    const int logical = 22;
    const qreal dpr = 3.0;
    const QRectF drawRect(2.5, 2.5, logical - 5.0, logical - 5.0);

    auto makePixmap = [&](const QColor& color) {
        QPixmap pixmap(int(logical * dpr), int(logical * dpr));
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform
                               | QPainter::TextAntialiasing);
        QPen pen(color, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        draw(painter, drawRect);
        return pixmap;
    };

    QIcon icon;
    icon.addPixmap(makePixmap(offColor), QIcon::Normal, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Normal, QIcon::On);
    icon.addPixmap(makePixmap(offColor), QIcon::Active, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Active, QIcon::On);
    icon.addPixmap(makePixmap(offColor), QIcon::Selected, QIcon::Off);
    icon.addPixmap(makePixmap(onColor),  QIcon::Selected, QIcon::On);
    icon.addPixmap(makePixmap(disabledColor), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(makePixmap(disabledColor), QIcon::Disabled, QIcon::On);
    return icon;
}

QIcon MainWindow::createSelectIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        const qreal L = r.left(), T = r.top();
        QPainterPath arrow;
        arrow.moveTo(L + 1.2, T + 0.6);
        arrow.lineTo(L + 1.2, T + 14.0);
        arrow.lineTo(L + 4.6, T + 10.8);
        arrow.lineTo(L + 6.8, T + 15.6);
        arrow.lineTo(L + 8.9, T + 14.7);
        arrow.lineTo(L + 6.6, T + 9.8);
        arrow.lineTo(L + 11.2, T + 9.8);
        arrow.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(arrow);
    });
}

QIcon MainWindow::createLineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a(r.left() + 1.5, r.bottom() - 1.5);
        const QPointF b(r.right() - 1.5, r.top() + 1.5);
        p.drawLine(a, b);
        const QColor c = p.pen().color();
        p.setBrush(c);
        p.drawEllipse(a, 1.6, 1.6);
        p.drawEllipse(b, 1.6, 1.6);
    });
}

QIcon MainWindow::createAngleLineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF origin(r.left() + 2.5, r.bottom() - 2.5);
        p.drawLine(origin, QPointF(r.right() - 1.5, r.bottom() - 2.5));
        p.drawLine(origin, QPointF(r.left() + 11.0, r.top() + 2.0));
        p.drawArc(QRectF(origin.x() - 5.5, origin.y() - 5.5, 11.0, 11.0), 0 * 16, 55 * 16);
    });
}

QIcon MainWindow::createCurveIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        QPainterPath path;
        path.moveTo(r.left() + 1.0, r.bottom() - 2.0);
        path.cubicTo(r.left() + 2.0, r.top() + 1.0,
                     r.right() - 2.0, r.top() + 1.0,
                     r.right() - 1.0, r.bottom() - 2.0);
        p.drawPath(path);
    });
}

QIcon MainWindow::createBezierIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a1(r.left() + 1.5, r.bottom() - 1.5);
        const QPointF a2(r.right() - 1.5, r.top() + 2.0);
        const QPointF h1(r.left() + 5.5, r.top() + 3.0);
        const QPointF h2(r.right() - 5.5, r.bottom() - 3.0);
        QPainterPath path;
        path.moveTo(a1);
        path.cubicTo(h1, h2, a2);
        p.drawPath(path);
        QPen thin = p.pen();
        thin.setWidthF(1.15);
        p.setPen(thin);
        p.drawLine(a1, h1);
        p.drawLine(a2, h2);
        const QColor c = thin.color();
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(a1, 1.7, 1.7);
        p.drawEllipse(a2, 1.7, 1.7);
        p.setBrush(Qt::NoBrush);
        p.setPen(thin);
        p.drawEllipse(h1, 1.5, 1.5);
        p.drawEllipse(h2, 1.5, 1.5);
    });
}

QIcon MainWindow::createSplineIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center());
        p.rotate(-40);
        const qreal h = r.height();
        p.drawRoundedRect(QRectF(-1.7, -h * 0.42, 3.4, h * 0.55), 0.8, 0.8);
        QPolygonF tip;
        tip << QPointF(-1.7, h * 0.13) << QPointF(1.7, h * 0.13) << QPointF(0.0, h * 0.42);
        p.drawPolygon(tip);
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(0.0, h * 0.42), 1.1, 1.1);
        p.restore();
    });
}

QIcon MainWindow::createPolygonIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const qreal cx = r.center().x(), cy = r.center().y();
        const qreal rad = qMin(r.width(), r.height()) * 0.48;
        QPolygonF poly;
        for (int i = 0; i < 6; ++i) {
            const qreal a = -M_PI / 2.0 + i * M_PI / 3.0;
            poly << QPointF(cx + rad * qCos(a), cy + rad * qSin(a));
        }
        p.drawPolygon(poly);
    });
}

QIcon MainWindow::createRectangleIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.drawRoundedRect(r.adjusted(0.8, 2.0, -0.8, -2.0), 1.5, 1.5);
    });
}

QIcon MainWindow::createEllipseIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.drawEllipse(r.adjusted(0.2, 2.4, -0.2, -2.4));
    });
}

QIcon MainWindow::createCircleIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        // Perfect circle — no crosshair (reads cleaner vs ellipse).
        p.drawEllipse(r.adjusted(1.0, 1.0, -1.0, -1.0));
    });
}

QIcon MainWindow::createArcIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QRectF ar = r.adjusted(1.2, 1.2, -1.2, -1.2);
        p.drawArc(ar, -40 * 16, 250 * 16);
        const qreal cx = ar.center().x(), cy = ar.center().y();
        const qreal rad = ar.width() * 0.5;
        auto tick = [&](qreal deg) {
            const qreal a = deg * M_PI / 180.0;
            const QPointF dir(qCos(a), -qSin(a));
            const QPointF pt(cx + rad * dir.x(), cy + rad * dir.y());
            p.drawLine(pt - dir * 2.0, pt + dir * 1.2);
        };
        tick(-40.0);
        tick(210.0);
    });
}

QIcon MainWindow::createEraserIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center());
        p.rotate(-28);
        const QRectF body(-r.width() * 0.42, -r.height() * 0.24,
                          r.width() * 0.84, r.height() * 0.48);
        p.drawRoundedRect(body, 1.8, 1.8);
        const qreal divX = body.left() + body.width() * 0.38;
        p.drawLine(QPointF(divX, body.top() + 0.4), QPointF(divX, body.bottom() - 0.4));
        QPainterPath tip;
        tip.addRoundedRect(QRectF(body.left(), body.top(), body.width() * 0.38, body.height()), 1.8, 1.8);
        QColor c = p.pen().color();
        c.setAlpha(55);
        p.fillPath(tip, c);
        p.restore();
    });
}

QIcon MainWindow::createFillIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        p.save();
        p.translate(r.center().x() - 0.5, r.center().y() - 0.8);
        p.rotate(-28);
        QPainterPath bucket;
        bucket.moveTo(-4.6, -4.2);
        bucket.lineTo(4.6, -4.2);
        bucket.lineTo(3.2, 5.0);
        bucket.lineTo(-3.2, 5.0);
        bucket.closeSubpath();
        p.drawPath(bucket);
        p.drawArc(QRectF(-2.8, -7.6, 5.6, 5.0), 20 * 16, 140 * 16);
        p.restore();
        QPainterPath drop;
        const qreal dx = r.right() - 2.8, dy = r.bottom() - 4.2;
        drop.moveTo(dx, dy);
        drop.cubicTo(dx + 2.2, dy + 1.8, dx + 1.6, dy + 4.4, dx - 0.2, dy + 4.4);
        drop.cubicTo(dx - 1.8, dy + 4.4, dx - 2.0, dy + 2.0, dx, dy);
        const QColor c = p.pen().color();
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawPath(drop);
    });
}

QIcon MainWindow::createBrushIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        // Photoshop-style brush tip: outer soft ring + solid core.
        const QPointF c = r.center();
        const qreal outer = qMin(r.width(), r.height()) * 0.42;
        p.drawEllipse(c, outer, outer);
        QPen thin = p.pen();
        thin.setWidthF(1.15);
        thin.setStyle(Qt::DashLine);
        p.setPen(thin);
        p.drawEllipse(c, outer * 0.62, outer * 0.62);
        // Solid core
        const QColor col = thin.color();
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(c, outer * 0.28, outer * 0.28);
    });
}

QIcon MainWindow::createBlurIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const qreal cx = r.center().x();
        const qreal top = r.top() + 0.8;
        const qreal bot = r.bottom() - 0.8;
        const qreal w = r.width() * 0.32;
        QPainterPath drop;
        drop.moveTo(cx, top);
        drop.cubicTo(cx + w * 0.15, top + 3.0, cx + w, bot - w * 1.1, cx + w, bot - w);
        drop.cubicTo(cx + w, bot + 0.2, cx - w, bot + 0.2, cx - w, bot - w);
        drop.cubicTo(cx - w, bot - w * 1.1, cx - w * 0.15, top + 3.0, cx, top);
        p.drawPath(drop);
        QPen thin = p.pen();
        thin.setWidthF(1.1);
        p.setPen(thin);
        p.drawLine(QPointF(cx - w * 0.35, top + 5.0), QPointF(cx - w * 0.15, top + 8.5));
    });
}

QIcon MainWindow::createMeasureIcon() {
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QPointF a(r.left() + 1.2, r.bottom() - 2.5);
        const QPointF b(r.right() - 1.2, r.top() + 2.5);
        p.drawLine(a, b);
        QLineF line(a, b);
        const QPointF dir = line.unitVector().p2() - line.unitVector().p1();
        const QPointF n(-dir.y(), dir.x());
        auto endTick = [&](const QPointF &pt) { p.drawLine(pt - n * 2.6, pt + n * 2.6); };
        endTick(a); endTick(b);
        for (int i = 1; i <= 3; ++i) {
            const QPointF pt = a + (b - a) * (i / 4.0);
            p.drawLine(pt - n * 1.4, pt + n * 1.4);
        }
    });
}

QIcon MainWindow::createImageIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QRectF fr = r.adjusted(0.8, 1.6, -0.8, -1.6);
        p.drawRoundedRect(fr, 1.6, 1.6);
        p.drawEllipse(QPointF(fr.right() - 3.4, fr.top() + 3.2), 1.7, 1.7);
        QPainterPath mt;
        mt.moveTo(fr.left() + 0.8, fr.bottom() - 0.8);
        mt.lineTo(fr.left() + fr.width() * 0.32, fr.top() + fr.height() * 0.48);
        mt.lineTo(fr.left() + fr.width() * 0.50, fr.top() + fr.height() * 0.68);
        mt.lineTo(fr.left() + fr.width() * 0.72, fr.top() + fr.height() * 0.38);
        mt.lineTo(fr.right() - 0.8, fr.bottom() - 0.8);
        p.drawPath(mt);
    });
}

void MainWindow::toggleRulers()
{
    if (m_canvas) {
        bool areVisible = m_canvas->areRulersVisible();
        m_canvas->setRulersVisible(!areVisible);
        m_statusLabel->setText(QString("Rulers %1").arg(!areVisible ? "shown" : "hidden"));
    }
}

QIcon MainWindow::loadIconFromFile(const QString& filename)
{
    QString iconPath = QString(":/icons/%1").arg(filename);
    QIcon icon(iconPath);
    
    // If resource loading fails, try loading from file system
    if (icon.isNull()) {
        // Try relative path from build directory
        QString filePath = QString("../resources/icons/%1").arg(filename);
        if (QFile::exists(filePath)) {
            icon = QIcon(filePath);
        } else {
            // Try absolute path
            QString absolutePath = QString("/Users/Lukovic/Apps/DrawingStudio/resources/icons/%1").arg(filename);
            if (QFile::exists(absolutePath)) {
                icon = QIcon(absolutePath);
            }
        }
    }
    
    return icon;
}


void MainWindow::resetAIJobState()
{
    m_aiJobKind = AIJobKind::None;
    m_aiReplaceSelected = false;
    m_compositeSubjects.clear();
}

void MainWindow::onAIImageFinished(const QImage &image, const QString &prompt)
{
    const AIJobKind job = m_aiJobKind;
    statusBar()->clearMessage();

    if (job == AIJobKind::Generate) {
        const bool replace = m_aiReplaceSelected;
        resetAIJobState();
        importAIImageToCanvas(image, prompt, replace, true);
        return;
    }
    if (job == AIJobKind::Edit) {
        resetAIJobState();
        importAIImageToCanvas(image, prompt, true, true);
        return;
    }
    if (job == AIJobKind::CompositeBackground) {
        if (image.isNull()) {
            resetAIJobState();
            QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("Background generation failed."));
            return;
        }
        const auto dlg = m_compositeDlgResult;
        const QVector<CompositeHelper::SubjectSpec> subjects = m_compositeSubjects;
        m_aiJobKind = AIJobKind::None;
        if (dlg.placeBackgroundOnCanvas)
            importAIImageToCanvas(image, dlg.prompt, false, false);
        startCompositeBlend(subjects, image, dlg);
        return;
    }
    if (job == AIJobKind::CompositeBlend) {
        finishCompositePipeline(image, prompt);
        return;
    }
    if (job == AIJobKind::CompositeAiIntegrate) {
        resetAIJobState();
        statusBar()->clearMessage();
        if (image.isNull()) {
            QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("AI integrate returned an empty image."));
            return;
        }
        // Pure AI result only — never paste cutouts on top (that doubles people)
        importAIImageToCanvas(image, prompt, false, true);
        if (m_statusLabel)
            m_statusLabel->setText(
                QStringLiteral("AI integrate ready — subjects redrawn by AI"));
        return;
    }

    // Stale/unknown finished signal — ignore safely
    resetAIJobState();
}

void MainWindow::connectAIImageClient()
{
    if (m_aiImageClient)
        return;
    m_aiImageClient = new AIImageClient(this);
    connect(m_aiImageClient, &AIImageClient::started, this, [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
        statusBar()->showMessage(msg, 0);
    });
    connect(m_aiImageClient, &AIImageClient::progress, this, [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
        statusBar()->showMessage(msg, 0);
    });
    // Single permanent finished handler — no disconnect/reconnect races.
    connect(m_aiImageClient, &AIImageClient::finished, this,
            &MainWindow::onAIImageFinished);
    connect(m_aiImageClient, &AIImageClient::failed, this, [this](const QString &err) {
        resetAIJobState();
        statusBar()->clearMessage();
        const QString firstLine = err.section(QLatin1Char('\n'), 0, 0).trimmed();
        if (m_statusLabel)
            m_statusLabel->setText(
                firstLine.isEmpty() ? QStringLiteral("AI failed")
                                    : firstLine.left(120));
        QMessageBox::critical(this, QStringLiteral("AI Image"), err);
    });
}

void MainWindow::showAISettings()
{
    AISettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && m_statusLabel) {
        m_statusLabel->setText(
            QStringLiteral("AI provider: %1")
                .arg(AISettingsDialog::providerDisplayName(
                    AISettingsDialog::activeProvider())));
    }
}

void MainWindow::generateAIImage()
{
    if (!m_canvas)
        return;

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(this, QStringLiteral("AI Image"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    bool hasSelectedImage = false;
    for (auto *obj : m_canvas->selectedObjects()) {
        if (dynamic_cast<ImagePrimitive *>(obj)) {
            hasSelectedImage = true;
            break;
        }
    }

    AIGenerateDialog dlg(AIGenerateDialog::Mode::Generate, hasSelectedImage,
                         m_aiImageClient, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();

    m_aiJobKind = AIJobKind::Generate;
    m_aiReplaceSelected = result.replaceSelected;

    AIImageClient::Request req;
    req.prompt = result.prompt;
    req.provider = result.provider;
    req.model = result.model;
    req.size = result.size;
    req.aspectRatio = result.aspectRatio;
    req.imageSize = result.imageSize;
    req.quality = result.quality;
    m_aiImageClient->generate(req);
}

void MainWindow::editSelectedWithAI()
{
    if (!m_canvas)
        return;

    ImagePrimitive *selectedImage = nullptr;
    for (auto *obj : m_canvas->selectedObjects()) {
        if ((selectedImage = dynamic_cast<ImagePrimitive *>(obj)))
            break;
    }
    if (!selectedImage) {
        QMessageBox::information(
            this, QStringLiteral("Edit with AI"),
            QStringLiteral("Select an image on the canvas first, then choose "
                           "AI → Edit Selected Image."));
        return;
    }

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(this, QStringLiteral("AI Image"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    const QImage source = DrawingCanvas::getImageFromPrimitive(selectedImage);
    if (source.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Edit with AI"),
                             QStringLiteral("Could not read the selected image."));
        return;
    }

    AIGenerateDialog dlg(AIGenerateDialog::Mode::Edit, true, m_aiImageClient, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();

    m_aiJobKind = AIJobKind::Edit;

    AIImageClient::Request req;
    req.prompt = result.prompt;
    req.provider = result.provider;
    req.model = result.model;
    req.size = result.size;
    req.aspectRatio = result.aspectRatio;
    req.imageSize = result.imageSize;
    req.quality = result.quality;
    req.sourceImage = source;
    m_aiImageClient->edit(req);
}

namespace {

bool imageLooksLikeCutout(const QImage &im)
{
    if (im.isNull() || !im.hasAlphaChannel())
        return false;
    const QImage a = im.convertToFormat(QImage::Format_ARGB32);
    const int w = a.width();
    const int h = a.height();
    if (w < 1 || h < 1)
        return false;

    auto alphaAt = [&](int x, int y) -> int {
        return qAlpha(a.pixel(qBound(0, x, w - 1), qBound(0, y, h - 1)));
    };
    // Tight SAM2 crops almost always have transparent corners.
    if (alphaAt(0, 0) < 250 || alphaAt(w - 1, 0) < 250 ||
        alphaAt(0, h - 1) < 250 || alphaAt(w - 1, h - 1) < 250)
        return true;

    const int stepX = qMax(1, w / 128);
    const int stepY = qMax(1, h / 128);
    for (int y = 0; y < h; y += stepY) {
        const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        for (int x = 0; x < w; x += stepX) {
            if (qAlpha(line[x]) < 250)
                return true;
        }
    }
    return false;
}

} // namespace

void MainWindow::extractSelectedSubjects()
{
    if (!m_canvas)
        return;

    // Snapshot sources first — selection will change as we add cutouts.
    QVector<ImagePrimitive *> sources;
    for (auto *obj : m_canvas->selectedObjects()) {
        auto *img = dynamic_cast<ImagePrimitive *>(obj);
        if (!img)
            continue;
        if (img->getMaskCandidateCount() > 0 ||
            !img->getEditableContour().empty())
            sources.append(img);
    }

    if (sources.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Extract Subjects"),
            QStringLiteral(
                "Select one or more images with a SAM2 mask first "
                "(Detect Subjects → click a green outline)."));
        return;
    }

    QVector<ImagePrimitive *> created;
    int failed = 0;
    for (ImagePrimitive *src : sources) {
        auto extracted = src->extractDetectedSubject();
        if (!extracted || extracted->image().isNull()) {
            ++failed;
            continue;
        }
        ImagePrimitive *ptr = extracted.get();
        if (m_commandManager) {
            auto cmd =
                std::make_unique<ImportImageCommand>(m_canvas, std::move(extracted));
            m_commandManager->executeCommand(std::move(cmd));
        } else {
            m_canvas->addPrimitive(std::move(extracted));
        }
        created.append(ptr);
    }

    m_canvas->clearSelection();
    for (ImagePrimitive *cutout : created) {
        if (!cutout)
            continue;
        cutout->setSelected(true);
        m_canvas->addToSelection(cutout);
    }
    m_canvas->setCurrentTool(DrawingTool::Select);
    m_canvas->update();

    if (created.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("Extract Subjects"),
            QStringLiteral(
                "Could not extract any subjects. Click a green mask outline "
                "on each image, then try again."));
        return;
    }

    const QString msg =
        QStringLiteral("Extracted %1 cutout%2 — all selected. "
                       "Shift-click to add/remove, then AI → Place Subject in Scene.")
            .arg(created.size())
            .arg(created.size() == 1 ? QString() : QStringLiteral("s"));
    if (m_statusLabel)
        m_statusLabel->setText(msg);
    statusBar()->showMessage(msg, 8000);
    if (failed > 0) {
        QMessageBox::information(
            this, QStringLiteral("Extract Subjects"),
            QStringLiteral("Extracted %1 of %2 (some masks were missing).")
                .arg(created.size())
                .arg(sources.size()));
    }
}

bool MainWindow::resolveCompositeSubjects(
    QVector<CompositeHelper::SubjectSpec> *subjectsOut, QString *errorOut)
{
    if (!subjectsOut)
        return false;
    subjectsOut->clear();

    QVector<ImagePrimitive *> seen;

    auto appendCutout = [&](ImagePrimitive *img) -> bool {
        if (!img || seen.contains(img))
            return false;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (!imageLooksLikeCutout(im))
            return false;
        seen.append(img);
        CompositeHelper::SubjectSpec spec;
        spec.image = im;
        // Already-extracted cutouts: no source layout → auto pack
        subjectsOut->append(spec);
        return true;
    };

    auto appendFromMask = [&](ImagePrimitive *img) -> bool {
        if (!img || seen.contains(img))
            return false;
        if (img->getMaskCandidateCount() <= 0 &&
            img->getEditableContour().empty())
            return false;
        img->setMaskOverlayVisible(false);
        if (m_canvas)
            m_canvas->update();

        // Separate cutouts + original relative layout (not one glued crop)
        const auto pieces = img->extractSelectedMasksSeparately();
        if (pieces.isEmpty())
            return false;
        seen.append(img);
        for (const auto &piece : pieces) {
            if (piece.image.isNull())
                continue;
            CompositeHelper::SubjectSpec spec;
            spec.image = piece.image;
            spec.sourceNormRect = piece.sourceNormRect;
            subjectsOut->append(spec);
        }
        return !subjectsOut->isEmpty();
    };

    // Prefer already-extracted cutouts when multi-selected
    for (auto *obj : m_canvas->selectedObjects())
        appendCutout(dynamic_cast<ImagePrimitive *>(obj));

    // Also extract from any selected masked photos
    for (auto *obj : m_canvas->selectedObjects())
        appendFromMask(dynamic_cast<ImagePrimitive *>(obj));

    if (subjectsOut->isEmpty()) {
        if (ImagePrimitive *det = imageForDetection()) {
            if (!appendCutout(det))
                appendFromMask(det);
        }
    }

    if (subjectsOut->isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "No subjects found.\n\n"
                "• Extract cutouts (Image → Extract Selected Subject(s)), then "
                "Shift-click to select several, or\n"
                "• Select photos that still have a green SAM2 mask.");
        }
        return false;
    }
    return true;
}

bool MainWindow::resolveCompositeInputs(bool preferMaskedSubject,
                                        bool placeCutoutOnCanvas,
                                        QImage *subjectOut, QImage *sceneOut,
                                        QString *errorOut)
{
    if (!subjectOut)
        return false;
    *subjectOut = QImage();
    if (sceneOut)
        *sceneOut = QImage();

    ImagePrimitive *masked = nullptr;
    ImagePrimitive *cutout = nullptr;
    ImagePrimitive *opaqueScene = nullptr;

    auto consider = [&](ImagePrimitive *img) {
        if (!img)
            return;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (im.isNull())
            return;
        if (img->getMaskCandidateCount() > 0 && !masked)
            masked = img;
        // Treat images with meaningful alpha as cutouts
        if (im.hasAlphaChannel()) {
            bool anyAlpha = false;
            const QImage a = im.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < a.height() && !anyAlpha; y += qMax(1, a.height() / 32)) {
                const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
                for (int x = 0; x < a.width(); x += qMax(1, a.width() / 32)) {
                    if (qAlpha(line[x]) < 250) {
                        anyAlpha = true;
                        break;
                    }
                }
            }
            if (anyAlpha && !cutout)
                cutout = img;
            else if (!anyAlpha && !opaqueScene && img != masked)
                opaqueScene = img;
        } else if (!opaqueScene && img != masked) {
            // Never use the green-masked source photo as the scene background —
            // that pastes the cutout onto itself and doubles the subject.
            opaqueScene = img;
        }
    };

    for (auto *obj : m_canvas->selectedObjects())
        consider(dynamic_cast<ImagePrimitive *>(obj));

    // Also allow mask on the detection target even if not selected alone
    if (!masked) {
        if (ImagePrimitive *det = imageForDetection())
            consider(det);
    }

    // Fall back: scan layers for an opaque scene if needed
    if (sceneOut && !opaqueScene && m_canvas->layerManager()) {
        for (const auto &layer : m_canvas->layerManager()->layers()) {
            if (!layer)
                continue;
            for (const auto &p : layer->primitives()) {
                if (auto *img = dynamic_cast<ImagePrimitive *>(p.get())) {
                    if (img == cutout || img == masked)
                        continue;
                    const QImage im = DrawingCanvas::getImageFromPrimitive(img);
                    if (!im.isNull() && !im.hasAlphaChannel()) {
                        opaqueScene = img;
                        break;
                    }
                }
            }
            if (opaqueScene)
                break;
        }
    }

    // Final guard: never composite subject onto its own source photo
    if (opaqueScene && (opaqueScene == masked || opaqueScene == cutout))
        opaqueScene = nullptr;

    auto takeMaskSubject = [&](ImagePrimitive *src) -> bool {
        // Hide green overlay so it doesn't look like a second subject while composing
        src->setMaskOverlayVisible(false);
        if (m_canvas)
            m_canvas->update();
        auto extracted = src->extractDetectedSubject();
        if (!extracted) {
            if (errorOut)
                *errorOut = QStringLiteral("Could not extract the selected mask.");
            return false;
        }
        *subjectOut = extracted->image();
        // Never leave a cutout on canvas during compose unless explicitly requested.
        if (placeCutoutOnCanvas) {
            extracted->setMaskOverlayVisible(false);
            m_canvas->addPrimitiveWithCommand(std::move(extracted));
        }
        // else: discard the temporary cutout primitive — pixels already in subjectOut
        return true;
    };

    if (preferMaskedSubject && masked) {
        if (!takeMaskSubject(masked))
            return false;
    } else if (cutout) {
        *subjectOut = DrawingCanvas::getImageFromPrimitive(cutout);
    } else if (masked) {
        if (!takeMaskSubject(masked))
            return false;
    } else {
        if (errorOut)
            *errorOut = QStringLiteral(
                "No subject found. Detect Subjects on a photo, or select a cutout.");
        return false;
    }

    if (subjectOut->isNull()) {
        if (errorOut)
            *errorOut = QStringLiteral("Subject image is empty.");
        return false;
    }

    if (sceneOut) {
        if (!opaqueScene) {
            if (errorOut)
                *errorOut = QStringLiteral(
                    "No photo background found. Select a full image as the scene.");
            return false;
        }
        *sceneOut = DrawingCanvas::getImageFromPrimitive(opaqueScene);
        if (sceneOut->isNull()) {
            if (errorOut)
                *errorOut = QStringLiteral("Scene image is empty.");
            return false;
        }
    }
    return true;
}

void MainWindow::startCompositeBlend(
    const QVector<CompositeHelper::SubjectSpec> &subjects,
    const QImage &background, const AICompositeDialog::Result &dlgResult)
{
    if (subjects.isEmpty()) {
        resetAIJobState();
        QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("No subject cutouts to composite."));
        return;
    }

    const QSize plateSize = CompositeHelper::platePixelSize(
        dlgResult.imageSize.isEmpty() ? QStringLiteral("1K") : dlgResult.imageSize,
        dlgResult.aspectRatio.isEmpty() ? QStringLiteral("1:1") : dlgResult.aspectRatio);

    // Original SAM2/cutout pixels with alpha — separate silhouettes, layout preserved.
    const QImage plate =
        CompositeHelper::buildPlate(background, subjects, plateSize,
                                    /*matchLighting=*/true);
    if (plate.isNull()) {
        resetAIJobState();
        QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("Could not build the composite plate."));
        return;
    }

    if (!dlgResult.aiLightingRefine) {
        finishCompositePipeline(
            plate, dlgResult.prompt.isEmpty()
                       ? QStringLiteral("Local composite (%1 subject%2 preserved)")
                             .arg(subjects.size())
                             .arg(subjects.size() == 1 ? QString() : QStringLiteral("s"))
                       : dlgResult.prompt);
        return;
    }

    QString blendPrompt =
        QStringLiteral(
            "This image already has the correct foreground subject cutout(s). "
            "ONLY adjust global lighting, color temperature, and soft contact "
            "shadows so the subjects match the scene. "
            "Do NOT redraw, duplicate, move, or change faces, bodies, clothing, "
            "poses, or silhouettes. Keep transparent-cutout edges — no boxes.");
    if (!dlgResult.prompt.isEmpty())
        blendPrompt += QStringLiteral(" Notes: ") + dlgResult.prompt;

    m_aiJobKind = AIJobKind::CompositeBlend;
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("AI lighting refine (subject preserve)…"));
    statusBar()->showMessage(QStringLiteral("AI lighting refine…"), 0);

    AIImageClient::Request req;
    req.prompt = blendPrompt;
    req.provider = dlgResult.provider;
    req.model = dlgResult.model;
    req.size = dlgResult.size;
    req.aspectRatio = dlgResult.aspectRatio;
    req.imageSize = dlgResult.imageSize;
    req.quality = dlgResult.quality;
    req.sourceImage = plate;
    m_aiImageClient->edit(req);
}

void MainWindow::finishCompositePipeline(const QImage &blended, const QString &prompt)
{
    resetAIJobState();
    statusBar()->clearMessage();
    if (blended.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("Blend returned an empty image."));
        return;
    }
    // Import only the final composite (no cutout clone; green overlay already hidden)
    importAIImageToCanvas(blended, prompt, false, true);
    if (m_statusLabel)
        m_statusLabel->setText(
            QStringLiteral("Composite ready — original subject pixels preserved"));
}

void MainWindow::startAiIntegrateCutouts(
    const QVector<CompositeHelper::SubjectSpec> &subjects,
    const QImage &optionalScene,
    const AICompositeDialog::Result &dlgResult)
{
    if (subjects.isEmpty()) {
        resetAIJobState();
        QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("No subject cutouts to send to AI."));
        return;
    }

    QString integratePrompt =
        QStringLiteral(
            "The attached image(s) are SUBJECT CUTOUTS (people/objects with "
            "transparent backgrounds). Create ONE new photorealistic photograph "
            "that implements these exact subjects into the scene described below. "
            "CRITICAL: keep the SAME pose, body orientation, scale, and framing "
            "as each cutout — do not rotate or re-pose them. "
            "Match lighting, color temperature, and soft contact shadows so they "
            "look naturally photographed there. Preserve faces, bodies, clothing, "
            "and silhouettes — do not invent different people. Relative positions "
            "between multiple cutouts should stay similar to the references. ");
    if (!optionalScene.isNull()) {
        integratePrompt +=
            QStringLiteral(
                "Also attached is a SCENE / BACKGROUND photo — place the "
                "subjects into that environment (you may extend or adjust the "
                "background for a natural fit). ");
    }
    if (!dlgResult.prompt.isEmpty()) {
        integratePrompt += QStringLiteral("Scene / notes: ") + dlgResult.prompt;
    } else if (optionalScene.isNull()) {
        integratePrompt +=
            QStringLiteral(
                "Scene: a natural outdoor or indoor environment that fits the "
                "subjects.");
    }

    AIImageClient::Request req;
    req.prompt = integratePrompt;
    req.provider = dlgResult.provider;
    req.model = dlgResult.model;
    req.size = dlgResult.size;
    req.aspectRatio = dlgResult.aspectRatio;
    req.imageSize = dlgResult.imageSize;
    req.quality = dlgResult.quality;

    for (const auto &spec : subjects) {
        if (!spec.image.isNull())
            req.referenceImages.append(spec.image);
    }
    if (!optionalScene.isNull()) {
        req.sourceImage = optionalScene;
    } else if (!req.referenceImages.isEmpty()) {
        // Providers that need a primary image (OpenAI) use the first cutout
        req.sourceImage = req.referenceImages.first();
    }

    // Non-Gemini providers only accept one image — pack cutouts onto a plate
    const QString provider = dlgResult.provider.isEmpty()
                                 ? QStringLiteral("nanobanana")
                                 : dlgResult.provider;
    if (provider != QLatin1String("nanobanana") && subjects.size() >= 1) {
        const QSize plateSize = CompositeHelper::platePixelSize(
            dlgResult.imageSize.isEmpty() ? QStringLiteral("1K")
                                          : dlgResult.imageSize,
            dlgResult.aspectRatio.isEmpty() ? QStringLiteral("1:1")
                                            : dlgResult.aspectRatio);
        QImage bg = optionalScene;
        if (bg.isNull()) {
            bg = QImage(plateSize, QImage::Format_RGB888);
            bg.fill(QColor(180, 190, 200)); // neutral stand-in scene
        }
        const QImage plate =
            CompositeHelper::buildPlate(bg, subjects, plateSize,
                                        /*matchLighting=*/true);
        if (!plate.isNull()) {
            req.sourceImage = plate;
            // Still keep cutouts as refs where supported; primary is the plate
        }
    }

    m_aiJobKind = AIJobKind::CompositeAiIntegrate;
    if (m_statusLabel)
        m_statusLabel->setText(
            QStringLiteral("AI integrating %1 cutout(s) into new scene…")
                .arg(req.referenceImages.size()));
    statusBar()->showMessage(QStringLiteral("AI integrating cutouts…"), 0);
    m_aiImageClient->edit(req);
}

void MainWindow::placeSubjectInScene()
{
    if (!m_canvas)
        return;

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(this, QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    bool hasMasked = false;
    bool hasCutout = false;
    bool hasDistinctScene = false;
    ImagePrimitive *maskedPrim = nullptr;

    // Pass 1: find the green-masked source (must not count as "scene")
    auto findMasked = [&](ImagePrimitive *img) {
        if (!img || maskedPrim)
            return;
        if (img->getMaskCandidateCount() > 0)
            maskedPrim = img;
    };
    for (auto *obj : m_canvas->selectedObjects())
        findMasked(dynamic_cast<ImagePrimitive *>(obj));
    if (!maskedPrim)
        findMasked(imageForDetection());
    hasMasked = maskedPrim != nullptr;

    auto checkImg = [&](ImagePrimitive *img) {
        if (!img)
            return;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (im.isNull())
            return;
        if (imageLooksLikeCutout(im)) {
            hasCutout = true;
            return;
        }
        // Distinct scene = full photo that is NOT the SAM2-masked source
        if (img != maskedPrim)
            hasDistinctScene = true;
    };

    for (auto *obj : m_canvas->selectedObjects())
        checkImg(dynamic_cast<ImagePrimitive *>(obj));
    if (ImagePrimitive *det = imageForDetection())
        checkImg(det);

    // Also scan canvas for a separate opaque photo (for SceneOntoSubject)
    if (!hasDistinctScene && m_canvas->layerManager()) {
        for (const auto &layer : m_canvas->layerManager()->layers()) {
            if (!layer)
                continue;
            for (const auto &p : layer->primitives()) {
                auto *img = dynamic_cast<ImagePrimitive *>(p.get());
                if (!img || img == maskedPrim)
                    continue;
                const QImage im = DrawingCanvas::getImageFromPrimitive(img);
                if (im.isNull())
                    continue;
                if (!im.hasAlphaChannel()) {
                    hasDistinctScene = true;
                    break;
                }
            }
            if (hasDistinctScene)
                break;
        }
    }

    AICompositeDialog dlg(hasMasked, hasCutout, hasDistinctScene, m_aiImageClient, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto dlgResult = dlg.result();
    m_compositeDlgResult = dlgResult;

    if (dlgResult.mode == AICompositeDialog::Mode::SubjectIntoScene) {
        QVector<CompositeHelper::SubjectSpec> subjects;
        QString err;
        if (!resolveCompositeSubjects(&subjects, &err)) {
            QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"), err);
            return;
        }
        m_compositeSubjects = subjects;

        // Preserve faces = empty AI background + exact local cutouts (no doubles).
        // Full AI redraw only when integrate is on AND preserve is off.
        if (dlgResult.aiIntegrateCutouts && !dlgResult.preserveOriginalFaces) {
            startAiIntegrateCutouts(subjects, QImage(), dlgResult);
            return;
        }

        m_aiJobKind = AIJobKind::CompositeBackground;
        if (m_statusLabel)
            m_statusLabel->setText(
                QStringLiteral("Generating empty AI background for %1 subject%2…")
                    .arg(subjects.size())
                    .arg(subjects.size() == 1 ? QString() : QStringLiteral("s")));
        statusBar()->showMessage(QStringLiteral("Generating background…"), 0);

        AIImageClient::Request req;
        req.prompt =
            QStringLiteral(
                "Photorealistic EMPTY background plate only. "
                "Scene description: ") +
            dlgResult.prompt +
            QStringLiteral(
                ". Strict rules: zero people, zero humans, zero faces, zero "
                "silhouettes, zero mannequins, zero statues of people. "
                "Clean environment only — %1 foreground subject cutout(s) "
                "will be composited later.")
                .arg(subjects.size());
        req.provider = dlgResult.provider;
        req.model = dlgResult.model;
        req.size = dlgResult.size;
        req.aspectRatio = dlgResult.aspectRatio;
        req.imageSize = dlgResult.imageSize;
        req.quality = dlgResult.quality;
        m_aiImageClient->generate(req);
        return;
    }

    // SceneOntoSubject — multi cutouts onto a separate photo background
    QVector<CompositeHelper::SubjectSpec> subjects;
    QImage scene;
    QString err;
    if (!resolveCompositeSubjects(&subjects, &err)) {
        QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"), err);
        return;
    }
    {
        QImage unusedSubject;
        QString sceneErr;
        if (!resolveCompositeInputs(false, /*placeCutoutOnCanvas=*/false,
                                    &unusedSubject, &scene, &sceneErr)) {
            QMessageBox::warning(this, QStringLiteral("Place Subject in Scene"),
                                 sceneErr);
            return;
        }
    }
    if (dlgResult.aiIntegrateCutouts && !dlgResult.preserveOriginalFaces) {
        startAiIntegrateCutouts(subjects, scene, dlgResult);
        return;
    }
    startCompositeBlend(subjects, scene, dlgResult);
}

void MainWindow::importAIImageToCanvas(const QImage &image, const QString &prompt,
                                       bool replaceSelectedImage,
                                       bool showSuccessDialog)
{
    if (!m_canvas || image.isNull()) {
        QMessageBox::warning(
            this, QStringLiteral("AI Image"),
            QStringLiteral("AI returned an empty image — nothing to place on canvas."));
        return;
    }

    if (replaceSelectedImage) {
        replaceSelectedObjectWithImage(image, prompt, 0);
        return;
    }

    // Place near view center at a readable size — offset so it doesn't sit on
    // top of the original photo (keeps generated + source as separate objects).
    const float targetW = 400.0f;
    const float aspect =
        image.height() > 0
            ? static_cast<float>(image.width()) / static_cast<float>(image.height())
            : 1.0f;
    const float targetH = targetW / std::max(0.01f, aspect);
    const QVector2D center = m_canvas->viewCenter();
    QVector2D pos(center.x() - targetW * 0.5f + 420.0f,
                  center.y() - targetH * 0.5f);
    const QVector2D size(targetW, targetH);

    auto imagePrimitive = std::make_unique<ImagePrimitive>(image, pos, size);
    imagePrimitive->setVisible(true);
    imagePrimitive->setSelected(true);
    const QUuid id = imagePrimitive->id();

    // ImportImageCommand avoids AddPrimitiveCommand's full-image JSON clone
    if (m_commandManager) {
        auto cmd =
            std::make_unique<ImportImageCommand>(m_canvas, std::move(imagePrimitive));
        m_commandManager->executeCommand(std::move(cmd));
    } else {
        m_canvas->addPrimitive(std::move(imagePrimitive));
    }

    m_canvas->clearSelection();
    m_canvas->selectPrimitiveById(id);
    m_canvas->update();

    if (m_statusLabel)
        m_statusLabel->setText(
            QStringLiteral("AI image on canvas (%1×%2)")
                .arg(image.width())
                .arg(image.height()));
    if (showSuccessDialog) {
        QMessageBox::information(
            this, QStringLiteral("AI Image"),
            QStringLiteral("Image imported to canvas.\n\nPrompt: %1").arg(prompt));
    }
}

void MainWindow::replaceSelectedObjectWithImage(const QImage &image, const QString &prompt, int mode)
{
    if (!m_canvas || !m_layerManager) return;
    
    if (mode == 0) {
        // Mode 0: Replace Selected Object
        auto selectedObjects = m_canvas->selectedObjects();
        if (selectedObjects.empty()) {
            // No selection — import as new instead of failing
            importAIImageToCanvas(image, prompt, false);
            return;
        }

        // Prefer replacing an ImagePrimitive in-place when possible
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObjects[0])) {
            const QRectF bounds = imgPrim->boundingRect();
            imgPrim->setImage(image);
            imgPrim->setPosition(QVector2D(bounds.left(), bounds.top()));
            imgPrim->setSize(QVector2D(bounds.width(), bounds.height()));
            m_canvas->update();
            QMessageBox::information(
                this, QStringLiteral("Success"),
                QStringLiteral("Selected image updated with AI result.\n\nPrompt: %1")
                    .arg(prompt));
            return;
        }
        
        // Get the first selected object
        DrawingPrimitive* selectedObj = selectedObjects[0];
        QRectF bounds = selectedObj->boundingRect();
        
        // Create ImagePrimitive with the generated image
        QVector2D position(bounds.left(), bounds.top());
        QVector2D size(bounds.width(), bounds.height());
        
        auto imagePrimitive = std::make_unique<ImagePrimitive>(image, position, size);
        imagePrimitive->setVisible(true);
        
        // Get the layer of the selected object
        Layer* targetLayer = nullptr;
        const auto& layers = m_layerManager->layers();
        for (const auto& layer : layers) {
            const auto& primitives = layer->primitives();
            for (const auto& prim : primitives) {
                if (prim.get() == selectedObj) {
                    targetLayer = layer.get();
                    break;
                }
            }
            if (targetLayer) break;
        }
        
        // Delete the selected object
        m_canvas->deleteSelectedPrimitives();
        
        // Add the new image primitive to the same layer
        if (targetLayer) {
            targetLayer->addPrimitive(std::move(imagePrimitive));
        } else {
            // Fallback: add to active layer
            m_canvas->addPrimitive(std::move(imagePrimitive));
        }
        
        m_canvas->update();
        
        QMessageBox::information(this, "Success!", 
            QString("✨ AI-generated image created and replaced selected object!\n\nPrompt: %1").arg(prompt));
            
    } else if (mode == 1) {
        // Mode 1: Fill Closed Shape
        auto selectedObjects = m_canvas->selectedObjects();
        if (selectedObjects.empty()) {
            QMessageBox::information(this, "No Selection", 
                "No closed shape selected.\n\nTip: Select a rectangle, ellipse, or closed curve first.");
            return;
        }
        
        // Get the first selected object
        DrawingPrimitive* selectedObj = selectedObjects[0];
        QRectF bounds = selectedObj->boundingRect();
        
        // Create ImagePrimitive with the generated image (clipped to shape)
        QVector2D position(bounds.left(), bounds.top());
        QVector2D size(bounds.width(), bounds.height());
        
        auto imagePrimitive = std::make_unique<ImagePrimitive>(image, position, size);
        imagePrimitive->setVisible(true);
        
        // Get the layer of the selected object
        Layer* targetLayer = nullptr;
        const auto& layers = m_layerManager->layers();
        for (const auto& layer : layers) {
            const auto& primitives = layer->primitives();
            for (const auto& prim : primitives) {
                if (prim.get() == selectedObj) {
                    targetLayer = layer.get();
                    break;
                }
            }
            if (targetLayer) break;
        }
        
        // Add the new image primitive to the same layer (keep the original shape)
        if (targetLayer) {
            targetLayer->addPrimitive(std::move(imagePrimitive));
        } else {
            // Fallback: add to active layer
            m_canvas->addPrimitive(std::move(imagePrimitive));
        }
        
        m_canvas->update();
        
        QMessageBox::information(this, "Success!", 
            QString("✨ AI-generated image created inside the closed shape!\n\nPrompt: %1\n\nNote: The original shape is preserved.").arg(prompt));
    }
}

QIcon MainWindow::loadSVGIcon(const QString& iconName, const QSize& size)
{
    QString pngPath = QString(":/ai_icons/%1.png").arg(iconName);
    
    // Try loading from resources first
    if (QFile::exists(pngPath)) {
        QPixmap pixmap(pngPath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            return icon;
        }
    }
    
    // Try loading from file system (relative to build directory)
    QString filePath = QString("../resources/ai_icons/%1.png").arg(iconName);
    if (QFile::exists(filePath)) {
        QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            return icon;
        }
    }
    
    // Try absolute path
    QString absolutePath = QString("/Users/Lukovic/Apps/DrawingStudio/resources/ai_icons/%1.png").arg(iconName);
    if (QFile::exists(absolutePath)) {
        QPixmap pixmap(absolutePath);
        if (!pixmap.isNull()) {
            // Use normal icon size
             QSize newSize = QSize(24, 24); // Normal icon size
            QPixmap scaledPixmap = pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
            // Remove background by making it fully transparent
            QImage image = scaledPixmap.toImage();
            image = image.convertToFormat(QImage::Format_ARGB32);
            
            // Make white pixels transparent
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QRgb pixel = image.pixel(x, y);
                    QColor color(pixel);
                    
                    // If pixel is white or very light, make it transparent
                    if (color.red() > 240 && color.green() > 240 && color.blue() > 240) {
                        image.setPixel(x, y, qRgba(0, 0, 0, 0));
                    }
                }
            }
            
            scaledPixmap = QPixmap::fromImage(image);
            
            // Create icon with transparency
            QIcon icon(scaledPixmap);
            
            
            return icon;
        }
    }
    
    // Fallback to programmatic icon
    qDebug() << "PNG icon not found:" << iconName << ", using programmatic fallback";
    return QIcon();
}

QIcon MainWindow::loadAIIcon(const QString& toolName, const QSize& size)
{
    // Use modern programmatic icons directly
    if (toolName == "select") return createSelectIcon();
    if (toolName == "line") return createLineIcon();
    if (toolName == "angleline") return createAngleLineIcon();
    if (toolName == "curve") return createCurveIcon();
    if (toolName == "bezier") return createBezierIcon();
    if (toolName == "spline") return createSplineIcon();
    if (toolName == "polygon") return createPolygonIcon();
    if (toolName == "rectangle") return createRectangleIcon();
    if (toolName == "ellipse") return createEllipseIcon();
    if (toolName == "eraser") return createEraserIcon();
    if (toolName == "fill") return createFillIcon();
    if (toolName == "brush") return createBrushIcon();
    if (toolName == "blur") return createBlurIcon();
    if (toolName == "hand") return createHandIcon();
    if (toolName == "measure") return createMeasureIcon();
    if (toolName == "image") return createImageIcon();
    if (toolName == "text") return createTextIcon();
    
    // Default fallback
    return QIcon();
}

QIcon MainWindow::createHandIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal cx = r.center().x();
        const qreal T = r.top();
        p.drawRoundedRect(QRectF(cx - 4.8, T + 7.4, 9.6, 7.4), 3.2, 3.2);
        const qreal fw = 2.15;
        const qreal xs[4] = { cx - 4.2, cx - 1.5, cx + 1.2, cx + 3.9 };
        const qreal tops[4] = { T + 3.4, T + 1.8, T + 2.4, T + 4.2 };
        for (int i = 0; i < 4; ++i) {
            p.drawRoundedRect(QRectF(xs[i] - fw * 0.5, tops[i], fw, T + 11.0 - tops[i]), 1.05, 1.05);
        }
        p.save();
        p.translate(cx - 4.6, T + 9.0);
        p.rotate(-48);
        p.drawRoundedRect(QRectF(-1.1, -1.0, 2.2, 6.2), 1.05, 1.05);
        p.restore();
    });
}

QIcon MainWindow::createTextIcon()
{
    return createMonoIcon([](QPainter &p, const QRectF &r) {
        const QColor c = p.pen().color();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal cx = r.center().x();
        const qreal top = r.top() + 1.4;
        const qreal bot = r.bottom() - 1.2;
        const qreal barH = 2.35;
        const qreal stemW = 2.5;
        const qreal barL = r.left() + 1.6;
        const qreal barR = r.right() - 1.6;
        p.drawRect(QRectF(barL, top, barR - barL, barH));
        p.drawRect(QRectF(barL, top, 1.7, barH + 1.6));
        p.drawRect(QRectF(barR - 1.7, top, 1.7, barH + 1.6));
        p.drawRect(QRectF(cx - stemW * 0.5, top, stemW, bot - top - 1.5));
        p.drawRect(QRectF(cx - 3.8, bot - 1.7, 7.6, 1.7));
    });
}

void MainWindow::createFloatingMaskPanel() {
  if (!m_canvas) {
    qWarning() << "MainWindow: Cannot create floating mask panel - canvas not initialized";
    return;
  }

  m_floatingMaskPanel = new QWidget(m_canvas);
  m_floatingMaskPanel->setWindowFlags(Qt::Widget);
  m_floatingMaskPanel->setAutoFillBackground(true);

  m_floatingMaskPanel->setStyleSheet(R"(
    QWidget#floatingMaskPanel {
        background-color: rgba(18, 20, 26, 210);
        border: 1px solid rgba(74, 144, 226, 0.25);
        border-radius: 14px;
        color: white;
    }
    QLabel { color: #c8c8d0; background: transparent; border: none; }
  )");
  m_floatingMaskPanel->setObjectName("floatingMaskPanel");
  m_floatingMaskPanel->setMouseTracking(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(m_floatingMaskPanel);
  mainLayout->setContentsMargins(16, 14, 16, 14);
  mainLayout->setSpacing(10);

  // ── Title bar ──
  QWidget *titleBar = new QWidget();
  titleBar->setObjectName("maskPanelTitleBar");
  titleBar->setStyleSheet("background: transparent; border: none;");
  titleBar->setCursor(Qt::SizeAllCursor);
  titleBar->setMouseTracking(true);
  QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(0, 0, 0, 0);

  QLabel *dragDots = new QLabel("⠿");
  dragDots->setStyleSheet("color: #505060; font-size: 14px;");
  titleLayout->addWidget(dragDots);

  QLabel *titleLabel = new QLabel("MASK SELECTION");
  titleLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #707080; letter-spacing: 2px;");
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch();

  QPushButton *miniClose = new QPushButton(QString::fromUtf8("\xc3\x97"));
  miniClose->setFixedSize(22, 22);
  miniClose->setStyleSheet(R"(
    QPushButton { border: none; color: #606070; background: rgba(255,255,255,0.04); border-radius: 11px; font-size: 14px; }
    QPushButton:hover { color: #ff6b6b; background: rgba(255,100,100,0.12); }
  )");
  connect(miniClose, &QPushButton::clicked, this, &MainWindow::hideFloatingMaskPanel);
  titleLayout->addWidget(miniClose);
  mainLayout->addWidget(titleBar);
  titleBar->installEventFilter(this);

  // ── Mask counter ──
  m_fmpCounterLabel = new QLabel("- / -");
  m_fmpCounterLabel->setAlignment(Qt::AlignCenter);
  m_fmpCounterLabel->setStyleSheet("font-size: 28px; font-weight: 200; color: #4a90e2; letter-spacing: 4px; margin: 2px 0;");
  mainLayout->addWidget(m_fmpCounterLabel);

  // ── Score bar ──
  m_fmpScoreBar = new QWidget();
  m_fmpScoreBar->setFixedHeight(4);
  m_fmpScoreBar->setStyleSheet("background: rgba(74, 144, 226, 0.5); border-radius: 2px;");
  mainLayout->addWidget(m_fmpScoreBar);

  // ── Stats row ──
  QHBoxLayout *statsLayout = new QHBoxLayout();
  statsLayout->setSpacing(6);

  auto makeStatLabel = [](const QString &text) {
    QLabel *lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("font-size: 10px; color: #808090; background: rgba(255,255,255,0.03); border-radius: 4px; padding: 3px 4px;");
    return lbl;
  };

  m_fmpScoreLabel = makeStatLabel("Score --");
  m_fmpAreaLabel = makeStatLabel("Area --%");
  m_fmpStabilityLabel = makeStatLabel("Stab --");
  m_fmpIoULabel = makeStatLabel("IoU --");

  statsLayout->addWidget(m_fmpScoreLabel);
  statsLayout->addWidget(m_fmpAreaLabel);
  statsLayout->addWidget(m_fmpStabilityLabel);
  statsLayout->addWidget(m_fmpIoULabel);
  mainLayout->addLayout(statsLayout);

  // ── Prev / Next nav ──
  QString navBtnStyle = R"(
    QPushButton {
        background: rgba(255, 255, 255, 0.05);
        color: #c0c0cc;
        border: 1px solid rgba(255, 255, 255, 0.08);
        border-radius: 16px;
        font-size: 13px;
        min-width: 36px; min-height: 32px;
    }
    QPushButton:hover { background: rgba(74, 144, 226, 0.2); border-color: rgba(74, 144, 226, 0.4); color: white; }
    QPushButton:pressed { background: rgba(74, 144, 226, 0.1); }
    QPushButton:disabled { color: #404050; border-color: rgba(255,255,255,0.03); }
  )";

  QHBoxLayout *navLayout = new QHBoxLayout();
  navLayout->setSpacing(8);

  m_fmpPrevBtn = new QPushButton(QString::fromUtf8("\xe2\x97\x80"));
  m_fmpPrevBtn->setStyleSheet(navBtnStyle);
  connect(m_fmpPrevBtn, &QPushButton::clicked, this, [this]() {
    selectPreviousMask();
  });

  m_fmpNextBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"));
  m_fmpNextBtn->setStyleSheet(navBtnStyle);
  connect(m_fmpNextBtn, &QPushButton::clicked, this, [this]() {
    selectNextMask();
  });

  navLayout->addStretch();
  navLayout->addWidget(m_fmpPrevBtn);
  navLayout->addWidget(m_fmpNextBtn);
  navLayout->addStretch();
  mainLayout->addLayout(navLayout);

  // ── Slider for scrubbing ──
  m_fmpSlider = new QSlider(Qt::Horizontal);
  m_fmpSlider->setStyleSheet(R"(
    QSlider::groove:horizontal { background: rgba(255,255,255,0.06); height: 4px; border-radius: 2px; }
    QSlider::handle:horizontal { background: #4a90e2; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
    QSlider::handle:horizontal:hover { background: #5ba0f2; }
  )");
  connect(m_fmpSlider, &QSlider::valueChanged, this, [this](int val) {
    onMaskSelectionChanged(val);
    updateFloatingMaskPanel();
  });
  mainLayout->addWidget(m_fmpSlider);

  // ── Action buttons (2x2 grid) ──
  QGridLayout *grid = new QGridLayout();
  grid->setSpacing(6);

  QString actionBtnStyle = R"(
    QPushButton {
        background: rgba(255, 255, 255, 0.04);
        color: #c0c0cc;
        border: 1px solid rgba(255, 255, 255, 0.06);
        border-radius: 8px;
        font-size: 11px;
        padding: 10px 6px;
    }
    QPushButton:hover { background: rgba(74, 144, 226, 0.25); border-color: rgba(74, 144, 226, 0.4); color: white; }
    QPushButton:pressed { background: rgba(74, 144, 226, 0.15); }
  )";

  QPushButton *editBtn = new QPushButton("Edit Mask");
  editBtn->setStyleSheet(actionBtnStyle);
  connect(editBtn, &QPushButton::clicked, [this]() {
    for (auto *obj : m_canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        imgPrim->setEditMode(true);
        if (m_statusLabel) m_statusLabel->setText("Editing mask contour");
        break;
      }
    }
  });

  QPushButton *cutBtn = new QPushButton("Cut Out");
  cutBtn->setStyleSheet(actionBtnStyle);
  connect(cutBtn, &QPushButton::clicked, [this]() {
    for (auto *obj : m_canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        auto command = std::make_unique<ExtractSubjectCommand>(m_canvas, imgPrim);
        m_commandManager->executeCommand(std::move(command));
        if (m_statusLabel) m_statusLabel->setText("Subject extracted");
        break;
      }
    }
    hideFloatingMaskPanel();
  });

  QPushButton *invertBtn = new QPushButton("Invert");
  invertBtn->setStyleSheet(actionBtnStyle);
  connect(invertBtn, &QPushButton::clicked, [this]() {
    invertSelectedMask();
  });

  QPushButton *removeBgBtn = new QPushButton("Remove BG");
  removeBgBtn->setStyleSheet(actionBtnStyle);
  connect(removeBgBtn, &QPushButton::clicked, [this]() {
    for (auto *obj : m_canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        // Only proceed when there's actually a mask/contour to extract.
        if (imgPrim->getEditableContour().empty()) {
          if (m_statusLabel)
            m_statusLabel->setText("No mask to remove. Run detection first.");
          break;
        }
        // Remove BG = extract the subject and delete the original image,
        // wrapped in a single undoable compound command (like Cut Out but the
        // background-carrying original is removed too).
        auto compound = std::make_unique<CompoundCommand>("Remove Background");
        compound->addCommand(
            std::make_unique<ExtractSubjectCommand>(m_canvas, imgPrim));
        std::vector<DrawingPrimitive *> toDelete = {imgPrim};
        compound->addCommand(
            std::make_unique<DeletePrimitivesCommand>(m_canvas, toDelete));
        if (m_commandManager) {
          m_commandManager->executeCommand(std::move(compound));
        }
        if (m_statusLabel) m_statusLabel->setText("Background removed");
        break;
      }
    }
    hideFloatingMaskPanel();
  });

  grid->addWidget(editBtn, 0, 0);
  grid->addWidget(cutBtn, 0, 1);
  grid->addWidget(invertBtn, 1, 0);
  grid->addWidget(removeBgBtn, 1, 1);
  mainLayout->addLayout(grid);

  m_floatingMaskPanel->setFixedWidth(280);
  m_floatingMaskPanel->adjustSize();
  m_floatingMaskPanel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_floatingMaskPanel->hide();
}

// Helper function to load custom icons from file (Added by Rale)
QIcon MainWindow::loadCustomIcon(const QString &iconName,
                                 std::function<QIcon()> fallbackGenerator) {
  // Directly use the provided generator.
  if (fallbackGenerator) {
      return fallbackGenerator();
  }
  return QIcon();
}

// --- RESTORED MISSING IMPLEMENTATIONS ---

// Event Filter — only used for floating mask panel dragging
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (m_floatingMaskPanel) {
        static QWidget *cachedTitleBar = nullptr;
        if (!cachedTitleBar) {
            cachedTitleBar = m_floatingMaskPanel->findChild<QWidget*>("maskPanelTitleBar");
        }
        if (cachedTitleBar && obj == cachedTitleBar) {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_isDragging = true;
                    m_dragStartPosition = mouseEvent->pos();
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove) {
                if (m_isDragging) {
                    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                    QPoint newPos = m_floatingMaskPanel->mapToParent(mouseEvent->pos() - m_dragStartPosition);
                    m_floatingMaskPanel->move(newPos);
                    return true;
                }
            } else if (event->type() == QEvent::MouseButtonRelease) {
                m_isDragging = false;
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// Export
QString MainWindow::imageFormatFromPath(const QString &path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "jpg" || ext == "jpeg" || ext == "jfif") return QStringLiteral("JPEG");
    if (ext == "png") return QStringLiteral("PNG");
    if (ext == "bmp") return QStringLiteral("BMP");
    if (ext == "tif" || ext == "tiff") return QStringLiteral("TIFF");
    if (ext == "webp") return QStringLiteral("WEBP");
    if (ext == "gif") return QStringLiteral("GIF");
    if (ext == "ppm") return QStringLiteral("PPM");
    if (ext == "pbm") return QStringLiteral("PBM");
    if (ext == "pgm") return QStringLiteral("PGM");
    if (ext == "xbm") return QStringLiteral("XBM");
    if (ext == "xpm") return QStringLiteral("XPM");
    if (ext == "ico") return QStringLiteral("ICO");
    if (ext == "icns") return QStringLiteral("ICNS");
    if (ext == "jp2") return QStringLiteral("JP2");
    if (ext == "heic") return QStringLiteral("HEIC");
    if (ext == "heif") return QStringLiteral("HEIF");
    return QStringLiteral("PNG");
}

QString MainWindow::defaultExtensionForFormat(const QString &format) {
    const QString fmt = format.toUpper();
    if (fmt == QLatin1String("JPEG") || fmt == QLatin1String("JPG"))
        return QStringLiteral(".jpg");
    if (fmt == QLatin1String("PNG")) return QStringLiteral(".png");
    if (fmt == QLatin1String("BMP")) return QStringLiteral(".bmp");
    if (fmt == QLatin1String("TIFF") || fmt == QLatin1String("TIF"))
        return QStringLiteral(".tiff");
    if (fmt == QLatin1String("WEBP")) return QStringLiteral(".webp");
    if (fmt == QLatin1String("GIF")) return QStringLiteral(".gif");
    if (fmt == QLatin1String("PPM")) return QStringLiteral(".ppm");
    if (fmt == QLatin1String("PBM")) return QStringLiteral(".pbm");
    if (fmt == QLatin1String("PGM")) return QStringLiteral(".pgm");
    if (fmt == QLatin1String("XBM")) return QStringLiteral(".xbm");
    if (fmt == QLatin1String("XPM")) return QStringLiteral(".xpm");
    if (fmt == QLatin1String("ICO")) return QStringLiteral(".ico");
    if (fmt == QLatin1String("ICNS")) return QStringLiteral(".icns");
    if (fmt == QLatin1String("JP2")) return QStringLiteral(".jp2");
    if (fmt == QLatin1String("HEIC")) return QStringLiteral(".heic");
    if (fmt == QLatin1String("HEIF")) return QStringLiteral(".heif");
    return QStringLiteral(".png");
}

QString MainWindow::ensureImageExtension(const QString &path, const QString &format) {
    if (path.isEmpty()) return path;
    const QString expected = defaultExtensionForFormat(format);
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) {
        return path + expected;
    }

    // If the existing suffix already maps to the requested format, keep it.
    if (imageFormatFromPath(path).compare(format, Qt::CaseInsensitive) == 0) {
        return path;
    }

    // Replace mismatched extension with the format default.
    QFileInfo info(path);
    return info.path() + QLatin1Char('/') + info.completeBaseName() + expected;
}

QString MainWindow::imageExportFilterString() {
    // Prefer formats Qt can actually write on this build.
    const QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    auto has = [&](const char *fmt) {
        return supported.contains(QByteArray(fmt));
    };

    QStringList filters;
    filters << QStringLiteral("PNG (*.png)");
    if (has("jpg") || has("jpeg"))
        filters << QStringLiteral("JPEG (*.jpg *.jpeg)");
    if (has("bmp"))
        filters << QStringLiteral("BMP (*.bmp)");
    if (has("tif") || has("tiff"))
        filters << QStringLiteral("TIFF (*.tif *.tiff)");
    if (has("webp"))
        filters << QStringLiteral("WebP (*.webp)");
    if (has("gif"))
        filters << QStringLiteral("GIF (*.gif)");
    if (has("ppm"))
        filters << QStringLiteral("PPM (*.ppm)");
    if (has("jp2"))
        filters << QStringLiteral("JPEG 2000 (*.jp2)");
    if (has("ico"))
        filters << QStringLiteral("ICO (*.ico)");
    if (has("heic"))
        filters << QStringLiteral("HEIC (*.heic)");
    if (has("xbm"))
        filters << QStringLiteral("XBM (*.xbm)");
    if (has("xpm"))
        filters << QStringLiteral("XPM (*.xpm)");
    filters << QStringLiteral("All Files (*)");
    return filters.join(QStringLiteral(";;"));
}

QString MainWindow::formatFromFilter(const QString &selectedFilter) {
    const QString f = selectedFilter.toLower();
    if (f.contains(QLatin1String("jpeg 2000")) || f.contains(QLatin1String("*.jp2")))
        return QStringLiteral("JP2");
    if (f.contains(QLatin1String("jpeg")) || f.contains(QLatin1String("*.jpg")))
        return QStringLiteral("JPEG");
    if (f.contains(QLatin1String("png"))) return QStringLiteral("PNG");
    if (f.contains(QLatin1String("bmp"))) return QStringLiteral("BMP");
    if (f.contains(QLatin1String("tiff")) || f.contains(QLatin1String("*.tif")))
        return QStringLiteral("TIFF");
    if (f.contains(QLatin1String("webp"))) return QStringLiteral("WEBP");
    if (f.contains(QLatin1String("gif"))) return QStringLiteral("GIF");
    if (f.contains(QLatin1String("ppm"))) return QStringLiteral("PPM");
    if (f.contains(QLatin1String("heic"))) return QStringLiteral("HEIC");
    if (f.contains(QLatin1String("xbm"))) return QStringLiteral("XBM");
    if (f.contains(QLatin1String("xpm"))) return QStringLiteral("XPM");
    if (f.contains(QLatin1String("ico"))) return QStringLiteral("ICO");
    return QString();
}

bool MainWindow::exportCanvasToFile(const QString &path, const QString &format,
                                    int quality) {
    if (!m_canvas || path.isEmpty()) return false;

    QImage img = m_canvas->renderToImage();
    if (img.isNull()) return false;

    QString fmt = format;
    if (fmt.isEmpty()) {
        fmt = imageFormatFromPath(path);
    }
    fmt = fmt.toUpper();
    if (fmt == QLatin1String("JPG")) {
        fmt = QStringLiteral("JPEG");
    }

    // Formats that don't support alpha: flatten onto white.
    if (fmt == QLatin1String("JPEG") || fmt == QLatin1String("BMP") ||
        fmt == QLatin1String("PPM") || fmt == QLatin1String("GIF")) {
        if (img.hasAlphaChannel()) {
            QImage flat(img.size(), QImage::Format_RGB32);
            flat.fill(Qt::white);
            QPainter painter(&flat);
            painter.drawImage(0, 0, img);
            painter.end();
            img = flat;
        }
    }

    int q = quality;
    if (q < 0 && (fmt == QLatin1String("JPEG") || fmt == QLatin1String("WEBP"))) {
        q = 92;
    }

    bool ok = false;
    if (q >= 0) {
        ok = img.save(path, fmt.toUtf8().constData(), q);
    } else {
        ok = img.save(path, fmt.toUtf8().constData());
    }

    if (ok && m_statusLabel) {
        m_statusLabel->setText(
            QStringLiteral("Exported %1 (%2)")
                .arg(QFileInfo(path).fileName(), fmt));
    }
    return ok;
}

bool MainWindow::exportImageWithDialog(const QString &preferredFormat) {
    if (!m_canvas) return false;

    QString selectedFilter;
    if (!preferredFormat.isEmpty()) {
        const QString name = preferredFormat.toUpper();
        if (name == QLatin1String("JPEG") || name == QLatin1String("JPG")) {
            selectedFilter = QStringLiteral("JPEG (*.jpg *.jpeg)");
        } else if (name == QLatin1String("TIFF") || name == QLatin1String("TIF")) {
            selectedFilter = QStringLiteral("TIFF (*.tif *.tiff)");
        } else if (name == QLatin1String("WEBP")) {
            selectedFilter = QStringLiteral("WebP (*.webp)");
        } else if (name == QLatin1String("JP2")) {
            selectedFilter = QStringLiteral("JPEG 2000 (*.jp2)");
        } else if (name == QLatin1String("HEIC")) {
            selectedFilter = QStringLiteral("HEIC (*.heic)");
        } else {
            selectedFilter = QStringLiteral("%1 (*%2)")
                                 .arg(name, defaultExtensionForFormat(name));
        }
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Image"), QString(),
        imageExportFilterString(), &selectedFilter);

    if (fileName.isEmpty()) return false;

    QString format = preferredFormat;
    if (format.isEmpty()) {
        format = formatFromFilter(selectedFilter);
    }
    if (format.isEmpty()) {
        format = imageFormatFromPath(fileName);
    }

    fileName = ensureImageExtension(fileName, format);

    int quality = -1;
    if (format.compare(QLatin1String("JPEG"), Qt::CaseInsensitive) == 0 ||
        format.compare(QLatin1String("WEBP"), Qt::CaseInsensitive) == 0) {
        bool ok = false;
        quality = QInputDialog::getInt(
            this, QStringLiteral("Export Quality"),
            QStringLiteral("Quality (1–100):"), 92, 1, 100, 1, &ok);
        if (!ok) return false;
    }

    if (!exportCanvasToFile(fileName, format, quality)) {
        QMessageBox::warning(
            this, QStringLiteral("Export Image"),
            QStringLiteral("Failed to export image to:\n%1\n\n"
                           "Supported writers: %2")
                .arg(fileName,
                     QString::fromLatin1(
                         QImageWriter::supportedImageFormats().join(", "))));
        return false;
    }
    return true;
}

void MainWindow::exportImageAs() {
    exportImageWithDialog();
}

void MainWindow::exportAsJPG() {
    exportImageWithDialog(QStringLiteral("JPEG"));
}

void MainWindow::exportAsPNG() {
    exportImageWithDialog(QStringLiteral("PNG"));
}

void MainWindow::exportAsBMP() {
    exportImageWithDialog(QStringLiteral("BMP"));
}

void MainWindow::exportAsTIFF() {
    exportImageWithDialog(QStringLiteral("TIFF"));
}

void MainWindow::exportAsWebP() {
    exportImageWithDialog(QStringLiteral("WEBP"));
}

void MainWindow::exportAsGIF() {
    exportImageWithDialog(QStringLiteral("GIF"));
}

void MainWindow::exportAsPPM() {
    exportImageWithDialog(QStringLiteral("PPM"));
}

void MainWindow::exportAsDXF() {
    if (!m_layerManager) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Export as DXF", "", "DXF Files (*.dxf)");
    if (fileName.isEmpty()) return;

    DXFExporter exporter;
    DXFExporter::Options options;
    if (m_canvas) {
        switch (m_canvas->getUnits()) {
        case DrawingCanvas::Units::Millimeters:
            options.units = DXFExporter::Units::Millimeters;
            break;
        case DrawingCanvas::Units::Centimeters:
            options.units = DXFExporter::Units::Centimeters;
            break;
        case DrawingCanvas::Units::Inches:
            options.units = DXFExporter::Units::Inches;
            break;
        }
        options.scaleFactor = 1.0 / m_canvas->pixelsPerUnit();
    }

    if (exporter.exportToFile(fileName, m_layerManager->layers(), options)) {
        m_statusLabel->setText("Exported DXF to " + fileName);
    } else {
        QMessageBox::warning(this, "DXF Export", "Failed to export DXF file.");
    }
}

void MainWindow::extractLinesFromImage() {
    if (!m_canvas || !m_layerManager) return;

    QImage sourceImage;

    // Try to get image from selected ImagePrimitive
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto *obj : selectedObjects) {
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
            sourceImage = DrawingCanvas::getImageFromPrimitive(imgPrim);
            break;
        }
    }

    // Fallback: find any ImagePrimitive on any layer
    if (sourceImage.isNull() && m_layerManager) {
        for (const auto &layer : m_layerManager->layers()) {
            for (const auto &prim : layer->primitives()) {
                if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(prim.get())) {
                    sourceImage = DrawingCanvas::getImageFromPrimitive(imgPrim);
                    break;
                }
            }
            if (!sourceImage.isNull()) break;
        }
    }

    // If still no image, open file dialog
    if (sourceImage.isNull()) {
        QString fileName = QFileDialog::getOpenFileName(
            this, "Open Building Plan Image", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.tiff *.tif)");
        if (fileName.isEmpty()) return;
        sourceImage.load(fileName);
        if (sourceImage.isNull()) {
            QMessageBox::warning(this, "Error", "Failed to load image file.");
            return;
        }
    }

    auto *dialog = new LineExtractionDialog(sourceImage, m_canvas, m_layerManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

// Image Helpers
QImage MainWindow::applyBoxBlur(const QImage &source, int radius) {
    if (radius <= 0) return source;
    QImage img = source.convertToFormat(QImage::Format_ARGB32);
    QImage result(img.size(), QImage::Format_ARGB32);
    int w = img.width();
    int h = img.height();

    // Horizontal pass
    QImage temp(img.size(), QImage::Format_ARGB32);
    for (int y = 0; y < h; y++) {
        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        // Initialize window
        for (int x = -radius; x <= radius; x++) {
            int sx = qBound(0, x, w - 1);
            QRgb pixel = img.pixel(sx, y);
            rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
        }
        int windowSize = 2 * radius + 1;
        for (int x = 0; x < w; x++) {
            temp.setPixel(x, y, qRgba(rSum / windowSize, gSum / windowSize, bSum / windowSize, aSum / windowSize));
            // Slide window
            int removeX = qBound(0, x - radius, w - 1);
            int addX = qBound(0, x + radius + 1, w - 1);
            QRgb removePixel = img.pixel(removeX, y);
            QRgb addPixel = img.pixel(addX, y);
            rSum += qRed(addPixel) - qRed(removePixel);
            gSum += qGreen(addPixel) - qGreen(removePixel);
            bSum += qBlue(addPixel) - qBlue(removePixel);
            aSum += qAlpha(addPixel) - qAlpha(removePixel);
        }
    }

    // Vertical pass
    for (int x = 0; x < w; x++) {
        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        for (int y = -radius; y <= radius; y++) {
            int sy = qBound(0, y, h - 1);
            QRgb pixel = temp.pixel(x, sy);
            rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
        }
        int windowSize = 2 * radius + 1;
        for (int y = 0; y < h; y++) {
            result.setPixel(x, y, qRgba(rSum / windowSize, gSum / windowSize, bSum / windowSize, aSum / windowSize));
            int removeY = qBound(0, y - radius, h - 1);
            int addY = qBound(0, y + radius + 1, h - 1);
            QRgb removePixel = temp.pixel(x, removeY);
            QRgb addPixel = temp.pixel(x, addY);
            rSum += qRed(addPixel) - qRed(removePixel);
            gSum += qGreen(addPixel) - qGreen(removePixel);
            bSum += qBlue(addPixel) - qBlue(removePixel);
            aSum += qAlpha(addPixel) - qAlpha(removePixel);
        }
    }

    return result;
}

// Shadows


void MainWindow::addAutoShadow() {
    auto selected = m_canvas->selectedObjects();
    for (auto* obj : selected) {
        obj->setShadowEnabled(true);
        obj->setShadowBlur(10);
        obj->setShadowOffset(5, 5);
    }
    m_canvas->update();
}


// OCR


// Assistant


void MainWindow::applyCommonParams(DrawingPrimitive* prim, const QJsonObject& params) {
    if (!prim) return;

    // Opacity
    if (params.contains("opacity"))
        prim->setOpacityMultiplier(params["opacity"].toDouble(1.0));

    // Shadow
    if (params.contains("shadow")) {
        QJsonObject s = params["shadow"].toObject();
        prim->setShadowEnabled(s["enabled"].toBool(true));
        prim->setShadowColor(QColor(s["color"].toString("#00000080")));
        prim->setShadowOffset(s["offsetX"].toDouble(5), s["offsetY"].toDouble(5));
        prim->setShadowBlur(s["blur"].toDouble(5));
    }

    // Gradient fill
    if (params.contains("gradient")) {
        QJsonObject g = params["gradient"].toObject();
        QString type = g["type"].toString("linear");
        prim->setGradientFillType(type == "radial"
            ? DrawingPrimitive::GradientFillType::Radial
            : DrawingPrimitive::GradientFillType::Linear);
        prim->setGradientStartColor(QColor(g["startColor"].toString("#FFFFFF")));
        prim->setGradientEndColor(QColor(g["endColor"].toString("#000000")));
        prim->setGradientAngle(g["angle"].toDouble(0));
    }
}

void MainWindow::executeDrawingCommand(const QString& action, const QJsonObject& params) {
    if (!m_canvas) return;

    // Clear previous result so analysis commands can populate it
    m_lastCommandResult = QJsonObject();

    auto parseColor = [](const QJsonObject& p, const QString& key, const QColor& defaultColor = Qt::black) -> QColor {
        if (p.contains(key)) return QColor(p[key].toString());
        return defaultColor;
    };
    auto getDouble = [](const QJsonObject& p, const QString& key, double def = 0.0) -> double {
        if (p.contains(key)) return p[key].toDouble(def);
        return def;
    };
    auto getBool = [](const QJsonObject& p, const QString& key, bool def = false) -> bool {
        if (p.contains(key)) return p[key].toBool(def);
        return def;
    };

    if (action == "draw_line") {
        auto line = std::make_unique<LinePrimitive>(
            QVector2D(getDouble(params, "x1"), getDouble(params, "y1")),
            QVector2D(getDouble(params, "x2"), getDouble(params, "y2")));
        line->setColor(parseColor(params, "color"));
        line->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        if (params.contains("lineStyle")) {
            int style = params["lineStyle"].toInt(1);
            line->setLineStyle(static_cast<Qt::PenStyle>(style));
        }
        applyCommonParams(line.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(line));
    }
    else if (action == "draw_rectangle") {
        auto rect = std::make_unique<RectanglePrimitive>(
            QVector2D(getDouble(params, "x1"), getDouble(params, "y1")),
            QVector2D(getDouble(params, "x2"), getDouble(params, "y2")));
        rect->setColor(parseColor(params, "color"));
        rect->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        if (getBool(params, "fill")) {
            rect->setFilled(true);
            rect->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
        }
        if (params.contains("cornerRadius"))
            rect->setCornerRadius(getDouble(params, "cornerRadius"));
        // Gradient implies fill
        if (params.contains("gradient"))
            rect->setFilled(true);
        applyCommonParams(rect.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(rect));
    }
    else if (action == "draw_circle") {
        auto circle = std::make_unique<CirclePrimitive>(
            QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
            getDouble(params, "radius", 50.0));
        circle->setColor(parseColor(params, "color"));
        circle->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        if (getBool(params, "fill")) {
            circle->setFilled(true);
            circle->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
        }
        if (params.contains("gradient"))
            circle->setFilled(true);
        applyCommonParams(circle.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(circle));
    }
    else if (action == "draw_ellipse") {
        auto ellipse = std::make_unique<EllipsePrimitive>(
            QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
            getDouble(params, "rx", 50.0),
            getDouble(params, "ry", 30.0));
        ellipse->setColor(parseColor(params, "color"));
        ellipse->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        if (getBool(params, "fill")) {
            ellipse->setFilled(true);
            ellipse->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
        }
        if (params.contains("gradient"))
            ellipse->setFilled(true);
        applyCommonParams(ellipse.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(ellipse));
    }
    else if (action == "draw_polygon") {
        auto polygon = std::make_unique<PolygonPrimitive>();
        QJsonArray pointsArr = params["points"].toArray();
        for (const auto& pt : pointsArr) {
            QJsonObject p = pt.toObject();
            polygon->addPoint(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
        }
        polygon->setColor(parseColor(params, "color"));
        polygon->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        polygon->setClosed(getBool(params, "closed", true));
        if (getBool(params, "fill")) {
            polygon->setFilled(true);
            polygon->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
        }
        if (params.contains("gradient"))
            polygon->setFilled(true);
        applyCommonParams(polygon.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(polygon));
    }
    else if (action == "draw_text") {
        auto text = std::make_unique<TextPrimitive>(
            QVector2D(getDouble(params, "x"), getDouble(params, "y")),
            params["text"].toString("Text"));
        text->setColor(parseColor(params, "color"));
        if (params.contains("fontFamily"))
            text->setFontFamily(params["fontFamily"].toString());
        if (params.contains("fontSize"))
            text->setFontSize(getDouble(params, "fontSize", 24.0));
        text->setBold(getBool(params, "bold"));
        text->setItalic(getBool(params, "italic"));
        applyCommonParams(text.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(text));
    }
    else if (action == "draw_line_multi") {
        // Polyline: sequence of connected line segments
        QJsonArray pointsArr = params["points"].toArray();
        QColor color = parseColor(params, "color");
        float lineWidth = getDouble(params, "lineWidth", m_canvas->defaultLineWidth());
        for (int i = 0; i + 1 < pointsArr.size(); ++i) {
            QJsonObject p1 = pointsArr[i].toObject();
            QJsonObject p2 = pointsArr[i + 1].toObject();
            auto line = std::make_unique<LinePrimitive>(
                QVector2D(p1["x"].toDouble(), p1["y"].toDouble()),
                QVector2D(p2["x"].toDouble(), p2["y"].toDouble()));
            line->setColor(color);
            line->setLineWidth(lineWidth);
            applyCommonParams(line.get(), params);
            m_canvas->addPrimitiveWithCommand(std::move(line));
        }
    }
    else if (action == "draw_bezier") {
        auto bezier = std::make_unique<BezierCurvePrimitive>();
        QJsonArray pointsArr = params["points"].toArray();
        std::vector<QVector2D> pts;
        for (const auto& pt : pointsArr) {
            QJsonObject p = pt.toObject();
            pts.push_back(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
        }
        bezier->setControlPoints(pts);
        bezier->setColor(parseColor(params, "color"));
        bezier->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        applyCommonParams(bezier.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(bezier));
    }
    else if (action == "draw_spline") {
        auto spline = std::make_unique<SplinePrimitive>();
        QJsonArray pointsArr = params["points"].toArray();
        for (const auto& pt : pointsArr) {
            QJsonObject p = pt.toObject();
            spline->addPoint(QVector2D(p["x"].toDouble(), p["y"].toDouble()));
        }
        spline->setColor(parseColor(params, "color"));
        spline->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        if (params.contains("smoothness"))
            spline->setSmoothness(getDouble(params, "smoothness", 0.5));
        spline->setClosed(getBool(params, "closed"));
        if (getBool(params, "fill")) {
            spline->setFilled(true);
            spline->setFillColor(parseColor(params, "fillColor", QColor("#CCCCCC")));
        }
        if (params.contains("gradient"))
            spline->setFilled(true);
        applyCommonParams(spline.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(spline));
    }
    else if (action == "draw_arc") {
        auto arc = std::make_unique<ArcPrimitive>(
            QVector2D(getDouble(params, "cx"), getDouble(params, "cy")),
            getDouble(params, "radius", 50.0),
            getDouble(params, "startAngle", 0.0),
            getDouble(params, "endAngle", 90.0));
        arc->setColor(parseColor(params, "color"));
        arc->setLineWidth(getDouble(params, "lineWidth", m_canvas->defaultLineWidth()));
        applyCommonParams(arc.get(), params);
        m_canvas->addPrimitiveWithCommand(std::move(arc));
    }
    else if (action == "deselect") {
        m_canvas->clearSelection();
    }
    else if (action == "set_grid") {
        m_canvas->setGridVisible(getBool(params, "visible", false));
        m_canvas->setSnapEnabled(getBool(params, "snap", false));
    }
    else if (action == "set_background") {
        m_canvas->setBackgroundColor(parseColor(params, "color", QColor("#ffffff")));
        if (params.contains("paperColor"))
            m_canvas->setPaperColor(parseColor(params, "paperColor", QColor("#ffffff")));
    }
    else if (action == "set_rulers") {
        m_canvas->setRulersVisible(getBool(params, "visible", false));
    }
    else if (action == "set_color") {
        m_canvas->setDefaultDrawingColor(parseColor(params, "color"));
    }
    else if (action == "set_line_width") {
        m_canvas->setDefaultLineWidth(getDouble(params, "width", 2.0));
    }
    else if (action == "set_fill") {
        m_canvas->setDefaultFillEnabled(getBool(params, "enabled", true));
        if (params.contains("color"))
            m_canvas->setDefaultFillColor(parseColor(params, "color"));
    }
    else if (action == "select_all") {
        selectAll();
    }
    else if (action == "delete_selected") {
        deleteSelected();
    }
    else if (action == "copy_selected") {
        copySelected();
        QJsonObject result;
        result["success"] = !m_clipboard.empty();
        result["count"] = static_cast<int>(m_clipboard.size());
        m_lastCommandResult = result;
    }
    else if (action == "cut_selected") {
        copySelected();
        const int count = static_cast<int>(m_clipboard.size());
        if (count > 0)
            deleteSelected();
        QJsonObject result;
        result["success"] = count > 0;
        result["count"] = count;
        m_lastCommandResult = result;
    }
    else if (action == "paste") {
        const int before = m_clipboard.empty() ? 0 : static_cast<int>(m_clipboard.size());
        pasteClipboard();
        QJsonObject result;
        result["success"] = before > 0;
        result["count"] = before;
        m_lastCommandResult = result;
    }
    else if (action == "duplicate_selected") {
        const int count = m_canvas ? static_cast<int>(m_canvas->selectedObjects().size()) : 0;
        duplicateSelected();
        QJsonObject result;
        result["success"] = count > 0;
        result["count"] = count;
        m_lastCommandResult = result;
    }
    else if (action == "set_tool") {
        const QString tool = params.value("tool").toString().trimmed().toLower();
        DrawingTool mapped = DrawingTool::Select;
        bool ok = true;
        if (tool == "select") mapped = DrawingTool::Select;
        else if (tool == "move") mapped = DrawingTool::Move;
        else if (tool == "line") mapped = DrawingTool::Line;
        else if (tool == "curve") mapped = DrawingTool::Curve;
        else if (tool == "bezier" || tool == "beziercurve") mapped = DrawingTool::BezierCurve;
        else if (tool == "spline") mapped = DrawingTool::Spline;
        else if (tool == "polygon") mapped = DrawingTool::Polygon;
        else if (tool == "arc") mapped = DrawingTool::Arc;
        else if (tool == "circle") mapped = DrawingTool::Circle;
        else if (tool == "rectangle" || tool == "rect") mapped = DrawingTool::Rectangle;
        else if (tool == "ellipse") mapped = DrawingTool::Ellipse;
        else if (tool == "angleline" || tool == "angle_line") mapped = DrawingTool::AngleLine;
        else if (tool == "eraser") mapped = DrawingTool::Eraser;
        else if (tool == "fill") mapped = DrawingTool::Fill;
        else if (tool == "brush") mapped = DrawingTool::Brush;
        else if (tool == "blur") mapped = DrawingTool::Blur;
        else if (tool == "measure") mapped = DrawingTool::Measure;
        else if (tool == "image") mapped = DrawingTool::Image;
        else if (tool == "text") mapped = DrawingTool::Text;
        else ok = false;

        QJsonObject result;
        if (ok) {
            activateDrawingTool(mapped);
            result["success"] = true;
            result["tool"] = tool;
        } else {
            result["success"] = false;
            result["error"] = QStringLiteral("Unknown tool: %1").arg(tool);
        }
        m_lastCommandResult = result;
    }
    else if (action == "export_dxf") {
        QString path = params.value("path").toString();
        QJsonObject result;
        if (path.isEmpty() || !m_layerManager) {
            result["success"] = false;
            result["error"] = QStringLiteral("Missing path or layer manager");
        } else {
            if (!path.endsWith(".dxf", Qt::CaseInsensitive))
                path += ".dxf";
            DXFExporter exporter;
            DXFExporter::Options options;
            if (m_canvas) {
                switch (m_canvas->getUnits()) {
                case DrawingCanvas::Units::Millimeters:
                    options.units = DXFExporter::Units::Millimeters;
                    break;
                case DrawingCanvas::Units::Centimeters:
                    options.units = DXFExporter::Units::Centimeters;
                    break;
                case DrawingCanvas::Units::Inches:
                    options.units = DXFExporter::Units::Inches;
                    break;
                }
                options.scaleFactor = 1.0 / m_canvas->pixelsPerUnit();
            }
            const bool ok = exporter.exportToFile(path, m_layerManager->layers(), options);
            result["success"] = ok;
            result["path"] = path;
            if (!ok)
                result["error"] = QStringLiteral("DXF export failed");
        }
        m_lastCommandResult = result;
    }
    else if (action == "clear_canvas") {
        if (m_layerManager) {
            m_layerManager->clearLayers();
        }
        m_canvas->clearPrimitives();
        m_canvas->update();
    }
    else if (action == "zoom_fit") {
        m_canvas->zoomFit();
    }
    else if (action == "undo") {
        undo();
    }
    else if (action == "redo") {
        redo();
    }
    else if (action == "export_png") {
        QString path = params["path"].toString();
        if (!path.isEmpty()) {
            const bool ok = exportCanvasToFile(path, "PNG");
            QJsonObject result;
            result["success"] = ok;
            result["path"] = path;
            result["format"] = "PNG";
            if (ok) {
                result["bytes"] = static_cast<qint64>(QFileInfo(path).size());
            }
            m_lastCommandResult = result;
        }
    }
    else if (action == "export_image") {
        QString path = params["path"].toString();
        QString format = params["format"].toString();
        int quality = params.contains("quality") ? params["quality"].toInt(-1) : -1;
        if (!path.isEmpty()) {
            if (format.isEmpty()) {
                format = imageFormatFromPath(path);
            }
            path = ensureImageExtension(path, format);
            const bool ok = exportCanvasToFile(path, format, quality);
            QJsonObject result;
            result["success"] = ok;
            result["path"] = path;
            result["format"] = format.toUpper();
            if (ok) {
                result["bytes"] = static_cast<qint64>(QFileInfo(path).size());
            }
            m_lastCommandResult = result;
        }
    }
    else if (action == "list_export_formats") {
        QJsonArray formats;
        for (const QByteArray &fmt : QImageWriter::supportedImageFormats()) {
            formats.append(QString::fromLatin1(fmt));
        }
        QJsonObject result;
        result["formats"] = formats;
        result["filter"] = imageExportFilterString();
        m_lastCommandResult = result;
    }
    else if (action == "save_project") {
        QString path = params["path"].toString();
        if (path.isEmpty()) {
            qWarning() << "save_project: missing 'path' param";
        } else {
            QString savePath = path;
            if (!savePath.endsWith(".drawing", Qt::CaseInsensitive)) {
                savePath += ".drawing";
            }
            const bool ok = saveProjectToFile(savePath);
            QJsonObject result;
            result["success"] = ok;
            result["path"] = savePath;
            if (ok) {
                result["bytes"] = static_cast<qint64>(QFileInfo(savePath).size());
            }
            m_lastCommandResult = result;
        }
    }
    else if (action == "open_project") {
        QString path = params["path"].toString();
        if (path.isEmpty()) {
            qWarning() << "open_project: missing 'path' param";
        } else {
            const bool ok = loadProjectFromFile(path, true);
            QJsonObject result;
            result["success"] = ok;
            result["path"] = path;
            if (ok && m_canvas && m_canvas->layerManager()) {
                int count = 0;
                for (auto *p : m_canvas->layerManager()->getAllPrimitives()) {
                    if (p) ++count;
                }
                result["objectCount"] = count;
            }
            m_lastCommandResult = result;
        }
    }
    // ---- Image-to-drawing API actions ----
    else if (action == "import_image") {
        QString path = params["path"].toString();
        if (path.isEmpty()) {
            qWarning() << "import_image: missing 'path' param";
        } else {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            reader.setDecideFormatFromContent(true);

            // Scale down large images during load
            QSize originalSize = reader.size();
            const int MAX_LOAD_DIMENSION = 2048;
            if (originalSize.width() > MAX_LOAD_DIMENSION ||
                originalSize.height() > MAX_LOAD_DIMENSION) {
                QSize scaledSize = originalSize.scaled(
                    MAX_LOAD_DIMENSION, MAX_LOAD_DIMENSION, Qt::KeepAspectRatio);
                reader.setScaledSize(scaledSize);
            }

            QImage image = reader.read();
            if (image.isNull()) {
                qWarning() << "import_image: failed to load" << path;
            } else {
                double x = getDouble(params, "x", 0.0);
                double y = getDouble(params, "y", 0.0);

                // Auto-size if width/height omitted: fit to 300 world units
                double w, h;
                float aspect = static_cast<float>(image.width()) / static_cast<float>(image.height());
                if (params.contains("width") || params.contains("height")) {
                    if (params.contains("width") && params.contains("height")) {
                        w = getDouble(params, "width");
                        h = getDouble(params, "height");
                    } else if (params.contains("width")) {
                        w = getDouble(params, "width");
                        h = w / aspect;
                    } else {
                        h = getDouble(params, "height");
                        w = h * aspect;
                    }
                } else {
                    double maxSize = 300.0;
                    if (aspect > 1.0f) {
                        w = maxSize;
                        h = maxSize / aspect;
                    } else {
                        h = maxSize;
                        w = maxSize * aspect;
                    }
                }

                auto imgPrim = std::make_unique<ImagePrimitive>(
                    image, QVector2D(x, y), QVector2D(w, h));
                imgPrim->setColor(Qt::black);
                m_canvas->addPrimitiveWithCommand(std::move(imgPrim));

                // Count image primitives to determine index
                int imageIndex = 0;
                auto allPrims = m_canvas->layerManager()->getAllPrimitives();
                for (auto* p : allPrims) {
                    if (dynamic_cast<ImagePrimitive*>(p)) imageIndex++;
                }
                imageIndex--; // zero-based: just-added is the last

                QJsonObject result;
                result["imageWidth"] = image.width();
                result["imageHeight"] = image.height();
                result["worldX"] = x;
                result["worldY"] = y;
                result["worldWidth"] = w;
                result["worldHeight"] = h;
                result["imageIndex"] = imageIndex;
                m_lastCommandResult = result;
            }
        }
    }
    else if (action == "sample_color") {
        int imageIndex = params["imageIndex"].toInt(0);
        int pixelX = params["pixelX"].toInt(0);
        int pixelY = params["pixelY"].toInt(0);
        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "sample_color: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image();
            if (pixelX < 0 || pixelX >= img.width() || pixelY < 0 || pixelY >= img.height()) {
                qWarning() << "sample_color: pixel out of bounds";
            } else {
                QColor c = img.pixelColor(pixelX, pixelY);
                QJsonObject result;
                result["color"] = c.name(QColor::HexArgb);
                result["r"] = c.red();
                result["g"] = c.green();
                result["b"] = c.blue();
                result["a"] = c.alpha();
                m_lastCommandResult = result;
            }
        }
    }
    else if (action == "sample_colors_grid") {
        int imageIndex = params["imageIndex"].toInt(0);
        int gridX = params["gridX"].toInt(10);
        int gridY = params["gridY"].toInt(10);
        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "sample_colors_grid: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image();
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();

            double cellW = static_cast<double>(img.width()) / gridX;
            double cellH = static_cast<double>(img.height()) / gridY;
            double worldCellW = worldSize.x() / gridX;
            double worldCellH = worldSize.y() / gridY;

            QJsonArray samples;
            for (int row = 0; row < gridY; ++row) {
                for (int col = 0; col < gridX; ++col) {
                    int startX = static_cast<int>(col * cellW);
                    int startY = static_cast<int>(row * cellH);
                    int endX = static_cast<int>((col + 1) * cellW);
                    int endY = static_cast<int>((row + 1) * cellH);
                    endX = qMin(endX, img.width());
                    endY = qMin(endY, img.height());

                    // Average color with subsampling for large cells
                    long rSum = 0, gSum = 0, bSum = 0;
                    int count = 0;
                    int step = qMax(1, qMin((endX - startX), (endY - startY)) / 8);
                    for (int py = startY; py < endY; py += step) {
                        for (int px = startX; px < endX; px += step) {
                            QColor c = img.pixelColor(px, py);
                            rSum += c.red();
                            gSum += c.green();
                            bSum += c.blue();
                            count++;
                        }
                    }
                    QColor avg(count > 0 ? rSum / count : 0,
                               count > 0 ? gSum / count : 0,
                               count > 0 ? bSum / count : 0);

                    // World-space rectangle for this cell
                    double wx1 = worldPos.x() + col * worldCellW;
                    double wy1 = worldPos.y() + row * worldCellH;
                    double wx2 = wx1 + worldCellW;
                    double wy2 = wy1 + worldCellH;

                    QJsonObject sample;
                    sample["row"] = row;
                    sample["col"] = col;
                    sample["avgColor"] = avg.name();
                    sample["pixelX"] = (startX + endX) / 2;
                    sample["pixelY"] = (startY + endY) / 2;
                    sample["x1"] = wx1;
                    sample["y1"] = wy1;
                    sample["x2"] = wx2;
                    sample["y2"] = wy2;
                    samples.append(sample);
                }
            }

            QJsonObject result;
            result["samples"] = samples;
            result["gridX"] = gridX;
            result["gridY"] = gridY;
            result["imageWidth"] = img.width();
            result["imageHeight"] = img.height();
            m_lastCommandResult = result;
        }
    }
    else if (action == "detect_subjects") {
        int imageIndex = params["imageIndex"].toInt(0);
        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "detect_subjects: image not found at index" << imageIndex;
        } else {
            selectImageForMaskUI(imgPrim);
            imgPrim->startSubjectDetection();
            if (m_statusLabel) m_statusLabel->setText("Detecting subjects…");
        }
    }
    else if (action == "get_mask_info") {
        int imageIndex = params["imageIndex"].toInt(0);
        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (imgPrim) {
            QJsonObject result;
            result["candidateCount"] = imgPrim->getMaskCandidateCount();
            result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
            const auto *c = imgPrim->getSelectedCandidate();
            if (c) {
                result["score"] = c->score;
                result["areaPercent"] = c->area_percent;
            }
            m_lastCommandResult = result;
        }
    }
    else if (action == "next_mask") {
        selectNextMask();
        ImagePrimitive* imgPrim = selectedImageWithMasks();
        if (imgPrim) {
            QJsonObject result;
            result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
            result["candidateCount"] = imgPrim->getMaskCandidateCount();
            m_lastCommandResult = result;
        }
    }
    else if (action == "prev_mask") {
        selectPreviousMask();
        ImagePrimitive* imgPrim = selectedImageWithMasks();
        if (imgPrim) {
            QJsonObject result;
            result["selectedIndex"] = imgPrim->getSelectedMaskIndex();
            result["candidateCount"] = imgPrim->getMaskCandidateCount();
            m_lastCommandResult = result;
        }
    }
    else if (action == "invert_mask") {
        invertSelectedMask();
        ImagePrimitive* imgPrim = selectedImageWithMasks();
        if (imgPrim) {
            QJsonObject result;
            result["inverted"] = imgPrim->isMaskInverted();
            m_lastCommandResult = result;
        }
    }
    else if (action == "detect_edges") {
        int imageIndex = params["imageIndex"].toInt(0);
        int seedX = params["seedX"].toInt(0);
        int seedY = params["seedY"].toInt(0);
        float tolerance = static_cast<float>(getDouble(params, "tolerance", 30.0));

        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "detect_edges: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image();
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();

            EdgeSelectionTool edgeTool;
            // tolerance param is 0-255; EdgeSelectionTool uses 0-1 normalized
            edgeTool.setGrowthTolerance(tolerance / 255.0f);
            auto selResult = edgeTool.selectByEdges(img, QPointF(seedX, seedY));

            QJsonObject result;
            result["success"] = selResult.success;
            result["confidence"] = static_cast<double>(selResult.confidence);
            result["pointCount"] = static_cast<int>(selResult.contour.size());

            if (selResult.success && !selResult.contour.empty()) {
                // Scale factors: pixel → world
                double scaleX = worldSize.x() / img.width();
                double scaleY = worldSize.y() / img.height();

                QJsonArray contourPixels;
                QJsonArray contourWorld;
                for (const auto& pt : selResult.contour) {
                    QJsonObject pixelPt;
                    pixelPt["x"] = pt.x();
                    pixelPt["y"] = pt.y();
                    contourPixels.append(pixelPt);

                    QJsonObject worldPt;
                    worldPt["x"] = worldPos.x() + pt.x() * scaleX;
                    worldPt["y"] = worldPos.y() + pt.y() * scaleY;
                    contourWorld.append(worldPt);
                }
                result["contourPixels"] = contourPixels;
                result["contourWorld"] = contourWorld;
            }
            m_lastCommandResult = result;
        }
    }
    else if (action == "analyze_regions") {
        int imageIndex = params["imageIndex"].toInt(0);
        int rows = params["rows"].toInt(10);
        int cols = params["cols"].toInt(10);
        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "analyze_regions: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image();
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();

            double cellW = static_cast<double>(img.width()) / cols;
            double cellH = static_cast<double>(img.height()) / rows;
            double worldCellW = worldSize.x() / cols;
            double worldCellH = worldSize.y() / rows;

            QJsonArray regions;
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    int startX = static_cast<int>(col * cellW);
                    int startY = static_cast<int>(row * cellH);
                    int endX = static_cast<int>((col + 1) * cellW);
                    int endY = static_cast<int>((row + 1) * cellH);
                    endX = qMin(endX, img.width());
                    endY = qMin(endY, img.height());

                    long rSum = 0, gSum = 0, bSum = 0;
                    int minBright = 255, maxBright = 0;
                    int count = 0;
                    int step = qMax(1, qMin((endX - startX), (endY - startY)) / 8);
                    for (int py = startY; py < endY; py += step) {
                        for (int px = startX; px < endX; px += step) {
                            QColor c = img.pixelColor(px, py);
                            rSum += c.red();
                            gSum += c.green();
                            bSum += c.blue();
                            int brightness = (c.red() + c.green() + c.blue()) / 3;
                            minBright = qMin(minBright, brightness);
                            maxBright = qMax(maxBright, brightness);
                            count++;
                        }
                    }
                    QColor avg(count > 0 ? rSum / count : 0,
                               count > 0 ? gSum / count : 0,
                               count > 0 ? bSum / count : 0);
                    int contrast = maxBright - minBright;

                    double wx1 = worldPos.x() + col * worldCellW;
                    double wy1 = worldPos.y() + row * worldCellH;
                    double wx2 = wx1 + worldCellW;
                    double wy2 = wy1 + worldCellH;

                    QJsonObject region;
                    region["row"] = row;
                    region["col"] = col;
                    region["avgColor"] = avg.name();
                    region["contrast"] = contrast;
                    region["x1"] = wx1;
                    region["y1"] = wy1;
                    region["x2"] = wx2;
                    region["y2"] = wy2;
                    regions.append(region);
                }
            }

            QJsonObject result;
            result["regions"] = regions;
            result["rows"] = rows;
            result["cols"] = cols;
            m_lastCommandResult = result;
        }
    }
    else if (action == "delete_primitive") {
        int index = params["index"].toInt(-1);
        auto allPrims = m_canvas->layerManager()->getAllPrimitives();
        int totalCount = static_cast<int>(allPrims.size());

        if (totalCount == 0) {
            qWarning() << "delete_primitive: no primitives on canvas";
        } else {
            // Resolve index (-1 = last)
            int resolvedIndex = (index < 0) ? totalCount + index : index;
            if (resolvedIndex < 0 || resolvedIndex >= totalCount) {
                qWarning() << "delete_primitive: index out of range:" << index;
            } else {
                DrawingPrimitive* target = allPrims[resolvedIndex];
                // Search all layers and remove the primitive
                for (const auto& layer : m_canvas->layerManager()->layers()) {
                    layer->removePrimitive(target);
                }
                m_canvas->update();
            }
        }
    }
    // ---- Server-side rendering actions (photorealistic image-to-drawing) ----
    else if (action == "render_mosaic") {
        int imageIndex = params["imageIndex"].toInt(0);
        QString quality = params["quality"].toString("medium");
        QString mode = params["mode"].toString("adaptive");

        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "render_mosaic: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();

            // Save undo state once before bulk insert
            saveUndoState("Render Mosaic");
            Layer* layer = m_canvas->layerManager()->activeLayer();
            int primitivesCreated = 0;

            // Helper: average color of a small region using fast scanline access
            auto sampleArea = [&](int px, int py, int regionW, int regionH) -> QColor {
                long rSum = 0, gSum = 0, bSum = 0;
                int count = 0;
                int x0 = qMax(0, px);
                int y0 = qMax(0, py);
                int x1 = qMin(img.width(), px + regionW);
                int y1 = qMin(img.height(), py + regionH);
                // For small cells (<16px), sample every pixel
                int minDim = qMin(x1 - x0, y1 - y0);
                int step = (minDim < 16) ? 1 : qMax(1, minDim / 4);
                for (int sy = y0; sy < y1; sy += step) {
                    const QRgb* scanLine = reinterpret_cast<const QRgb*>(img.constScanLine(sy));
                    for (int sx = x0; sx < x1; sx += step) {
                        QRgb pixel = scanLine[sx];
                        rSum += qRed(pixel);
                        gSum += qGreen(pixel);
                        bSum += qBlue(pixel);
                        count++;
                    }
                }
                if (count == 0) return QColor(128, 128, 128);
                return QColor(rSum / count, gSum / count, bSum / count);
            };

            // 3x3 averaged corner sample for better accuracy
            auto sampleCorner = [&](int cx, int cy) -> QColor {
                long rSum = 0, gSum = 0, bSum = 0;
                int count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int sx = qBound(0, cx + dx, img.width() - 1);
                        int sy = qBound(0, cy + dy, img.height() - 1);
                        QRgb pixel = reinterpret_cast<const QRgb*>(img.constScanLine(sy))[sx];
                        rSum += qRed(pixel);
                        gSum += qGreen(pixel);
                        bSum += qBlue(pixel);
                        count++;
                    }
                }
                return QColor(rSum / count, gSum / count, bSum / count);
            };

            // Color distance (Euclidean in RGB)
            auto colorDist = [](const QColor& a, const QColor& b) -> float {
                float dr = a.red() - b.red();
                float dg = a.green() - b.green();
                float db = a.blue() - b.blue();
                return std::sqrt(dr * dr + dg * dg + db * db);
            };

            // Average two colors
            auto avgColor = [](const QColor& a, const QColor& b) -> QColor {
                return QColor((a.red() + b.red()) / 2, (a.green() + b.green()) / 2, (a.blue() + b.blue()) / 2);
            };

            // Lambda to create a gradient-filled rect from pixel and world coords
            auto createGradientRect = [&](int px, int py, int cw, int ch,
                                          double wx1, double wy1, double wx2, double wy2)
                -> std::unique_ptr<RectanglePrimitive> {
                auto rect = std::make_unique<RectanglePrimitive>(
                    QVector2D(wx1, wy1), QVector2D(wx2, wy2));
                rect->setFilled(true);
                rect->setLineWidth(0);

                // 3x3 averaged corner samples
                int cornerW = qMax(1, cw / 4);
                int cornerH = qMax(1, ch / 4);
                QColor tl = sampleCorner(px + cornerW / 2, py + cornerH / 2);
                QColor tr = sampleCorner(px + cw - cornerW / 2, py + cornerH / 2);
                QColor bl = sampleCorner(px + cornerW / 2, py + ch - cornerH / 2);
                QColor br = sampleCorner(px + cw - cornerW / 2, py + ch - cornerH / 2);

                float distH = colorDist(avgColor(tl, bl), avgColor(tr, br));
                float distV = colorDist(avgColor(tl, tr), avgColor(bl, br));
                float distD1 = colorDist(tl, br);
                float distD2 = colorDist(tr, bl);
                float maxDist = qMax(qMax(distH, distV), qMax(distD1, distD2));

                if (maxDist < 15.0f) {
                    QColor center = sampleArea(px + cw / 4, py + ch / 4, cw / 2, ch / 2);
                    rect->setFillColor(center);
                } else {
                    rect->setGradientFillType(DrawingPrimitive::GradientFillType::Linear);
                    if (maxDist == distH) {
                        rect->setGradientAngle(0);
                        rect->setGradientStartColor(avgColor(tl, bl));
                        rect->setGradientEndColor(avgColor(tr, br));
                    } else if (maxDist == distV) {
                        rect->setGradientAngle(90);
                        rect->setGradientStartColor(avgColor(bl, br));
                        rect->setGradientEndColor(avgColor(tl, tr));
                    } else if (maxDist == distD1) {
                        rect->setGradientAngle(135);
                        rect->setGradientStartColor(br);
                        rect->setGradientEndColor(tl);
                    } else {
                        rect->setGradientAngle(45);
                        rect->setGradientStartColor(bl);
                        rect->setGradientEndColor(tr);
                    }
                }
                return rect;
            };

            if (mode == "adaptive") {
                // --- Adaptive quadtree subdivision ---
                int adaptiveBase = 80; // medium default
                int maxDepth = 5;
                float varianceThreshold = 20.0f;
                bool blending = getBool(params, "blending", true);

                if (quality == "low")        { adaptiveBase = 50;  maxDepth = 4; varianceThreshold = 40.0f; }
                else if (quality == "medium") { adaptiveBase = 80;  maxDepth = 5; varianceThreshold = 20.0f; }
                else if (quality == "high")   { adaptiveBase = 150; maxDepth = 6; varianceThreshold = 12.0f; }
                else if (quality == "ultra")  { adaptiveBase = 250; maxDepth = 6; varianceThreshold = 8.0f; }

                // Allow explicit overrides
                if (params.contains("maxDepth")) maxDepth = params["maxDepth"].toInt(maxDepth);
                if (params.contains("varianceThreshold")) varianceThreshold = static_cast<float>(getDouble(params, "varianceThreshold", varianceThreshold));

                // Compute base grid from aspect ratio
                float aspect = static_cast<float>(img.width()) / static_cast<float>(img.height());
                int gridX, gridY;
                if (aspect > 1.0f) {
                    gridX = adaptiveBase;
                    gridY = qMax(1, static_cast<int>(adaptiveBase / aspect));
                } else {
                    gridY = adaptiveBase;
                    gridX = qMax(1, static_cast<int>(adaptiveBase * aspect));
                }

                // Quadtree cell struct
                struct QuadCell {
                    int px, py, cw, ch; // pixel coords in image
                    double wx, wy, ww, wh; // world coords
                    int depth;
                };

                std::queue<QuadCell> cellQueue;
                // Leaf cells stored for blending pass
                struct LeafCell {
                    int px, py, cw, ch;
                    double wx, wy, ww, wh;
                    float variance;
                };
                std::vector<LeafCell> leafCells;

                double baseCellW = static_cast<double>(img.width()) / gridX;
                double baseCellH = static_cast<double>(img.height()) / gridY;
                double worldBaseCellW = static_cast<double>(worldSize.x()) / gridX;
                double worldBaseCellH = static_cast<double>(worldSize.y()) / gridY;

                // Seed the queue with base grid cells
                for (int row = 0; row < gridY; ++row) {
                    for (int col = 0; col < gridX; ++col) {
                        QuadCell cell;
                        cell.px = static_cast<int>(col * baseCellW);
                        cell.py = static_cast<int>(row * baseCellH);
                        cell.cw = static_cast<int>(baseCellW);
                        cell.ch = static_cast<int>(baseCellH);
                        cell.wx = worldPos.x() + col * worldBaseCellW;
                        cell.wy = worldPos.y() + row * worldBaseCellH;
                        cell.ww = worldBaseCellW;
                        cell.wh = worldBaseCellH;
                        cell.depth = 0;
                        cellQueue.push(cell);
                    }
                }

                // Process queue iteratively
                while (!cellQueue.empty()) {
                    QuadCell cell = cellQueue.front();
                    cellQueue.pop();

                    // Compute variance: sample 3x3 grid inside cell, max pairwise distance
                    QColor samples[9];
                    int si = 0;
                    for (int gy = 0; gy < 3; ++gy) {
                        for (int gx = 0; gx < 3; ++gx) {
                            int sx = cell.px + (cell.cw * (gx + 1)) / 4;
                            int sy = cell.py + (cell.ch * (gy + 1)) / 4;
                            sx = qBound(0, sx, img.width() - 1);
                            sy = qBound(0, sy, img.height() - 1);
                            samples[si++] = sampleCorner(sx, sy);
                        }
                    }
                    float maxVar = 0;
                    for (int i = 0; i < 9; ++i) {
                        for (int j = i + 1; j < 9; ++j) {
                            float d = colorDist(samples[i], samples[j]);
                            if (d > maxVar) maxVar = d;
                        }
                    }

                    bool shouldSubdivide = (maxVar > varianceThreshold)
                                           && (cell.depth < maxDepth)
                                           && (cell.cw > 4) && (cell.ch > 4);

                    if (shouldSubdivide) {
                        // Subdivide into 4 sub-cells
                        int halfW = cell.cw / 2;
                        int halfH = cell.ch / 2;
                        double wHalfW = cell.ww / 2.0;
                        double wHalfH = cell.wh / 2.0;

                        QuadCell tl = { cell.px, cell.py, halfW, halfH,
                                        cell.wx, cell.wy, wHalfW, wHalfH, cell.depth + 1 };
                        QuadCell tr = { cell.px + halfW, cell.py, cell.cw - halfW, halfH,
                                        cell.wx + wHalfW, cell.wy, cell.ww - wHalfW, wHalfH, cell.depth + 1 };
                        QuadCell bl = { cell.px, cell.py + halfH, halfW, cell.ch - halfH,
                                        cell.wx, cell.wy + wHalfH, wHalfW, cell.wh - wHalfH, cell.depth + 1 };
                        QuadCell br = { cell.px + halfW, cell.py + halfH, cell.cw - halfW, cell.ch - halfH,
                                        cell.wx + wHalfW, cell.wy + wHalfH, cell.ww - wHalfW, cell.wh - wHalfH, cell.depth + 1 };
                        cellQueue.push(tl);
                        cellQueue.push(tr);
                        cellQueue.push(bl);
                        cellQueue.push(br);
                    } else {
                        // Leaf cell: render gradient rect
                        auto rect = createGradientRect(cell.px, cell.py, cell.cw, cell.ch,
                                                       cell.wx, cell.wy, cell.wx + cell.ww, cell.wy + cell.wh);
                        layer->addPrimitive(std::move(rect));
                        primitivesCreated++;

                        LeafCell lc;
                        lc.px = cell.px; lc.py = cell.py; lc.cw = cell.cw; lc.ch = cell.ch;
                        lc.wx = cell.wx; lc.wy = cell.wy; lc.ww = cell.ww; lc.wh = cell.wh;
                        lc.variance = maxVar;
                        leafCells.push_back(lc);
                    }
                }

                // Blending pass: add thin overlap rectangles at adjacent cell boundaries
                if (blending && leafCells.size() > 1) {
                    // Build a spatial index: map from (approx right edge, approx top) to leaf index
                    // Simple approach: for each pair of adjacent leaves, create blend strip
                    // Use a grid-based spatial lookup for efficiency
                    double minCellW = worldBaseCellW;
                    double minCellH = worldBaseCellH;
                    for (const auto& lc : leafCells) {
                        if (lc.ww < minCellW) minCellW = lc.ww;
                        if (lc.wh < minCellH) minCellH = lc.wh;
                    }
                    // Check all pairs (for moderate cell counts this is fine)
                    // Limit blending to avoid excessive primitives
                    int blendCount = 0;
                    int maxBlends = static_cast<int>(leafCells.size()) * 2;
                    for (size_t i = 0; i < leafCells.size() && blendCount < maxBlends; ++i) {
                        const auto& a = leafCells[i];
                        for (size_t j = i + 1; j < leafCells.size() && blendCount < maxBlends; ++j) {
                            const auto& b = leafCells[j];
                            double smallerW = qMin(a.ww, b.ww);
                            double smallerH = qMin(a.wh, b.wh);

                            // Check horizontal adjacency (a's right edge == b's left edge)
                            bool hAdj = (std::abs((a.wx + a.ww) - b.wx) < smallerW * 0.1)
                                        && (std::abs(a.wy - b.wy) < smallerH * 0.5)
                                        && (a.wy + a.wh > b.wy + smallerH * 0.1)
                                        && (b.wy + b.wh > a.wy + smallerH * 0.1);
                            // Check vertical adjacency (a's bottom edge == b's top edge)
                            bool vAdj = (std::abs((a.wy + a.wh) - b.wy) < smallerH * 0.1)
                                        && (std::abs(a.wx - b.wx) < smallerW * 0.5)
                                        && (a.wx + a.ww > b.wx + smallerW * 0.1)
                                        && (b.wx + b.ww > a.wx + smallerW * 0.1);

                            if (hAdj) {
                                double overlapW = smallerW * 0.25;
                                double overlapTop = qMax(a.wy, b.wy);
                                double overlapBot = qMin(a.wy + a.wh, b.wy + b.wh);
                                double edgeX = a.wx + a.ww;
                                // Sample colors at boundary
                                int aPx = qBound(0, a.px + a.cw - 1, img.width() - 1);
                                int bPx = qBound(0, b.px, img.width() - 1);
                                int midPy = qBound(0, (a.py + a.py + a.ch) / 2, img.height() - 1);
                                QColor aColor = sampleCorner(aPx, midPy);
                                QColor bColor = sampleCorner(bPx, midPy);
                                QColor blendColor = avgColor(aColor, bColor);

                                auto blendRect = std::make_unique<RectanglePrimitive>(
                                    QVector2D(edgeX - overlapW / 2, overlapTop),
                                    QVector2D(edgeX + overlapW / 2, overlapBot));
                                blendRect->setFilled(true);
                                blendRect->setLineWidth(0);
                                blendRect->setFillColor(blendColor);
                                blendRect->setOpacityMultiplier(0.4f);
                                layer->addPrimitive(std::move(blendRect));
                                primitivesCreated++;
                                blendCount++;
                            }
                            if (vAdj) {
                                double overlapH = smallerH * 0.25;
                                double overlapLeft = qMax(a.wx, b.wx);
                                double overlapRight = qMin(a.wx + a.ww, b.wx + b.ww);
                                double edgeY = a.wy + a.wh;
                                int aPy = qBound(0, a.py + a.ch - 1, img.height() - 1);
                                int bPy = qBound(0, b.py, img.height() - 1);
                                int midPx = qBound(0, (a.px + a.px + a.cw) / 2, img.width() - 1);
                                QColor aColor = sampleCorner(midPx, aPy);
                                QColor bColor = sampleCorner(midPx, bPy);
                                QColor blendColor = avgColor(aColor, bColor);

                                auto blendRect = std::make_unique<RectanglePrimitive>(
                                    QVector2D(overlapLeft, edgeY - overlapH / 2),
                                    QVector2D(overlapRight, edgeY + overlapH / 2));
                                blendRect->setFilled(true);
                                blendRect->setLineWidth(0);
                                blendRect->setFillColor(blendColor);
                                blendRect->setOpacityMultiplier(0.4f);
                                layer->addPrimitive(std::move(blendRect));
                                primitivesCreated++;
                                blendCount++;
                            }
                        }
                    }
                }

                m_canvas->update();
                QJsonObject result;
                result["primitivesCreated"] = primitivesCreated;
                result["leafCells"] = static_cast<int>(leafCells.size());
                result["mode"] = mode;
                // Store leaf cell data in m_adaptiveLeafCells for use by render_photo_copy detail overlay
                m_adaptiveLeafCells = QJsonArray();
                for (const auto& lc : leafCells) {
                    QJsonObject lcObj;
                    lcObj["px"] = lc.px; lcObj["py"] = lc.py;
                    lcObj["cw"] = lc.cw; lcObj["ch"] = lc.ch;
                    lcObj["wx"] = lc.wx; lcObj["wy"] = lc.wy;
                    lcObj["ww"] = lc.ww; lcObj["wh"] = lc.wh;
                    lcObj["variance"] = static_cast<double>(lc.variance);
                    m_adaptiveLeafCells.append(lcObj);
                }
                m_lastCommandResult = result;
            } else {
                // --- Legacy flat/gradient uniform grid mode ---
                int baseGrid = 40;
                if (quality == "low") baseGrid = 20;
                else if (quality == "high") baseGrid = 60;
                else if (quality == "ultra") baseGrid = 100;

                int gridX = params.contains("gridX") ? params["gridX"].toInt(baseGrid) : baseGrid;
                int gridY = params.contains("gridY") ? params["gridY"].toInt(baseGrid) : baseGrid;

                if (!params.contains("gridX") && !params.contains("gridY")) {
                    float aspect = static_cast<float>(img.width()) / static_cast<float>(img.height());
                    if (aspect > 1.0f) {
                        gridX = baseGrid;
                        gridY = qMax(1, static_cast<int>(baseGrid / aspect));
                    } else {
                        gridY = baseGrid;
                        gridX = qMax(1, static_cast<int>(baseGrid * aspect));
                    }
                }

                double cellW = static_cast<double>(img.width()) / gridX;
                double cellH = static_cast<double>(img.height()) / gridY;
                double worldCellW = static_cast<double>(worldSize.x()) / gridX;
                double worldCellH = static_cast<double>(worldSize.y()) / gridY;
                bool useGradient = (mode == "gradient");

                for (int row = 0; row < gridY; ++row) {
                    for (int col = 0; col < gridX; ++col) {
                        int px = static_cast<int>(col * cellW);
                        int py = static_cast<int>(row * cellH);
                        int cw = static_cast<int>(cellW);
                        int ch = static_cast<int>(cellH);
                        double wx1 = worldPos.x() + col * worldCellW;
                        double wy1 = worldPos.y() + row * worldCellH;
                        double wx2 = wx1 + worldCellW;
                        double wy2 = wy1 + worldCellH;

                        if (useGradient) {
                            auto rect = createGradientRect(px, py, cw, ch, wx1, wy1, wx2, wy2);
                            layer->addPrimitive(std::move(rect));
                        } else {
                            auto rect = std::make_unique<RectanglePrimitive>(
                                QVector2D(wx1, wy1), QVector2D(wx2, wy2));
                            rect->setFilled(true);
                            rect->setLineWidth(0);
                            QColor center = sampleArea(px + cw / 4, py + ch / 4, cw / 2, ch / 2);
                            rect->setFillColor(center);
                            layer->addPrimitive(std::move(rect));
                        }
                        primitivesCreated++;
                    }
                }

                m_canvas->update();
                QJsonObject result;
                result["primitivesCreated"] = primitivesCreated;
                result["gridX"] = gridX;
                result["gridY"] = gridY;
                result["mode"] = mode;
                m_lastCommandResult = result;
            }
        }
    }
    else if (action == "auto_trace") {
        int imageIndex = params["imageIndex"].toInt(0);
        int threshold = params["threshold"].toInt(50);
        double simplifyEpsilon = getDouble(params, "simplify", 2.0);
        int minLength = params["minLength"].toInt(20);
        int maxContours = params["maxContours"].toInt(200);
        bool render = getBool(params, "render", true);
        double lineWidth = getDouble(params, "lineWidth", 1.0);
        QString colorParam = params["color"].toString("auto");
        bool colorMatch = getBool(params, "colorMatch", true);
        bool variableWidth = getBool(params, "variableWidth", true);
        bool gaussianBlur = getBool(params, "gaussianBlur", true);
        QColor lineColor = (colorParam != "auto") ? parseColor(params, "color", QColor("#000000")) : QColor("#000000");
        bool useColorMatch = (colorParam == "auto" && colorMatch);
        double opacity = getDouble(params, "opacity", 0.8);

        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "auto_trace: image not found at index" << imageIndex;
        } else {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();
            int w = img.width();
            int h = img.height();

            // Step 1: Convert to grayscale
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);

            // Step 1b: Gaussian blur pre-filter (3x3 kernel)
            QImage blurred = gray;
            if (gaussianBlur && w > 4 && h > 4) {
                blurred = QImage(w, h, QImage::Format_Grayscale8);
                // Kernel: [1,2,1; 2,4,2; 1,2,1] / 16
                for (int y = 1; y < h - 1; ++y) {
                    const uchar* rA = gray.constScanLine(y - 1);
                    const uchar* rC = gray.constScanLine(y);
                    const uchar* rB = gray.constScanLine(y + 1);
                    uchar* dst = blurred.scanLine(y);
                    for (int x = 1; x < w - 1; ++x) {
                        int val = 1 * rA[x-1] + 2 * rA[x] + 1 * rA[x+1]
                                + 2 * rC[x-1] + 4 * rC[x] + 2 * rC[x+1]
                                + 1 * rB[x-1] + 2 * rB[x] + 1 * rB[x+1];
                        dst[x] = static_cast<uchar>(val / 16);
                    }
                }
                // Copy borders
                memcpy(blurred.scanLine(0), gray.constScanLine(0), w);
                memcpy(blurred.scanLine(h - 1), gray.constScanLine(h - 1), w);
                for (int y = 0; y < h; ++y) {
                    blurred.scanLine(y)[0] = gray.constScanLine(y)[0];
                    blurred.scanLine(y)[w - 1] = gray.constScanLine(y)[w - 1];
                }
            }

            // Step 2: Sobel edge detection with gradient direction
            std::vector<float> magnitude(w * h, 0.0f);
            std::vector<float> gradX(w * h, 0.0f);
            std::vector<float> gradY(w * h, 0.0f);
            for (int y = 1; y < h - 1; ++y) {
                const uchar* rowAbove = blurred.constScanLine(y - 1);
                const uchar* rowCurr  = blurred.constScanLine(y);
                const uchar* rowBelow = blurred.constScanLine(y + 1);
                for (int x = 1; x < w - 1; ++x) {
                    float gx = -1.0f * rowAbove[x-1] + 1.0f * rowAbove[x+1]
                              -2.0f * rowCurr[x-1]  + 2.0f * rowCurr[x+1]
                              -1.0f * rowBelow[x-1]  + 1.0f * rowBelow[x+1];
                    float gy = -1.0f * rowAbove[x-1] - 2.0f * rowAbove[x] - 1.0f * rowAbove[x+1]
                              +1.0f * rowBelow[x-1] + 2.0f * rowBelow[x] + 1.0f * rowBelow[x+1];
                    gradX[y * w + x] = gx;
                    gradY[y * w + x] = gy;
                    magnitude[y * w + x] = std::sqrt(gx * gx + gy * gy);
                }
            }

            // Step 2b: Non-maximum suppression
            std::vector<float> nms(w * h, 0.0f);
            for (int y = 1; y < h - 1; ++y) {
                for (int x = 1; x < w - 1; ++x) {
                    float mag = magnitude[y * w + x];
                    if (mag < 1e-6f) continue;

                    // Quantize gradient direction to 4 angles
                    float angle = std::atan2(gradY[y * w + x], gradX[y * w + x]);
                    // Normalize to [0, pi)
                    if (angle < 0) angle += static_cast<float>(M_PI);
                    // 0: horizontal (0°), 1: diagonal 45°, 2: vertical (90°), 3: diagonal 135°
                    int dir;
                    if (angle < M_PI / 8 || angle >= 7 * M_PI / 8) dir = 0;
                    else if (angle < 3 * M_PI / 8) dir = 1;
                    else if (angle < 5 * M_PI / 8) dir = 2;
                    else dir = 3;

                    float n1 = 0, n2 = 0;
                    switch (dir) {
                        case 0: // horizontal: check left/right
                            n1 = magnitude[y * w + (x - 1)];
                            n2 = magnitude[y * w + (x + 1)];
                            break;
                        case 1: // 45°: check top-right/bottom-left
                            n1 = magnitude[(y - 1) * w + (x + 1)];
                            n2 = magnitude[(y + 1) * w + (x - 1)];
                            break;
                        case 2: // vertical: check above/below
                            n1 = magnitude[(y - 1) * w + x];
                            n2 = magnitude[(y + 1) * w + x];
                            break;
                        case 3: // 135°: check top-left/bottom-right
                            n1 = magnitude[(y - 1) * w + (x - 1)];
                            n2 = magnitude[(y + 1) * w + (x + 1)];
                            break;
                    }
                    nms[y * w + x] = (mag >= n1 && mag >= n2) ? mag : 0.0f;
                }
            }

            // Find max magnitude for variable width scaling
            float maxMagnitude = 1.0f;
            for (int y = 1; y < h - 1; ++y) {
                for (int x = 1; x < w - 1; ++x) {
                    if (nms[y * w + x] > maxMagnitude) maxMagnitude = nms[y * w + x];
                }
            }

            // Step 3: Trace edge chains (8-connected) using NMS result
            std::vector<bool> visited(w * h, false);
            const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
            const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};

            struct ChainData {
                std::vector<QPointF> points;
                float avgMagnitude;
                QColor matchedColor;
            };
            std::vector<ChainData> chains;

            for (int y = 1; y < h - 1 && static_cast<int>(chains.size()) < maxContours * 2; ++y) {
                for (int x = 1; x < w - 1 && static_cast<int>(chains.size()) < maxContours * 2; ++x) {
                    if (visited[y * w + x] || nms[y * w + x] < threshold)
                        continue;

                    std::vector<QPointF> chain;
                    float magSum = 0;
                    int cx = x, cy = y;
                    int lastDir = -1;

                    while (true) {
                        visited[cy * w + cx] = true;
                        chain.push_back(QPointF(cx, cy));
                        magSum += nms[cy * w + cx];

                        int bestDir = -1;
                        float bestMag = 0;

                        auto tryDir = [&](int d) {
                            int nx = cx + dx[d];
                            int ny = cy + dy[d];
                            if (nx >= 1 && nx < w - 1 && ny >= 1 && ny < h - 1 &&
                                !visited[ny * w + nx] && nms[ny * w + nx] >= threshold) {
                                float mag = nms[ny * w + nx];
                                if (bestDir == -1 || mag > bestMag) {
                                    bestDir = d;
                                    bestMag = mag;
                                }
                            }
                        };

                        if (lastDir >= 0) {
                            tryDir(lastDir);
                            tryDir((lastDir + 1) % 8);
                            tryDir((lastDir + 7) % 8);
                            tryDir((lastDir + 2) % 8);
                            tryDir((lastDir + 6) % 8);
                            tryDir((lastDir + 3) % 8);
                            tryDir((lastDir + 5) % 8);
                        } else {
                            for (int d = 0; d < 8; ++d) tryDir(d);
                        }

                        if (bestDir == -1) break;
                        cx += dx[bestDir];
                        cy += dy[bestDir];
                        lastDir = bestDir;
                    }

                    if (static_cast<int>(chain.size()) >= minLength) {
                        ChainData cd;
                        cd.points = std::move(chain);
                        cd.avgMagnitude = magSum / static_cast<float>(cd.points.size());

                        // Color matching: sample both sides of each edge point, pick darker side
                        if (useColorMatch) {
                            long rSum = 0, gSum = 0, bSum = 0;
                            int count = 0;
                            int sampleStep = qMax(1, static_cast<int>(cd.points.size()) / 20);
                            for (size_t pi = 0; pi < cd.points.size(); pi += sampleStep) {
                                int epx = static_cast<int>(cd.points[pi].x());
                                int epy = static_cast<int>(cd.points[pi].y());
                                // Get gradient direction at this point for perpendicular offset
                                float gxv = gradX[epy * w + epx];
                                float gyv = gradY[epy * w + epx];
                                float gmag = std::sqrt(gxv * gxv + gyv * gyv);
                                if (gmag < 1e-6f) continue;
                                // Perpendicular: rotate 90 degrees
                                float perpX = -gyv / gmag;
                                float perpY = gxv / gmag;
                                int off = 2;
                                // Sample both sides
                                int s1x = qBound(0, epx + static_cast<int>(perpX * off), w - 1);
                                int s1y = qBound(0, epy + static_cast<int>(perpY * off), h - 1);
                                int s2x = qBound(0, epx - static_cast<int>(perpX * off), w - 1);
                                int s2y = qBound(0, epy - static_cast<int>(perpY * off), h - 1);
                                const QRgb* scanLine1 = reinterpret_cast<const QRgb*>(img.constScanLine(s1y));
                                const QRgb* scanLine2 = reinterpret_cast<const QRgb*>(img.constScanLine(s2y));
                                QRgb p1 = scanLine1[s1x];
                                QRgb p2 = scanLine2[s2x];
                                // Pick darker side (lower luminance)
                                int lum1 = qRed(p1) * 299 + qGreen(p1) * 587 + qBlue(p1) * 114;
                                int lum2 = qRed(p2) * 299 + qGreen(p2) * 587 + qBlue(p2) * 114;
                                QRgb chosen = (lum1 < lum2) ? p1 : p2;
                                rSum += qRed(chosen);
                                gSum += qGreen(chosen);
                                bSum += qBlue(chosen);
                                count++;
                            }
                            if (count > 0) {
                                cd.matchedColor = QColor(rSum / count, gSum / count, bSum / count);
                            } else {
                                cd.matchedColor = lineColor;
                            }
                        } else {
                            cd.matchedColor = lineColor;
                        }

                        chains.push_back(std::move(cd));
                    }
                }
            }

            // Step 4: Douglas-Peucker simplification
            std::function<void(const std::vector<QPointF>&, int, int, double, std::vector<bool>&)> dpSimplify;
            dpSimplify = [&dpSimplify](const std::vector<QPointF>& pts, int start, int end, double epsilon, std::vector<bool>& keep) {
                if (end <= start + 1) return;
                double maxDist = 0;
                int maxIdx = start;
                QPointF lineStart = pts[start];
                QPointF lineEnd = pts[end];
                double lineLen = std::sqrt(std::pow(lineEnd.x() - lineStart.x(), 2) + std::pow(lineEnd.y() - lineStart.y(), 2));
                for (int i = start + 1; i < end; ++i) {
                    double dist;
                    if (lineLen < 1e-6) {
                        dist = std::sqrt(std::pow(pts[i].x() - lineStart.x(), 2) + std::pow(pts[i].y() - lineStart.y(), 2));
                    } else {
                        double t = ((pts[i].x() - lineStart.x()) * (lineEnd.x() - lineStart.x()) +
                                    (pts[i].y() - lineStart.y()) * (lineEnd.y() - lineStart.y())) / (lineLen * lineLen);
                        t = qBound(0.0, t, 1.0);
                        double projX = lineStart.x() + t * (lineEnd.x() - lineStart.x());
                        double projY = lineStart.y() + t * (lineEnd.y() - lineStart.y());
                        dist = std::sqrt(std::pow(pts[i].x() - projX, 2) + std::pow(pts[i].y() - projY, 2));
                    }
                    if (dist > maxDist) {
                        maxDist = dist;
                        maxIdx = i;
                    }
                }
                if (maxDist > epsilon) {
                    keep[maxIdx] = true;
                    dpSimplify(pts, start, maxIdx, epsilon, keep);
                    dpSimplify(pts, maxIdx, end, epsilon, keep);
                }
            };

            // Simplify all chains
            struct SimplifiedChain {
                std::vector<QPointF> points;
                float avgMagnitude;
                QColor matchedColor;
            };
            std::vector<SimplifiedChain> simplified;
            for (auto& cd : chains) {
                int n = static_cast<int>(cd.points.size());
                if (n < 2) continue;
                std::vector<bool> keep(n, false);
                keep[0] = true;
                keep[n - 1] = true;
                dpSimplify(cd.points, 0, n - 1, simplifyEpsilon, keep);
                std::vector<QPointF> result;
                for (int i = 0; i < n; ++i) {
                    if (keep[i]) result.push_back(cd.points[i]);
                }
                if (result.size() >= 2) {
                    SimplifiedChain sc;
                    sc.points = std::move(result);
                    sc.avgMagnitude = cd.avgMagnitude;
                    sc.matchedColor = cd.matchedColor;
                    simplified.push_back(std::move(sc));
                }
            }

            // Sort by length (longest first) and limit to maxContours
            std::sort(simplified.begin(), simplified.end(),
                [](const SimplifiedChain& a, const SimplifiedChain& b) {
                    return a.points.size() > b.points.size();
                });
            if (static_cast<int>(simplified.size()) > maxContours) {
                simplified.resize(maxContours);
            }

            // Step 5: Render as open PolygonPrimitives with variable width
            int contoursRendered = 0;
            double scaleX = static_cast<double>(worldSize.x()) / w;
            double scaleY = static_cast<double>(worldSize.y()) / h;

            if (render) {
                saveUndoState("Auto Trace");
                Layer* layer = m_canvas->layerManager()->activeLayer();

                for (const auto& sc : simplified) {
                    if (variableWidth) {
                        // Split chain into 3 width buckets based on magnitude
                        float relMag = sc.avgMagnitude / maxMagnitude;
                        double w_thin   = lineWidth * 0.5;
                        double w_medium = lineWidth * 0.75;
                        double w_thick  = lineWidth * 1.0;
                        double chosenWidth;
                        if (relMag < 0.33f) chosenWidth = w_thin;
                        else if (relMag < 0.66f) chosenWidth = w_medium;
                        else chosenWidth = w_thick;

                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(false);
                        poly->setFilled(false);
                        poly->setColor(sc.matchedColor);
                        poly->setLineWidth(chosenWidth);
                        poly->setOpacityMultiplier(opacity);

                        for (const auto& pt : sc.points) {
                            double wx = worldPos.x() + pt.x() * scaleX;
                            double wy = worldPos.y() + pt.y() * scaleY;
                            poly->addPoint(QVector2D(wx, wy));
                        }
                        layer->addPrimitive(std::move(poly));
                    } else {
                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(false);
                        poly->setFilled(false);
                        poly->setColor(sc.matchedColor);
                        poly->setLineWidth(lineWidth);
                        poly->setOpacityMultiplier(opacity);

                        for (const auto& pt : sc.points) {
                            double wx = worldPos.x() + pt.x() * scaleX;
                            double wy = worldPos.y() + pt.y() * scaleY;
                            poly->addPoint(QVector2D(wx, wy));
                        }
                        layer->addPrimitive(std::move(poly));
                    }
                    contoursRendered++;
                }
                m_canvas->update();
            }

            // Build result
            QJsonObject result;
            result["contoursFound"] = static_cast<int>(simplified.size());
            result["contoursRendered"] = contoursRendered;

            QJsonArray contoursArr;
            for (const auto& sc : simplified) {
                QJsonObject contourObj;
                QJsonArray pointsArr;
                for (const auto& pt : sc.points) {
                    QJsonObject ptObj;
                    ptObj["x"] = worldPos.x() + pt.x() * scaleX;
                    ptObj["y"] = worldPos.y() + pt.y() * scaleY;
                    pointsArr.append(ptObj);
                }
                contourObj["points"] = pointsArr;
                contourObj["pointCount"] = static_cast<int>(sc.points.size());
                contoursArr.append(contourObj);
            }
            result["contours"] = contoursArr;
            m_lastCommandResult = result;
        }
    }
    else if (action == "render_photo_copy") {
        int imageIndex = params["imageIndex"].toInt(0);
        QString quality = params["quality"].toString("high");
        bool removeImage = getBool(params, "removeImage", true);
        QString style = params["style"].toString("photorealistic");

        ImagePrimitive* imgPrim = findImageByIndex(imageIndex);
        if (!imgPrim) {
            qWarning() << "render_photo_copy: image not found at index" << imageIndex;
        } else if (style == "sketch") {
            // ===================== PENCIL SKETCH STYLE =====================
            // Creates hatching/cross-hatching based on luminance — looks like a real pencil drawing
            QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();
            int imgW = srcImg.width();
            int imgH = srcImg.height();
            double scaleX = static_cast<double>(worldSize.x()) / imgW;
            double scaleY = static_cast<double>(worldSize.y()) / imgH;

            // Build luminance map
            std::vector<uchar> lum(imgW * imgH);
            for (int y = 0; y < imgH; ++y) {
                const QRgb* row = reinterpret_cast<const QRgb*>(srcImg.constScanLine(y));
                for (int x = 0; x < imgW; ++x) {
                    QRgb p = row[x];
                    lum[y * imgW + x] = static_cast<uchar>((qRed(p) * 299 + qGreen(p) * 587 + qBlue(p) * 114) / 1000);
                }
            }

            saveUndoState("Render Sketch");
            Layer* layer = m_canvas->layerManager()->activeLayer();
            int primCount = 0;

            // Paper background
            auto bg = std::make_unique<RectanglePrimitive>(
                QVector2D(worldPos.x(), worldPos.y()),
                QVector2D(worldPos.x() + worldSize.x(), worldPos.y() + worldSize.y()));
            bg->setFilled(true);
            bg->setFillColor(QColor(252, 250, 245));
            bg->setLineWidth(0);
            layer->addPrimitive(std::move(bg));
            primCount++;

            // Hatching spacing based on quality
            int sp = 4;
            if (quality == "low") sp = 7;
            else if (quality == "medium") sp = 5;
            else if (quality == "high") sp = 3;
            else if (quality == "ultra") sp = 2;

            QColor pencil(35, 30, 25);

            // Lambda: 45° hatching (lines where x - y = k)
            auto hatch45 = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
                for (int k = -(imgH - 1) + offset; k < imgW; k += spacing) {
                    int xS = qMax(0, k);
                    int xE = qMin(imgW - 1, k + imgH - 1);
                    bool inSeg = false;
                    int sx0 = 0;
                    for (int x = xS; x <= xE; ++x) {
                        int y = x - k;
                        if (lum[y * imgW + x] < lumThresh) {
                            if (!inSeg) { sx0 = x; inSeg = true; }
                        } else {
                            if (inSeg && (x - sx0) >= 3) {
                                auto poly = std::make_unique<PolygonPrimitive>();
                                poly->setClosed(false); poly->setFilled(false);
                                poly->setColor(pencil); poly->setLineWidth(lw);
                                poly->setOpacityMultiplier(opa);
                                poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (sx0 - k) * scaleY));
                                poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + (x - 1 - k) * scaleY));
                                layer->addPrimitive(std::move(poly));
                                primCount++;
                            }
                            inSeg = false;
                        }
                    }
                    if (inSeg && (xE - sx0) >= 3) {
                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(false); poly->setFilled(false);
                        poly->setColor(pencil); poly->setLineWidth(lw);
                        poly->setOpacityMultiplier(opa);
                        poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (sx0 - k) * scaleY));
                        poly->addPoint(QVector2D(worldPos.x() + xE * scaleX, worldPos.y() + (xE - k) * scaleY));
                        layer->addPrimitive(std::move(poly));
                        primCount++;
                    }
                }
            };

            // Lambda: 135° hatching (lines where x + y = k)
            auto hatch135 = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
                for (int k = offset; k < imgW + imgH - 1; k += spacing) {
                    int xS = qMax(0, k - imgH + 1);
                    int xE = qMin(imgW - 1, k);
                    bool inSeg = false;
                    int sx0 = 0;
                    for (int x = xS; x <= xE; ++x) {
                        int y = k - x;
                        if (lum[y * imgW + x] < lumThresh) {
                            if (!inSeg) { sx0 = x; inSeg = true; }
                        } else {
                            if (inSeg && (x - sx0) >= 3) {
                                auto poly = std::make_unique<PolygonPrimitive>();
                                poly->setClosed(false); poly->setFilled(false);
                                poly->setColor(pencil); poly->setLineWidth(lw);
                                poly->setOpacityMultiplier(opa);
                                poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (k - sx0) * scaleY));
                                poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + (k - x + 1) * scaleY));
                                layer->addPrimitive(std::move(poly));
                                primCount++;
                            }
                            inSeg = false;
                        }
                    }
                    if (inSeg && (xE - sx0) >= 3) {
                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(false); poly->setFilled(false);
                        poly->setColor(pencil); poly->setLineWidth(lw);
                        poly->setOpacityMultiplier(opa);
                        poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + (k - sx0) * scaleY));
                        poly->addPoint(QVector2D(worldPos.x() + xE * scaleX, worldPos.y() + (k - xE) * scaleY));
                        layer->addPrimitive(std::move(poly));
                        primCount++;
                    }
                }
            };

            // Lambda: horizontal hatching (y = const)
            auto hatchH = [&](int spacing, int lumThresh, double lw, float opa, int offset = 0) {
                for (int y = offset; y < imgH; y += spacing) {
                    bool inSeg = false;
                    int sx0 = 0;
                    for (int x = 0; x < imgW; ++x) {
                        if (lum[y * imgW + x] < lumThresh) {
                            if (!inSeg) { sx0 = x; inSeg = true; }
                        } else {
                            if (inSeg && (x - sx0) >= 3) {
                                auto poly = std::make_unique<PolygonPrimitive>();
                                poly->setClosed(false); poly->setFilled(false);
                                poly->setColor(pencil); poly->setLineWidth(lw);
                                poly->setOpacityMultiplier(opa);
                                poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + y * scaleY));
                                poly->addPoint(QVector2D(worldPos.x() + (x - 1) * scaleX, worldPos.y() + y * scaleY));
                                layer->addPrimitive(std::move(poly));
                                primCount++;
                            }
                            inSeg = false;
                        }
                    }
                    if (inSeg && (imgW - 1 - sx0) >= 3) {
                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(false); poly->setFilled(false);
                        poly->setColor(pencil); poly->setLineWidth(lw);
                        poly->setOpacityMultiplier(opa);
                        poly->addPoint(QVector2D(worldPos.x() + sx0 * scaleX, worldPos.y() + y * scaleY));
                        poly->addPoint(QVector2D(worldPos.x() + (imgW - 1) * scaleX, worldPos.y() + y * scaleY));
                        layer->addPrimitive(std::move(poly));
                        primCount++;
                    }
                }
            };

            // Hatching passes — graduated tone through multiple threshold layers
            // Pass 1: light hatching 45° (shadows begin)
            hatch45(sp, 210, 0.35, 0.45f);
            // Pass 2: medium hatching 45° (mid-tones)
            hatch45(sp, 160, 0.45, 0.55f, sp / 2);
            // Pass 3: cross-hatching 135° (darker areas)
            hatch135(sp, 140, 0.35, 0.5f);
            // Pass 4: dense cross-hatching 135° (deep shadows)
            hatch135(sp, 90, 0.45, 0.6f, sp / 2);
            // Pass 5: horizontal for extra density in very dark
            hatchH(sp, 70, 0.3, 0.4f);
            // Pass 6: extra 45° fill for near-black
            hatch45(qMax(1, sp - 1), 50, 0.5, 0.7f, sp / 3);

            // Edge contours for structural definition
            QJsonObject traceParams;
            traceParams["imageIndex"] = imageIndex;
            traceParams["threshold"] = 30;
            traceParams["simplify"] = 1.5;
            traceParams["minLength"] = 8;
            traceParams["render"] = true;
            traceParams["lineWidth"] = 0.7;
            traceParams["color"] = "#28231E";
            traceParams["colorMatch"] = false;
            traceParams["variableWidth"] = true;
            traceParams["gaussianBlur"] = true;
            traceParams["opacity"] = 0.75;
            traceParams["maxContours"] = 1500;
            executeDrawingCommand("auto_trace", traceParams);
            QJsonObject traceResult = m_lastCommandResult;

            bool imageRemoved2 = false;
            if (removeImage) {
                for (const auto& ly : m_canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
                imageRemoved2 = true;
                m_canvas->update();
            }

            QJsonObject result;
            result["style"] = QString("sketch");
            result["hatchPrimitives"] = primCount;
            result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
            result["quality"] = quality;
            result["imageRemoved"] = imageRemoved2;
            m_lastCommandResult = result;

        } else if (style == "painterly") {
            // ===================== PAINTERLY / IMPRESSIONIST STYLE =====================
            // Visible oriented brush strokes that follow the image's color contours
            QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();
            int imgW = srcImg.width();
            int imgH = srcImg.height();
            double scaleX = static_cast<double>(worldSize.x()) / imgW;
            double scaleY = static_cast<double>(worldSize.y()) / imgH;

            // Build grayscale for gradient direction
            QImage gray = srcImg.convertToFormat(QImage::Format_Grayscale8);
            // Compute Sobel gradient for stroke orientation
            std::vector<float> gxArr(imgW * imgH, 0.0f);
            std::vector<float> gyArr(imgW * imgH, 0.0f);
            for (int y = 1; y < imgH - 1; ++y) {
                const uchar* rA = gray.constScanLine(y - 1);
                const uchar* rC = gray.constScanLine(y);
                const uchar* rB = gray.constScanLine(y + 1);
                for (int x = 1; x < imgW - 1; ++x) {
                    gxArr[y * imgW + x] = -1.0f * rA[x-1] + rA[x+1] - 2.0f * rC[x-1] + 2.0f * rC[x+1] - rB[x-1] + rB[x+1];
                    gyArr[y * imgW + x] = -1.0f * rA[x-1] - 2.0f * rA[x] - rA[x+1] + rB[x-1] + 2.0f * rB[x] + rB[x+1];
                }
            }

            saveUndoState("Render Painterly");
            Layer* layer = m_canvas->layerManager()->activeLayer();
            int strokeCount = 0;

            // Canvas background — warm off-white
            auto bg = std::make_unique<RectanglePrimitive>(
                QVector2D(worldPos.x(), worldPos.y()),
                QVector2D(worldPos.x() + worldSize.x(), worldPos.y() + worldSize.y()));
            bg->setFilled(true);
            bg->setFillColor(QColor(245, 242, 235));
            bg->setLineWidth(0);
            layer->addPrimitive(std::move(bg));

            // Multi-pass brush strokes: large → medium → small
            struct StrokePass {
                int step;       // pixel sampling step
                float major;    // major axis scale (world units factor of step*scaleX)
                float minor;    // minor axis scale
                float opacity;
            };

            std::vector<StrokePass> passes;
            if (quality == "low") {
                passes = {{12, 1.4f, 0.5f, 0.85f}, {6, 1.0f, 0.4f, 0.7f}};
            } else if (quality == "medium") {
                passes = {{8, 1.4f, 0.5f, 0.85f}, {4, 1.0f, 0.4f, 0.7f}, {2, 0.7f, 0.3f, 0.6f}};
            } else if (quality == "high") {
                passes = {{6, 1.3f, 0.45f, 0.85f}, {3, 1.0f, 0.35f, 0.75f}, {2, 0.7f, 0.25f, 0.6f}};
            } else { // ultra
                passes = {{8, 1.5f, 0.5f, 0.9f}, {4, 1.1f, 0.4f, 0.8f}, {2, 0.75f, 0.3f, 0.65f}};
            }

            for (const auto& pass : passes) {
                for (int iy = pass.step / 2; iy < imgH; iy += pass.step) {
                    const QRgb* scanLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(iy));
                    for (int ix = pass.step / 2; ix < imgW; ix += pass.step) {
                        QRgb pixel = scanLine[ix];
                        QColor color(qRed(pixel), qGreen(pixel), qBlue(pixel));

                        // Gradient direction → stroke perpendicular to edge (along the form)
                        float gx = gxArr[iy * imgW + ix];
                        float gy = gyArr[iy * imgW + ix];
                        float gmag = std::sqrt(gx * gx + gy * gy);
                        float angle;
                        if (gmag > 5.0f) {
                            angle = std::atan2(gy, gx) + static_cast<float>(M_PI) / 2.0f; // perpendicular
                        } else {
                            // In flat areas, use a pseudo-random angle based on position
                            angle = static_cast<float>((ix * 7 + iy * 13) % 628) / 100.0f;
                        }

                        float cos_a = std::cos(angle);
                        float sin_a = std::sin(angle);
                        float mx = pass.major * pass.step * static_cast<float>(scaleX);
                        float my = pass.minor * pass.step * static_cast<float>(scaleY);

                        double cx = worldPos.x() + ix * scaleX;
                        double cy = worldPos.y() + iy * scaleY;

                        // Rotated rectangle as 4-point filled polygon (brush stroke)
                        QVector2D p1(cx + mx * cos_a - my * sin_a, cy + mx * sin_a + my * cos_a);
                        QVector2D p2(cx - mx * cos_a - my * sin_a, cy - mx * sin_a + my * cos_a);
                        QVector2D p3(cx - mx * cos_a + my * sin_a, cy - mx * sin_a - my * cos_a);
                        QVector2D p4(cx + mx * cos_a + my * sin_a, cy + mx * sin_a - my * cos_a);

                        auto poly = std::make_unique<PolygonPrimitive>();
                        poly->setClosed(true);
                        poly->setFilled(true);
                        poly->setFillColor(color);
                        poly->setLineWidth(0);
                        poly->setOpacityMultiplier(pass.opacity);
                        poly->addPoint(p1);
                        poly->addPoint(p2);
                        poly->addPoint(p3);
                        poly->addPoint(p4);
                        layer->addPrimitive(std::move(poly));
                        strokeCount++;
                    }
                }
            }

            // Subtle edge contours
            QJsonObject traceParams;
            traceParams["imageIndex"] = imageIndex;
            traceParams["threshold"] = 35;
            traceParams["simplify"] = 2.0;
            traceParams["minLength"] = 12;
            traceParams["render"] = true;
            traceParams["lineWidth"] = 0.6;
            traceParams["color"] = "auto";
            traceParams["colorMatch"] = true;
            traceParams["variableWidth"] = true;
            traceParams["gaussianBlur"] = true;
            traceParams["opacity"] = 0.4;
            traceParams["maxContours"] = 1000;
            executeDrawingCommand("auto_trace", traceParams);
            QJsonObject traceResult = m_lastCommandResult;

            bool imageRemoved2 = false;
            if (removeImage) {
                for (const auto& ly : m_canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
                imageRemoved2 = true;
                m_canvas->update();
            }

            QJsonObject result;
            result["style"] = QString("painterly");
            result["strokeCount"] = strokeCount;
            result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
            result["quality"] = quality;
            result["imageRemoved"] = imageRemoved2;
            m_lastCommandResult = result;

        } else {
            // ===================== PHOTOREALISTIC STYLE (original) =====================
            bool traceEdges = getBool(params, "traceEdges", true);
            bool detailOverlay = getBool(params, "detailOverlay", true);
            QImage srcImg = imgPrim->image().convertToFormat(QImage::Format_ARGB32).flipped(Qt::Vertical);
            QVector2D worldPos = imgPrim->position();
            QVector2D worldSize = imgPrim->size();

            QJsonObject mosaicParams;
            mosaicParams["imageIndex"] = imageIndex;
            mosaicParams["quality"] = quality;
            mosaicParams["mode"] = "adaptive";
            executeDrawingCommand("render_mosaic", mosaicParams);
            QJsonObject mosaicResult = m_lastCommandResult;

            QJsonObject traceResult;
            if (traceEdges) {
                QJsonObject traceParams;
                traceParams["imageIndex"] = imageIndex;
                traceParams["threshold"] = 25;
                traceParams["simplify"] = 1.5;
                traceParams["minLength"] = 10;
                traceParams["render"] = true;
                traceParams["lineWidth"] = 0.8;
                traceParams["color"] = "auto";
                traceParams["colorMatch"] = true;
                traceParams["variableWidth"] = true;
                traceParams["gaussianBlur"] = true;
                traceParams["opacity"] = 0.6;
                traceParams["maxContours"] = 2000;
                executeDrawingCommand("auto_trace", traceParams);
                traceResult = m_lastCommandResult;
            }

            int overlayCount = 0;
            if (detailOverlay) {
                Layer* layer = m_canvas->layerManager()->activeLayer();
                int imgW = srcImg.width();
                int imgH = srcImg.height();
                double scaleX = static_cast<double>(worldSize.x()) / imgW;
                double scaleY = static_cast<double>(worldSize.y()) / imgH;
                int sampleStep = 4;
                float strokeOpacity = 0.5f;
                if (quality == "low")         { sampleStep = 8;  strokeOpacity = 0.4f; }
                else if (quality == "medium")  { sampleStep = 4;  strokeOpacity = 0.5f; }
                else if (quality == "high")    { sampleStep = 3;  strokeOpacity = 0.55f; }
                else if (quality == "ultra")   { sampleStep = 2;  strokeOpacity = 0.6f; }
                float rx = static_cast<float>(sampleStep * scaleX * 0.7);
                float ry = static_cast<float>(sampleStep * scaleY * 0.7);
                for (int iy = sampleStep / 2; iy < imgH; iy += sampleStep) {
                    const QRgb* scanLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(iy));
                    for (int ix = sampleStep / 2; ix < imgW; ix += sampleStep) {
                        QRgb pixel = scanLine[ix];
                        QColor color(qRed(pixel), qGreen(pixel), qBlue(pixel));
                        double wx = worldPos.x() + ix * scaleX;
                        double wy = worldPos.y() + iy * scaleY;
                        auto ellipse = std::make_unique<EllipsePrimitive>(QVector2D(wx, wy), rx, ry);
                        ellipse->setFilled(true);
                        ellipse->setFillColor(color);
                        ellipse->setLineWidth(0);
                        ellipse->setOpacityMultiplier(strokeOpacity);
                        layer->addPrimitive(std::move(ellipse));
                        overlayCount++;
                    }
                }
            }

            bool imageRemoved2 = false;
            if (removeImage) {
                for (const auto& ly : m_canvas->layerManager()->layers()) { ly->removePrimitive(imgPrim); }
                imageRemoved2 = true;
                m_canvas->update();
            }

            QJsonObject result;
            result["style"] = QString("photorealistic");
            result["mosaicPrimitives"] = mosaicResult["primitivesCreated"].toInt(0);
            result["edgeContours"] = traceResult["contoursRendered"].toInt(0);
            result["detailOverlayCount"] = overlayCount;
            result["quality"] = quality;
            result["imageRemoved"] = imageRemoved2;
            m_lastCommandResult = result;
        }
    }
    else {
        qWarning() << "CommandServer: Unknown action:" << action;
        QJsonObject result;
        result["success"] = false;
        result["error"] = QStringLiteral("Unknown action: %1").arg(action);
        m_lastCommandResult = result;
        return;
    }

    m_canvas->update();
}

ImagePrimitive* MainWindow::findImageByIndex(int index) {
    auto allPrims = m_canvas->layerManager()->getAllPrimitives();
    std::vector<ImagePrimitive*> images;
    for (auto* p : allPrims) {
        if (auto* img = dynamic_cast<ImagePrimitive*>(p))
            images.push_back(img);
    }
    if (images.empty()) return nullptr;
    // Negative index: count from end (-1 = last)
    int resolved = (index < 0) ? static_cast<int>(images.size()) + index : index;
    if (resolved < 0 || resolved >= static_cast<int>(images.size())) return nullptr;
    return images[resolved];
}

ImagePrimitive* MainWindow::imageForDetection() {
    if (!m_canvas || !m_canvas->layerManager()) return nullptr;

    for (auto *obj : m_canvas->selectedObjects()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
            return ip;
        }
    }

    ImagePrimitive *sole = nullptr;
    int count = 0;
    for (auto *p : m_canvas->layerManager()->getAllPrimitives()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
            sole = ip;
            if (++count > 1) {
                return nullptr;
            }
        }
    }
    return count == 1 ? sole : nullptr;
}

void MainWindow::selectImageForMaskUI(ImagePrimitive *image) {
    if (!m_canvas || !image) return;
    // Keep other selected images — accumulate green-masked subjects
    image->setSelected(true);
    image->setMaskOverlayVisible(true);
    m_canvas->addToSelection(image);
    m_canvas->update();
}

ImagePrimitive* MainWindow::selectedImageWithMasks() {
    if (!m_canvas) return nullptr;

    ImagePrimitive *imgPrim = nullptr;
    for (auto *obj : m_canvas->selectedObjects()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
            imgPrim = ip;
            break;
        }
    }

    if ((!imgPrim || imgPrim->getMaskCandidateCount() == 0) &&
        m_canvas->layerManager()) {
        for (auto *p : m_canvas->layerManager()->getAllPrimitives()) {
            if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
                if (ip->getMaskCandidateCount() > 0) {
                    imgPrim = ip;
                    break;
                }
            }
        }
    }

    return (imgPrim && imgPrim->getMaskCandidateCount() > 0) ? imgPrim : nullptr;
}

// Masks
void MainWindow::selectNextMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No masks detected. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();
    int next = (current + 1) % count;
    if (m_commandManager) {
        m_commandManager->executeCommand(
            std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, next));
    } else {
        imgPrim->selectMaskCandidate(next);
    }
    if (m_statusLabel) {
        m_statusLabel->setText(QString("Mask %1 of %2").arg(next + 1).arg(count));
    }
    updateMaskSelectionUI();
    updateFloatingMaskPanel();
    m_canvas->update();
}

void MainWindow::selectPreviousMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No masks detected. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();
    int prev = current - 1;
    if (prev < 0) prev = count - 1;
    if (m_commandManager) {
        m_commandManager->executeCommand(
            std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, prev));
    } else {
        imgPrim->selectMaskCandidate(prev);
    }
    if (m_statusLabel) {
        m_statusLabel->setText(QString("Mask %1 of %2").arg(prev + 1).arg(count));
    }
    updateMaskSelectionUI();
    updateFloatingMaskPanel();
    m_canvas->update();
}

void MainWindow::invertSelectedMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No mask to invert. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    imgPrim->invertMask();
    m_canvas->update();
    updateFloatingMaskPanel();
    if (m_statusLabel) {
        m_statusLabel->setText("Mask inverted");
    }
}

void MainWindow::updateMaskSelectionUI() {
    if (!m_canvas) return;
    ImagePrimitive* imgPrim = nullptr;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* ip = dynamic_cast<ImagePrimitive*>(obj)) {
            imgPrim = ip;
            break;
        }
    }

    bool hasMasks = imgPrim && imgPrim->getMaskCandidateCount() > 0;

    // The floating mask panel is the real UI; here we just keep the status bar
    // in sync with the current selection.
    if (hasMasks && m_statusLabel) {
        int count = imgPrim->getMaskCandidateCount();
        int current = imgPrim->getSelectedMaskIndex();
        m_statusLabel->setText(QString("Mask %1 of %2").arg(current + 1).arg(count));
    }
}

void MainWindow::onMaskSelectionChanged(int index) {
    if (!m_canvas) return;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            int current = imgPrim->getSelectedMaskIndex();
            if (index >= 0 && index < imgPrim->getMaskCandidateCount() &&
                index != current) {
                // Skip no-op selections (e.g. slider programmatically synced to
                // the current index) so they don't pollute the undo stack.
                if (m_commandManager) {
                    m_commandManager->executeCommand(
                        std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, index));
                } else {
                    imgPrim->selectMaskCandidate(index);
                }
                updateMaskSelectionUI();
                m_canvas->update();
            }
            return;
        }
    }
}


bool MainWindow::ensureRemoteSDHelper() {
    if (!m_remoteSDHelper) {
        m_remoteSDHelper = new RemoteSDHelper(this);
        connect(m_remoteSDHelper, &RemoteSDHelper::imageGenerated,
                this, &MainWindow::onImageGenerated);
        connect(m_remoteSDHelper, &RemoteSDHelper::errorOccurred, this, [this](const QString& err) {
            if (m_statusLabel) m_statusLabel->setText("Stable Diffusion error: " + err);
        });
    }
    // initialize() probes the server (5s timeout); only succeeds when reachable.
    if (!m_remoteSDHelper->isInitialized()) {
        QSettings settings;
        QString url = settings.value("AI/remoteSDUrl", "http://192.168.1.58:8000").toString();
        m_remoteSDHelper->initialize(url);
    }
    return m_remoteSDHelper->isInitialized();
}


// Text
void MainWindow::commitTextPropertyEdits(
    const QString &description,
    const std::function<void(TextPrimitive *)> &mutate)
{
    if (!m_canvas || !mutate)
        return;

    std::vector<TextPrimitive *> texts;
    for (auto *obj : m_canvas->selectedObjects()) {
        if (auto *tp = dynamic_cast<TextPrimitive *>(obj))
            texts.push_back(tp);
    }
    if (texts.empty()) {
        if (m_statusLabel)
            m_statusLabel->setText(QStringLiteral("Select a text object first."));
        return;
    }

    auto compound = std::make_unique<CompoundCommand>(description);
    for (TextPrimitive *tp : texts) {
        QJsonObject oldState = tp->toJson();
        mutate(tp);
        QJsonObject newState = tp->toJson();
        if (oldState == newState)
            continue;
        auto cmd = std::make_unique<EditTextCommand>(tp, description);
        cmd->storeOldState(oldState);
        cmd->storeNewState(newState);
        cmd->markAlreadyApplied();
        compound->addCommand(std::move(cmd));
    }

    if (!compound->isEmpty() && m_commandManager) {
        compound->markAlreadyApplied();
        m_commandManager->addCommandWithoutExecuting(std::move(compound));
        syncModifiedFlag();
    }
    m_canvas->update();
}

void MainWindow::setTextAlignLeft() {
    commitTextPropertyEdits(QStringLiteral("Align Text Left"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Left);
    });
    if (m_statusLabel && m_canvas && !m_canvas->selectedObjects().empty())
        m_statusLabel->setText(QStringLiteral("Text alignment: Left"));
}

void MainWindow::setTextAlignRight() {
    commitTextPropertyEdits(QStringLiteral("Align Text Right"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Right);
    });
    if (m_statusLabel && m_canvas && !m_canvas->selectedObjects().empty())
        m_statusLabel->setText(QStringLiteral("Text alignment: Right"));
}

void MainWindow::setTextAlignCenter() {
    commitTextPropertyEdits(QStringLiteral("Align Text Center"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Center);
    });
    if (m_statusLabel && m_canvas && !m_canvas->selectedObjects().empty())
        m_statusLabel->setText(QStringLiteral("Text alignment: Center"));
}

void MainWindow::setTextAlignJustify() {
    commitTextPropertyEdits(QStringLiteral("Align Text Justify"), [](TextPrimitive *tp) {
        tp->setAlignment(TextPrimitive::TextAlignment::Justify);
    });
    if (m_statusLabel && m_canvas && !m_canvas->selectedObjects().empty())
        m_statusLabel->setText(QStringLiteral("Text alignment: Justify"));
}


void MainWindow::showShadowDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Text Shadow");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->shadowEnabled());
    QDoubleSpinBox *offsetX = new QDoubleSpinBox(); offsetX->setRange(-50, 50); offsetX->setValue(textPrim->shadowOffsetX());
    QDoubleSpinBox *offsetY = new QDoubleSpinBox(); offsetY->setRange(-50, 50); offsetY->setValue(textPrim->shadowOffsetY());
    QDoubleSpinBox *blur = new QDoubleSpinBox(); blur->setRange(0, 50); blur->setValue(textPrim->shadowBlur());
    QPushButton *colorBtn = new QPushButton();
    QColor shadowColor = textPrim->shadowColor();
    colorBtn->setFixedSize(48, 24);
    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(shadowColor.name()));
    connect(colorBtn, &QPushButton::clicked, &dialog, [colorBtn, &shadowColor, &dialog]() {
        QColor c = QColorDialog::getColor(shadowColor, &dialog, "Shadow Color");
        if (c.isValid()) {
            shadowColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Color:", colorBtn);
    form->addRow("Offset X:", offsetX);
    form->addRow("Offset Y:", offsetY);
    form->addRow("Blur:", blur);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double ox = offsetX->value();
        const double oy = offsetY->value();
        const double bl = blur->value();
        const QColor col = shadowColor;
        commitTextPropertyEdits(QStringLiteral("Text Shadow"), [=](TextPrimitive *tp) {
            tp->setShadowEnabled(on);
            tp->setShadowColor(col);
            tp->setShadowOffsetX(ox);
            tp->setShadowOffsetY(oy);
            tp->setShadowBlur(bl);
        });
    }
}

void MainWindow::showStrokeDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Text Stroke");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->strokeEnabled());
    QDoubleSpinBox *width = new QDoubleSpinBox(); width->setRange(0.1, 20); width->setValue(textPrim->strokeWidth());
    QPushButton *colorBtn = new QPushButton();
    QColor strokeColor = textPrim->strokeColor();
    colorBtn->setFixedSize(48, 24);
    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(strokeColor.name()));
    connect(colorBtn, &QPushButton::clicked, &dialog, [colorBtn, &strokeColor, &dialog]() {
        QColor c = QColorDialog::getColor(strokeColor, &dialog, "Stroke Color");
        if (c.isValid()) {
            strokeColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Color:", colorBtn);
    form->addRow("Width:", width);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double w = width->value();
        const QColor col = strokeColor;
        commitTextPropertyEdits(QStringLiteral("Text Stroke"), [=](TextPrimitive *tp) {
            tp->setStrokeEnabled(on);
            tp->setStrokeColor(col);
            tp->setStrokeWidth(w);
        });
    }
}

void MainWindow::showTextBoxDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Text Box Settings");
    QFormLayout *form = new QFormLayout(&dialog);

    QDoubleSpinBox *boxWidth = new QDoubleSpinBox(); boxWidth->setRange(0, 2000); boxWidth->setValue(textPrim->textBoxWidth());
    QDoubleSpinBox *boxHeight = new QDoubleSpinBox(); boxHeight->setRange(0, 2000); boxHeight->setValue(textPrim->textBoxHeight());

    form->addRow("Width (0=auto):", boxWidth);
    form->addRow("Height (0=auto):", boxHeight);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const double w = boxWidth->value();
        const double h = boxHeight->value();
        commitTextPropertyEdits(QStringLiteral("Text Box"), [=](TextPrimitive *tp) {
            tp->setTextBoxWidth(w);
            tp->setTextBoxHeight(h);
        });
    }
}

void MainWindow::showFontFamilyDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    QFont initial(textPrim->fontFamily(), qMax(1, static_cast<int>(textPrim->fontSize())));
    initial.setBold(textPrim->isBold());
    initial.setItalic(textPrim->isItalic());
    initial.setUnderline(textPrim->isUnderline());

    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, initial, this);
    if (!ok)
        return;

    const QString family = font.family();
    const qreal size = font.pointSizeF() > 0 ? font.pointSizeF() : textPrim->fontSize();
    const bool bold = font.bold();
    const bool italic = font.italic();
    const bool underline = font.underline();
    commitTextPropertyEdits(QStringLiteral("Change Font"), [=](TextPrimitive *tp) {
        tp->setFontFamily(family);
        tp->setFontSize(size);
        tp->setBold(bold);
        tp->setItalic(italic);
        tp->setUnderline(underline);
    });
    if (m_statusLabel)
        m_statusLabel->setText("Font applied: " + family);
}

void MainWindow::showLineSpacingDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    bool ok = false;
    double spacing = QInputDialog::getDouble(this, "Line Spacing",
        "Line spacing multiplier (1.0 = normal):", textPrim->lineSpacing(), 0.5, 5.0, 2, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Line Spacing"), [spacing](TextPrimitive *tp) {
        tp->setLineSpacing(spacing);
    });
    if (m_statusLabel)
        m_statusLabel->setText(QString("Line spacing: %1").arg(spacing));
}

void MainWindow::showLetterSpacingDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    bool ok = false;
    double spacing = QInputDialog::getDouble(this, "Letter Spacing",
        "Extra spacing between letters (px):", textPrim->letterSpacing(), -10.0, 50.0, 1, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Letter Spacing"), [spacing](TextPrimitive *tp) {
        tp->setLetterSpacing(spacing);
    });
    if (m_statusLabel)
        m_statusLabel->setText(QString("Letter spacing: %1px").arg(spacing));
}

void MainWindow::showAdvancedTextEditor() {
    if (!m_canvas)
        return;

    // Commit any in-place ClassicTextTool edit first — it hides the
    // canvas primitive and shows an overlay that would mask ATE updates.
    if (m_classicTextTool && m_classicTextTool->isEditing())
        m_classicTextTool->finishEditing();

    TextPrimitive *textPrim = nullptr;
    for (auto *obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive *>(obj)))
            break;
    }
    if (!textPrim) {
        if (m_statusLabel)
            m_statusLabel->setText(QStringLiteral("Select a text object first."));
        return;
    }

    if (!m_advancedTextEditor) {
        m_advancedTextEditor = new AdvancedTextEditor(this);
        connect(m_advancedTextEditor, &AdvancedTextEditor::canvasNeedsUpdate,
                this, [this]() {
                    if (m_canvas) {
                        m_canvas->update();
                        m_canvas->repaint();
                    }
                });
        connect(m_advancedTextEditor, &QDialog::accepted, this, [this]() {
            TextPrimitive *prim = m_advancedTextEditor
                                      ? m_advancedTextEditor->currentPrimitive()
                                      : nullptr;
            if (!prim || !m_commandManager || !m_advancedTextEditor)
                return;
            auto cmd = std::make_unique<EditTextCommand>(
                prim, QStringLiteral("Advanced Text Edit"));
            cmd->storeOldState(m_advancedTextEditor->editSnapshot());
            cmd->storeNewState(prim->toJson());
            cmd->markAlreadyApplied();
            m_commandManager->addCommandWithoutExecuting(std::move(cmd));
            syncModifiedFlag();
            if (m_canvas) {
                m_canvas->update();
                m_canvas->repaint();
            }
            if (m_statusLabel)
                m_statusLabel->setText(QStringLiteral("Text updated"));
        });
    }

    m_advancedTextEditor->setCanvas(m_canvas);
    textPrim->setVisible(true);
    m_advancedTextEditor->setTextPrimitive(textPrim);
    m_advancedTextEditor->show();
    m_advancedTextEditor->raise();
    m_advancedTextEditor->activateWindow();
}


void MainWindow::showGradientDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Text Gradient");
    QFormLayout *form = new QFormLayout(&dialog);

    QCheckBox *enabled = new QCheckBox(); enabled->setChecked(textPrim->gradientEnabled());
    QDoubleSpinBox *angle = new QDoubleSpinBox(); angle->setRange(0, 360); angle->setValue(textPrim->gradientAngle());
    QPushButton *startBtn = new QPushButton();
    QPushButton *endBtn = new QPushButton();
    QColor startColor = textPrim->gradientStartColor();
    QColor endColor = textPrim->gradientEndColor();
    startBtn->setFixedSize(48, 24);
    endBtn->setFixedSize(48, 24);
    startBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(startColor.name()));
    endBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(endColor.name()));
    connect(startBtn, &QPushButton::clicked, &dialog, [startBtn, &startColor, &dialog]() {
        QColor c = QColorDialog::getColor(startColor, &dialog, "Gradient Start");
        if (c.isValid()) {
            startColor = c;
            startBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });
    connect(endBtn, &QPushButton::clicked, &dialog, [endBtn, &endColor, &dialog]() {
        QColor c = QColorDialog::getColor(endColor, &dialog, "Gradient End");
        if (c.isValid()) {
            endColor = c;
            endBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(c.name()));
        }
    });

    form->addRow("Enabled:", enabled);
    form->addRow("Start color:", startBtn);
    form->addRow("End color:", endBtn);
    form->addRow("Angle:", angle);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        const bool on = enabled->isChecked();
        const double ang = angle->value();
        const QColor sc = startColor;
        const QColor ec = endColor;
        commitTextPropertyEdits(QStringLiteral("Text Gradient"), [=](TextPrimitive *tp) {
            tp->setGradientEnabled(on);
            tp->setGradientStartColor(sc);
            tp->setGradientEndColor(ec);
            tp->setGradientAngle(ang);
        });
    }
}

void MainWindow::showTrackingDialog() {
    if (!m_canvas) return;
    TextPrimitive* textPrim = nullptr;
    for (auto* obj : m_canvas->selectedObjects()) {
        if ((textPrim = dynamic_cast<TextPrimitive*>(obj))) break;
    }
    if (!textPrim) {
        if (m_statusLabel) m_statusLabel->setText("Select a text object first.");
        return;
    }

    bool ok = false;
    double tracking = QInputDialog::getDouble(this, "Tracking",
        "Tracking (extra letter spacing, px):", textPrim->letterSpacing(), -10.0, 50.0, 1, &ok);
    if (!ok)
        return;
    commitTextPropertyEdits(QStringLiteral("Tracking"), [tracking](TextPrimitive *tp) {
        tp->setLetterSpacing(tracking);
    });
    if (m_statusLabel)
        m_statusLabel->setText(QString("Tracking: %1px").arg(tracking));
}

// Persistence
bool MainWindow::saveProjectToFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonObject json;
    json["version"] = 1;

    // Serialize all layers and their primitives
    QJsonArray layersArray;
    if (m_layerManager) {
        for (size_t i = 0; i < m_layerManager->layerCount(); i++) {
            Layer* layer = m_layerManager->getLayerAt(i);
            if (!layer) continue;
            QJsonObject layerObj;
            layerObj["id"] = layer->id().toString();
            layerObj["name"] = layer->name();
            layerObj["visible"] = layer->isVisible();
            layerObj["locked"] = layer->isLocked();
            layerObj["opacity"] = layer->opacity();

            QJsonArray primitivesArray;
            for (const auto& prim : layer->primitives()) {
                if (prim) {
                    primitivesArray.append(prim->toJson());
                }
            }
            layerObj["primitives"] = primitivesArray;
            layersArray.append(layerObj);
        }
    }
    json["layers"] = layersArray;

    // Save canvas settings
    if (m_canvas) {
        QJsonObject canvasObj;
        canvasObj["backgroundColor"] = m_canvas->backgroundColor().name(QColor::HexArgb);
        canvasObj["paperColor"] = m_canvas->paperColor().name(QColor::HexArgb);
        canvasObj["gridVisible"] = m_canvas->isGridVisible();
        canvasObj["snapEnabled"] = m_canvas->isSnapEnabled();
        json["canvas"] = canvasObj;
    }

    QJsonDocument doc(json);
    file.write(doc.toJson());
    setCurrentFile(fileName);
    addToRecentFiles(fileName);
    m_statusLabel->setText("Saved: " + fileName);
    return true;
}

bool MainWindow::loadProjectFromFile(const QString& fileName) {
    return loadProjectFromFile(fileName, false);
}

bool MainWindow::loadProjectFromFile(const QString& fileName, bool waitUntilLoaded) {
    // Read file bytes on main thread (fast)
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray fileData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileData);
    if (doc.isNull()) return false;

    QJsonObject json = doc.object();

    // Clear existing content
    if (m_layerManager) {
        m_layerManager->clearLayersNoDefault();
    }
    if (m_canvas) {
        m_canvas->clearPrimitives();
    }
    if (m_commandManager) {
        m_commandManager->clear();
    }

    // Restore canvas settings (fast, stays on main thread)
    if (json.contains("canvas") && m_canvas) {
        QJsonObject canvasObj = json["canvas"].toObject();
        if (canvasObj.contains("backgroundColor"))
            m_canvas->setBackgroundColor(QColor(canvasObj["backgroundColor"].toString()));
        if (canvasObj.contains("paperColor"))
            m_canvas->setPaperColor(QColor(canvasObj["paperColor"].toString()));
        if (canvasObj.contains("gridVisible"))
            m_canvas->setGridVisible(canvasObj["gridVisible"].toBool());
        if (canvasObj.contains("snapEnabled"))
            m_canvas->setSnapEnabled(canvasObj["snapEnabled"].toBool());
    }

    // Struct to hold parsed results from worker thread
    struct ParsedLayer {
        QString name;
        QString id;
        bool visible = true;
        bool locked = false;
        double opacity = 1.0;
        std::vector<std::unique_ptr<DrawingPrimitive>> primitives;
    };

    // Show spinner for progress feedback
    auto* spinner = new SpinnerDialog("Loading Project", "Parsing project file...", this);
    spinner->setCancelEnabled(false);
    spinner->show();
    QCoreApplication::processEvents();

    // Parse layers and primitives on a worker thread (the expensive part)
    auto* parsedLayers = new std::vector<ParsedLayer>();
    bool isLegacy = json["layers"].toArray().isEmpty();
    QJsonArray layersArray = json["layers"].toArray();
    QJsonArray legacyPrimitives = json["primitives"].toArray();

    QThread* thread = QThread::create([parsedLayers, isLegacy, layersArray, legacyPrimitives]() {
        if (isLegacy) {
            // Legacy format: flat primitives array
            ParsedLayer pl;
            pl.name = "Default";
            for (const QJsonValue& val : legacyPrimitives) {
                auto prim = DrawingPrimitive::createFromJson(val.toObject());
                if (prim) {
                    pl.primitives.push_back(std::move(prim));
                }
            }
            parsedLayers->push_back(std::move(pl));
        } else {
            for (const QJsonValue& layerVal : layersArray) {
                QJsonObject layerObj = layerVal.toObject();
                ParsedLayer pl;
                pl.name = layerObj["name"].toString("Layer");
                pl.id = layerObj["id"].toString();
                pl.visible = layerObj["visible"].toBool(true);
                pl.locked = layerObj["locked"].toBool(false);
                pl.opacity = layerObj["opacity"].toDouble(1.0);

                QJsonArray primitivesArray = layerObj["primitives"].toArray();
                for (const QJsonValue& val : primitivesArray) {
                    auto prim = DrawingPrimitive::createFromJson(val.toObject());
                    if (prim) {
                        pl.primitives.push_back(std::move(prim));
                    }
                }
                parsedLayers->push_back(std::move(pl));
            }
        }
    });

    auto* loadDone = waitUntilLoaded ? new QEventLoop(this) : nullptr;

    connect(thread, &QThread::finished, this, [this, thread, parsedLayers, spinner, fileName, loadDone]() {
        // Install parsed results into layers (main thread)
        if (m_layerManager) {
            int totalInstalled = 0;
            for (auto& pl : *parsedLayers) {
                Layer* layer = nullptr;
                if (!pl.id.isEmpty()) {
                    layer = m_layerManager->createLayer(QUuid(pl.id), pl.name);
                } else {
                    layer = m_layerManager->createLayer(pl.name);
                }
                if (!layer) continue;

                layer->setVisible(pl.visible);
                layer->setLocked(pl.locked);
                layer->setOpacity(pl.opacity);

                for (int i = 0; i < static_cast<int>(pl.primitives.size()); ++i) {
                    layer->addPrimitive(std::move(pl.primitives[i]));
                    ++totalInstalled;
                    if (totalInstalled % 2000 == 0)
                        QCoreApplication::processEvents();
                }
            }
        }

        delete parsedLayers;
        spinner->hide();
        spinner->deleteLater();

        setCurrentFile(fileName);
        addToRecentFiles(fileName);
        if (m_canvas) m_canvas->update();
        if (m_layerPanel) m_layerPanel->refresh();
        if (m_statusLabel) m_statusLabel->setText("Loaded: " + fileName);

        thread->deleteLater();
        if (loadDone)
            loadDone->quit();
    });

    thread->start();

    if (loadDone) {
        loadDone->exec();
        loadDone->deleteLater();
    }
    return true;
}

// AI
void MainWindow::onImageGenerated(const QImage& image, const QString& prompt) {
    // Add image to canvas
    if (m_canvas) {
        auto img = std::make_unique<ImagePrimitive>(image, QVector2D(0,0), QVector2D(512,512));
        m_canvas->addPrimitiveWithCommand(std::move(img));
    }
}


// Image Filters
int MainWindow::applyBlurToSelected(int radius) {
    if (!m_canvas || radius <= 0) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image();
            QImage blurred = applyBoxBlur(img, radius);
            imgPrim->setImage(blurred);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyEdgeBlurToSelected(int radius) {
    if (!m_canvas || radius <= 0) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            QImage blurred = applyBoxBlur(img, radius);
            // Only apply blur to edge pixels (where alpha gradient exists)
            for (int y = 0; y < img.height(); y++) {
                for (int x = 0; x < img.width(); x++) {
                    QColor orig = img.pixelColor(x, y);
                    if (orig.alpha() > 0 && orig.alpha() < 255) {
                        img.setPixelColor(x, y, blurred.pixelColor(x, y));
                    }
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyGrayscaleToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int gray = qGray(line[x]);
                    int a = qAlpha(line[x]);
                    line[x] = qRgba(gray, gray, gray, a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applySepiaToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int r = qRed(line[x]);
                    int g = qGreen(line[x]);
                    int b = qBlue(line[x]);
                    int a = qAlpha(line[x]);
                    int tr = qMin(255, (int)(0.393 * r + 0.769 * g + 0.189 * b));
                    int tg = qMin(255, (int)(0.349 * r + 0.686 * g + 0.168 * b));
                    int tb = qMin(255, (int)(0.272 * r + 0.534 * g + 0.131 * b));
                    line[x] = qRgba(tr, tg, tb, a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyInvertToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int a = qAlpha(line[x]);
                    line[x] = qRgba(255 - qRed(line[x]), 255 - qGreen(line[x]), 255 - qBlue(line[x]), a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyFlipVerticalToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().flipped(Qt::Vertical);
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyFlipHorizontalToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().flipped(Qt::Horizontal);
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

// Helpers


void MainWindow::executeCommand(Command* command) {
    if (!command) {
        return;
    }
    if (!m_commandManager) {
        delete command;
        return;
    }

    // Some canvas interactions (e.g., drag-move / resize / rotate) apply
    // changes immediately and then emit a command purely for undo/redo.
    if (dynamic_cast<MovePrimitivesCommand *>(command) != nullptr ||
        dynamic_cast<TransformPrimitivesCommand *>(command) != nullptr) {
        m_commandManager->addCommandWithoutExecuting(std::unique_ptr<Command>(command));
    } else {
        m_commandManager->executeCommand(std::unique_ptr<Command>(command));
    }

    syncModifiedFlag();
    if (m_canvas) {
        m_canvas->update();
    }
}

void MainWindow::hideFloatingMaskPanel() {
    if (m_floatingMaskPanel) {
        m_floatingMaskPanel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_floatingMaskPanel->hide();
    }
}

void MainWindow::showFloatingMaskPanel() {
    if (!m_floatingMaskPanel || !m_canvas) return;

    // Position at right edge of canvas, vertically centered, 20px margin
    int panelW = m_floatingMaskPanel->width();
    int panelH = m_floatingMaskPanel->height();
    int x = m_canvas->width() - panelW - 20;
    int y = (m_canvas->height() - panelH) / 2;
    if (x < 0) x = 10;
    if (y < 0) y = 10;
    m_floatingMaskPanel->move(x, y);

    m_floatingMaskPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_floatingMaskPanel->show();
    m_floatingMaskPanel->raise();
}

void MainWindow::updateFloatingMaskPanel() {
    if (!m_floatingMaskPanel || !m_canvas) return;

    ImagePrimitive *imgPrim = nullptr;
    for (auto *obj : m_canvas->selectedObjects()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
            imgPrim = ip;
            break;
        }
    }

    if ((!imgPrim || imgPrim->getMaskCandidateCount() == 0) && m_canvas->layerManager()) {
        for (auto *p : m_canvas->layerManager()->getAllPrimitives()) {
            if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
                if (ip->getMaskCandidateCount() > 0) {
                    imgPrim = ip;
                    break;
                }
            }
        }
    }

    if (!imgPrim || imgPrim->getMaskCandidateCount() == 0) {
        if (m_fmpCounterLabel) m_fmpCounterLabel->setText("- / -");
        return;
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();

    // Counter
    if (m_fmpCounterLabel)
        m_fmpCounterLabel->setText(QString("%1 / %2").arg(current + 1).arg(count));

    // Stats from selected candidate
    const auto *c = imgPrim->getSelectedCandidate();
    if (c) {
        if (m_fmpScoreLabel) m_fmpScoreLabel->setText(QString("Score %1").arg(c->score, 0, 'f', 2));
        if (m_fmpAreaLabel) m_fmpAreaLabel->setText(QString("Area %1%").arg(c->area_percent, 0, 'f', 1));
        if (m_fmpStabilityLabel) m_fmpStabilityLabel->setText(QString("Stab %1").arg(c->stability, 0, 'f', 2));
        if (m_fmpIoULabel) m_fmpIoULabel->setText(QString("IoU %1").arg(c->predicted_iou, 0, 'f', 2));

        // Score bar width as percentage of panel width
        if (m_fmpScoreBar) {
            int barW = static_cast<int>(c->score * 248); // 280 - 32 margins
            if (barW < 4) barW = 4;
            m_fmpScoreBar->setFixedWidth(barW);
        }
    }

    // Nav buttons
    if (m_fmpPrevBtn) m_fmpPrevBtn->setEnabled(count > 1);
    if (m_fmpNextBtn) m_fmpNextBtn->setEnabled(count > 1);

    // Slider
    if (m_fmpSlider) {
        m_fmpSlider->blockSignals(true);
        m_fmpSlider->setRange(0, count - 1);
        m_fmpSlider->setValue(current);
        m_fmpSlider->blockSignals(false);
        m_fmpSlider->setVisible(count > 1);
    }
}
