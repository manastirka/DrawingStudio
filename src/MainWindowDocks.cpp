#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "PropertyPanel.h"
#include "LayerPanel.h"
#include "ColorPalette.h"
#include "CommandManager.h"
#include "Commands.h"
#include "ClassicTextTool.h"
#include <QDockWidget>
#include <QStatusBar>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QWidget>
#include <QDebug>

// Dock widgets, status bar, undo history (refactor E15).

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
    ensureToolOptionsHost();
    if (m_canvas) {
        m_canvas->setClassicTextTool(m_classicTextTool);
    }
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

// connectSignals → MainWindowConnect.cpp (E14)

