
 

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

#include "CompositeHelper.h"
#include "AICompositeDialog.h"

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
class AIImageClient;

// Forward declare enum from DrawingCanvas.h
enum class DrawingTool;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
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
    void onImageGenerated(const QImage& image, const QString& prompt);
    
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
    
    // Icon creation methods
    QIcon createSelectIcon();
    QIcon createLineIcon();
    QIcon createCurveIcon();
    QIcon createBezierIcon();
    QIcon createSplineIcon();
    QIcon createPolygonIcon();
    QIcon createRectangleIcon();
    QIcon createEllipseIcon();
    QIcon createCircleIcon();
    QIcon createArcIcon();
    QIcon createEraserIcon();
    QIcon createFillIcon();
    QIcon createBrushIcon();
    QIcon createBlurIcon();
    QIcon createMeasureIcon();
    QIcon createImageIcon();
    QIcon createHandIcon();
    QIcon createTextIcon();
    QIcon createAngleLineIcon();
    QIcon createMonoIcon(const std::function<void(QPainter&, const QRectF&)> &draw);
    QIcon iconForTool(DrawingTool tool);
    QString displayNameForTool(DrawingTool tool) const;
    QKeySequence defaultShortcutForTool(DrawingTool tool) const;
    void setupToolFlyoutSlot(const QList<DrawingTool> &tools, DrawingTool defaultTool);
    void syncToolSlotAppearance(DrawingTool tool);
    void persistToolSlotChoice(DrawingTool tool);
    
    // Helper methods to load icons
    QIcon loadCustomIcon(const QString& iconName, std::function<QIcon()> fallbackGenerator);
    QIcon loadIconFromFile(const QString& filename);
    
    // SVG icon loading system
    QIcon loadSVGIcon(const QString& iconName, const QSize& size = QSize(48, 48));
    QIcon loadAIIcon(const QString& toolName, const QSize& size = QSize(48, 48));
    

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
    RemoteSDHelper *m_remoteSDHelper = nullptr;
    
    // SD Backend selection
    enum class SDBackend {
        Local,      // Local ggml-based SD
        Remote      // Remote server (192.168.1.58)
    };
    SDBackend m_currentSDBackend = SDBackend::Local;
    bool m_modelsLoading = false;
    std::atomic_bool m_cancelModelInit{false};
    QFuture<void> m_modelInitFuture;
    
    // Command result for HTTP API
    QJsonObject m_lastCommandResult;

    // Internal clipboard for cut/copy/paste of primitives
    std::vector<QJsonObject> m_clipboard;

    // Adaptive mosaic leaf cell data (shared between render_mosaic and render_photo_copy)
    QJsonArray m_adaptiveLeafCells;

    // Helper for image analysis API
    ImagePrimitive* findImageByIndex(int index);
    ImagePrimitive* imageForDetection();
    ImagePrimitive* selectedImageWithMasks();
    void selectImageForMaskUI(ImagePrimitive *image);

    // File management
    QString m_currentFile;
    bool m_isModified = false;

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
    QWidget *m_toolSettingsWidget = nullptr;
    QHBoxLayout *m_toolSettingsLayout = nullptr;
    QToolBar *m_mainToolbar = nullptr;
    QToolBar *m_leftToolbar = nullptr;
    QToolBar *m_favoritesToolbar = nullptr;

    // Smart tool suggestions (options-bar chips + status coaching)
    QWidget *m_suggestionStrip = nullptr;
    QHBoxLayout *m_suggestionStripLayout = nullptr;
    QString m_liveSmartHint;
    QTimer *m_suggestionRestoreTimer = nullptr;
    
    // Common tool settings
    QLabel *m_setting1Label = nullptr;
    QSlider *m_setting1Slider = nullptr;
    QLabel *m_setting1ValueLabel = nullptr;
    
    QLabel *m_setting2Label = nullptr;
    QSlider *m_setting2Slider = nullptr;
    QLabel *m_setting2ValueLabel = nullptr;
    
    QLabel *m_setting3Label = nullptr;
    QSlider *m_setting3Slider = nullptr;
    QLabel *m_setting3ValueLabel = nullptr;
    
    QCheckBox *m_boolSetting1 = nullptr;
    QCheckBox *m_boolSetting2 = nullptr;

    // Hairline vertical group separators (Photoshop-style options bar)
    QFrame *m_optSep1 = nullptr;  // between numeric/style group and toggles
    QFrame *m_optSep2 = nullptr;  // before the preset group

    // Text tool widgets
    QFontComboBox *m_textFontCombo = nullptr;
    QToolButton *m_textUnderlineBtn = nullptr;
    QToolButton *m_textColorBtn = nullptr;

    // Selection tool widgets
    QLabel *m_selectionModeLabel = nullptr;
    QComboBox *m_selectionModeCombo = nullptr;
    QPushButton *m_selectSimilarButton = nullptr;

    // Presets widgets
    QLabel *m_presetLabel = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QToolButton *m_savePresetButton = nullptr;
    
    // SAM2 specific widgets
    QPushButton *m_detectMasksButton = nullptr;
    QPushButton *m_maskSettingsButton = nullptr;
    QPushButton *m_extractButton = nullptr;
    QPushButton *m_extractAllButton = nullptr;
    QPushButton *m_editMaskButton = nullptr;
    QCheckBox *m_maskInvertCheck = nullptr;
    QCheckBox *m_maskOverlayCheck = nullptr;
    QLabel *m_maskFeatherLabel = nullptr;
    QSlider *m_maskFeatherSlider = nullptr;
    QLabel *m_maskFeatherValue = nullptr;
    QLabel *m_maskBlurLabel = nullptr;
    QSlider *m_maskBlurSlider = nullptr;
    QLabel *m_maskBlurValue = nullptr;
    QLabel *m_maskExpandLabel = nullptr;
    QSlider *m_maskExpandSlider = nullptr;
    QLabel *m_maskExpandValue = nullptr;
    
    
    // Floating mask panel
    QWidget *m_floatingMaskPanel = nullptr;
    QPoint m_dragStartPosition;
    bool m_isDragging = false;
    void createFloatingMaskPanel();
    void showFloatingMaskPanel();
    void hideFloatingMaskPanel();
    void updateFloatingMaskPanel();
    bool eventFilter(QObject *obj, QEvent *event) override;
    
    // Line style selector
    QLabel *m_lineStyleLabel = nullptr;
    QComboBox *m_lineStyleCombo = nullptr;
    
    // Select by color widgets
    QLabel *m_selectColorLabel = nullptr;
    QPushButton *m_selectColorButton = nullptr;
    QPushButton *m_pipetteButton = nullptr;
    QColor m_selectByColor = Qt::black;
    
    // Floating mask panel child widgets
    QLabel *m_fmpCounterLabel = nullptr;
    QLabel *m_fmpScoreLabel = nullptr;
    QLabel *m_fmpStabilityLabel = nullptr;
    QLabel *m_fmpAreaLabel = nullptr;
    QLabel *m_fmpIoULabel = nullptr;
    QWidget *m_fmpScoreBar = nullptr;
    QPushButton *m_fmpPrevBtn = nullptr;
    QPushButton *m_fmpNextBtn = nullptr;
    QSlider *m_fmpSlider = nullptr;
    
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
    void showMaskSettingsPopup();
    
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
    // Lazily construct/initialize AI helpers; return true only if usable.
    bool ensureRemoteSDHelper();
    void replaceSelectedObjectWithImage(const QImage &image, const QString &prompt, int mode = 0);
    void importAIImageToCanvas(const QImage &image, const QString &prompt,
                               bool replaceSelectedImage,
                               bool showSuccessDialog = true);
    void connectAIImageClient();
    void onAIImageFinished(const QImage &image, const QString &prompt);
    void resetAIJobState();

    /** Resolve one or more subject cutouts (multi-select) + optional scene. */
    bool resolveCompositeSubjects(QVector<CompositeHelper::SubjectSpec> *subjectsOut,
                                  QString *errorOut);
    bool resolveCompositeInputs(bool preferMaskedSubject, bool placeCutoutOnCanvas,
                                QImage *subjectOut, QImage *sceneOut,
                                QString *errorOut);
    void startCompositeBlend(const QVector<CompositeHelper::SubjectSpec> &subjects,
                             const QImage &background,
                             const AICompositeDialog::Result &dlgResult);
    /** Send cutouts to AI to generate a new integrated scene (lighting etc.). */
    void startAiIntegrateCutouts(const QVector<CompositeHelper::SubjectSpec> &subjects,
                                 const QImage &optionalScene,
                                 const AICompositeDialog::Result &dlgResult);
    void finishCompositePipeline(const QImage &blended, const QString &prompt);

    // Icon theme toggle: when true, always use generic (programmatic) icons
    bool m_useGenericIcons = true;
    AIImageClient *m_aiImageClient = nullptr;

    /** Active AI job — finished handler dispatches on this (no reconnect races). */
    enum class AIJobKind {
        None,
        Generate,
        Edit,
        CompositeBackground,
        CompositeBlend,
        CompositeAiIntegrate
    };
    AIJobKind m_aiJobKind = AIJobKind::None;
    bool m_aiReplaceSelected = false;
    QVector<CompositeHelper::SubjectSpec> m_compositeSubjects;
    AICompositeDialog::Result m_compositeDlgResult;
};