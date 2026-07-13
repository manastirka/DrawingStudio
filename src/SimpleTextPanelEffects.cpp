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

// SimpleTextPanel text effects slots (refactor E30).

// --- updateShadowColorButton ---
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


// --- updateStrokeColorButton ---
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


// --- updateGradientColorButtons ---
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


// --- onTextEffectsToggled ---
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


// --- onShadowToggled ---
void SimpleTextPanel::onShadowToggled(bool enabled)
{
    if (m_updatingFromTool) return;
    
    if (m_textTool) m_textTool->setShadowEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setShadowEnabled(enabled);
        emit onToolPropertyChanged();
    }
}


// --- onShadowColorClicked ---
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


// --- onShadowOffsetChanged ---
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


// --- onStrokeToggled ---
void SimpleTextPanel::onStrokeToggled(bool enabled)
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setStrokeEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setStrokeEnabled(enabled);
        emit onToolPropertyChanged();
    }
}


// --- onStrokeColorClicked ---
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


// --- onStrokeWidthChanged ---
void SimpleTextPanel::onStrokeWidthChanged()
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setStrokeWidth(m_strokeWidth->value());
    if (m_currentPrimitive) {
        m_currentPrimitive->setStrokeWidth(m_strokeWidth->value());
        emit onToolPropertyChanged();
    }
}


// --- onGradientToggled ---
void SimpleTextPanel::onGradientToggled(bool enabled)
{
    if (m_updatingFromTool) return;

    if (m_textTool) m_textTool->setGradientEnabled(enabled);
    if (m_currentPrimitive) {
        m_currentPrimitive->setGradientEnabled(enabled);
        emit onToolPropertyChanged();
    }
}


// --- onGradientStartColorClicked ---
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


// --- onGradientEndColorClicked ---
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


// --- onGradientAngleChanged ---
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

