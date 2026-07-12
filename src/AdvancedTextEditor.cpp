#include "AdvancedTextEditor.h"
#include "DrawingCanvas.h"
#include "LayerManager.h"
#include <QApplication>
#include <QScreen>
#include <QSplitter>
#include <QGridLayout>
#include <QFormLayout>
#include <QPainter>
#include <QTimer>
#include <QColorDialog>
#include <QFontDialog>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextListFormat>
#include <QDebug>
#include <QSignalBlocker>
#include <cmath>

AdvancedTextEditor::AdvancedTextEditor(QWidget* parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_textEditor(nullptr)
    , m_currentPrimitive(nullptr)
    , m_currentTextColor(Qt::black)
    , m_currentShadowColor(QColor(0, 0, 0, 128))
    , m_currentStrokeColor(Qt::black)
    , m_gradientStart(Qt::white)
    , m_gradientEnd(Qt::black)
    , m_glowColor(QColor(255, 255, 0, 180))
    , m_livePreview(true)
    , m_previewTimer(new QTimer(this))
{
    setWindowTitle("Advanced Text Editor");
    setModal(false);
    resize(800, 700);
    
    setupUI();
    connectSignals();
    
    // Setup live preview timer
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(150); // Faster response for better UX
    connect(m_previewTimer, &QTimer::timeout, this, &AdvancedTextEditor::updateLivePreview);
}

void AdvancedTextEditor::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    
    setupToolbar();
    
    // Main splitter for panels
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Text editor
    m_textEditor = new QTextEdit(this);
    m_textEditor->setMinimumWidth(400);
    m_textEditor->setAcceptRichText(true);
    m_textEditor->setFont(QFont("Arial", 24));  // MATCH TEXT TOOL DEFAULT
    m_textEditor->setTextColor(Qt::black); // Set default text color to black
    m_textEditor->setStyleSheet(R"(
        QTextEdit {
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            padding: 12px;
            background: white;
            color: black;
            selection-background-color: #4CAF50;
        }
        QTextEdit:focus {
            border-color: #2196F3;
        }
    )");
    
    // Control panels container
    QWidget* controlsWidget = new QWidget();
    controlsWidget->setMinimumWidth(380);
    controlsWidget->setMaximumWidth(380);
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setSpacing(8);
    controlsLayout->setContentsMargins(8, 8, 8, 8);
    
    setupCharacterPanel();
    setupParagraphPanel();
    setupEffectsPanel();
    
    controlsLayout->addWidget(m_characterGroup);
    controlsLayout->addWidget(m_paragraphGroup);
    controlsLayout->addWidget(m_effectsGroup);
    controlsLayout->addStretch();
    
    mainSplitter->addWidget(m_textEditor);
    mainSplitter->addWidget(controlsWidget);
    mainSplitter->setSizes({500, 380});
    
    m_mainLayout->addWidget(m_toolbar);
    m_mainLayout->addWidget(mainSplitter);
    
    // Button layout
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();
    
    m_resetButton = new QPushButton("Reset", this);
    m_applyButton = new QPushButton("Apply", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_okButton = new QPushButton("OK", this);
    
    m_okButton->setDefault(true);
    
    m_buttonLayout->addWidget(m_resetButton);
    m_buttonLayout->addWidget(m_applyButton);
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_okButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
}

void AdvancedTextEditor::setupToolbar()
{
    m_toolbar = new QToolBar("Text Formatting", this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_toolbar->setStyleSheet(R"(
        QToolBar {
            background: #f0f2f5;
            border: 1px solid #c5c9d0;
            border-radius: 6px;
            padding: 4px;
            spacing: 4px;
        }
        QToolButton {
            background: #ffffff;
            color: #1a1a1a;
            border: 1px solid #c5c9d0;
            border-radius: 4px;
            padding: 6px 10px;
            margin: 2px;
            font-size: 13px;
            font-weight: 600;
        }
        QToolButton:hover {
            background: #e8f0fe;
            color: #0b57d0;
            border-color: #0b57d0;
        }
        QToolButton:checked,
        QToolButton:pressed {
            background: #d3e3fd;
            color: #0842a0;
            border-color: #0b57d0;
        }
        QToolBar::separator {
            background: #c5c9d0;
            width: 1px;
            margin: 4px 6px;
        }
    )");
    
    // Quick formatting actions — keep in sync with Character panel buttons
    m_toolbarBold = m_toolbar->addAction("B");
    m_toolbarBold->setCheckable(true);
    m_toolbarBold->setShortcut(QKeySequence::Bold);
    m_toolbarBold->setToolTip("Bold");
    
    m_toolbarItalic = m_toolbar->addAction("I");
    m_toolbarItalic->setCheckable(true);
    m_toolbarItalic->setShortcut(QKeySequence::Italic);
    m_toolbarItalic->setToolTip("Italic");
    
    m_toolbarUnderline = m_toolbar->addAction("U");
    m_toolbarUnderline->setCheckable(true);
    m_toolbarUnderline->setShortcut(QKeySequence::Underline);
    m_toolbarUnderline->setToolTip("Underline");
    
    m_toolbar->addSeparator();
    
    QAction* colorAction = m_toolbar->addAction("Color");
    colorAction->setToolTip("Text Color");
    
    QAction* fontAction = m_toolbar->addAction("Font…");
    fontAction->setToolTip("Font Dialog");
    
    connect(m_toolbarBold, &QAction::toggled, this, [this](bool checked) {
        if (m_boldButton && m_boldButton->isChecked() != checked)
            m_boldButton->setChecked(checked);
        pushToCanvas();
    });
    connect(m_toolbarItalic, &QAction::toggled, this, [this](bool checked) {
        if (m_italicButton && m_italicButton->isChecked() != checked)
            m_italicButton->setChecked(checked);
        pushToCanvas();
    });
    connect(m_toolbarUnderline, &QAction::toggled, this, [this](bool checked) {
        if (m_underlineButton && m_underlineButton->isChecked() != checked)
            m_underlineButton->setChecked(checked);
        pushToCanvas();
    });
    connect(colorAction, &QAction::triggered, this, &AdvancedTextEditor::showColorDialog);
    connect(fontAction, &QAction::triggered, this, &AdvancedTextEditor::showFontDialog);
}

void AdvancedTextEditor::setupCharacterPanel()
{
    m_characterGroup = new QGroupBox("Character", this);
    m_characterGroup->setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
        }
    )");
    
    QGridLayout* layout = new QGridLayout(m_characterGroup);
    layout->setSpacing(8);
    
    // Font family
    layout->addWidget(new QLabel("Font:"), 0, 0);
    m_fontFamily = new QFontComboBox();
    m_fontFamily->setFontFilters(QFontComboBox::ScalableFonts);
    layout->addWidget(m_fontFamily, 0, 1, 1, 2);
    
    // Font size
    layout->addWidget(new QLabel("Size:"), 1, 0);
    m_fontSize = new QSpinBox();
    m_fontSize->setRange(8, 500);
    m_fontSize->setValue(24);  // MATCH TEXT TOOL DEFAULT
    m_fontSize->setSuffix(" pt");
    layout->addWidget(m_fontSize, 1, 1);
    
    // Style buttons
    m_boldButton = new QPushButton("B");
    m_boldButton->setCheckable(true);
    m_boldButton->setFixedSize(32, 32);
    m_boldButton->setFont(QFont("Arial", 10, QFont::Bold));
    layout->addWidget(m_boldButton, 1, 2);
    
    m_italicButton = new QPushButton("I");
    m_italicButton->setCheckable(true);
    m_italicButton->setFixedSize(32, 32);
    m_italicButton->setFont(QFont("Arial", 10, QFont::Normal, true));
    layout->addWidget(m_italicButton, 2, 2);
    
    m_underlineButton = new QPushButton("U");
    m_underlineButton->setCheckable(true);
    m_underlineButton->setFixedSize(32, 32);
    QFont underlineFont("Arial", 10);
    underlineFont.setUnderline(true);
    m_underlineButton->setFont(underlineFont);
    layout->addWidget(m_underlineButton, 3, 2);
    
    // Color
    layout->addWidget(new QLabel("Color:"), 2, 0);
    m_colorButton = new QPushButton();
    m_colorButton->setFixedSize(50, 24);
    m_colorButton->setStyleSheet("background-color: black; border: 1px solid gray;");
    layout->addWidget(m_colorButton, 2, 1);
    
    // Character spacing (tracking) — matches TextPrimitive letterSpacing (px)
    layout->addWidget(new QLabel("Letter spacing:"), 3, 0);
    m_characterSpacing = new QDoubleSpinBox();
    m_characterSpacing->setRange(-50.0, 200.0);
    m_characterSpacing->setValue(0.0);
    m_characterSpacing->setSuffix(" px");
    m_characterSpacing->setDecimals(1);
    m_characterSpacing->setSingleStep(0.5);
    layout->addWidget(m_characterSpacing, 3, 1);
    
    // Vertical scale
    layout->addWidget(new QLabel("V Scale:"), 4, 0);
    m_verticalScale = new QDoubleSpinBox();
    m_verticalScale->setRange(10.0, 500.0);
    m_verticalScale->setValue(100.0);
    m_verticalScale->setSuffix(" %");
    layout->addWidget(m_verticalScale, 4, 1);
    
    // Horizontal scale
    layout->addWidget(new QLabel("H Scale:"), 4, 2);
    m_horizontalScale = new QDoubleSpinBox();
    m_horizontalScale->setRange(10.0, 500.0);
    m_horizontalScale->setValue(100.0);
    m_horizontalScale->setSuffix(" %");
    layout->addWidget(m_horizontalScale, 5, 1);
    
    // Baseline shift
    layout->addWidget(new QLabel("Baseline:"), 5, 0);
    m_baselineShift = new QDoubleSpinBox();
    m_baselineShift->setRange(-100.0, 100.0);
    m_baselineShift->setValue(0.0);
    m_baselineShift->setSuffix(" pt");
    layout->addWidget(m_baselineShift, 5, 2);
}

void AdvancedTextEditor::setupParagraphPanel()
{
    m_paragraphGroup = new QGroupBox("Paragraph", this);
    m_paragraphGroup->setStyleSheet(m_characterGroup->styleSheet());
    
    QGridLayout* layout = new QGridLayout(m_paragraphGroup);
    layout->setSpacing(8);
    
    // Alignment buttons
    layout->addWidget(new QLabel("Align:"), 0, 0);
    
    m_alignmentGroup = new QButtonGroup(this);
    
    m_alignLeft = new QPushButton("⬅");
    m_alignLeft->setCheckable(true);
    m_alignLeft->setFixedSize(32, 24);
    m_alignmentGroup->addButton(m_alignLeft, 0);
    layout->addWidget(m_alignLeft, 0, 1);
    
    m_alignCenter = new QPushButton("⬇");
    m_alignCenter->setCheckable(true);
    m_alignCenter->setFixedSize(32, 24);
    m_alignmentGroup->addButton(m_alignCenter, 1);
    layout->addWidget(m_alignCenter, 0, 2);
    
    m_alignRight = new QPushButton("➡");
    m_alignRight->setCheckable(true);
    m_alignRight->setFixedSize(32, 24);
    m_alignmentGroup->addButton(m_alignRight, 2);
    layout->addWidget(m_alignRight, 0, 3);
    
    m_alignJustify = new QPushButton("⬌");
    m_alignJustify->setCheckable(true);
    m_alignJustify->setFixedSize(32, 24);
    m_alignmentGroup->addButton(m_alignJustify, 3);
    layout->addWidget(m_alignJustify, 0, 4);
    
    // Line spacing
    layout->addWidget(new QLabel("Leading:"), 1, 0);
    m_lineSpacing = new QDoubleSpinBox();
    m_lineSpacing->setRange(0.5, 10.0);
    m_lineSpacing->setValue(1.2);
    m_lineSpacing->setSingleStep(0.1);
    m_lineSpacing->setDecimals(1);
    layout->addWidget(m_lineSpacing, 1, 1, 1, 2);
    
    // Paragraph spacing
    layout->addWidget(new QLabel("Space:"), 2, 0);
    m_paragraphSpacing = new QDoubleSpinBox();
    m_paragraphSpacing->setRange(0.0, 100.0);
    m_paragraphSpacing->setValue(0.0);
    m_paragraphSpacing->setSuffix(" pt");
    layout->addWidget(m_paragraphSpacing, 2, 1, 1, 2);
    
    // Indents
    layout->addWidget(new QLabel("Indent:"), 3, 0);
    m_firstLineIndent = new QDoubleSpinBox();
    m_firstLineIndent->setRange(-100.0, 100.0);
    m_firstLineIndent->setValue(0.0);
    m_firstLineIndent->setSuffix(" pt");
    layout->addWidget(m_firstLineIndent, 3, 1);
    
    layout->addWidget(new QLabel("Left:"), 4, 0);
    m_leftIndent = new QDoubleSpinBox();
    m_leftIndent->setRange(0.0, 500.0);
    m_leftIndent->setValue(0.0);
    m_leftIndent->setSuffix(" pt");
    layout->addWidget(m_leftIndent, 4, 1);
    
    layout->addWidget(new QLabel("Right:"), 4, 2);
    m_rightIndent = new QDoubleSpinBox();
    m_rightIndent->setRange(0.0, 500.0);
    m_rightIndent->setValue(0.0);
    m_rightIndent->setSuffix(" pt");
    layout->addWidget(m_rightIndent, 4, 3);
}

void AdvancedTextEditor::setupEffectsPanel()
{
    m_effectsGroup = new QGroupBox("Effects", this);
    m_effectsGroup->setStyleSheet(m_characterGroup->styleSheet());
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_effectsGroup);
    
    // Drop Shadow
    QGroupBox* shadowGroup = new QGroupBox("Drop Shadow");
    QGridLayout* shadowLayout = new QGridLayout(shadowGroup);

    m_dropShadowEnabled = new QCheckBox("Enabled");
    m_dropShadowEnabled->setChecked(false);
    shadowLayout->addWidget(m_dropShadowEnabled, 0, 0, 1, 2);
    
    shadowLayout->addWidget(new QLabel("Color:"), 1, 0);
    m_shadowColorButton = new QPushButton();
    m_shadowColorButton->setFixedSize(40, 20);
    m_shadowColorButton->setStyleSheet("background-color: rgba(0,0,0,180); border: 1px solid black;");
    shadowLayout->addWidget(m_shadowColorButton, 1, 1);
    
    shadowLayout->addWidget(new QLabel("Offset X:"), 2, 0);
    m_shadowOffsetX = new QDoubleSpinBox();
    m_shadowOffsetX->setRange(-100.0, 100.0);
    m_shadowOffsetX->setValue(0.0);
    m_shadowOffsetX->setSuffix(" px");
    m_shadowOffsetX->setSingleStep(1.0);
    shadowLayout->addWidget(m_shadowOffsetX, 2, 1);
    
    shadowLayout->addWidget(new QLabel("Offset Y:"), 3, 0);
    m_shadowOffsetY = new QDoubleSpinBox();
    m_shadowOffsetY->setRange(-100.0, 100.0);
    m_shadowOffsetY->setValue(0.0);
    m_shadowOffsetY->setSuffix(" px");
    m_shadowOffsetY->setSingleStep(1.0);
    shadowLayout->addWidget(m_shadowOffsetY, 3, 1);
    
    shadowLayout->addWidget(new QLabel("Blur:"), 4, 0);
    m_shadowBlur = new QDoubleSpinBox();
    m_shadowBlur->setRange(0.0, 50.0);
    m_shadowBlur->setValue(0.0);
    m_shadowBlur->setSuffix(" px");
    m_shadowBlur->setSingleStep(1.0);
    shadowLayout->addWidget(m_shadowBlur, 4, 1);
    
    mainLayout->addWidget(shadowGroup);
    
    // Stroke
    QGroupBox* strokeGroup = new QGroupBox("Stroke");
    QGridLayout* strokeLayout = new QGridLayout(strokeGroup);

    m_strokeEnabled = new QCheckBox("Enabled");
    m_strokeEnabled->setChecked(false);
    strokeLayout->addWidget(m_strokeEnabled, 0, 0, 1, 2);
    
    strokeLayout->addWidget(new QLabel("Color:"), 1, 0);
    m_strokeColorButton = new QPushButton();
    m_strokeColorButton->setFixedSize(40, 20);
    m_strokeColorButton->setStyleSheet("background-color: black; border: 1px solid gray;");
    strokeLayout->addWidget(m_strokeColorButton, 1, 1);
    
    strokeLayout->addWidget(new QLabel("Width:"), 2, 0);
    m_strokeWidth = new QDoubleSpinBox();
    m_strokeWidth->setRange(0.0, 20.0);
    m_strokeWidth->setValue(0.0);
    m_strokeWidth->setSuffix(" px");
    m_strokeWidth->setSingleStep(0.5);
    strokeLayout->addWidget(m_strokeWidth, 2, 1);
    
    mainLayout->addWidget(strokeGroup);
}

void AdvancedTextEditor::connectSignals()
{
    // Character controls
    connect(m_fontFamily, QOverload<const QFont&>::of(&QFontComboBox::currentFontChanged), 
            this, &AdvancedTextEditor::onFontFamilyChanged);
    connect(m_fontSize, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &AdvancedTextEditor::onFontSizeChanged);
    connect(m_boldButton, &QPushButton::toggled, this, &AdvancedTextEditor::onBoldToggled);
    connect(m_italicButton, &QPushButton::toggled, this, &AdvancedTextEditor::onItalicToggled);
    connect(m_underlineButton, &QPushButton::toggled, this, &AdvancedTextEditor::onUnderlineToggled);
    connect(m_colorButton, &QPushButton::clicked, this, &AdvancedTextEditor::showColorDialog);
    connect(m_characterSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvancedTextEditor::onCharacterSpacingChanged);
    connect(m_lineSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvancedTextEditor::onLineSpacingChanged);
    
    // Paragraph controls
    connect(m_alignmentGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            [this](QAbstractButton*) { onAlignmentChanged(); });
    
    // Text content changes → live preview
    connect(m_textEditor, &QTextEdit::textChanged, this, [this]() {
        if (m_livePreview)
            m_previewTimer->start();
    });
    
    // Buttons
    connect(m_okButton, &QPushButton::clicked, this, &AdvancedTextEditor::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &AdvancedTextEditor::onCancelClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &AdvancedTextEditor::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &AdvancedTextEditor::onResetClicked);
    
    // Effects
    connect(m_dropShadowEnabled, &QCheckBox::toggled, this, [this](bool) {
        updateLivePreview();
    });
    connect(m_strokeEnabled, &QCheckBox::toggled, this, [this](bool) {
        updateLivePreview();
    });
    connect(m_shadowColorButton, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(m_currentShadowColor, this, "Shadow Color");
        if (color.isValid()) {
            m_currentShadowColor = color;
            m_shadowColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid black;")
                                             .arg(color.name()));
            updateLivePreview();
        }
    });
    
    connect(m_shadowOffsetX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double) {
        updateLivePreview();
    });
    
    connect(m_shadowOffsetY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double) {
        updateLivePreview();
    });
    
    connect(m_shadowBlur, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double) {
        updateLivePreview();
    });
    
    connect(m_strokeColorButton, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(m_currentStrokeColor, this, "Stroke Color");
        if (color.isValid()) {
            m_currentStrokeColor = color;
            m_strokeColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
                                             .arg(color.name()));
            updateLivePreview();
        }
    });
    
    connect(m_strokeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double) {
        updateLivePreview();
    });
}

void AdvancedTextEditor::setText(const QString& text)
{
    m_textEditor->setPlainText(text);
}

void AdvancedTextEditor::setCanvas(DrawingCanvas* canvas)
{
    m_canvas = canvas;
}

TextPrimitive* AdvancedTextEditor::resolvePrimitive() const
{
    if (m_currentPrimitive)
        return m_currentPrimitive.data();
    if (!m_canvas || m_primitiveId.isNull() || !m_canvas->layerManager())
        return nullptr;
    for (DrawingPrimitive *p : m_canvas->layerManager()->getAllPrimitives()) {
        if (p && p->id() == m_primitiveId)
            return dynamic_cast<TextPrimitive *>(p);
    }
    return nullptr;
}

TextPrimitive* AdvancedTextEditor::currentPrimitive() const
{
    return resolvePrimitive();
}

void AdvancedTextEditor::setTextPrimitive(TextPrimitive* primitive)
{
    if (!primitive) return;

    m_currentPrimitive = primitive;
    m_primitiveId = primitive->id();
    // Ensure canvas text is visible (inline editor may have hidden it)
    primitive->setVisible(true);
    m_editSnapshot = primitive->toJson();
    loadUiFromPrimitive(primitive);
}

void AdvancedTextEditor::loadUiFromPrimitive(TextPrimitive* primitive)
{
    if (!primitive) return;

    const QSignalBlocker blockers[] = {
        QSignalBlocker(m_fontFamily),
        QSignalBlocker(m_fontSize),
        QSignalBlocker(m_boldButton),
        QSignalBlocker(m_italicButton),
        QSignalBlocker(m_underlineButton),
        QSignalBlocker(m_characterSpacing),
        QSignalBlocker(m_lineSpacing),
        QSignalBlocker(m_dropShadowEnabled),
        QSignalBlocker(m_strokeEnabled),
        QSignalBlocker(m_shadowOffsetX),
        QSignalBlocker(m_shadowOffsetY),
        QSignalBlocker(m_shadowBlur),
        QSignalBlocker(m_strokeWidth),
        QSignalBlocker(m_textEditor),
    };
    Q_UNUSED(blockers);

    m_textEditor->setPlainText(primitive->text());

    m_fontFamily->setCurrentFont(QFont(primitive->fontFamily()));
    m_fontSize->setValue(qMax(1, static_cast<int>(std::lround(primitive->fontSize()))));
    m_boldButton->setChecked(primitive->isBold());
    m_italicButton->setChecked(primitive->isItalic());
    m_underlineButton->setChecked(primitive->isUnderline());
    if (m_toolbarBold) m_toolbarBold->setChecked(primitive->isBold());
    if (m_toolbarItalic) m_toolbarItalic->setChecked(primitive->isItalic());
    if (m_toolbarUnderline) m_toolbarUnderline->setChecked(primitive->isUnderline());

    m_currentTextColor = primitive->color();
    m_textEditor->setTextColor(m_currentTextColor);
    m_colorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
                                .arg(m_currentTextColor.name()));

    switch (primitive->alignment()) {
        case TextPrimitive::TextAlignment::Left:
            m_alignLeft->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Center:
            m_alignCenter->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Right:
            m_alignRight->setChecked(true);
            break;
        case TextPrimitive::TextAlignment::Justify:
            m_alignJustify->setChecked(true);
            break;
    }

    m_characterSpacing->setValue(primitive->letterSpacing());
    m_lineSpacing->setValue(primitive->lineSpacing());

    m_dropShadowEnabled->setChecked(primitive->shadowEnabled());
    m_currentShadowColor = primitive->shadowColor();
    m_shadowColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid black;")
                                      .arg(m_currentShadowColor.name()));
    m_shadowOffsetX->setValue(primitive->shadowOffsetX());
    m_shadowOffsetY->setValue(primitive->shadowOffsetY());
    m_shadowBlur->setValue(primitive->shadowBlur());

    m_strokeEnabled->setChecked(primitive->strokeEnabled());
    m_currentStrokeColor = primitive->strokeColor();
    m_strokeColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
                                      .arg(m_currentStrokeColor.name()));
    m_strokeWidth->setValue(primitive->strokeWidth());
}

QString AdvancedTextEditor::getPlainText() const
{
    return m_textEditor->toPlainText();
}

QString AdvancedTextEditor::getRichText() const
{
    return m_textEditor->toHtml();
}

void AdvancedTextEditor::applyToTextPrimitive(TextPrimitive* primitive)
{
    if (!primitive) return;
    
    // Basic properties
    primitive->setText(m_textEditor->toPlainText());
    primitive->setFontFamily(m_fontFamily->currentFont().family());
    primitive->setFontSize(m_fontSize->value());
    primitive->setBold(m_boldButton->isChecked());
    primitive->setItalic(m_italicButton->isChecked());
    primitive->setUnderline(m_underlineButton->isChecked());
    primitive->setColor(m_currentTextColor);
    
    // Alignment
    if (m_alignLeft->isChecked()) {
        primitive->setAlignment(TextPrimitive::TextAlignment::Left);
    } else if (m_alignCenter->isChecked()) {
        primitive->setAlignment(TextPrimitive::TextAlignment::Center);
    } else if (m_alignRight->isChecked()) {
        primitive->setAlignment(TextPrimitive::TextAlignment::Right);
    } else if (m_alignJustify->isChecked()) {
        primitive->setAlignment(TextPrimitive::TextAlignment::Justify);
    }
    
    // Spacing (px)
    primitive->setLetterSpacing(static_cast<float>(m_characterSpacing->value()));
    primitive->setLineSpacing(static_cast<float>(m_lineSpacing->value()));
    
    // Effects
    primitive->setShadowEnabled(m_dropShadowEnabled->isChecked());
    primitive->setShadowColor(m_currentShadowColor);
    primitive->setShadowOffsetX(static_cast<float>(m_shadowOffsetX->value()));
    primitive->setShadowOffsetY(static_cast<float>(m_shadowOffsetY->value()));
    primitive->setShadowBlur(static_cast<float>(m_shadowBlur->value()));
    
    primitive->setStrokeEnabled(m_strokeEnabled->isChecked());
    primitive->setStrokeColor(m_currentStrokeColor);
    primitive->setStrokeWidth(static_cast<float>(m_strokeWidth->value()));
}

void AdvancedTextEditor::pushToCanvas()
{
    TextPrimitive *prim = resolvePrimitive();
    if (!prim)
        return;
    m_currentPrimitive = prim;
    applyToTextPrimitive(prim);
    emit textChanged();
    emit canvasNeedsUpdate();
    if (m_canvas) {
        m_canvas->update();
        m_canvas->repaint();
    }
}

void AdvancedTextEditor::revertToSnapshot()
{
    TextPrimitive *prim = resolvePrimitive();
    if (!prim || m_editSnapshot.isEmpty())
        return;
    prim->fromJson(m_editSnapshot);
    prim->setVisible(true);
    m_currentPrimitive = prim;
}

void AdvancedTextEditor::onOkClicked()
{
    pushToCanvas();
    emit applied();
    accept();
}

void AdvancedTextEditor::onApplyClicked()
{
    pushToCanvas();
    if (TextPrimitive *prim = resolvePrimitive())
        m_editSnapshot = prim->toJson();
    emit applied();
}

void AdvancedTextEditor::onCancelClicked()
{
    revertToSnapshot();
    emit canvasNeedsUpdate();
    if (m_canvas) {
        m_canvas->update();
        m_canvas->repaint();
    }
    reject();
}

void AdvancedTextEditor::onResetClicked()
{
    revertToSnapshot();
    if (TextPrimitive *prim = resolvePrimitive())
        loadUiFromPrimitive(prim);
    emit canvasNeedsUpdate();
    if (m_canvas) {
        m_canvas->update();
        m_canvas->repaint();
    }
}

void AdvancedTextEditor::showFontDialog()
{
    bool ok;
    QFont currentFont = m_textEditor->currentFont();
    QFont font = QFontDialog::getFont(&ok, currentFont, this, "Select Font");
    
    if (ok) {
        m_fontFamily->setCurrentFont(font);
        m_fontSize->setValue(font.pointSize());
        m_boldButton->setChecked(font.bold());
        m_italicButton->setChecked(font.italic());
        m_underlineButton->setChecked(font.underline());
        applyFormatting();
    }
}

void AdvancedTextEditor::showColorDialog()
{
    QColor color = QColorDialog::getColor(m_currentTextColor, this, "Text Color");
    if (color.isValid()) {
        m_currentTextColor = color;
        m_colorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
                                    .arg(color.name()));
        applyFormatting();
    }
}

void AdvancedTextEditor::updateFormatting()
{
    applyFormatting();
}

void AdvancedTextEditor::applyCharacterFormatting()
{
    applyFormatting();
}

void AdvancedTextEditor::applyFormatting()
{
    QTextCursor cursor = m_textEditor->textCursor();
    QTextCharFormat format;
    
    // Apply font properties
    QFont font(m_fontFamily->currentFont().family(), m_fontSize->value());
    font.setBold(m_boldButton->isChecked());
    font.setItalic(m_italicButton->isChecked());
    font.setUnderline(m_underlineButton->isChecked());
    
    format.setFont(font);
    format.setForeground(QBrush(m_currentTextColor));
    
    // Apply character spacing if supported
    if (m_characterSpacing->value() != 0.0) {
        format.setFontLetterSpacing(m_characterSpacing->value());
    }
    
    cursor.mergeCharFormat(format);
    
    // Apply paragraph formatting
    QTextBlockFormat blockFormat;
    
    if (m_alignLeft->isChecked()) {
        blockFormat.setAlignment(Qt::AlignLeft);
    } else if (m_alignCenter->isChecked()) {
        blockFormat.setAlignment(Qt::AlignCenter);
    } else if (m_alignRight->isChecked()) {
        blockFormat.setAlignment(Qt::AlignRight);
    } else if (m_alignJustify->isChecked()) {
        blockFormat.setAlignment(Qt::AlignJustify);
    }
    
    blockFormat.setLineHeight(m_lineSpacing->value() * 100.0, QTextBlockFormat::ProportionalHeight);
    blockFormat.setTopMargin(m_paragraphSpacing->value());
    blockFormat.setTextIndent(m_firstLineIndent->value());
    blockFormat.setLeftMargin(m_leftIndent->value());
    blockFormat.setRightMargin(m_rightIndent->value());
    
    cursor.mergeBlockFormat(blockFormat);
    
    // Push property panel state to the canvas primitive immediately
    if (m_livePreview)
        pushToCanvas();
    
    emit textChanged();
}

void AdvancedTextEditor::onFontFamilyChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::onFontSizeChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::onBoldToggled(bool checked)
{
    if (m_toolbarBold && m_toolbarBold->isChecked() != checked)
        m_toolbarBold->setChecked(checked);
    applyFormatting();
}

void AdvancedTextEditor::onItalicToggled(bool checked)
{
    if (m_toolbarItalic && m_toolbarItalic->isChecked() != checked)
        m_toolbarItalic->setChecked(checked);
    applyFormatting();
}

void AdvancedTextEditor::onUnderlineToggled(bool checked)
{
    if (m_toolbarUnderline && m_toolbarUnderline->isChecked() != checked)
        m_toolbarUnderline->setChecked(checked);
    applyFormatting();
}

void AdvancedTextEditor::onAlignmentChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::onTextColorChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::onCharacterSpacingChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::onLineSpacingChanged()
{
    applyFormatting();
}

void AdvancedTextEditor::updateLivePreview()
{
    pushToCanvas();
}

void AdvancedTextEditor::updatePreview()
{
    pushToCanvas();
}

void AdvancedTextEditor::onStyleEffectChanged()
{
    if (m_livePreview)
        pushToCanvas();
}