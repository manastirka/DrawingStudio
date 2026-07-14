#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "IconFactory.h"
#include "CommandManager.h"
#include "Commands.h"
#include "PropertyPanel.h"
#include "ImagePrimitive.h"
#include "DrawingPrimitive.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QImageWriter>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include <memory>

// Menu bar construction (refactor E15).

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

    QAction *autosaveNote = fileMenu->addAction(
        QStringLiteral("Autosave recovery every 2 min (when dirty)"));
    autosaveNote->setEnabled(false);
    autosaveNote->setStatusTip(
        QStringLiteral("Background recovery snapshot is written automatically while the project is modified"));

    QAction *clearRecovery = fileMenu->addAction(
        QStringLiteral("Clear Recovery Autosave…"), this, [this]() {
            clearRecoveryFile();
            if (m_statusLabel)
                m_statusLabel->setText(QStringLiteral("Recovery autosave cleared"));
        });
    clearRecovery->setStatusTip(
        QStringLiteral("Delete the crash-recovery snapshot without opening it"));

    fileMenu->addSeparator();

    QAction *importFloorPlanAction =
        fileMenu->addAction("Import &Floor Plan...", this, &MainWindow::importFloorPlan);
    importFloorPlanAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    importFloorPlanAction->setStatusTip(
        QStringLiteral("Import a floor-plan image and convert walls to drawable lines"));

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

    // Insert Menu
    QMenu *insertMenu = menuBar()->addMenu("&Insert");
    QAction *formulaAction =
        insertMenu->addAction("Math &Formula...", this, &MainWindow::insertMathFormula);
    formulaAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    formulaAction->setStatusTip(QStringLiteral("Insert a rendered math formula"));

    QAction *graphAction =
        insertMenu->addAction("Function &Graph...", this, &MainWindow::insertMathGraph);
    graphAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    graphAction->setStatusTip(QStringLiteral("Plot y = f(x) and place the graph on canvas"));

    insertMenu->addSeparator();
    QAction *physicsAction =
        insertMenu->addAction("Physics Problem &Solver...", this,
                              &MainWindow::insertPhysicsSolver);
    physicsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    physicsAction->setStatusTip(
        QStringLiteral("Solve physics equations and insert the result formula"));

    insertMenu->addSeparator();
    QAction *tutorialAction =
        insertMenu->addAction("Math &Tutorial...", this, &MainWindow::showMathTutorial);
    tutorialAction->setStatusTip(
        QStringLiteral("How to enter formulas and graph expressions"));
    
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

    imageMenu->addAction("Extract Lines from Building &Plan...", this,
                         &MainWindow::extractLinesFromImage);
    imageMenu->addAction("Import &Floor Plan...", this, &MainWindow::importFloorPlan);

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
    QAction *cancelAi = aiMenu->addAction("Cancel AI &Job", this, &MainWindow::cancelAIJob);
    cancelAi->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+.")));
    cancelAi->setStatusTip(QStringLiteral("Abort the current AI generate/edit/composite request"));
    
    // Help Menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *helpAction = helpMenu->addAction("&Help", this, &MainWindow::showHelp);
    helpAction->setShortcut(QKeySequence::HelpContents);

    helpMenu->addAction(QStringLiteral("Reset &Window Layout…"), this,
                        &MainWindow::resetWindowLayout);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction("&About", this, &MainWindow::showAbout);
    aboutAction->setStatusTip("About Drawing Studio");
}

