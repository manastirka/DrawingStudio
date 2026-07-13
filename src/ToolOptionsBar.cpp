#include "ToolOptionsBar.h"

#include "ClassicTextTool.h"
#include "CommandManager.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "IconFactory.h"
#include "ImagePrimitive.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVariantMap>

// ToolOptionsBar core + buildUi (refactor E23).

// --- ToolOptionsBar ---
ToolOptionsBar::ToolOptionsBar(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    updateForTool(DrawingTool::Select);
}


// --- setHost ---
void ToolOptionsBar::setHost(Host host)
{
    m_host = std::move(host);
}


// --- setStatus ---
void ToolOptionsBar::setStatus(const QString &msg)
{
    if (m_host.setStatusText)
        m_host.setStatusText(msg);
}


// --- buildUi ---
void ToolOptionsBar::buildUi()
{
    // root is ToolOptionsBar (this)
    // Allow the widget to use available toolbar space naturally.
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    this->setMinimumWidth(0);
    this->setMaximumWidth(1600);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(10, 3, 10, 3);
    m_layout->setSpacing(10);
    
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
    m_selectByColor = Qt::black;
    
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
    m_layout->addWidget(m_setting1Label);
    m_layout->addWidget(m_setting1Slider);
    m_layout->addWidget(m_setting1ValueLabel);
    m_layout->addWidget(m_setting2Label);
    m_layout->addWidget(m_setting2Slider);
    m_layout->addWidget(m_setting2ValueLabel);
    m_layout->addWidget(m_setting3Label);
    m_layout->addWidget(m_setting3Slider);
    m_layout->addWidget(m_setting3ValueLabel);
    m_layout->addWidget(m_lineStyleLabel);
    m_layout->addWidget(m_lineStyleCombo);
    m_layout->addWidget(m_selectColorLabel);
    m_layout->addWidget(m_selectColorButton);
    m_layout->addWidget(m_pipetteButton);
    m_layout->addWidget(m_selectionModeLabel);
    m_layout->addWidget(m_selectionModeCombo);
    m_layout->addWidget(m_selectSimilarButton);
    m_layout->addWidget(m_textFontCombo);
    m_layout->addWidget(m_optSep1);
    m_layout->addWidget(m_boolSetting1);
    m_layout->addWidget(m_boolSetting2);
    m_layout->addWidget(m_textUnderlineBtn);
    m_layout->addWidget(m_textColorBtn);
    m_layout->addWidget(m_extractButton);
    m_layout->addWidget(m_maskSettingsButton);
    m_layout->addWidget(m_optSep2);
    m_layout->addWidget(m_presetLabel);
    m_layout->addWidget(m_presetCombo);
    m_layout->addWidget(m_savePresetButton);
    m_layout->addStretch();

    m_suggestionStrip = new QWidget(this);
    m_suggestionStripLayout = new QHBoxLayout(m_suggestionStrip);
    m_suggestionStripLayout->setContentsMargins(8, 0, 0, 0);
    m_suggestionStripLayout->setSpacing(4);
    m_suggestionStrip->setVisible(false);
    m_layout->addWidget(m_suggestionStrip);
// Initialize with Select tool settings
    if (m_extractButton) {
        connect(m_extractButton, &QPushButton::clicked, this, [this]() {
            emit extractSubjectClicked();
            if (m_host.extractSelectedSubjects)
                m_host.extractSelectedSubjects();
        });
    }
}


// --- toolKey ---
QString ToolOptionsBar::toolKey(DrawingTool tool) const
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


