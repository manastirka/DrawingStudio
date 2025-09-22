#pragma once

#include <QString>
#include <QVector2D>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>

// Structure to hold complete guitar specifications
struct GuitarDimensions {
    // Body dimensions (in mm)
    float bodyLength = 460.0f;       // Total body length
    float bodyWidth = 325.0f;        // Maximum body width
    float bodyThickness = 45.0f;     // Body thickness
    float upperBoutWidth = 280.0f;   // Upper bout width
    float lowerBoutWidth = 325.0f;   // Lower bout width  
    float waistWidth = 220.0f;       // Waist width (narrowest point)
    float cutawayDepth = 65.0f;      // Cutaway depth (0 for no cutaway)
    float cutawayLength = 95.0f;     // Cutaway length
    
    // Neck dimensions
    float scaleLength = 628.0f;      // Scale length (nut to bridge)
    float neckLength = 435.0f;       // Total neck length
    float nutWidth = 42.5f;          // Nut width
    float neckWidth12th = 52.0f;     // Neck width at 12th fret
    float neckThickness1st = 20.0f;  // Neck thickness at 1st fret
    float neckThickness12th = 22.0f; // Neck thickness at 12th fret
    int fretCount = 22;              // Number of frets
    
    // Headstock dimensions
    float headstockLength = 175.0f;  // Headstock length
    float headstockWidth = 85.0f;    // Headstock width
    float headstockThickness = 15.0f;// Headstock thickness
    QString tunerLayout = "3+3";     // Tuner layout ("3+3", "6-inline", "4-inline")
    
    // Pickup specifications  
    int pickupCount = 2;             // Number of pickups
    QString pickupType = "Humbucker";// "Humbucker", "Single-coil", "P90"
    float neckPickupPosition = 146.0f; // Distance from bridge to neck pickup center
    float middlePickupPosition = 85.0f; // Distance from bridge to middle pickup (if applicable)
    float bridgePickupPosition = 25.0f; // Distance from bridge to bridge pickup center
    float pickupWidth = 38.0f;       // Pickup width
    float pickupLength = 85.0f;      // Pickup length (humbucker)
    
    // Bridge specifications
    QString bridgeType = "Tune-O-Matic"; // "Tune-O-Matic", "Tremolo", "Hardtail", "Wraparound"
    float bridgeWidth = 70.0f;       // Bridge width
    float bridgeLength = 85.0f;      // Bridge length
    float stringSpacing = 10.5f;     // String spacing at bridge
    
    // Control layout
    int volumeKnobs = 2;             // Number of volume controls
    int toneKnobs = 2;               // Number of tone controls
    QString switchType = "3-way";    // "3-way", "5-way", "None"
    float knobSpacing = 50.0f;       // Distance between control knobs
    
    // Display settings
    float displayScale = 1.0f;       // Scale factor for display
    QString guitarModel = "Custom";  // Guitar model name
    
    // Preset specifications for famous guitars
    static GuitarDimensions LesPaulStandard() {
        GuitarDimensions dims;
        dims.guitarModel = "Les Paul Standard";
        dims.bodyLength = 460.0f;
        dims.bodyWidth = 325.0f;
        dims.bodyThickness = 50.0f;
        dims.upperBoutWidth = 280.0f;
        dims.lowerBoutWidth = 325.0f;
        dims.waistWidth = 220.0f;
        dims.cutawayDepth = 65.0f;
        dims.cutawayLength = 95.0f;
        dims.scaleLength = 628.0f;
        dims.nutWidth = 42.5f;
        dims.tunerLayout = "3+3";
        dims.pickupCount = 2;
        dims.pickupType = "Humbucker";
        dims.neckPickupPosition = 146.0f;
        dims.bridgePickupPosition = 25.0f;
        dims.bridgeType = "Tune-O-Matic";
        dims.volumeKnobs = 2;
        dims.toneKnobs = 2;
        dims.switchType = "3-way";
        return dims;
    }
    
    static GuitarDimensions StratocasterStandard() {
        GuitarDimensions dims;
        dims.guitarModel = "Stratocaster";
        dims.bodyLength = 430.0f;
        dims.bodyWidth = 325.0f;
        dims.bodyThickness = 44.0f;
        dims.upperBoutWidth = 280.0f;
        dims.lowerBoutWidth = 325.0f;
        dims.waistWidth = 210.0f;
        dims.cutawayDepth = 85.0f; // Double cutaway
        dims.cutawayLength = 120.0f;
        dims.scaleLength = 648.0f;
        dims.nutWidth = 41.3f;
        dims.tunerLayout = "6-inline";
        dims.pickupCount = 3;
        dims.pickupType = "Single-coil";
        dims.neckPickupPosition = 175.0f;
        dims.middlePickupPosition = 115.0f;
        dims.bridgePickupPosition = 55.0f;
        dims.bridgeType = "Tremolo";
        dims.volumeKnobs = 1;
        dims.toneKnobs = 2;
        dims.switchType = "5-way";
        dims.pickupWidth = 18.0f;
        dims.pickupLength = 85.0f;
        return dims;
    }
    
    static GuitarDimensions TelecasterStandard() {
        GuitarDimensions dims;
        dims.guitarModel = "Telecaster";
        dims.bodyLength = 430.0f;
        dims.bodyWidth = 325.0f;
        dims.bodyThickness = 44.0f;
        dims.upperBoutWidth = 280.0f;
        dims.lowerBoutWidth = 325.0f;
        dims.waistWidth = 220.0f;
        dims.cutawayDepth = 65.0f;
        dims.cutawayLength = 95.0f;
        dims.scaleLength = 648.0f;
        dims.nutWidth = 41.3f;
        dims.tunerLayout = "6-inline";
        dims.pickupCount = 2;
        dims.pickupType = "Single-coil";
        dims.neckPickupPosition = 146.0f;
        dims.bridgePickupPosition = 25.0f;
        dims.bridgeType = "Hardtail";
        dims.volumeKnobs = 1;
        dims.toneKnobs = 1;
        dims.switchType = "3-way";
        dims.pickupWidth = 18.0f;
        dims.pickupLength = 85.0f;
        return dims;
    }
};

// Dialog for editing guitar dimensions
class GuitarDimensionsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit GuitarDimensionsDialog(const GuitarDimensions& dimensions, QWidget* parent = nullptr);
    
    GuitarDimensions getDimensions() const;
    void setDimensions(const GuitarDimensions& dimensions);
    
private slots:
    void onPresetChanged();
    void onResetToPreset();
    
private:
    void setupUI();
    void createBodyGroup();
    void createNeckGroup(); 
    void createPickupGroup();
    void createBridgeGroup();
    void createControlGroup();
    void updateFromPreset(const GuitarDimensions& preset);
    
    // UI elements
    QVBoxLayout* m_mainLayout;
    QComboBox* m_presetCombo;
    QPushButton* m_resetButton;
    QDialogButtonBox* m_buttonBox;
    
    // Body dimensions
    QGroupBox* m_bodyGroup;
    QDoubleSpinBox* m_bodyLengthSpin;
    QDoubleSpinBox* m_bodyWidthSpin;
    QDoubleSpinBox* m_bodyThicknessSpin;
    QDoubleSpinBox* m_upperBoutSpin;
    QDoubleSpinBox* m_lowerBoutSpin;
    QDoubleSpinBox* m_waistWidthSpin;
    QDoubleSpinBox* m_cutawayDepthSpin;
    QDoubleSpinBox* m_cutawayLengthSpin;
    
    // Neck dimensions
    QGroupBox* m_neckGroup;
    QDoubleSpinBox* m_scaleLengthSpin;
    QDoubleSpinBox* m_neckLengthSpin;
    QDoubleSpinBox* m_nutWidthSpin;
    QDoubleSpinBox* m_neckWidth12Spin;
    QSpinBox* m_fretCountSpin;
    QComboBox* m_tunerLayoutCombo;
    
    // Pickup specifications
    QGroupBox* m_pickupGroup;
    QSpinBox* m_pickupCountSpin;
    QComboBox* m_pickupTypeCombo;
    QDoubleSpinBox* m_neckPickupPosSpin;
    QDoubleSpinBox* m_middlePickupPosSpin;
    QDoubleSpinBox* m_bridgePickupPosSpin;
    QDoubleSpinBox* m_pickupWidthSpin;
    QDoubleSpinBox* m_pickupLengthSpin;
    
    // Bridge specifications  
    QGroupBox* m_bridgeGroup;
    QComboBox* m_bridgeTypeCombo;
    QDoubleSpinBox* m_bridgeWidthSpin;
    QDoubleSpinBox* m_bridgeLengthSpin;
    QDoubleSpinBox* m_stringSpacingSpin;
    
    // Control layout
    QGroupBox* m_controlGroup;
    QSpinBox* m_volumeKnobsSpin;
    QSpinBox* m_toneKnobsSpin;
    QComboBox* m_switchTypeCombo;
    QDoubleSpinBox* m_knobSpacingSpin;
    
    // Display settings
    QDoubleSpinBox* m_displayScaleSpin;
    
    GuitarDimensions m_currentDimensions;
};