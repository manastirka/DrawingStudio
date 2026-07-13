#include "SimpleTextPanel.h"
#include "ClassicTextTool.h"
#include "DrawingPrimitive.h"
#include "IconFactory.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QFont>
#include <QFontComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
// Shared logical icon size and square button footprint for the text toolbar.
constexpr int kToolIconPx = 20;
constexpr int kToolButtonPx = 32;

// Configures a flat, checkable-capable QToolButton with a consistent footprint.
void configureToolButton(QToolButton* btn, const QIcon& icon,
                         const QString& tooltip, bool checkable)
{
    btn->setIcon(icon);
    btn->setIconSize(QSize(kToolIconPx, kToolIconPx));
    btn->setToolTip(tooltip);
    btn->setCheckable(checkable);
    btn->setAutoRaise(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFixedSize(kToolButtonPx, kToolButtonPx);
}
} // namespace


// SimpleTextPanel::setupUI (refactor E30).

// --- setupUI ---
void SimpleTextPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);

    // Font section
    m_fontGroup = new QGroupBox("Font", this);
    QGridLayout* fontLayout = new QGridLayout(m_fontGroup);
    
    // Font family
    fontLayout->addWidget(new QLabel("Family:"), 0, 0);
    m_fontComboBox = new QFontComboBox();
    fontLayout->addWidget(m_fontComboBox, 0, 1, 1, 2);
    
    // Font size
    fontLayout->addWidget(new QLabel("Size:"), 1, 0);
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 200);
    m_fontSizeSpinBox->setValue(24);
    fontLayout->addWidget(m_fontSizeSpinBox, 1, 1);
    
    // Quick size buttons (icon-based, flanking the size spinbox)
    m_sizeLayout = new QHBoxLayout();
    m_sizeLayout->setSpacing(4);
    m_decreaseSizeButton = new QToolButton();
    configureToolButton(m_decreaseSizeButton, IconFactory::fontDecrease(),
                        "Decrease font size", false);
    m_increaseSizeButton = new QToolButton();
    configureToolButton(m_increaseSizeButton, IconFactory::fontIncrease(),
                        "Increase font size", false);
    m_sizeLayout->addWidget(m_decreaseSizeButton);
    m_sizeLayout->addWidget(m_increaseSizeButton);
    fontLayout->addLayout(m_sizeLayout, 1, 2);

    // Style row: B I U  |  sub sup  |  color
    QHBoxLayout* styleLayout = new QHBoxLayout();
    styleLayout->setSpacing(4);

    m_boldButton = new QToolButton();
    configureToolButton(m_boldButton, IconFactory::bold(), "Bold (Ctrl+B)", true);

    m_italicButton = new QToolButton();
    configureToolButton(m_italicButton, IconFactory::italic(), "Italic (Ctrl+I)", true);

    m_underlineButton = new QToolButton();
    configureToolButton(m_underlineButton, IconFactory::underline(), "Underline (Ctrl+U)", true);

    m_subscriptButton = new QToolButton();
    configureToolButton(m_subscriptButton, IconFactory::subscript(), "Subscript", true);

    m_superscriptButton = new QToolButton();
    configureToolButton(m_superscriptButton, IconFactory::superscript(), "Superscript", true);

    m_colorButton = new QToolButton();
    configureToolButton(m_colorButton, IconFactory::textColor(m_currentColor), "Text color", false);

    styleLayout->addWidget(m_boldButton);
    styleLayout->addWidget(m_italicButton);
    styleLayout->addWidget(m_underlineButton);

    QFrame* styleSep = new QFrame();
    styleSep->setFrameShape(QFrame::VLine);
    styleSep->setStyleSheet("color: #43454a;");
    styleLayout->addWidget(styleSep);

    styleLayout->addWidget(m_subscriptButton);
    styleLayout->addWidget(m_superscriptButton);
    styleLayout->addStretch();
    styleLayout->addWidget(m_colorButton);

    fontLayout->addLayout(styleLayout, 2, 0, 1, 3);

    m_mainLayout->addWidget(m_fontGroup);

    // Alignment section
    m_alignGroup = new QGroupBox("Alignment", this);
    m_alignLayout = new QHBoxLayout(m_alignGroup);
    
    m_alignLayout->setSpacing(4);
    m_alignButtonGroup = new QButtonGroup(this);
    m_alignButtonGroup->setExclusive(true);

    m_alignLeftButton = new QToolButton();
    configureToolButton(m_alignLeftButton, IconFactory::alignLeft(), "Align left", true);
    m_alignLeftButton->setChecked(true);
    m_alignButtonGroup->addButton(m_alignLeftButton, 0);

    m_alignCenterButton = new QToolButton();
    configureToolButton(m_alignCenterButton, IconFactory::alignCenter(), "Align center", true);
    m_alignButtonGroup->addButton(m_alignCenterButton, 1);

    m_alignRightButton = new QToolButton();
    configureToolButton(m_alignRightButton, IconFactory::alignRight(), "Align right", true);
    m_alignButtonGroup->addButton(m_alignRightButton, 2);

    m_alignJustifyButton = new QToolButton();
    configureToolButton(m_alignJustifyButton, IconFactory::alignJustify(), "Justify", true);
    m_alignButtonGroup->addButton(m_alignJustifyButton, 3);

    m_alignLayout->addWidget(m_alignLeftButton);
    m_alignLayout->addWidget(m_alignCenterButton);
    m_alignLayout->addWidget(m_alignRightButton);
    m_alignLayout->addWidget(m_alignJustifyButton);
    m_alignLayout->addStretch();

    m_mainLayout->addWidget(m_alignGroup);
    
    // Effects section — visually flat container so the inner effect groups
    // get the full panel width instead of being squeezed by double nesting.
    m_effectsGroup = new QGroupBox(this);
    m_effectsGroup->setObjectName("effectsContainer");
    m_effectsGroup->setTitle(QString());
    QVBoxLayout* effectsLayout = new QVBoxLayout(m_effectsGroup);
    effectsLayout->setContentsMargins(0, 0, 0, 0);
    effectsLayout->setSpacing(8);

    // Master enable checkbox for all text effects
    m_textEffectsEnabled = new QCheckBox("Enable Text Effects");
    m_textEffectsEnabled->setChecked(false);
    effectsLayout->addWidget(m_textEffectsEnabled);
    
    // Drop Shadow
    QGroupBox* shadowGroup = new QGroupBox("Drop Shadow");
    QGridLayout* shadowLayout = new QGridLayout(shadowGroup);
    
    m_shadowEnabled = new QCheckBox("Enable Shadow");
    m_shadowEnabled->setChecked(false);
    m_shadowEnabled->setEnabled(false);
    shadowLayout->addWidget(m_shadowEnabled, 0, 0, 1, 2);
    
    shadowLayout->addWidget(new QLabel("Color:"), 1, 0);
    m_shadowColorButton = new QPushButton();
    m_shadowColorButton->setFixedSize(40, 24);
    m_shadowColorButton->setEnabled(false);
    shadowLayout->addWidget(m_shadowColorButton, 1, 1);
    
    shadowLayout->addWidget(new QLabel("Offset X:"), 2, 0);
    m_shadowOffsetX = new QDoubleSpinBox();
    m_shadowOffsetX->setRange(-100.0, 100.0);
    m_shadowOffsetX->setValue(0.0);
    m_shadowOffsetX->setSuffix(" px");
    m_shadowOffsetX->setEnabled(false);
    shadowLayout->addWidget(m_shadowOffsetX, 2, 1);
    
    shadowLayout->addWidget(new QLabel("Offset Y:"), 3, 0);
    m_shadowOffsetY = new QDoubleSpinBox();
    m_shadowOffsetY->setRange(-100.0, 100.0);
    m_shadowOffsetY->setValue(0.0);
    m_shadowOffsetY->setSuffix(" px");
    m_shadowOffsetY->setEnabled(false);
    shadowLayout->addWidget(m_shadowOffsetY, 3, 1);
    
    shadowLayout->addWidget(new QLabel("Blur:"), 4, 0);
    m_shadowBlur = new QDoubleSpinBox();
    m_shadowBlur->setRange(0.0, 50.0);
    m_shadowBlur->setValue(0.0);
    m_shadowBlur->setSuffix(" px");
    m_shadowBlur->setEnabled(false);
    shadowLayout->addWidget(m_shadowBlur, 4, 1);
    
    effectsLayout->addWidget(shadowGroup);
    
    // Stroke
    QGroupBox* strokeGroup = new QGroupBox("Stroke");
    QGridLayout* strokeLayout = new QGridLayout(strokeGroup);
    
    m_strokeEnabled = new QCheckBox("Enable Stroke");
    m_strokeEnabled->setChecked(false);
    m_strokeEnabled->setEnabled(false);
    strokeLayout->addWidget(m_strokeEnabled, 0, 0, 1, 2);
    
    strokeLayout->addWidget(new QLabel("Color:"), 1, 0);
    m_strokeColorButton = new QPushButton();
    m_strokeColorButton->setFixedSize(40, 24);
    m_strokeColorButton->setEnabled(false);
    strokeLayout->addWidget(m_strokeColorButton, 1, 1);
    
    strokeLayout->addWidget(new QLabel("Width:"), 2, 0);
    m_strokeWidth = new QDoubleSpinBox();
    m_strokeWidth->setRange(0.1, 20.0);
    m_strokeWidth->setValue(2.0);
    m_strokeWidth->setSuffix(" px");
    m_strokeWidth->setEnabled(false);
    strokeLayout->addWidget(m_strokeWidth, 2, 1);
    
    effectsLayout->addWidget(strokeGroup);
    
    // Gradient
    QGroupBox* gradientGroup = new QGroupBox("Gradient Fill");
    QGridLayout* gradientLayout = new QGridLayout(gradientGroup);
    
    m_gradientEnabled = new QCheckBox("Enable Gradient");
    m_gradientEnabled->setChecked(false);
    m_gradientEnabled->setEnabled(false);
    gradientLayout->addWidget(m_gradientEnabled, 0, 0, 1, 2);
    
    gradientLayout->addWidget(new QLabel("Start:"), 1, 0);
    m_gradientStartButton = new QPushButton();
    m_gradientStartButton->setFixedSize(40, 24);
    m_gradientStartButton->setEnabled(false);
    gradientLayout->addWidget(m_gradientStartButton, 1, 1);
    
    gradientLayout->addWidget(new QLabel("End:"), 2, 0);
    m_gradientEndButton = new QPushButton();
    m_gradientEndButton->setFixedSize(40, 24);
    m_gradientEndButton->setEnabled(false);
    gradientLayout->addWidget(m_gradientEndButton, 2, 1);
    
    gradientLayout->addWidget(new QLabel("Angle:"), 3, 0);
    QHBoxLayout* angleLayout = new QHBoxLayout();
    m_gradientAngleSlider = new QSlider(Qt::Horizontal);
    m_gradientAngleSlider->setRange(0, 360);
    m_gradientAngleSlider->setValue(0);
    m_gradientAngleSlider->setEnabled(false);
    m_gradientAngleLabel = new QLabel("0°");
    angleLayout->addWidget(m_gradientAngleSlider);
    angleLayout->addWidget(m_gradientAngleLabel);
    gradientLayout->addLayout(angleLayout, 3, 1);
    
    effectsLayout->addWidget(gradientGroup);
    
    m_mainLayout->addWidget(m_effectsGroup);
    
    // Add stretch at bottom
    m_mainLayout->addStretch();

    // Styling: match the app's dark, modern UI (Photoshop/Figma-ish)
    setStyleSheet(R"(
        SimpleTextPanel {
            background: transparent;
        }
        QLabel {
            color: #dfe1e5;
            font-size: 12px;
            font-weight: 400;
        }
        QLabel:disabled {
            color: #9298a3;
        }
        QGroupBox {
            color: #f2f3f5;
            font-size: 13px;
            font-weight: 600;
            border: 1px solid #43454a;
            border-radius: 8px;
            margin-top: 12px;
            padding: 12px 8px 8px 8px;
            background: rgba(43, 45, 48, 0.35);
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px 0 6px;
            background: #2b2d30;
            border-radius: 4px;
        }
        QGroupBox#effectsContainer {
            border: none;
            background: transparent;
            margin-top: 0px;
            padding: 0px;
        }
        QCheckBox {
            color: #dfe1e5;
            font-size: 12px;
            spacing: 6px;
        }
        QCheckBox:disabled {
            color: #6f737c;
        }
        QFontComboBox, QComboBox, QSpinBox, QDoubleSpinBox {
            background-color: #1e1f22;
            color: #dfe1e5;
            border: 1px solid #43454a;
            border-radius: 6px;
            padding: 6px 8px;
            min-height: 26px;
        }
        QFontComboBox:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: rgba(138, 180, 255, 0.65);
        }
        QFontComboBox:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #8ab4ff;
        }
        QFontComboBox:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            color: #9298a3;
            background-color: #232428;
            border-color: #3a3c40;
        }
        QPushButton:disabled {
            color: #9298a3;
            background-color: #232428;
            border-color: #3a3c40;
        }
        QPushButton {
            background-color: #2b2d30;
            color: #dfe1e5;
            border: 1px solid #43454a;
            border-radius: 6px;
            padding: 6px 10px;
            min-height: 26px;
        }
        QPushButton:hover {
            border-color: rgba(138, 180, 255, 0.65);
            background-color: rgba(99, 102, 241, 0.12);
        }
        QPushButton:checked {
            background-color: rgba(99, 102, 241, 0.25);
            border-color: rgba(99, 102, 241, 0.6);
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 2px;
        }
        QToolButton:hover {
            background-color: rgba(138, 180, 255, 0.12);
            border-color: rgba(138, 180, 255, 0.45);
        }
        QToolButton:pressed {
            background-color: rgba(138, 180, 255, 0.20);
        }
        QToolButton:checked {
            background-color: rgba(42, 130, 218, 0.28);
            border-color: rgba(42, 130, 218, 0.9);
        }
        QCheckBox {
            color: #dfe1e5;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #1e1f22;
            border: 1px solid #43454a;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            border-radius: 7px;
            background: #8ab4ff;
        }
    )");

    updateColorButton();
    updateShadowColorButton();
    updateStrokeColorButton();
    updateGradientColorButtons();
}


