#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFontComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSlider>
#include <QColor>

class ClassicTextTool;

/**
 * @brief Simple text properties panel for classic text tool
 * 
 * Provides essential text formatting controls:
 * - Font family and size
 * - Bold, italic toggles
 * - Text color picker
 * - Text alignment buttons
 * - Quick size adjustment
 */
class SimpleTextPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleTextPanel(QWidget* parent = nullptr);

    // Connect to text tool
    void setTextTool(ClassicTextTool* textTool);
    ClassicTextTool* textTool() const { return m_textTool; }

    // Update panel from tool properties
    void updateFromTool();
    
    // Update panel from selected text primitive
    void updateFromPrimitive(class TextPrimitive* primitive);

public slots:
    // Font controls
    void onFontFamilyChanged(const QString& fontFamily);
    void onFontSizeChanged(int size);
    void onBoldToggled(bool bold);
    void onItalicToggled(bool italic);
    void onUnderlineToggled(bool underline);
    void onSubscriptToggled(bool subscript);
    void onSuperscriptToggled(bool superscript);
    
    // Color control
    void onColorButtonClicked();
    
    // Alignment controls
    void onAlignLeftClicked();
    void onAlignCenterClicked();
    void onAlignRightClicked();
    void onAlignJustifyClicked();
    
    // Quick size controls
    void onIncreaseSizeClicked();
    void onDecreaseSizeClicked();
    
    // Effect controls
    void onTextEffectsToggled(bool enabled);
    void onShadowToggled(bool enabled);
    void onShadowColorClicked();
    void onShadowOffsetChanged();
    void onStrokeToggled(bool enabled);
    void onStrokeColorClicked();
    void onStrokeWidthChanged();
    void onGradientToggled(bool enabled);
    void onGradientStartColorClicked();
    void onGradientEndColorClicked();
    void onGradientAngleChanged();

private slots:
    void onToolPropertyChanged();

private:
    void setupUI();
    void connectSignals();
    void updateColorButton();
    void updateShadowColorButton();
    void updateStrokeColorButton();
    void updateGradientColorButtons();
    void updateAlignmentButtons();
    void blockSignalsTemporarily(bool blocked);

    // Text tool reference
    ClassicTextTool* m_textTool;
    
    // Current text primitive being edited
    class TextPrimitive* m_currentPrimitive;

    // Main layout
    QVBoxLayout* m_mainLayout;

    // Font section
    QGroupBox* m_fontGroup;
    QFontComboBox* m_fontComboBox;
    QSpinBox* m_fontSizeSpinBox;
    QToolButton* m_boldButton;
    QToolButton* m_italicButton;
    QToolButton* m_underlineButton;
    QToolButton* m_subscriptButton;
    QToolButton* m_superscriptButton;
    QToolButton* m_colorButton;

    // Size adjustment
    QHBoxLayout* m_sizeLayout;
    QToolButton* m_decreaseSizeButton;
    QToolButton* m_increaseSizeButton;

    // Alignment section
    QGroupBox* m_alignGroup;
    QHBoxLayout* m_alignLayout;
    QButtonGroup* m_alignButtonGroup;
    QToolButton* m_alignLeftButton;
    QToolButton* m_alignCenterButton;
    QToolButton* m_alignRightButton;
    QToolButton* m_alignJustifyButton;

    // Effects section
    QGroupBox* m_effectsGroup;
    QCheckBox* m_textEffectsEnabled;  // Master enable for all effects
    QCheckBox* m_shadowEnabled;
    QPushButton* m_shadowColorButton;
    QDoubleSpinBox* m_shadowOffsetX;
    QDoubleSpinBox* m_shadowOffsetY;
    QDoubleSpinBox* m_shadowBlur;
    
    // Stroke controls
    QCheckBox* m_strokeEnabled;
    QPushButton* m_strokeColorButton;
    QDoubleSpinBox* m_strokeWidth;
    
    // Gradient controls
    QCheckBox* m_gradientEnabled;
    QPushButton* m_gradientStartButton;
    QPushButton* m_gradientEndButton;
    QSlider* m_gradientAngleSlider;
    QLabel* m_gradientAngleLabel;

    // Current colors
    QColor m_currentColor;
    QColor m_currentShadowColor;
    QColor m_currentStrokeColor;
    QColor m_gradientStartColor;
    QColor m_gradientEndColor;
    
    // Update blocking
    bool m_updatingFromTool;
};
