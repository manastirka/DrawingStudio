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

// SimpleTextPanel core + format slots (refactor E30).

// --- SimpleTextPanel ---
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


// --- setTextTool ---
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


// --- updateFromTool ---
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


// --- updateFromPrimitive ---
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


// --- onFontFamilyChanged ---
void SimpleTextPanel::onFontFamilyChanged(const QString& fontFamily)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setFontFamily(fontFamily);
    if (m_currentPrimitive) {
        m_currentPrimitive->setFontFamily(fontFamily);
        emit onToolPropertyChanged();
    }
}


// --- onFontSizeChanged ---
void SimpleTextPanel::onFontSizeChanged(int size)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setFontSize(size);
    if (m_currentPrimitive) {
        m_currentPrimitive->setFontSize(size);
        emit onToolPropertyChanged();
    }
}


// --- onBoldToggled ---
void SimpleTextPanel::onBoldToggled(bool bold)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setBold(bold);
    if (m_currentPrimitive) {
        m_currentPrimitive->setBold(bold);
        emit onToolPropertyChanged();
    }
}


// --- onItalicToggled ---
void SimpleTextPanel::onItalicToggled(bool italic)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setItalic(italic);
    if (m_currentPrimitive) {
        m_currentPrimitive->setItalic(italic);
        emit onToolPropertyChanged();
    }
}


// --- onUnderlineToggled ---
void SimpleTextPanel::onUnderlineToggled(bool underline)
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setUnderline(underline);
    if (m_currentPrimitive) {
        m_currentPrimitive->setUnderline(underline);
        emit onToolPropertyChanged();
    }
}


// --- onSubscriptToggled ---
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


// --- onSuperscriptToggled ---
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


// --- onColorButtonClicked ---
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


// --- onAlignLeftClicked ---
void SimpleTextPanel::onAlignLeftClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Left);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Left);
        emit onToolPropertyChanged();
    }
}


// --- onAlignCenterClicked ---
void SimpleTextPanel::onAlignCenterClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Center);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Center);
        emit onToolPropertyChanged();
    }
}


// --- onAlignRightClicked ---
void SimpleTextPanel::onAlignRightClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Right);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Right);
        emit onToolPropertyChanged();
    }
}


// --- onAlignJustifyClicked ---
void SimpleTextPanel::onAlignJustifyClicked()
{
    if (m_updatingFromTool) return;
    if (m_textTool) m_textTool->setAlignment(ClassicTextTool::Alignment::Justify);
    if (m_currentPrimitive) {
        m_currentPrimitive->setAlignment(TextPrimitive::TextAlignment::Justify);
        emit onToolPropertyChanged();
    }
}


// --- onIncreaseSizeClicked ---
void SimpleTextPanel::onIncreaseSizeClicked()
{
    if (!m_textTool) return;
    m_textTool->increaseFontSize();
    updateFromTool();
}


// --- onDecreaseSizeClicked ---
void SimpleTextPanel::onDecreaseSizeClicked()
{
    if (!m_textTool) return;
    m_textTool->decreaseFontSize();
    updateFromTool();
}


// --- onToolPropertyChanged ---
void SimpleTextPanel::onToolPropertyChanged()
{
    updateFromTool();
}


// --- connectSignals ---
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


// --- updateColorButton ---
void SimpleTextPanel::updateColorButton()
{
    // Reflect the current text colour in the swatch bar of the "A" icon.
    m_colorButton->setIcon(IconFactory::textColor(m_currentColor));
}


// --- updateAlignmentButtons ---
void SimpleTextPanel::updateAlignmentButtons()
{
    if (!m_textTool) return;
    
    ClassicTextTool::Alignment alignment = m_textTool->alignment();
    
    m_alignLeftButton->setChecked(alignment == ClassicTextTool::Alignment::Left);
    m_alignCenterButton->setChecked(alignment == ClassicTextTool::Alignment::Center);
    m_alignRightButton->setChecked(alignment == ClassicTextTool::Alignment::Right);
    m_alignJustifyButton->setChecked(alignment == ClassicTextTool::Alignment::Justify);
}


// --- blockSignalsTemporarily ---
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


