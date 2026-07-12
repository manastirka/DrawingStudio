#include "SimpleTextPanel.h"
#include "ClassicTextTool.h"
#include "DrawingPrimitive.h"
#include "IconFactory.h"
#include <QColorDialog>
#include <QIcon>
#include <QFont>
#include <QToolButton>
#include <QSize>
#include <QSignalBlocker>
#include <QFrame>
#include <QDebug>

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

SimpleTextPanel::SimpleTextPanel(QWidget* parent)
    : QWidget(parent)
    , m_textTool(nullptr)
    , m_currentPrimitive(nullptr)
    , m_currentColor(Qt::black)
    , m_currentShadowColor(QColor(0, 0, 0, 128))
    , m_currentStrokeColor(Qt::black)
    , m_gradientStartColor(Qt::white)
    , m_gradientEndColor(Qt::black)
    , m_updatingFromTool(false)
{
    setupUI();
    connectSignals();
    setMaximumWidth(250);
    setMinimumWidth(200);
}

void SimpleTextPanel::setTextTool(ClassicTextTool* textTool)
{
    // Disconnect old tool
    if (m_textTool) {
        disconnect(m_textTool, nullptr, this, nullptr);
    }
    
    m_textTool = textTool;
    
    // Connect new tool
    if (m_textTool) {
        connect(m_textTool, &ClassicTextTool::propertyChanged,
                this, &SimpleTextPanel::onToolPropertyChanged);
        updateFromTool();
    }
}

void SimpleTextPanel::updateFromTool()
{
    if (!m_textTool) {
        return;
    }
    
    m_updatingFromTool = true;
    
    // Update font family
    m_fontComboBox->setCurrentFont(QFont(m_textTool->fontFamily()));
    
    // Update font size
    m_fontSizeSpinBox->setValue(m_textTool->fontSize());
    
    // Update style buttons
    m_boldButton->setChecked(m_textTool->isBold());
    m_italicButton->setChecked(m_textTool->isItalic());
    m_underlineButton->setChecked(m_textTool->isUnderline());
    m_subscriptButton->setChecked(m_textTool->isSubscript());
    m_superscriptButton->setChecked(m_textTool->isSuperscript());
    
    // Update color
    m_currentColor = m_textTool->textColor();
    updateColorButton();

    // Update alignment
    updateAlignmentButtons();

    // Update shadow properties
    m_shadowEnabled->setChecked(m_textTool->shadowEnabled());
    m_currentShadowColor = m_textTool->shadowColor();
    updateShadowColorButton();
    m_shadowOffsetX->setValue(m_textTool->shadowOffsetX());
    m_shadowOffsetY->setValue(m_textTool->shadowOffsetY());
    m_shadowBlur->setValue(m_textTool->shadowBlur());

    // Update stroke properties
    m_strokeEnabled->setChecked(m_textTool->strokeEnabled());
    m_currentStrokeColor = m_textTool->strokeColor();
    updateStrokeColorButton();
    m_strokeWidth->setValue(m_textTool->strokeWidth());

    // Update gradient properties
    m_gradientEnabled->setChecked(m_textTool->gradientEnabled());
    m_gradientStartColor = m_textTool->gradientStartColor();
    m_gradientEndColor = m_textTool->gradientEndColor();
    updateGradientColorButtons();
    m_gradientAngleSlider->setValue(static_cast<int>(m_textTool->gradientAngle()));
    m_gradientAngleLabel->setText(QString("%1°").arg(static_cast<int>(m_textTool->gradientAngle())));

    // Check if any effects are enabled
    bool hasEffects = m_textTool->shadowEnabled() || m_textTool->strokeEnabled() || m_textTool->gradientEnabled();
    m_textEffectsEnabled->setChecked(hasEffects);

    m_updatingFromTool = false;
}

void SimpleTextPanel::updateFromPrimitive(TextPrimitive* primitive)
{
    if (!primitive) {
        m_currentPrimitive = nullptr;
        return;
    }
    
    // Store reference to current primitive
    m_currentPrimitive = primitive;
    
    m_updatingFromTool = true;
    
    // Update font family
    m_fontComboBox->setCurrentFont(QFont(primitive->fontFamily()));
    
    // Update font size
    m_fontSizeSpinBox->setValue(static_cast<int>(primitive->fontSize()));
    
    // Update style buttons
    m_boldButton->setChecked(primitive->isBold());
    m_italicButton->setChecked(primitive->isItalic());
    m_underlineButton->setChecked(primitive->isUnderline());
    
    // Update baseline shift (subscript/superscript)
    m_subscriptButton->setChecked(primitive->baselineShift() == TextPrimitive::BaselineShift::Subscript);
    m_superscriptButton->setChecked(primitive->baselineShift() == TextPrimitive::BaselineShift::Superscript);
    
    // Update color
    m_currentColor = primitive->color();
    updateColorButton();
    
    // Update alignment buttons based on primitive alignment
    switch (primitive->alignment()) {
        case TextPrimitive::TextAlignment::Left:
            m_alignLeftButton->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Center:
            m_alignCenterButton->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Right:
            m_alignRightButton->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Justify:
            m_alignJustifyButton->setChecked(true);
            break;
    }
    
    // Update shadow properties
    m_shadowEnabled->setChecked(primitive->shadowEnabled());
    m_currentShadowColor = primitive->shadowColor();
    updateShadowColorButton();
    m_shadowOffsetX->setValue(primitive->shadowOffsetX());
    m_shadowOffsetY->setValue(primitive->shadowOffsetY());
    m_shadowBlur->setValue(primitive->shadowBlur());
    
    // Update stroke properties
    m_strokeEnabled->setChecked(primitive->strokeEnabled());
    m_currentStrokeColor = primitive->strokeColor();
    updateStrokeColorButton();
    m_strokeWidth->setValue(primitive->strokeWidth());
    
    // Update gradient properties
    m_gradientEnabled->setChecked(primitive->gradientEnabled());
    m_gradientStartColor = primitive->gradientStartColor();
    m_gradientEndColor = primitive->gradientEndColor();
    updateGradientColorButtons();
    m_gradientAngleSlider->setValue(static_cast<int>(primitive->gradientAngle()));
    m_gradientAngleLabel->setText(QString("%1°").arg(static_cast<int>(primitive->gradientAngle())));
    
    // Check if any effects are enabled
    bool hasEffects = primitive->shadowEnabled() || primitive->strokeEnabled() || primitive->gradientEnabled();
    m_textEffectsEnabled->setChecked(hasEffects);
    
    m_updatingFromTool = false;
}

// ============================================================================
// Font Controls
// ============================================================================

void SimpleTextPanel::onFontFamilyChanged(const QString& fontFamily)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setFontFamily(fontFamily);
    if (m_currentPrimitive) {
        m_currentPrimitive->setFontFamily(fontFamily);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onFontSizeChanged(int size)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setFontSize(size);
    if (m_currentPrimitive) {
        m_currentPrimitive->setFontSize(size);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onBoldToggled(bool bold)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setBold(bold);
    if (m_currentPrimitive) {
        m_currentPrimitive->setBold(bold);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onItalicToggled(bool italic)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setItalic(italic);
    if (m_currentPrimitive) {
        m_currentPrimitive->setItalic(italic);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onUnderlineToggled(bool underline)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setUnderline(underline);
    if (m_currentPrimitive) {
        m_currentPrimitive->setUnderline(underline);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onSubscriptToggled(bool subscript)
{
    if (m_updatingFromTool) return;

    if (subscript && m_superscriptButton->isChecked()) {
        QSignalBlocker blocker(m_superscriptButton);
        m_superscriptButton->setChecked(false);
    }

    if (m_textTool) m_textTool->setSubscript(subscript);
    if (m_currentPrimitive) {
        m_currentPrimitive->setBaselineShift(subscript ? TextPrimitive::BaselineShift::Subscript : TextPrimitive::BaselineShift::Normal);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onSuperscriptToggled(bool superscript)
{
    if (m_updatingFromTool) return;

    if (superscript && m_subscriptButton->isChecked()) {
        QSignalBlocker blocker(m_subscriptButton);
        m_subscriptButton->setChecked(false);
    }

    if (m_textTool) m_textTool->setSuperscript(superscript);
    if (m_currentPrimitive) {
        m_currentPrimitive->setBaselineShift(superscript ? TextPrimitive::BaselineShift::Superscript : TextPrimitive::BaselineShift::Normal);
        emit onToolPropertyChanged();
    }
}

// ============================================================================
// Color Control
// ============================================================================

void SimpleTextPanel::onColorButtonClicked()
{
    QColor newColor = QColorDialog::getColor(m_currentColor, this, "Select Text Color");
    if (newColor.isValid()) {
        m_currentColor = newColor;
        if (m_textTool) m_textTool->setTextColor(newColor);
        if (m_currentPrimitive) {
            m_currentPrimitive->setColor(newColor);
            emit onToolPropertyChanged();
        }
        updateColorButton();
    }
}

// ============================================================================
// Alignment Controls
// ============================================================================

void SimpleTextPanel::onAlignLeftClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Left);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Left);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onAlignCenterClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Center);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Center);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onAlignRightClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Right);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Right);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onAlignJustifyClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Justify);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Justify);
        emit onToolPropertyChanged();
    }
}

// ============================================================================
// Quick Size Controls
// ============================================================================

void SimpleTextPanel::onIncreaseSizeClicked()
{
    if (!m_textTool) return;
    m_textTool->increaseFontSize();
    updateFromTool();
}

void SimpleTextPanel::onDecreaseSizeClicked()
{
    if (!m_textTool) return;
    m_textTool->decreaseFontSize();
    updateFromTool();
}

// ============================================================================
// Private Slots
// ============================================================================

void SimpleTextPanel::onToolPropertyChanged()
{
    updateFromTool();
}

// ============================================================================
// Private Methods
// ============================================================================

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

void SimpleTextPanel::connectSignals()
{
    // Font controls
    connect(m_fontComboBox, &QFontComboBox::currentTextChanged,
            this, &SimpleTextPanel::onFontFamilyChanged);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SimpleTextPanel::onFontSizeChanged);
    connect(m_boldButton, &QAbstractButton::toggled, this, &SimpleTextPanel::onBoldToggled);
    connect(m_italicButton, &QAbstractButton::toggled, this, &SimpleTextPanel::onItalicToggled);
    connect(m_underlineButton, &QAbstractButton::toggled, this, &SimpleTextPanel::onUnderlineToggled);
    connect(m_subscriptButton, &QAbstractButton::toggled, this, &SimpleTextPanel::onSubscriptToggled);
    connect(m_superscriptButton, &QAbstractButton::toggled, this, &SimpleTextPanel::onSuperscriptToggled);
    connect(m_colorButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onColorButtonClicked);
    
    // Size controls
    connect(m_increaseSizeButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onIncreaseSizeClicked);
    connect(m_decreaseSizeButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onDecreaseSizeClicked);
    
    // Alignment controls
    connect(m_alignLeftButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onAlignLeftClicked);
    connect(m_alignCenterButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onAlignCenterClicked);
    connect(m_alignRightButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onAlignRightClicked);
    connect(m_alignJustifyButton, &QAbstractButton::clicked, this, &SimpleTextPanel::onAlignJustifyClicked);
    
    // Effects controls
    connect(m_textEffectsEnabled, &QCheckBox::toggled, this, &SimpleTextPanel::onTextEffectsToggled);
    connect(m_shadowEnabled, &QCheckBox::toggled, this, &SimpleTextPanel::onShadowToggled);
    connect(m_shadowColorButton, &QPushButton::clicked, this, &SimpleTextPanel::onShadowColorClicked);
    connect(m_shadowOffsetX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SimpleTextPanel::onShadowOffsetChanged);
    connect(m_shadowOffsetY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SimpleTextPanel::onShadowOffsetChanged);
    connect(m_shadowBlur, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SimpleTextPanel::onShadowOffsetChanged);
    
    // Stroke controls
    connect(m_strokeEnabled, &QCheckBox::toggled, this, &SimpleTextPanel::onStrokeToggled);
    connect(m_strokeColorButton, &QPushButton::clicked, this, &SimpleTextPanel::onStrokeColorClicked);
    connect(m_strokeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SimpleTextPanel::onStrokeWidthChanged);
    
    // Gradient controls
    connect(m_gradientEnabled, &QCheckBox::toggled, this, &SimpleTextPanel::onGradientToggled);
    connect(m_gradientStartButton, &QPushButton::clicked, this, &SimpleTextPanel::onGradientStartColorClicked);
    connect(m_gradientEndButton, &QPushButton::clicked, this, &SimpleTextPanel::onGradientEndColorClicked);
    connect(m_gradientAngleSlider, &QSlider::valueChanged, this, &SimpleTextPanel::onGradientAngleChanged);
}

void SimpleTextPanel::updateColorButton()
{
    // Reflect the current text colour in the swatch bar of the "A" icon.
    m_colorButton->setIcon(IconFactory::textColor(m_currentColor));
}

void SimpleTextPanel::updateAlignmentButtons()
{
    if (!m_textTool) return;
    
    ClassicTextTool::Alignment alignment = m_textTool->alignment();
    
    m_alignLeftButton->setChecked(alignment == ClassicTextTool::Alignment::Left);
    m_alignCenterButton->setChecked(alignment == ClassicTextTool::Alignment::Center);
    m_alignRightButton->setChecked(alignment == ClassicTextTool::Alignment::Right);
    m_alignJustifyButton->setChecked(alignment == ClassicTextTool::Alignment::Justify);
}

void SimpleTextPanel::blockSignalsTemporarily(bool blocked)
{
    m_fontComboBox->blockSignals(blocked);
    m_fontSizeSpinBox->blockSignals(blocked);
    m_boldButton->blockSignals(blocked);
    m_italicButton->blockSignals(blocked);
    m_underlineButton->blockSignals(blocked);
    m_subscriptButton->blockSignals(blocked);
    m_superscriptButton->blockSignals(blocked);
    m_alignLeftButton->blockSignals(blocked);
    m_alignCenterButton->blockSignals(blocked);
    m_alignRightButton->blockSignals(blocked);
    m_alignJustifyButton->blockSignals(blocked);
    
    // Block effect signals too
    if (m_textEffectsEnabled) m_textEffectsEnabled->blockSignals(blocked);
    if (m_shadowEnabled) m_shadowEnabled->blockSignals(blocked);
    if (m_shadowOffsetX) m_shadowOffsetX->blockSignals(blocked);
    if (m_shadowOffsetY) m_shadowOffsetY->blockSignals(blocked);
    if (m_shadowBlur) m_shadowBlur->blockSignals(blocked);
}

void SimpleTextPanel::updateShadowColorButton()
{
    QString styleSheet = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px solid #666666;"
        "   border-radius: 3px;"
        "   padding: 2px;"
        "   color: %2;"
        "}"
        "QPushButton:hover {"
        "   border: 2px solid #333333;"
        "}"
    ).arg(m_currentShadowColor.name())
     .arg(m_currentShadowColor.lightness() > 128 ? "black" : "white");
    
    if (m_shadowColorButton) {
        m_shadowColorButton->setStyleSheet(styleSheet);
    }
}

void SimpleTextPanel::updateStrokeColorButton()
{
    QString styleSheet = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px solid #666666;"
        "   border-radius: 3px;"
        "   padding: 2px;"
        "   color: %2;"
        "}"
        "QPushButton:hover {"
        "   border: 2px solid #333333;"
        "}"
    ).arg(m_currentStrokeColor.name())
     .arg(m_currentStrokeColor.lightness() > 128 ? "black" : "white");
    
    if (m_strokeColorButton) {
        m_strokeColorButton->setStyleSheet(styleSheet);
    }
}

void SimpleTextPanel::updateGradientColorButtons()
{
    QString startStyleSheet = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px solid #666666;"
        "   border-radius: 3px;"
        "   padding: 2px;"
        "   color: %2;"
        "}"
        "QPushButton:hover {"
        "   border: 2px solid #333333;"
        "}"
    ).arg(m_gradientStartColor.name())
     .arg(m_gradientStartColor.lightness() > 128 ? "black" : "white");
    
    QString endStyleSheet = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px solid #666666;"
        "   border-radius: 3px;"
        "   padding: 2px;"
        "   color: %2;"
        "}"
        "QPushButton:hover {"
        "   border: 2px solid #333333;"
        "}"
    ).arg(m_gradientEndColor.name())
     .arg(m_gradientEndColor.lightness() > 128 ? "black" : "white");
    
    if (m_gradientStartButton) {
        m_gradientStartButton->setStyleSheet(startStyleSheet);
    }
    if (m_gradientEndButton) {
        m_gradientEndButton->setStyleSheet(endStyleSheet);
    }
}

void SimpleTextPanel::onTextEffectsToggled(bool enabled)
{
    // Enable/disable all effect controls
    if (m_shadowEnabled) m_shadowEnabled->setEnabled(enabled);
    if (m_shadowColorButton) m_shadowColorButton->setEnabled(enabled);
    if (m_shadowOffsetX) m_shadowOffsetX->setEnabled(enabled);
    if (m_shadowOffsetY) m_shadowOffsetY->setEnabled(enabled);
    if (m_shadowBlur) m_shadowBlur->setEnabled(enabled);
    
    if (m_strokeEnabled) m_strokeEnabled->setEnabled(enabled);
    if (m_strokeColorButton) m_strokeColorButton->setEnabled(enabled);
    if (m_strokeWidth) m_strokeWidth->setEnabled(enabled);
    
    if (m_gradientEnabled) m_gradientEnabled->setEnabled(enabled);
    if (m_gradientStartButton) m_gradientStartButton->setEnabled(enabled);
    if (m_gradientEndButton) m_gradientEndButton->setEnabled(enabled);
    if (m_gradientAngleSlider) m_gradientAngleSlider->setEnabled(enabled);
    
    if (m_updatingFromTool) return;
    
    if (enabled) {
        // Enable shadow by default when effects are turned on
        if (m_shadowEnabled) m_shadowEnabled->setChecked(true);
        
        // Apply to text tool if available
        if (m_textTool) {
            m_textTool->setShadowEnabled(true);
            m_textTool->setShadowOffsetX(m_shadowOffsetX->value());
            m_textTool->setShadowOffsetY(m_shadowOffsetY->value());
            m_textTool->setShadowBlur(m_shadowBlur->value());
            m_textTool->setShadowColor(m_currentShadowColor);
        }
        
        // Apply to primitive if available
        if (m_currentPrimitive) {
            m_currentPrimitive->setShadowEnabled(true);
            m_currentPrimitive->setShadowOffsetX(m_shadowOffsetX->value());
            m_currentPrimitive->setShadowOffsetY(m_shadowOffsetY->value());
            m_currentPrimitive->setShadowBlur(m_shadowBlur->value());
            m_currentPrimitive->setShadowColor(m_currentShadowColor);
            emit onToolPropertyChanged();
        }
    } else {
        // Disable all effects
        if (m_shadowEnabled) m_shadowEnabled->setChecked(false);
        if (m_strokeEnabled) m_strokeEnabled->setChecked(false);
        if (m_gradientEnabled) m_gradientEnabled->setChecked(false);

        if (m_textTool) {
            m_textTool->setShadowEnabled(false);
            m_textTool->setStrokeEnabled(false);
            m_textTool->setGradientEnabled(false);
        }

        if (m_currentPrimitive) {
            m_currentPrimitive->setShadowEnabled(false);
            m_currentPrimitive->setStrokeEnabled(false);
            m_currentPrimitive->setGradientEnabled(false);
            emit onToolPropertyChanged();
        }
    }
}

void SimpleTextPanel::onShadowToggled(bool enabled)
{
    if (m_updatingFromTool) return;
    
    if (m_textTool) m_textTool->setShadowEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setShadowEnabled(enabled);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onShadowColorClicked()
{
    QColor color = QColorDialog::getColor(m_currentShadowColor, this, "Shadow Color");
    if (color.isValid()) {
        m_currentShadowColor = color;
        updateShadowColorButton();
        
        if (m_updatingFromTool) return;
        
        if (m_textTool) m_textTool->setShadowColor(color);
        if (m_currentPrimitive) {
            m_currentPrimitive->setShadowColor(color);
            emit onToolPropertyChanged();
        }
    }
}

void SimpleTextPanel::onShadowOffsetChanged()
{
    if (m_updatingFromTool) return;
    
    if (m_textTool) {
        m_textTool->setShadowOffsetX(m_shadowOffsetX->value());
        m_textTool->setShadowOffsetY(m_shadowOffsetY->value());
        m_textTool->setShadowBlur(m_shadowBlur->value());
    }
    
    if (m_currentPrimitive) {
        m_currentPrimitive->setShadowOffsetX(m_shadowOffsetX->value());
        m_currentPrimitive->setShadowOffsetY(m_shadowOffsetY->value());
        m_currentPrimitive->setShadowBlur(m_shadowBlur->value());
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onStrokeToggled(bool enabled)
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setStrokeEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setStrokeEnabled(enabled);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onStrokeColorClicked()
{
    QColor color = QColorDialog::getColor(m_currentStrokeColor, this, "Stroke Color");
    if (color.isValid()) {
        m_currentStrokeColor = color;
        updateStrokeColorButton();

        if (m_updatingFromTool) return;

        if (m_textTool) m_textTool->setStrokeColor(color);
        if (m_currentPrimitive) {
            m_currentPrimitive->setStrokeColor(color);
            emit onToolPropertyChanged();
        }
    }
}

void SimpleTextPanel::onStrokeWidthChanged()
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setStrokeWidth(m_strokeWidth->value());
    if (m_currentPrimitive) {
        m_currentPrimitive->setStrokeWidth(m_strokeWidth->value());
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onGradientToggled(bool enabled)
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setGradientEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setGradientEnabled(enabled);
        emit onToolPropertyChanged();
    }
}

void SimpleTextPanel::onGradientStartColorClicked()
{
    QColor color = QColorDialog::getColor(m_gradientStartColor, this, "Gradient Start Color");
    if (color.isValid()) {
        m_gradientStartColor = color;
        updateGradientColorButtons();

        if (m_updatingFromTool) return;

        if (m_textTool) m_textTool->setGradientStartColor(color);
        if (m_currentPrimitive) {
            m_currentPrimitive->setGradientStartColor(color);
            emit onToolPropertyChanged();
        }
    }
}

void SimpleTextPanel::onGradientEndColorClicked()
{
    QColor color = QColorDialog::getColor(m_gradientEndColor, this, "Gradient End Color");
    if (color.isValid()) {
        m_gradientEndColor = color;
        updateGradientColorButtons();

        if (m_updatingFromTool) return;

        if (m_textTool) m_textTool->setGradientEndColor(color);
        if (m_currentPrimitive) {
            m_currentPrimitive->setGradientEndColor(color);
            emit onToolPropertyChanged();
        }
    }
}

void SimpleTextPanel::onGradientAngleChanged()
{
    if (m_gradientAngleLabel) {
        m_gradientAngleLabel->setText(QString("%1°").arg(m_gradientAngleSlider->value()));
    }

    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setGradientAngle(m_gradientAngleSlider->value());
    if (m_currentPrimitive) {
        m_currentPrimitive->setGradientAngle(m_gradientAngleSlider->value());
        emit onToolPropertyChanged();
    }
}
