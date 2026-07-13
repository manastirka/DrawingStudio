

#include "MainWindow.h"
#include "IconFactory.h"
#include "DrawingCanvas.h"
#include "PropertyPanel.h"
#include "DrawingProject.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "ImageToDrawingEngine.h"
#include "DrawingCommandDispatcher.h"
#include "FloatingMaskPanel.h"
#include "ImageAdjustmentController.h"
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
#include "AIWorkflowController.h"
#include "ImageExportService.h"
#include "ProjectFileService.h"
#include "PrimitivePropertyApplier.h"
#include "TextEditingController.h"
#include "ToolOptionsBar.h"
#include "ToolIconProvider.h"
#include "AIGenerateDialog.h"
#include "AICompositeDialog.h"
#include "CompositeHelper.h"
#include "MathInsertDialog.h"
#include "PhysicsSolverDialog.h"
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
    m_commandServer->setWindowWidget(this);
    m_commandServer->setCommandResultProvider([this]() {
        return lastCommandResult();
    });
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
    delete m_imageToDrawingEngine;
    m_imageToDrawingEngine = nullptr;
    delete m_commandDispatcher;
    m_commandDispatcher = nullptr;
    delete m_imageExportService;
    m_imageExportService = nullptr;
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

// setupMenus → extracted E15

// setupToolbars → extracted E15

// setupFavoritesToolbar → extracted E15

// setupUndoHistoryDock → extracted E15

// setupStatusBar → extracted E15

// setupDockWidgets → extracted E15

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

        PrimitivePropertyApplier::apply(primitive, propertyName, value);

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

void MainWindow::toggleRulers()
{
    if (m_canvas) {
        bool areVisible = m_canvas->areRulersVisible();
        m_canvas->setRulersVisible(!areVisible);
        m_statusLabel->setText(QString("Rulers %1").arg(!areVisible ? "shown" : "hidden"));
    }
}

ImageAdjustmentController *MainWindow::imageAdjustmentController()
{
    if (!m_imageAdjustmentController)
        m_imageAdjustmentController = new ImageAdjustmentController(this);
    return m_imageAdjustmentController;
}

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

void MainWindow::applyPropertyToPrimitive(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value)
{
    PrimitivePropertyApplier::apply(primitive, propertyName, value);
}

