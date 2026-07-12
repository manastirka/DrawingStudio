#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QFontComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QSlider>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTextCharFormat>
#include <QPointer>
#include <QJsonObject>
#include <QUuid>
#include <QAction>
#include "DrawingPrimitive.h"

class DrawingCanvas;

class AdvancedTextEditor : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedTextEditor(QWidget* parent = nullptr);
    
    // Set text to edit
    void setText(const QString& text);
    void setTextPrimitive(TextPrimitive* primitive);
    void setCanvas(class DrawingCanvas* canvas);
    
    // Get formatted text result
    QString getPlainText() const;
    QString getRichText() const;
    
    // Get updated text primitive properties
    void applyToTextPrimitive(TextPrimitive* primitive);
    void pushToCanvas(); // apply + refresh canvas

    TextPrimitive* currentPrimitive() const;
    QJsonObject editSnapshot() const { return m_editSnapshot; }

public slots:
    void showFontDialog();
    void showColorDialog();
    void applyFormatting();
    
signals:
    void textChanged();
    void canvasNeedsUpdate();
    void applied();

private slots:
    void onFontFamilyChanged();
    void onFontSizeChanged();
    void onBoldToggled(bool checked);
    void onItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onAlignmentChanged();
    void onTextColorChanged();
    void onCharacterSpacingChanged();
    void onLineSpacingChanged();
    void onStyleEffectChanged();
    void onOkClicked();
    void onApplyClicked();
    void onCancelClicked();
    void onResetClicked();

private:
    void setupUI();
    void setupToolbar();
    void setupCharacterPanel();
    void setupParagraphPanel();
    void setupEffectsPanel();
    void connectSignals();
    void updateFormatting();
    void applyCharacterFormatting();
    void updatePreview();
    void updateLivePreview(); // Real-time preview update
    void loadUiFromPrimitive(TextPrimitive* primitive);
    void revertToSnapshot();
    TextPrimitive* resolvePrimitive() const;
    
    // Main UI components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QTextEdit* m_textEditor;
    
    // Character formatting controls
    QGroupBox* m_characterGroup;
    QFontComboBox* m_fontFamily;
    QSpinBox* m_fontSize;
    QPushButton* m_boldButton;
    QPushButton* m_italicButton;
    QPushButton* m_underlineButton;
    QPushButton* m_colorButton;
    QDoubleSpinBox* m_characterSpacing;
    QDoubleSpinBox* m_verticalScale;
    QDoubleSpinBox* m_horizontalScale;
    QDoubleSpinBox* m_baselineShift;
    
    // Paragraph formatting controls
    QGroupBox* m_paragraphGroup;
    QButtonGroup* m_alignmentGroup;
    QPushButton* m_alignLeft;
    QPushButton* m_alignCenter;
    QPushButton* m_alignRight;
    QPushButton* m_alignJustify;
    QDoubleSpinBox* m_lineSpacing;
    QDoubleSpinBox* m_paragraphSpacing;
    QDoubleSpinBox* m_firstLineIndent;
    QDoubleSpinBox* m_leftIndent;
    QDoubleSpinBox* m_rightIndent;
    
    // Effects controls
    QGroupBox* m_effectsGroup;
    QCheckBox* m_dropShadowEnabled;
    QColorDialog* m_shadowColorDialog;
    QPushButton* m_shadowColorButton;
    QDoubleSpinBox* m_shadowOffsetX;
    QDoubleSpinBox* m_shadowOffsetY;
    QDoubleSpinBox* m_shadowBlur;
    QDoubleSpinBox* m_shadowOpacity;
    
    QCheckBox* m_strokeEnabled;
    QPushButton* m_strokeColorButton;
    QDoubleSpinBox* m_strokeWidth;
    QComboBox* m_strokePosition; // Inside, Center, Outside
    
    QCheckBox* m_gradientEnabled;
    QPushButton* m_gradientStartColor;
    QPushButton* m_gradientEndColor;
    QDoubleSpinBox* m_gradientAngle;
    QComboBox* m_gradientType; // Linear, Radial, Angular
    
    // Advanced effects
    QCheckBox* m_bevelEnabled;
    QComboBox* m_bevelStyle;
    QDoubleSpinBox* m_bevelDepth;
    QDoubleSpinBox* m_bevelSize;
    
    QCheckBox* m_glowEnabled;
    QPushButton* m_glowColorButton;
    QDoubleSpinBox* m_glowSize;
    QDoubleSpinBox* m_glowOpacity;
    QComboBox* m_glowType; // Inner, Outer
    
    // Text transformation
    QGroupBox* m_transformGroup;
    QComboBox* m_warpStyle;
    QDoubleSpinBox* m_warpHorizontal;
    QDoubleSpinBox* m_warpVertical;
    QDoubleSpinBox* m_warpPerspective;
    QDoubleSpinBox* m_rotation;
    QDoubleSpinBox* m_skewX;
    QDoubleSpinBox* m_skewY;
    
    // Control buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_applyButton;
    QPushButton* m_resetButton;
    
    // Current state
    QPointer<TextPrimitive> m_currentPrimitive;
    QUuid m_primitiveId;
    DrawingCanvas* m_canvas = nullptr;
    QJsonObject m_editSnapshot;
    QAction* m_toolbarBold = nullptr;
    QAction* m_toolbarItalic = nullptr;
    QAction* m_toolbarUnderline = nullptr;
    QColor m_currentTextColor;
    QColor m_currentShadowColor;
    QColor m_currentStrokeColor;
    QColor m_gradientStart;
    QColor m_gradientEnd;
    QColor m_glowColor;
    
    // Live preview
    bool m_livePreview;
    QTimer* m_previewTimer;
};