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
#include <memory>
#include "GuitarDimensions.h"

class DrawingCanvas;
class PropertyPanel;
class GuitarSpecsBrowserDialog;
class GuitarProject;
class DrawingPrimitive;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void exitApplication();
    
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void deleteSelected();
    
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();
    void toggleGrid();
    void toggleSnap();
    void toggleMagneticConnection();
    void setGridSizeFine();
    void setGridSizeMedium();
    void setGridSizeCoarse();
    
    void selectTool();
    void lineTool();
    void curveTool();
    void bezierTool();
    void splineTool();
    void rectangleTool();
    void ellipseTool();
    void eraserTool();
    void measureTool();
    
    void setUnitsMillimeters();
    void setUnitsCentimeters();
    void setUnitsInches();
    
    void showAbout();
    void showHelp();
    void showGuitarDimensions();
    void showGuitarSpecsDatabase();
    void createParametricGuitar();
    
    // Blueprint functionality
    void loadBlueprint();
    void toggleBlueprint();
    void clearBlueprint();
    void adjustBlueprintOpacity();
    void adjustBlueprintScale();

private:
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupStatusBar();
    void setupDockWidgets();
    void connectSignals();
    
    bool maybeSave();
    void setCurrentFile(const QString &fileName);
    void updateRecentFileActions();
    void updateWindowTitle();
    void updatePropertyPanel();
    void onPropertyChanged(const QString& objectName, const QString& propertyName, const QVariant& value);
    void applyPropertyToPrimitive(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value);
    void addComponentToPanel(const QString& componentType, const QVector2D& position);
    void updateComponentsTree();
    void onComponentSelectionChanged();
    void onComponentPromoted(DrawingPrimitive* primitive, const QString& componentName);
    void showComponentProperties(const QString& componentType, const QString& componentName);
    
    // Parametric guitar generation
    void createParametricGuitar(const GuitarDimensions& dimensions);
    void generateGuitarBody(const GuitarDimensions& dimensions);
    void generateGuitarNeck(const GuitarDimensions& dimensions);
    void generateGuitarHeadstock(const GuitarDimensions& dimensions);
    void generateGuitarPickups(const GuitarDimensions& dimensions);
    void generateGuitarBridge(const GuitarDimensions& dimensions);
    void generateGuitarControls(const GuitarDimensions& dimensions);

    // UI Components
    DrawingCanvas *m_canvas;
    QSplitter *m_centralSplitter;
    
    // Project
    std::unique_ptr<GuitarProject> m_project;
    
    // Dock Widgets
    QDockWidget *m_componentsDock;
    QDockWidget *m_propertiesDock;
    QDockWidget *m_layersDock;
    
    // Component widgets
    QTreeWidget *m_componentsTree;
    PropertyPanel *m_propertyPanel;
    QTreeWidget *m_layersTree;
    
    // Managers (simplified for now)
    // TODO: Add back managers after implementing proper classes
    
    // File management
    QString m_currentFile;
    bool m_isModified;
    
    // Recent files
    enum { MaxRecentFiles = 10 };
    QAction *m_recentFileActions[MaxRecentFiles];
    QAction *m_separatorAction;
    
    // Status bar widgets
    QLabel *m_statusLabel;
    QLabel *m_coordsLabel;
    QLabel *m_zoomLabel;
    
    // Tool action group for mutual exclusion
    QActionGroup *m_toolActionGroup;
    
    // Parametric guitar dimensions
    GuitarDimensions m_currentGuitarDimensions;
};
