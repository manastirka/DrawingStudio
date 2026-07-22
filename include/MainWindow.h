
 

#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDockWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QAction>
#include <QActionGroup>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QFrame>
#include <QFontComboBox>
#include <QMap>
#include <QKeySequence>
#include <QColor>
#include <QFuture>
#include <QJsonObject>
#include <QImage>
#include <QVector>
#include <QJsonArray>
#include <QTimer>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>


class DrawingCanvas;
class PropertyPanel;
class DrawingProject;
class DrawingPrimitive;
class TextPrimitive;
class ImagePrimitive;
class LayerManager;
class LayerPanel;
class ColorPalette;
class Command;
class AdvancedTextEditor;
class ClassicTextTool;
class SimpleTextPanel;
class DiffusionHelper;
class CoreMLDiffusionHelper;
class RemoteSDHelper;
class CommandServer;
class SAM2ServiceManager;
class ImageToDrawingEngine;
class DrawingCommandDispatcher;
class FloatingMaskPanel;
class ImageAdjustmentController;
class AIWorkflowController;
class ImageExportService;
class ProjectFileService;
class PrimitivePropertyApplier;
class TextEditingController;
class ToolOptionsBar;
class ToolIconProvider;

// Forward declare enum from DrawingCanvas.h
enum class DrawingTool;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct AutomationOptions {
        bool enabled = false;
        quint16 port = 19100;
        QString token;
        bool allowFilesystemCommands = false;
    };

    explicit MainWindow(QWidget *parent = nullptr);
    MainWindow(const AutomationOptions &automation, QWidget *parent);
    ~MainWindow();
    
    // Undo/Redo support
    void saveUndoState(const QString& description);
    
    // Text tool access
    ClassicTextTool* getClassicTextTool() const { return m_classicTextTool; }

    // Command result (for HTTP API responses)
    QJsonObject lastCommandResult() const { return m_lastCommandResult; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    bool saveProjectToFile(const QString& fileName);
    bool loadProjectFromFile(const QString& fileName);
    bool loadProjectFromFile(const QString& fileName, bool waitUntilLoaded);
    void exportImageAs();
    void exportAsPNG();
    void exportAsJPG();
    void exportAsBMP();
    void exportAsTIFF();
    void exportAsWebP();
    void exportAsGIF();
    void exportAsPPM();
    void exportAsDXF();
    bool exportCanvasToFile(const QString &path, const QString &format = QString(),
                          int quality = -1);
    bool exportImageWithDialog(const QString &preferredFormat = QString());
    static QString imageFormatFromPath(const QString &path);
    static QString defaultExtensionForFormat(const QString &format);
    static QString ensureImageExtension(const QString &path, const QString &format);
    static QString imageExportFilterString();
    static QString formatFromFilter(const QString &selectedFilter);
    void extractLinesFromImage();
    void importFloorPlan();
    void exitApplication();
    
    void undo();
    void redo();
    
    // Command execution
    void executeCommand(class Command* command);
    void selectAll();
    void deleteSelected();
    void cutSelected();
    void copySelected();
    void pasteClipboard();
    void duplicateSelected();
    void groupSelected();
    void ungroupSelected();
    
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();
    void toggleGrid();
    void toggleSnap();
    void toggleMagneticConnection();
    void toggleRulers();
    void setGridSizeFine();
    void setGridSizeMedium();
    void setGridSizeCoarse();
    
    void selectTool();
    void lineTool();
    void curveTool();
    void bezierTool();
    void splineTool();
    void polygonTool();
    void rectangleTool();
    void ellipseTool();
    void circleTool();
    void arcTool();
    void angleLineTool();
    void eraserTool();
    void fillTool();
    void brushTool();
    void blurTool();
    void measureTool();
    void imageTool();
    void handTool();
    void textTool();
    void activateDrawingTool(DrawingTool tool);
    
    // Image generation
    
    // Tool change handler
    void onToolChanged();
    
    void setUnitsMillimeters();
    void setUnitsCentimeters();
    void setUnitsInches();
    
    // Color changes
    void changeBackgroundColor();
    void changePaperColor();
    
    void showAbout();
    void showHelp();
    
    // Format menu methods
    void showAdvancedTextEditor();
    void insertMathFormula();
    void insertMathGraph();
    void showMathTutorial();
    void insertPhysicsSolver();
    void commitTextPropertyEdits(const QString &description,
                                 const std::function<void(TextPrimitive *)> &mutate);
    
    // Text alignment
    void setTextAlignLeft();
    void setTextAlignCenter();
    void setTextAlignRight();
    void setTextAlignJustify();
    
    // Typography
    void showFontFamilyDialog();
    void showLetterSpacingDialog();
    void showLineSpacingDialog();
    void showTrackingDialog();
    
    // Text effects
    void showShadowDialog();
    void showStrokeDialog();
    void showGradientDialog();
    
    // Text box
    void showTextBoxDialog();
    
    // Shadow methods
    void addAutoShadow();
    
    // Color management
    void onColorChanged(const QColor &color);
    
    // Mask selection
    void onMaskSelectionChanged(int index);
    void selectPreviousMask();
    void selectNextMask();
    void invertSelectedMask();
    void updateMaskSelectionUI();
    void executeDrawingCommand(const QString& action, const QJsonObject& params);
    void applyCommonParams(DrawingPrimitive* prim, const QJsonObject& params);

    // Image effect helpers
    int applyBlurToSelected(int radius);
    int applyEdgeBlurToSelected(int radius);
    int applyGrayscaleToSelected();
    int applySepiaToSelected();
    int applyInvertToSelected();
    int applyFlipHorizontalToSelected();
    int applyFlipVerticalToSelected();
    QImage applyBoxBlur(const QImage& source, int radius);
    
    // Drawing tool methods
    

private:
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupFavoritesToolbar();
    void setupUndoHistoryDock();
    void setupStatusBar();
    void setupDockWidgets();
    void connectSignals();
    void initializeAIModelsAsync();
    
    bool maybeSave();
    void setCurrentFile(const QString &fileName);
    void updateWindowTitle();
    void syncModifiedFlag();
    void openRecentFile();
    void updateRecentFileActions();
    void addToRecentFiles(const QString &fileName);
    void updatePropertyPanel();
    void onPropertyChanged(const QString& objectName, const QString& propertyName, const QVariant& value);
    void applyPropertyToPrimitive(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value);
    void updateToolTooltips();
    void updateUndoHistoryPanel();
    void refreshSmartSuggestions();
    void applySmartSuggestion(const QString &actionId);
    void showShortcutEditor();
    QString toolKey(DrawingTool tool) const;
    void loadToolShortcuts();
    void saveToolShortcuts(const QMap<QString, QKeySequence>& shortcuts);
    void updateFavoritesToolbar();
    void toggleFavoriteTool(DrawingTool tool);
    void updatePresetList(DrawingTool tool);
    void applyPreset(const QString& presetName);
    void saveCurrentPreset();
    
    // Icons via ToolIconProvider (thin wrappers in .cpp)
    QIcon iconForTool(DrawingTool tool);
    QString displayNameForTool(DrawingTool tool) const;
    QKeySequence defaultShortcutForTool(DrawingTool tool) const;
    void setupToolFlyoutSlot(const QList<DrawingTool> &tools, DrawingTool defaultTool);
    void syncToolSlotAppearance(DrawingTool tool);
    void persistToolSlotChoice(DrawingTool tool);
    
    // Helper methods to load icons
    
    // SVG icon loading system
    

    // UI Components
    DrawingCanvas *m_canvas = nullptr;
    QSplitter *m_centralSplitter = nullptr;
    
    // Project
    std::unique_ptr<DrawingProject> m_project;
    
    // Dock Widgets
    QDockWidget *m_propertiesDock = nullptr;
    QDockWidget *m_layersDock = nullptr;
    QDockWidget *m_undoHistoryDock = nullptr;
    QListWidget *m_undoHistoryList = nullptr;
    
    // Object widgets
    PropertyPanel *m_propertyPanel = nullptr;
    LayerPanel *m_layerPanel = nullptr;
    ColorPalette *m_colorPalette = nullptr;
    AdvancedTextEditor *m_advancedTextEditor = nullptr;
    
    // Text tools
    ClassicTextTool *m_classicTextTool = nullptr;
    
    // Layer management
    LayerManager *m_layerManager = nullptr;
    
    // Undo/Redo manager
    class CommandManager *m_commandManager = nullptr;
    
    // Command Server (HTTP API for bots)
    CommandServer *m_commandServer = nullptr;
    DiffusionHelper *m_diffusionHelper = nullptr;
    CoreMLDiffusionHelper *m_coreMLDiffusionHelper = nullptr;
    
    // SD Backend selection
    enum class SDBackend {
        Local,      // Local ggml-based SD
        Remote      // User-configured HTTP Stable Diffusion server
    };
    SDBackend m_currentSDBackend = SDBackend::Local;
    bool m_modelsLoading = false;
    std::atomic_bool m_cancelModelInit{false};
    QFuture<void> m_modelInitFuture;
    
    // Command result for HTTP API
    QJsonObject m_lastCommandResult;

    // Internal clipboard for cut/copy/paste of primitives
    std::vector<QJsonObject> m_clipboard;

    // Image-to-drawing engine (render_mosaic / auto_trace / render_photo_copy)
    ImageToDrawingEngine *imageToDrawingEngine();
    ImageToDrawingEngine *m_imageToDrawingEngine = nullptr;

    // Bot / HTTP command dispatcher
    DrawingCommandDispatcher *commandDispatcher();
    void ensureCommandDispatcherHost();
    DrawingCommandDispatcher *m_commandDispatcher = nullptr;
    ImageAdjustmentController *m_imageAdjustmentController = nullptr;
    ImageAdjustmentController *imageAdjustmentController();
    void ensureImageAdjustmentHost();

    // Helper for image analysis API
    ImagePrimitive* findImageByIndex(int index);
    ImagePrimitive* imageForDetection();
    ImagePrimitive* selectedImageWithMasks();
    void selectImageForMaskUI(ImagePrimitive *image);

    // File management
    QString m_currentFile;
    bool m_isModified = false;
    QTimer *m_autosaveTimer = nullptr;

    // Recent files
    enum { MaxRecentFiles = 10 };
    QAction *m_recentFileActions[MaxRecentFiles] = {};
    QAction *m_recentFilesSeparator = nullptr;
    
    // Bump when the default dock arrangement changes, so stale saved
    // window states don't override the new defaults.
    enum { DOCK_LAYOUT_VERSION = 2 };
    
    // Status bar widgets
    QLabel *m_statusLabel = nullptr;
    QLabel *m_coordsLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_sam2StatusLabel = nullptr;
    SAM2ServiceManager *m_sam2Service = nullptr;
    
    // Tool settings widgets
    QToolBar *m_mainToolbar = nullptr;
    QToolBar *m_leftToolbar = nullptr;
    QToolBar *m_favoritesToolbar = nullptr;

    // Smart tool suggestions (options-bar chips + status coaching)
    QString m_liveSmartHint;
    QTimer *m_suggestionRestoreTimer = nullptr;
    




    
    
    
    // Floating mask panel
    FloatingMaskPanel *m_floatingMaskPanel = nullptr;
    void createFloatingMaskPanel();
    void ensureFloatingMaskHost();
    void showFloatingMaskPanel();
    void hideFloatingMaskPanel();
    void updateFloatingMaskPanel();
    
    
    
    // Tool action group for mutual exclusion (visible flyout slots)
    QActionGroup *m_toolActionGroup = nullptr;
    QMap<DrawingTool, QAction*> m_toolActions;      // every tool (menu/shortcut/favorites)
    QMap<DrawingTool, QAction*> m_toolSlotActions;  // visible toolbar slot for a tool's family
    QMap<DrawingTool, DrawingTool> m_toolSlotLeader; // tool -> currently shown slot tool
    QStringList m_favoriteTools;
    
    // Helper method to update tool settings
    void updateToolSettings(DrawingTool tool);
    
    // AI generation methods
    void generateAIImage();
    void editSelectedWithAI();
    void placeSubjectInScene();
    /** Extract SAM2 subjects from all selected images; leave all cutouts selected. */
    void extractSelectedSubjects();
    void showAISettings();
    void cancelAIJob();
    void showMaskSettingsPopup();

    // Autosave / crash recovery
    void setupAutosave();
    void performAutosave();
    void checkRecoveryFileOnStartup();
    void clearRecoveryFile();
    QString recoveryFilePath() const;
    void resetWindowLayout();
    
    // Image adjustments
    void showLevelsAdjustment();
    void showCurvesAdjustment();
    void showShadowsAdjustment();
    void showHighlightsAdjustment();
    void showBrightnessContrast();
    void showHueSaturation();
    
    // Blur effects
    void showGaussianBlur();
    void showMotionBlur();
    void showRadialBlur();
    void showBokehBlur();
    void showSurfaceBlur();
    
    // Auto enhance
    void autoEnhanceImage();


    // Icon theme toggle: when true, always use generic (programmatic) icons
    AIWorkflowController *m_aiWorkflow = nullptr;
    AIWorkflowController *aiWorkflow();
    void ensureAIWorkflowHost();
    ImageExportService *m_imageExportService = nullptr;
    ImageExportService *imageExportService();
    void ensureImageExportHost();
    ProjectFileService *m_projectFileService = nullptr;
    ProjectFileService *projectFileService();
    void ensureProjectFileHost();
    TextEditingController *m_textEditingController = nullptr;
    TextEditingController *textEditingController();
    void ensureTextEditingHost();
    ToolOptionsBar *m_toolOptionsBar = nullptr;
    void ensureToolOptionsHost();
};
