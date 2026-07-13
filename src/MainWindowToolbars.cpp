#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "IconFactory.h"
#include "ToolOptionsBar.h"
#include "ToolIconProvider.h"
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QSize>
#include <QIcon>
#include <QMenu>
#include <QDebug>

// Toolbars + favorites strip (refactor E15).

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
    m_toolOptionsBar = new ToolOptionsBar();
    m_toolOptionsBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolOptionsBar->setMinimumWidth(0);
    m_toolOptionsBar->setMaximumWidth(1600);
    ensureToolOptionsHost();
    connect(m_toolOptionsBar, &ToolOptionsBar::extractSubjectClicked,
            this, &MainWindow::extractSelectedSubjects);

    m_suggestionRestoreTimer = new QTimer(this);
    m_suggestionRestoreTimer->setSingleShot(true);
    connect(m_suggestionRestoreTimer, &QTimer::timeout, this, &MainWindow::refreshSmartSuggestions);

    m_mainToolbar->addWidget(m_toolOptionsBar);
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

