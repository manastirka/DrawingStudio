#include "GuitarDimensions.h"
#include <QSpinBox>
#include <QGridLayout>

GuitarDimensionsDialog::GuitarDimensionsDialog(const GuitarDimensions& dimensions, QWidget* parent)
    : QDialog(parent)
    , m_currentDimensions(dimensions)
{
    setWindowTitle("Guitar Dimensions");
    setModal(true);
    resize(500, 700);
    setupUI();
    setDimensions(dimensions);
}

void GuitarDimensionsDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Preset selection
    QHBoxLayout* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("Preset:"));
    
    m_presetCombo = new QComboBox();
    m_presetCombo->addItem("Custom");
    m_presetCombo->addItem("Les Paul Standard");
    m_presetCombo->addItem("Stratocaster");  
    m_presetCombo->addItem("Telecaster");
    presetLayout->addWidget(m_presetCombo);
    
    m_resetButton = new QPushButton("Load Preset");
    presetLayout->addWidget(m_resetButton);
    presetLayout->addStretch();
    
    m_mainLayout->addLayout(presetLayout);
    
    // Create dimension groups
    createBodyGroup();
    createNeckGroup();
    createPickupGroup();
    createBridgeGroup();
    createControlGroup();
    
    // Display scale
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    scaleLayout->addWidget(new QLabel("Display Scale:"));
    m_displayScaleSpin = new QDoubleSpinBox();
    m_displayScaleSpin->setRange(0.1, 5.0);
    m_displayScaleSpin->setSingleStep(0.1);
    m_displayScaleSpin->setValue(1.0);
    m_displayScaleSpin->setSuffix("x");
    scaleLayout->addWidget(m_displayScaleSpin);
    scaleLayout->addStretch();
    m_mainLayout->addLayout(scaleLayout);
    
    // Dialog buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_mainLayout->addWidget(m_buttonBox);
    
    // Connect signals
    connect(m_resetButton, &QPushButton::clicked, this, &GuitarDimensionsDialog::onResetToPreset);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void GuitarDimensionsDialog::createBodyGroup()
{
    m_bodyGroup = new QGroupBox("Body Dimensions (mm)");
    QFormLayout* layout = new QFormLayout(m_bodyGroup);
    
    m_bodyLengthSpin = new QDoubleSpinBox();
    m_bodyLengthSpin->setRange(300, 600);
    m_bodyLengthSpin->setSuffix(" mm");
    layout->addRow("Body Length:", m_bodyLengthSpin);
    
    m_bodyWidthSpin = new QDoubleSpinBox();
    m_bodyWidthSpin->setRange(200, 400);
    m_bodyWidthSpin->setSuffix(" mm");
    layout->addRow("Body Width:", m_bodyWidthSpin);
    
    m_bodyThicknessSpin = new QDoubleSpinBox();
    m_bodyThicknessSpin->setRange(30, 80);
    m_bodyThicknessSpin->setSuffix(" mm");
    layout->addRow("Body Thickness:", m_bodyThicknessSpin);
    
    m_upperBoutSpin = new QDoubleSpinBox();
    m_upperBoutSpin->setRange(200, 350);
    m_upperBoutSpin->setSuffix(" mm");
    layout->addRow("Upper Bout Width:", m_upperBoutSpin);
    
    m_lowerBoutSpin = new QDoubleSpinBox();
    m_lowerBoutSpin->setRange(200, 400);
    m_lowerBoutSpin->setSuffix(" mm");
    layout->addRow("Lower Bout Width:", m_lowerBoutSpin);
    
    m_waistWidthSpin = new QDoubleSpinBox();
    m_waistWidthSpin->setRange(150, 300);
    m_waistWidthSpin->setSuffix(" mm");
    layout->addRow("Waist Width:", m_waistWidthSpin);
    
    m_cutawayDepthSpin = new QDoubleSpinBox();
    m_cutawayDepthSpin->setRange(0, 150);
    m_cutawayDepthSpin->setSuffix(" mm");
    layout->addRow("Cutaway Depth:", m_cutawayDepthSpin);
    
    m_cutawayLengthSpin = new QDoubleSpinBox();
    m_cutawayLengthSpin->setRange(0, 200);
    m_cutawayLengthSpin->setSuffix(" mm");
    layout->addRow("Cutaway Length:", m_cutawayLengthSpin);
    
    m_mainLayout->addWidget(m_bodyGroup);
}

void GuitarDimensionsDialog::createNeckGroup()
{
    m_neckGroup = new QGroupBox("Neck Specifications");
    QFormLayout* layout = new QFormLayout(m_neckGroup);
    
    m_scaleLengthSpin = new QDoubleSpinBox();
    m_scaleLengthSpin->setRange(500, 800);
    m_scaleLengthSpin->setSuffix(" mm");
    layout->addRow("Scale Length:", m_scaleLengthSpin);
    
    m_neckLengthSpin = new QDoubleSpinBox();
    m_neckLengthSpin->setRange(300, 600);
    m_neckLengthSpin->setSuffix(" mm");
    layout->addRow("Neck Length:", m_neckLengthSpin);
    
    m_nutWidthSpin = new QDoubleSpinBox();
    m_nutWidthSpin->setRange(30, 60);
    m_nutWidthSpin->setSuffix(" mm");
    layout->addRow("Nut Width:", m_nutWidthSpin);
    
    m_neckWidth12Spin = new QDoubleSpinBox();
    m_neckWidth12Spin->setRange(40, 70);
    m_neckWidth12Spin->setSuffix(" mm");
    layout->addRow("12th Fret Width:", m_neckWidth12Spin);
    
    m_fretCountSpin = new QSpinBox();
    m_fretCountSpin->setRange(12, 27);
    layout->addRow("Fret Count:", m_fretCountSpin);
    
    m_tunerLayoutCombo = new QComboBox();
    m_tunerLayoutCombo->addItems({"3+3", "6-inline", "4-inline"});
    layout->addRow("Tuner Layout:", m_tunerLayoutCombo);
    
    m_mainLayout->addWidget(m_neckGroup);
}

void GuitarDimensionsDialog::createPickupGroup()
{
    m_pickupGroup = new QGroupBox("Pickup Configuration");
    QFormLayout* layout = new QFormLayout(m_pickupGroup);
    
    m_pickupCountSpin = new QSpinBox();
    m_pickupCountSpin->setRange(1, 4);
    layout->addRow("Pickup Count:", m_pickupCountSpin);
    
    m_pickupTypeCombo = new QComboBox();
    m_pickupTypeCombo->addItems({"Humbucker", "Single-coil", "P90"});
    layout->addRow("Pickup Type:", m_pickupTypeCombo);
    
    m_neckPickupPosSpin = new QDoubleSpinBox();
    m_neckPickupPosSpin->setRange(50, 300);
    m_neckPickupPosSpin->setSuffix(" mm");
    layout->addRow("Neck Pickup Position:", m_neckPickupPosSpin);
    
    m_middlePickupPosSpin = new QDoubleSpinBox();
    m_middlePickupPosSpin->setRange(30, 200);
    m_middlePickupPosSpin->setSuffix(" mm");
    layout->addRow("Middle Pickup Position:", m_middlePickupPosSpin);
    
    m_bridgePickupPosSpin = new QDoubleSpinBox();
    m_bridgePickupPosSpin->setRange(10, 100);
    m_bridgePickupPosSpin->setSuffix(" mm");
    layout->addRow("Bridge Pickup Position:", m_bridgePickupPosSpin);
    
    m_pickupWidthSpin = new QDoubleSpinBox();
    m_pickupWidthSpin->setRange(10, 50);
    m_pickupWidthSpin->setSuffix(" mm");
    layout->addRow("Pickup Width:", m_pickupWidthSpin);
    
    m_pickupLengthSpin = new QDoubleSpinBox();
    m_pickupLengthSpin->setRange(50, 120);
    m_pickupLengthSpin->setSuffix(" mm");
    layout->addRow("Pickup Length:", m_pickupLengthSpin);
    
    m_mainLayout->addWidget(m_pickupGroup);
}

void GuitarDimensionsDialog::createBridgeGroup()
{
    m_bridgeGroup = new QGroupBox("Bridge & Hardware");
    QFormLayout* layout = new QFormLayout(m_bridgeGroup);
    
    m_bridgeTypeCombo = new QComboBox();
    m_bridgeTypeCombo->addItems({"Tune-O-Matic", "Tremolo", "Hardtail", "Wraparound"});
    layout->addRow("Bridge Type:", m_bridgeTypeCombo);
    
    m_bridgeWidthSpin = new QDoubleSpinBox();
    m_bridgeWidthSpin->setRange(40, 120);
    m_bridgeWidthSpin->setSuffix(" mm");
    layout->addRow("Bridge Width:", m_bridgeWidthSpin);
    
    m_bridgeLengthSpin = new QDoubleSpinBox();
    m_bridgeLengthSpin->setRange(50, 150);
    m_bridgeLengthSpin->setSuffix(" mm");
    layout->addRow("Bridge Length:", m_bridgeLengthSpin);
    
    m_stringSpacingSpin = new QDoubleSpinBox();
    m_stringSpacingSpin->setRange(8, 15);
    m_stringSpacingSpin->setSuffix(" mm");
    layout->addRow("String Spacing:", m_stringSpacingSpin);
    
    m_mainLayout->addWidget(m_bridgeGroup);
}

void GuitarDimensionsDialog::createControlGroup()
{
    m_controlGroup = new QGroupBox("Control Layout");
    QFormLayout* layout = new QFormLayout(m_controlGroup);
    
    m_volumeKnobsSpin = new QSpinBox();
    m_volumeKnobsSpin->setRange(1, 4);
    layout->addRow("Volume Knobs:", m_volumeKnobsSpin);
    
    m_toneKnobsSpin = new QSpinBox();
    m_toneKnobsSpin->setRange(0, 4);
    layout->addRow("Tone Knobs:", m_toneKnobsSpin);
    
    m_switchTypeCombo = new QComboBox();
    m_switchTypeCombo->addItems({"3-way", "5-way", "None"});
    layout->addRow("Pickup Switch:", m_switchTypeCombo);
    
    m_knobSpacingSpin = new QDoubleSpinBox();
    m_knobSpacingSpin->setRange(20, 100);
    m_knobSpacingSpin->setSuffix(" mm");
    layout->addRow("Knob Spacing:", m_knobSpacingSpin);
    
    m_mainLayout->addWidget(m_controlGroup);
}

GuitarDimensions GuitarDimensionsDialog::getDimensions() const
{
    GuitarDimensions dims;
    
    // Body dimensions
    dims.bodyLength = m_bodyLengthSpin->value();
    dims.bodyWidth = m_bodyWidthSpin->value();
    dims.bodyThickness = m_bodyThicknessSpin->value();
    dims.upperBoutWidth = m_upperBoutSpin->value();
    dims.lowerBoutWidth = m_lowerBoutSpin->value();
    dims.waistWidth = m_waistWidthSpin->value();
    dims.cutawayDepth = m_cutawayDepthSpin->value();
    dims.cutawayLength = m_cutawayLengthSpin->value();
    
    // Neck dimensions  
    dims.scaleLength = m_scaleLengthSpin->value();
    dims.neckLength = m_neckLengthSpin->value();
    dims.nutWidth = m_nutWidthSpin->value();
    dims.neckWidth12th = m_neckWidth12Spin->value();
    dims.fretCount = m_fretCountSpin->value();
    dims.tunerLayout = m_tunerLayoutCombo->currentText();
    
    // Pickup specifications
    dims.pickupCount = m_pickupCountSpin->value();
    dims.pickupType = m_pickupTypeCombo->currentText();
    dims.neckPickupPosition = m_neckPickupPosSpin->value();
    dims.middlePickupPosition = m_middlePickupPosSpin->value();
    dims.bridgePickupPosition = m_bridgePickupPosSpin->value();
    dims.pickupWidth = m_pickupWidthSpin->value();
    dims.pickupLength = m_pickupLengthSpin->value();
    
    // Bridge specifications
    dims.bridgeType = m_bridgeTypeCombo->currentText();
    dims.bridgeWidth = m_bridgeWidthSpin->value();
    dims.bridgeLength = m_bridgeLengthSpin->value();
    dims.stringSpacing = m_stringSpacingSpin->value();
    
    // Control layout
    dims.volumeKnobs = m_volumeKnobsSpin->value();
    dims.toneKnobs = m_toneKnobsSpin->value();
    dims.switchType = m_switchTypeCombo->currentText();
    dims.knobSpacing = m_knobSpacingSpin->value();
    
    // Display settings
    dims.displayScale = m_displayScaleSpin->value();
    
    return dims;
}

void GuitarDimensionsDialog::setDimensions(const GuitarDimensions& dimensions)
{
    m_currentDimensions = dimensions;
    
    // Body dimensions
    m_bodyLengthSpin->setValue(dimensions.bodyLength);
    m_bodyWidthSpin->setValue(dimensions.bodyWidth);
    m_bodyThicknessSpin->setValue(dimensions.bodyThickness);
    m_upperBoutSpin->setValue(dimensions.upperBoutWidth);
    m_lowerBoutSpin->setValue(dimensions.lowerBoutWidth);
    m_waistWidthSpin->setValue(dimensions.waistWidth);
    m_cutawayDepthSpin->setValue(dimensions.cutawayDepth);
    m_cutawayLengthSpin->setValue(dimensions.cutawayLength);
    
    // Neck dimensions
    m_scaleLengthSpin->setValue(dimensions.scaleLength);
    m_neckLengthSpin->setValue(dimensions.neckLength);
    m_nutWidthSpin->setValue(dimensions.nutWidth);
    m_neckWidth12Spin->setValue(dimensions.neckWidth12th);
    m_fretCountSpin->setValue(dimensions.fretCount);
    m_tunerLayoutCombo->setCurrentText(dimensions.tunerLayout);
    
    // Pickup specifications
    m_pickupCountSpin->setValue(dimensions.pickupCount);
    m_pickupTypeCombo->setCurrentText(dimensions.pickupType);
    m_neckPickupPosSpin->setValue(dimensions.neckPickupPosition);
    m_middlePickupPosSpin->setValue(dimensions.middlePickupPosition);
    m_bridgePickupPosSpin->setValue(dimensions.bridgePickupPosition);
    m_pickupWidthSpin->setValue(dimensions.pickupWidth);
    m_pickupLengthSpin->setValue(dimensions.pickupLength);
    
    // Bridge specifications
    m_bridgeTypeCombo->setCurrentText(dimensions.bridgeType);
    m_bridgeWidthSpin->setValue(dimensions.bridgeWidth);
    m_bridgeLengthSpin->setValue(dimensions.bridgeLength);
    m_stringSpacingSpin->setValue(dimensions.stringSpacing);
    
    // Control layout
    m_volumeKnobsSpin->setValue(dimensions.volumeKnobs);
    m_toneKnobsSpin->setValue(dimensions.toneKnobs);
    m_switchTypeCombo->setCurrentText(dimensions.switchType);
    m_knobSpacingSpin->setValue(dimensions.knobSpacing);
    
    // Display settings
    m_displayScaleSpin->setValue(dimensions.displayScale);
}

void GuitarDimensionsDialog::onPresetChanged()
{
    // This could be used for real-time preview if needed
}

void GuitarDimensionsDialog::onResetToPreset()
{
    QString preset = m_presetCombo->currentText();
    
    GuitarDimensions dims;
    if (preset == "Les Paul Standard") {
        dims = GuitarDimensions::LesPaulStandard();
    } else if (preset == "Stratocaster") {
        dims = GuitarDimensions::StratocasterStandard();
    } else if (preset == "Telecaster") {
        dims = GuitarDimensions::TelecasterStandard();
    } else {
        return; // Custom - don't change
    }
    
    updateFromPreset(dims);
}

void GuitarDimensionsDialog::updateFromPreset(const GuitarDimensions& preset)
{
    setDimensions(preset);
}
