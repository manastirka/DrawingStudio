#pragma once

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QSplitter>
#include "ComponentDatabase.h"
#include "GuitarComponent.h"

class ComponentPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ComponentPropertiesDialog(GuitarComponent* component = nullptr, QWidget* parent = nullptr);
    
    // Component access
    void setComponent(GuitarComponent* component);
    GuitarComponent* component() const { return m_component; }
    
    // Database integration
    void loadComponentFromDatabase(const QString& manufacturer, const QString& model);
    void populateFromComponentSpec(const ComponentSpec& spec);
    
    // Property editing
    void updatePropertyValues();
    void applyChangesToComponent();

public slots:
    void accept() override;
    void onManufacturerChanged();
    void onModelChanged();
    void onDatabaseSelectionChanged();
    void resetToDefaults();
    void loadFromDatabase();

private slots:
    void onPropertyValueChanged();

private:
    void setupUI();
    void setupBasicProperties();
    void setupPhysicalProperties();
    void setupMaterialProperties();
    void setupDatabaseBrowser();
    void setupActionButtons();
    
    void populateManufacturers();
    void populateModels();
    void populateDatabaseTable();
    void updateDatabaseTable();
    
    void connectSignals();
    void disconnectSignals();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // Properties section
    QWidget* m_propertiesWidget;
    QFormLayout* m_propertiesLayout;
    
    // Basic properties
    QGroupBox* m_basicGroup;
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QLineEdit* m_descriptionEdit;
    
    // Physical properties  
    QGroupBox* m_physicalGroup;
    QDoubleSpinBox* m_widthSpin;
    QDoubleSpinBox* m_heightSpin;
    QDoubleSpinBox* m_thicknessSpin;
    QDoubleSpinBox* m_weightSpin;
    
    // Material properties
    QGroupBox* m_materialGroup;
    QLineEdit* m_materialEdit;
    QLineEdit* m_finishEdit;
    QComboBox* m_colorCombo;
    
    // Electrical properties (for electronic components)
    QGroupBox* m_electricalGroup;
    QLineEdit* m_resistanceEdit;
    QLineEdit* m_capacitanceEdit;
    QLineEdit* m_impedanceEdit;
    
    // Mounting properties
    QGroupBox* m_mountingGroup;
    QDoubleSpinBox* m_mountingHoleXSpin;
    QDoubleSpinBox* m_mountingHoleYSpin;
    QDoubleSpinBox* m_mountingDepthSpin;
    QComboBox* m_mountingTypeCombo;
    
    // Database browser
    QWidget* m_databaseWidget;
    QVBoxLayout* m_databaseLayout;
    QComboBox* m_manufacturerCombo;
    QComboBox* m_modelCombo;
    QTableWidget* m_databaseTable;
    QPushButton* m_loadFromDbButton;
    
    // Action buttons
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
    QPushButton* m_applyButton;
    
    // Data
    GuitarComponent* m_component;
    ComponentSpec m_currentSpec;
    bool m_updatingControls;
};