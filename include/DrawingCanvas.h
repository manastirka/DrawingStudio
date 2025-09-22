#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QVector2D>
#include <QColor>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QEnterEvent>
#include <QRectF>
#include <map>
#include <vector>
#include "GuitarComponent.h"

class DrawingPrimitive;
class DimensionPrimitive;
class GuitarComponent;
class GuitarProject;
class BlueprintManager;
class BlueprintParser;
struct GuitarOutline;

enum class DrawingTool {
    Select,
    Line,
    Curve,
    BezierCurve,
    Spline,
    Arc,
    Circle,
    Rectangle,
    Ellipse,
    Eraser,
    Measure,
    Fillet
};

class DrawingCanvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    // Component information structure
    struct ComponentInfo {
        ComponentType type;
        QString subType;
        QString displayName;
    };
    
    // Measurement system
    enum class Units { Millimeters, Centimeters, Inches };
    explicit DrawingCanvas(QWidget *parent = nullptr);
    ~DrawingCanvas();

    // View manipulation
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();
    void pan(const QVector2D &delta);
    
    // Grid and snap
    void setGridVisible(bool visible);
    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const { return m_snapEnabled; }
    void setGridSize(float size);
    
    // Magnetic connection
    void setMagneticConnectionEnabled(bool enabled);
    bool isMagneticConnectionEnabled() const { return m_magneticConnectionEnabled; }
    void setMagneticConnectionTolerance(float tolerance);
    
    // Tools
    void setCurrentTool(DrawingTool tool);
    DrawingTool currentTool() const { return m_currentTool; }
    
    // Project
    void setProject(GuitarProject *project);
    
    // Coordinate conversion
    QVector2D screenToWorld(const QPoint &screenPos) const;
    QPoint worldToScreen(const QVector2D &worldPos) const;
    QVector2D snapToGrid(const QVector2D &pos) const;

signals:
    void coordinatesChanged(const QVector2D &worldCoords);
    void zoomChanged(float zoomLevel);
    void selectionChanged();
    void componentAdded(const QString &componentType, const QVector2D &position);
    void componentPromoted(DrawingPrimitive* primitive, const QString &componentName);
    
    // Blueprint signals
    void blueprintLoaded(const QString &fileName);
    void blueprintOutlineGenerated(int pointCount);

protected:
    // OpenGL
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    
    // Events
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

private slots:
    void addPickup();
    void addBridge();
    void addTuners();
    void addNut();
    void addFrets();
    void addInlay();

private:
    void setupContextMenu();
    void addComponentAtPosition(const QString &componentType, const QString &subType, const QVector2D &position);
    void addComponentByType(ComponentType type, const QString &subType);
    void promoteSelectedToComponent(ComponentType type, const QString &subType);
    QColor getComponentColor(ComponentType type, const QString &subType);
    
    
    void addMeasurement(ComponentType measurementType);
    void addPresetMeasurement(const QString &presetName);
    void updateProjection();
    void renderGrid();
    void renderBackgroundImage();
    void renderObjects();
    void renderPrimitives();
    void renderComponents();
    void renderDimensionTexts();
    void renderDimensionText(QPainter *painter, DimensionPrimitive *dimension);
    void renderSelection();
    void renderCrosshair();
    void renderTool();
    
    void handleSelectTool(QMouseEvent *event);
    void handleLineTool(QMouseEvent *event);
    void handleCurveTool(QMouseEvent *event);
    void handleBezierTool(QMouseEvent *event);
    void handleSplineTool(QMouseEvent *event);
    void handleRectangleTool(QMouseEvent *event);
    void handleEllipseTool(QMouseEvent *event);
    void handleEraserTool(QMouseEvent *event);
    void handleMeasureTool(QMouseEvent *event);
    
    void selectObjectsInRect(const QRectF &rect);
    void selectObjectAt(const QVector2D &pos);
    void clearSelection();
    
    // Control point interaction
    int findControlPointAt(const QVector2D &pos, float tolerance = 8.0f);
    bool isControlPointVisible(DrawingPrimitive* primitive, int index);
    void renderControlPoint(const QVector2D &point, bool highlighted = false);
    void startControlPointEdit(DrawingPrimitive* primitive, int controlPointIndex);
    void updateControlPoint(const QVector2D &newPos);
    void finishControlPointEdit();
    
    // Bezier creation helpers
    int findClosestControlPoint(const QVector2D &pos);
    
    // Magnetic connection helpers
    QVector2D findNearestLineEndpoint(const QVector2D &pos, float tolerance);
    QVector2D snapToLineEndpoint(const QVector2D &pos);
    
    // View state
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    QVector2D m_viewCenter;
    float m_zoomLevel;
    
    // Grid
    bool m_gridVisible;
    bool m_snapEnabled;
    float m_gridSize;
    
    // Magnetic connection
    bool m_magneticConnectionEnabled;
    float m_magneticConnectionTolerance;
    
    // Tools and interaction
    DrawingTool m_currentTool;
    bool m_isDrawing;
    bool m_isPanning;
    QPoint m_lastMousePos;
    QVector2D m_drawStartPos;
    QVector2D m_drawCurrentPos;
    
    // Bezier creation state
    int m_bezierCreationStage; // 0: waiting for start, 1: dragging to set end, 2: adjusting control points
    
    // Selection
    std::vector<DrawingPrimitive*> m_selectedObjects;
    QRectF m_selectionRect;
    bool m_isSelecting;
    
    // Control point editing
    bool m_isEditingControlPoints;
    int m_selectedControlPoint;
    DrawingPrimitive* m_editingPrimitive;
    
    // Context menu
    QMenu *m_contextMenu;
    QMenu *m_componentsMenu;
    
    // Project reference
    GuitarProject *m_project;
    
    // Measurement system
    Units m_units;
    float m_pixelsPerMM; // Base unit is always mm for calculations
    
    // Drawing primitives
    std::vector<std::unique_ptr<DrawingPrimitive>> m_primitives;
    std::unique_ptr<DrawingPrimitive> m_currentPrimitive;
    
    // Component tracking - maps primitive pointer to component info
    std::map<DrawingPrimitive*, ComponentInfo> m_promotedComponents;
    // Reverse mapping - maps component name to primitive pointer
    std::map<QString, DrawingPrimitive*> m_componentNameToPrimitive;
    
    // Rendering resources
    unsigned int m_gridVBO;
    unsigned int m_gridVAO;
    unsigned int m_mainShaderProgram;
    unsigned int m_gridShaderProgram;
    
    // Helper methods
    unsigned int compileShader(const char* source, unsigned int type);
    unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource);
    void generateGridVertices(std::vector<float>& vertices);
    
    // Tool action group for mutual exclusion
    QActionGroup *m_toolActionGroup;
    
    // Blueprint support
    std::unique_ptr<BlueprintManager> m_blueprintManager;
    std::unique_ptr<BlueprintParser> m_blueprintParser;
    
public:
    // Primitive management
    void addPrimitive(std::unique_ptr<DrawingPrimitive> primitive);
    void clearPrimitives();
    const std::vector<std::unique_ptr<DrawingPrimitive>>& primitives() const { return m_primitives; }
    
    // Blueprint functionality
    bool loadBlueprint(const QString &filePath);
    void setBlueprintVisible(bool visible);
    void setBlueprintOpacity(float opacity);
    void setBlueprintScale(float scale);
    void setBlueprintPosition(const QVector2D &position);
    void clearBlueprint();
    void generateGuitarOutlineFromBlueprint();
    bool hasBlueprint() const;
    bool isBlueprintVisible() const;
    float getBlueprintOpacity() const;
    float getBlueprintScale() const;
    QVector2D getBlueprintPosition() const;
    
    // Selection management
    const std::vector<DrawingPrimitive*>& selectedObjects() const { return m_selectedObjects; }
    
    // Component checking methods
    bool isPromotedComponent(DrawingPrimitive* primitive) const;
    ComponentInfo getComponentInfo(DrawingPrimitive* primitive) const;
    DrawingPrimitive* findPrimitiveByComponentName(const QString& componentName) const;
    void selectComponentByName(const QString& componentName);
    
    // Measurement system
    void setUnits(Units units) { m_units = units; updateStatusBar(); }
    Units getUnits() const { return m_units; }
    QString getUnitsString() const;
    float worldToUnits(float worldDistance) const;
    float unitsToWorld(float unitDistance) const;
    void updateStatusBar();
    
private:
    void createGuitarOutlinePrimitives(const GuitarOutline& outline);
};