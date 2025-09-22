#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include "ComponentDatabase.h"

class SimpleComponentPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimpleComponentPropertiesDialog(const QString& componentType, const QString& componentName, QWidget* parent = nullptr);

public slots:
    void accept() override;
    void onManufacturerChanged();
    void onModelChanged();
    void applyDatabaseSelection();

private:
    void setupUI();
    void populateManufacturers();
    void populateModels();
    void updateSpecDisplay();
    void createSpecificUI();
    void createNutUI();
    void createPickupUI();
    void createBridgeUI();
    void createElectronicsUI();
    
    QString m_componentType;
    QString m_componentName;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QFormLayout* m_formLayout;
    
    // Database selection
    QGroupBox* m_databaseGroup;
    QComboBox* m_manufacturerCombo;
    QComboBox* m_modelCombo;
    QPushButton* m_applyDbButton;
    
    // Component-specific controls
    QGroupBox* m_propertiesGroup;
    QLineEdit* m_nameEdit;
    
    // For Nut components - scale length selection
    QComboBox* m_scaleLengthCombo;
    QDoubleSpinBox* m_customScaleSpin;
    QLabel* m_scaleInfoLabel;
    
    // For Pickups - output and magnet type
    QComboBox* m_pickupOutputCombo;
    QComboBox* m_magnetTypeCombo;
    
    // For Bridges - string count and type
    QComboBox* m_bridgeTypeCombo;
    QComboBox* m_stringCountCombo;
    
    // General properties
    QTextEdit* m_descriptionEdit;
    
    // Action buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    ComponentSpec m_currentSpec;
};