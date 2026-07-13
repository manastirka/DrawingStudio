#pragma once

#include <QWidget>
#include <QColor>
#include <QVector2D>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QPolygonF>
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <vector>

// Managers / models used as members (complete types required)
#include "SelectionManager.h"
#include "GridManager.h"
#include "CanvasRenderer.h"
#include "UnitsConverter.h"
#include "MagneticSnap.h"
#include "PaperModel.h"
#include "AirbrushEngine.h"
#include "AlignmentGuides.h"
#include "DrawingTool.h"
// Interaction hosts (return types for make*Host — E14: single cluster of includes)
#include "tools/IDrawingTool.h"
#include "MoveController.h"
#include "SelectController.h"
#include "SelectionGestureController.h"
#include "ControlPointEditController.h"
#include "ViewPanController.h"

class DrawingPrimitive;
class LinePrimitive;
class DimensionPrimitive;
class TextPrimitive;
class ImagePrimitive;
class DrawingProject;
class LayerManager;
class AdvancedTextEditor;
class ClassicTextTool;
class Command;

class QPainter;
class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QContextMenuEvent;
class QEnterEvent;
class QMenu;
class QTimer;
class QUuid;
class QFont;

class DrawingCanvas : public QWidget
{
    Q_OBJECT

public:
    // Measurement system (alias — implementation in UnitsConverter)
    using Units = UnitsConverter::Units;

    // Selection interaction mode
    enum class SelectionMode { Rectangle, Lasso };

    // Fill tool mode
    enum class FillMode { Normal, Splash };

    // Alignment (values match ObjectLayoutOps::Align integer order)
    enum class AlignmentType {
        None,
        Left, Right, CenterHorizontal,
        Top, Bottom, CenterVertical,
        PageLeft, PageRight, PageCenterHorizontal,
        PageTop, PageBottom, PageCenterVertical
    };

    // Paper formats (alias — implementation in PaperModel)
    using PaperFormat = PaperModel::Format;

    // Constants
    static constexpr float DEFAULT_GRID_SIZE = 7.5591f;   // 2mm grid
    static constexpr float FINE_GRID_SIZE = 3.77953f;     // 1mm grid
    static constexpr float COARSE_GRID_SIZE = 37.7953f;   // 10mm grid
    static constexpr float DEFAULT_PIXELS_PER_MM = UnitsConverter::DEFAULT_PIXELS_PER_MM;
    static constexpr float DEFAULT_MAGNETIC_TOLERANCE = MagneticSnap::DEFAULT_TOLERANCE;

    explicit DrawingCanvas(QWidget *parent = nullptr);
    ~DrawingCanvas();

    // View manipulation
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();
    float zoomLevel() const { return m_zoomLevel; }

    // Grid and snap
    void setGridVisible(bool visible);
    bool isGridVisible() const;
    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const;
    void setGridSize(float size);
    float gridSize() const;
    void setGridColor(const QColor &color);
    const QColor &gridColor() const;

    // Pixel snapping
    void setPixelSnapEnabled(bool enabled) { m_pixelSnapEnabled = enabled; }
    bool isPixelSnapEnabled() const { return m_pixelSnapEnabled; }

    // Background
    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const { return m_backgroundColor; }

    // Paper color
    void setPaperColor(const QColor &color);
    QColor paperColor() const { return m_paper.color(); }

    // Default drawing style
    void setDefaultDrawingColor(const QColor &color);
    QColor defaultDrawingColor() const { return m_defaultDrawingColor; }
    void setDefaultLineWidth(float width) { m_defaultLineWidth = width; }
    float defaultLineWidth() const { return m_defaultLineWidth; }
    void setDefaultLineStyle(Qt::PenStyle style) { m_defaultLineStyle = style; }
    Qt::PenStyle defaultLineStyle() const { return m_defaultLineStyle; }
    void setDefaultFillEnabled(bool enabled) { m_defaultFillEnabled = enabled; }
    bool defaultFillEnabled() const { return m_defaultFillEnabled; }
    void setDefaultFillColor(const QColor &color) { m_defaultFillColor = color; }
    QColor defaultFillColor() const { return m_defaultFillColor; }

    // Brush / eraser settings
    void setBrushSize(float size) { m_brushSize = size; update(); }
    float brushSize() const { return m_brushSize; }
    void setEraserSize(float size) { m_eraserSize = size; update(); }
    float eraserSize() const { return m_eraserSize; }
    void setBrushHardness(float hardness) { m_brushHardness = hardness; }
    float brushHardness() const { return m_brushHardness; }
    void setFillMode(FillMode mode) { m_fillMode = mode; }
    FillMode fillMode() const { return m_fillMode; }

    // Zoom sensitivity
    void setZoomSensitivity(float sensitivity);
    float zoomSensitivity() const { return m_zoomSensitivity; }
    QVector2D viewCenter() const { return m_viewCenter; }

    // Paper format (façade over PaperModel)
    void setPaperFormat(PaperFormat format);
    PaperFormat paperFormat() const { return m_paper.format(); }
    QSizeF paperSize() const { return m_paper.sizeMm(); } // Size in mm
    QString paperFormatName() const;

    // Rulers
    void setRulersVisible(bool visible);
    bool areRulersVisible() const { return m_rulersVisible; }

    // Magnetic connection (façade over MagneticSnap)
    void setMagneticConnectionEnabled(bool enabled);
    bool isMagneticConnectionEnabled() const { return m_magneticSnap.isEnabled(); }
    void setMagneticConnectionTolerance(float tolerance);

    // Tools
    void setCurrentTool(DrawingTool tool);
    DrawingTool currentTool() const { return m_currentTool; }

    // Selection mode
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const { return m_selectionMode; }
    void setAdditiveSelection(bool enabled);
    void setSubtractiveSelection(bool enabled);

    // Alignment guides
    void setAlignmentGuidesEnabled(bool enabled) { m_showAlignmentGuides = enabled; }
    bool alignmentGuidesEnabled() const { return m_showAlignmentGuides; }

    // Project
    void setProject(DrawingProject *project);

    // Layer management
    void setLayerManager(LayerManager *layerManager);
    LayerManager *layerManager() const { return m_layerManager; }

    // Selection manager access
    SelectionManager *selectionManager() const { return m_selectionManager.get(); }

    // Coordinate conversion
    QVector2D screenToWorld(const QPoint &screenPos) const;
    QPoint worldToScreen(const QVector2D &worldPos) const;
    QVector2D snapToGrid(const QVector2D &pos) const;

    // Primitive management
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void addPrimitiveWithCommand(std::unique_ptr<DrawingPrimitive> primitive);
    void addTextPrimitive(TextPrimitive *primitive);
    void clearPrimitives();
    const std::vector<std::unique_ptr<DrawingPrimitive>> &primitives() const {
        return m_primitives;
    }

    // Selection management
    const std::vector<DrawingPrimitive *> &selectedObjects() const {
        return m_selectionManager->selectedObjects();
    }
    void deleteSelectedPrimitives();
    void deleteSelectedPrimitivesWithCommand();
    void clearSelection() {
        m_selectionManager->clearSelection();
        update();
    }
    void addToSelection(DrawingPrimitive *primitive) {
        m_selectionManager->addToSelection(primitive);
    }
    bool selectPrimitiveById(const QUuid &id);
    void selectByColor(const QColor &color, int tolerance = 10) {
        m_selectionManager->selectByColor(color, m_primitives, tolerance);
        update();
    }
    void enablePipetteMode(int tolerance) {
        m_selectionManager->enablePipetteMode(tolerance);
    }
    void setColorForSelection(const QColor &color) {
        m_selectionManager->setColorForSelection(color);
        update();
    }

    // Alignment / distribution
    void alignSelectedObjects(AlignmentType alignmentType);
    void distributeSelectedObjects(bool horizontal);

    // Soft grouping (shared groupId)
    void groupSelected();
    void ungroupSelected();
    void selectGroupMembers(DrawingPrimitive *seed);

    // Text on spline support
    void setSplineSelectionMode(bool enabled, TextPrimitive *textPrim = nullptr);
    void setSplineVisibilityForText(TextPrimitive *textPrim, bool visible);
    bool getSplineVisibilityForText(TextPrimitive *textPrim) const;

    // Measurement system (façade over UnitsConverter)
    void setUnits(Units units);
    Units getUnits() const { return m_unitsConverter.units(); }
    QString getUnitsString() const;
    float worldToUnits(float worldDistance) const;
    float unitsToWorld(float unitDistance) const;
    double pixelsPerUnit() const;
    float pixelsPerMM() const { return m_unitsConverter.pixelsPerMM(); }
    void updateStatusBar();

    // Export helpers
    QImage renderToImage(int w = 0, int h = 0);
    static QImage getImageFromPrimitive(ImagePrimitive *img);

    /** Inject ClassicTextTool so canvas does not depend on MainWindow. */
    void setClassicTextTool(ClassicTextTool *tool) { m_classicTextTool = tool; }
    ClassicTextTool *classicTextTool() const { return m_classicTextTool; }

signals:
    void coordinatesChanged(const QVector2D &worldCoords);
    void zoomChanged(float zoomLevel);
    void selectionChanged();
    void commandRequested(Command *command);
    void maskDetectionComplete(ImagePrimitive *image);
    void maskDetectionFailed(const QString &error);
    void maskDetectionProgress(int progress, const QString &message);
    void maskNavigationRequested(int direction);
    void maskInvertRequested();
    /// Live coaching hint while drawing (empty string clears to default suggestions).
    void smartHintChanged(const QString &hint);
    /** Host should open the advanced text editor dialog. */
    void advancedTextEditorRequested();
    /** Host should activate a tool (toolbar / options bar sync). */
    void toolChangeRequested(DrawingTool tool);

protected:
    // Events
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

private slots:
    void onAdvancedTextEditorRequested();

private:
    // --- Private API (impl in Paint / Hosts / Facades / Document TUs) ---
    void setupWorldTransform(QPainter &painter);
    void setupContextMenu();

    // Paint (DrawingCanvasPaint.cpp)
    void renderGrid(QPainter &painter);
    void renderRulers(QPainter &painter);
    void renderRulerTexts(QPainter &painter);
    void renderPaper(QPainter &painter);
    void renderObjects(QPainter &painter);
    void renderSelection(QPainter &painter);
    void renderTool(QPainter &painter);
    void renderCursorPreview(QPainter &painter);
    void renderSnapIndicator(QPainter &painter);
    void renderLassoOverlay(QPainter &painter);
    void renderDimensionTexts(QPainter &painter);
    void renderDimensionText(QPainter *painter, DimensionPrimitive *dimension);
    void renderTextPrimitives(QPainter &painter);
    void renderAlignmentGuides(QPainter &painter);
    void renderControlPoint(QPainter &painter, const QVector2D &point,
                            bool highlighted = false);
    void renderTextOnSpline(const TextPrimitive *textPrim, QPainter &painter);
    void renderFormattedText(QPainter *painter, const TextPrimitive *textPrim,
                             const QPoint &pos, const QFont &font, bool showBox);

    // Hosts (DrawingCanvasHosts.cpp)
    static constexpr int kHandleRotate = 8;
    ToolHost makeToolHost();
    void bindToolHostCallbacks(ToolHost &host);
    void bindToolHostSession(ToolHost &host);
    MoveHost makeMoveHost();
    SelectHost makeSelectHost();
    SelectionGestureHost makeSelectionGestureHost();
    ControlPointEditHost makeControlPointEditHost();
    ViewPanHost makeViewPanHost();

    // Facades (DrawingCanvasFacades.cpp)
    void selectionHandlePositions(const QRectF &br, QPointF out[9]) const;
    float selectionHandleHalfSize() const;
    float selectionHandleHitRadius() const;
    void drawSelectionHandles(QPainter &painter, const QRectF &br,
                              bool showRotate) const;
    int hitTestSelectionHandle(const QRectF &br, const QVector2D &worldPos,
                               bool includeRotate) const;
    Qt::CursorShape cursorForSelectionHandle(int index) const;
    bool showsGeometryControlPoints(const DrawingPrimitive *obj) const;
    bool supportsRotationHandle(const DrawingPrimitive *obj) const;
    QRectF computeResizedBounds(const QRectF &orig, int handleIndex,
                                const QVector2D &worldPos,
                                Qt::KeyboardModifiers mods) const;
    void applyObjectRotation(DrawingPrimitive *obj, float degrees);
    float objectRotationDegrees(const DrawingPrimitive *obj) const;
    bool usesExternalRotation(const DrawingPrimitive *obj) const;
    QVector2D toObjectLocal(const DrawingPrimitive *obj,
                            const QVector2D &worldPos) const;
    void handleSelectTool(QMouseEvent *event);
    void handleMoveTool(QMouseEvent *event);
    QVector2D snapAngleLineEndpoint(const QVector2D &origin,
                                    const QVector2D &rawEnd,
                                    Qt::KeyboardModifiers mods) const;
    DrawingPrimitive *findPrimitiveById(const QUuid &id) const;
    QVector2D resolveLineMoveConstraint(const LinePrimitive *line) const;
    QVector2D constrainDeltaAlongConnectedLine(
        const QVector2D &delta, Qt::KeyboardModifiers mods) const;
    QVector2D projectPointOntoLineSegment(const QVector2D &point,
                                          const QVector2D &a,
                                          const QVector2D &b) const;
    void selectObjectsInRect(const QRectF &rect,
                             SelectionManager::SelectionOperation operation);
    void selectObjectsInLasso(const QPolygonF &polygon,
                              SelectionManager::SelectionOperation operation);
    QColor getColorAtPosition(const QVector2D &pos);
    int findControlPointAt(const QVector2D &pos, float tolerance = 8.0f);
    void startControlPointEdit(DrawingPrimitive *primitive, int controlPointIndex);
    void updateControlPoint(const QVector2D &newPos);
    void finishControlPointEdit();
    void handleMoveOperation(const QVector2D &worldPos);
    void finishMoveOperation();
    bool beginCopyAlongConnectedLineMove();
    static bool isCopyAlongModifier(Qt::KeyboardModifiers mods);
    DrawingPrimitive *findPrimitiveAt(const QVector2D &pos, float tolerance = 5.0f);
    QVector2D findNearestLineEndpoint(const QVector2D &pos, float tolerance);
    QVector2D snapToLineEndpoint(const QVector2D &pos);
    QVector2D applySmartDrawingConstraints(const QVector2D &rawPos,
                                           Qt::KeyboardModifiers mods,
                                           QString *hintOut);
    bool computeCircleThroughPoints(const QVector2D &p1, const QVector2D &p2,
                                    const QVector2D &p3, QVector2D &centerOut,
                                    float &radiusOut);
    bool pointInPolygon(const QVector2D &point,
                        const std::vector<QVector2D> &polygon) const;
    void updateAlignmentGuides(const QVector2D &mousePos);
    bool isObjectAtAlignmentPosition(DrawingPrimitive *obj, float tolerance);

    // ----- Member state -----

    // Managers / rendering
    std::unique_ptr<GridManager> m_gridManager;
    std::unique_ptr<SelectionManager> m_selectionManager;
    std::unique_ptr<CanvasRenderer> m_renderer;

    // View state
    QVector2D m_viewCenter;
    float m_zoomLevel;

    // Selection state
    SelectionMode m_selectionMode;
    SelectionManager::SelectionOperation m_selectionOperation;
    bool m_forceAdditiveSelection;
    bool m_forceSubtractiveSelection;
    QPolygonF m_lassoPoints;

    // Colors
    QColor m_backgroundColor;
    QColor m_defaultDrawingColor;
    bool m_defaultFillEnabled;
    QColor m_defaultFillColor;

    // Zoom
    float m_zoomSensitivity;

    // Paper
    PaperModel m_paper;

    // Rulers
    bool m_rulersVisible;

    // Magnetic connection
    MagneticSnap m_magneticSnap;

    // Tools and interaction
    DrawingTool m_currentTool;
    bool m_pixelSnapEnabled;
    bool m_showCursorPreview;
    bool m_snapIndicatorActive;
    bool m_isDrawing;
    bool m_isPanning;
    int m_bezierCreationStage;

    // Fill / brush
    FillMode m_fillMode;
    bool m_isBrushing;
    bool m_isBlurring;
    QVector2D m_cursorPreviewPos;
    QVector2D m_snapIndicatorPos;
    float m_brushSize;
    float m_eraserSize;
    float m_brushHardness;
    std::vector<QVector2D> m_brushStroke;
    std::vector<float> m_brushParticleScale;
    std::vector<float> m_brushParticleAlpha;

    // Airbrush (cursor + pure engine state — D1)
    QTimer *m_airbrushTimer;
    QVector2D m_airbrushPos;
    AirbrushEngine::State m_airbrushState;

    // Default line style
    Qt::PenStyle m_defaultLineStyle;
    float m_defaultLineWidth;

    // Mouse state
    QPoint m_lastMousePos;
    QVector2D m_drawStartPos;
    QVector2D m_drawCurrentPos;
    QString m_lastSmartHint;

    // Angle line creation state
    int m_angleLineStage;
    QVector2D m_angleBaselineStart;
    QVector2D m_angleBaselineEnd;
    QUuid m_angleBaselinePrimitiveId;
    QVector2D m_angleBaselineConstraintDir;

    // Arc creation state
    int m_arcStage;
    QVector2D m_arcCenter;
    QVector2D m_arcStart;
    QVector2D m_arcEnd;

    // Control point editing
    bool m_isEditingControlPoints;
    int m_selectedControlPoint;
    DrawingPrimitive *m_editingPrimitive;
    QVector2D m_originalControlPointPosition;
    QJsonObject m_controlEditOrigState;

    // Move operations
    bool m_isMoving;
    bool m_isCopyAlongMove = false;
    QVector2D m_moveStartPos;
    QVector2D m_totalMoveOffset;
    std::vector<QVector2D> m_originalPositions;

    // Object resize state
    bool m_isResizingObject;
    DrawingPrimitive *m_resizingObject;
    int m_resizeHandleIndex;
    QRectF m_resizeOrigBounds;
    std::vector<QVector2D> m_resizeOrigControlPoints;
    QJsonObject m_resizeOrigState;

    // Object rotate state (selection handle)
    bool m_isRotatingObject;
    DrawingPrimitive *m_rotatingObject;
    QPointF m_rotationPivot;
    QJsonObject m_rotateOrigState;

    // Text rotate / resize state
    bool m_isRotatingText;
    bool m_isResizingText;
    TextPrimitive *m_rotatingTextPrimitive;
    TextPrimitive *m_resizingTextPrimitive;
    float m_rotationStartAngle;
    float m_initialRotation;
    int m_resizeCornerIndex;
    QVector2D m_resizeStartPos;
    float m_initialScale;
    int m_initialFontSize;
    QRectF m_initialTextBounds;

    // Spline selection (text on path)
    bool m_splineSelectionMode;
    TextPrimitive *m_textAwaitingSpline;

    // Long-press move (text)
    QTimer *m_longPressTimer;
    TextPrimitive *m_longPressText;
    QVector2D m_longPressStartPos;

    // Advanced text editor
    AdvancedTextEditor *m_advancedTextEditor = nullptr;

    // Text tool (owned by MainWindow; injected, not owned)
    ClassicTextTool *m_classicTextTool = nullptr;

    // Alignment guides (pure Axis guides from AlignmentGuides module)
    std::vector<AlignmentGuides::Guide> m_alignmentGuides;
    bool m_showAlignmentGuides;

    // Context menu
    QMenu *m_contextMenu;

    // Project / layers
    DrawingProject *m_project;
    LayerManager *m_layerManager;

    // Measurement system
    UnitsConverter m_unitsConverter;

    // Drawing primitives (legacy / fallback storage)
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
    std::unique_ptr<DrawingPrimitive> m_currentPrimitive;
};
