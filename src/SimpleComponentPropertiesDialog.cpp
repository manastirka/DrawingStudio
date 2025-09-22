#include "SimpleComponentPropertiesDialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

SimpleComponentPropertiesDialog::SimpleComponentPropertiesDialog(const QString& componentType, const QString& componentName, QWidget* parent)
    : QDialog(parent)
    , m_componentType(componentType)
    , m_componentName(componentName)
    , m_mainLayout(nullptr)
    , m_formLayout(nullptr)
    , m_databaseGroup(nullptr)
    , m_manufacturerCombo(nullptr)
    , m_modelCombo(nullptr)
    , m_applyDbButton(nullptr)
    , m_propertiesGroup(nullptr)
    , m_nameEdit(nullptr)
    , m_scaleLengthCombo(nullptr)
    , m_customScaleSpin(nullptr)
    , m_scaleInfoLabel(nullptr)
    , m_pickupOutputCombo(nullptr)
    , m_magnetTypeCombo(nullptr)
    , m_bridgeTypeCombo(nullptr)
    , m_stringCountCombo(nullptr)
    , m_descriptionEdit(nullptr)
    , m_buttonLayout(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle("Component Properties: " + componentName);
    setMinimumSize(400, 300);
    
    // Ensure database is initialized
    ComponentDatabase& db = ComponentDatabase::instance();
    qDebug() << "Database initialized, available categories:" << db.getAvailableCategories();
    
    setupUI();
}

void SimpleComponentPropertiesDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Database selection group
    m_databaseGroup = new QGroupBox("Database Selection", this);
    QFormLayout* dbLayout = new QFormLayout(m_databaseGroup);
    
    m_manufacturerCombo = new QComboBox(this);
    m_manufacturerCombo->addItem("Select Manufacturer...");
    dbLayout->addRow("Manufacturer:", m_manufacturerCombo);
    
    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItem("Select Model...");
    m_modelCombo->setEnabled(false);
    dbLayout->addRow("Model:", m_modelCombo);
    
    m_applyDbButton = new QPushButton("Apply Database Selection", this);
    m_applyDbButton->setEnabled(false);
    dbLayout->addRow("", m_applyDbButton);
    
    m_mainLayout->addWidget(m_databaseGroup);
    
    // Component properties group
    m_propertiesGroup = new QGroupBox("Properties", this);
    m_formLayout = new QFormLayout(m_propertiesGroup);
    
    m_nameEdit = new QLineEdit(m_componentName, this);
    m_formLayout->addRow("Name:", m_nameEdit);
    
    createSpecificUI();
    
    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setMaximumHeight(80);
    m_formLayout->addRow("Description:", m_descriptionEdit);
    
    m_mainLayout->addWidget(m_propertiesGroup);
    
    // Action buttons
    m_buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addWidget(m_cancelButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_manufacturerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimpleComponentPropertiesDialog::onManufacturerChanged);
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimpleComponentPropertiesDialog::onModelChanged);
    connect(m_applyDbButton, &QPushButton::clicked,
            this, &SimpleComponentPropertiesDialog::applyDatabaseSelection);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Populate manufacturers
    populateManufacturers();
}

void SimpleComponentPropertiesDialog::createSpecificUI()
{
    if (m_componentType.contains("Nut")) {
        createNutUI();
    } else if (m_componentType.contains("Pickup")) {
        createPickupUI();
    } else if (m_componentType.contains("Bridge")) {
        createBridgeUI();
    } else if (m_componentType.contains("Knob") || m_componentType.contains("Switch") || 
               m_componentType.contains("Potentiometer") || m_componentType.contains("Capacitor")) {
        createElectronicsUI();
    }
}

void SimpleComponentPropertiesDialog::createNutUI()
{
    // Scale length selection with database integration
    m_scaleLengthCombo = new QComboBox(this);
    m_scaleLengthCombo->addItem("Custom", 0);
    m_scaleLengthCombo->addItem("Fender Scale (648mm)", 648);
    m_scaleLengthCombo->addItem("Gibson Scale (628mm)", 628);
    m_scaleLengthCombo->addItem("PRS Scale (635mm)", 635);
    m_scaleLengthCombo->addItem("Martin Short Scale (632mm)", 632);
    m_scaleLengthCombo->addItem("Bass Scale (864mm)", 864);
    m_formLayout->addRow("Scale Length:", m_scaleLengthCombo);
    
    m_customScaleSpin = new QDoubleSpinBox(this);
    m_customScaleSpin->setRange(500, 1000);
    m_customScaleSpin->setValue(648);
    m_customScaleSpin->setSuffix(" mm");
    m_customScaleSpin->setDecimals(1);
    m_formLayout->addRow("Custom Scale:", m_customScaleSpin);
    
    m_scaleInfoLabel = new QLabel("Standard guitar scale length", this);
    m_scaleInfoLabel->setStyleSheet("color: gray; font-style: italic;");
    m_formLayout->addRow("Info:", m_scaleInfoLabel);
    
    // Connect scale length combo to update info
    connect(m_scaleLengthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                double scaleLength = m_scaleLengthCombo->currentData().toDouble();
                if (scaleLength > 0) {
                    m_customScaleSpin->setValue(scaleLength);
                    m_customScaleSpin->setEnabled(false);
                    
                    if (scaleLength == 648) {
                        m_scaleInfoLabel->setText("Fender standard scale - bright tone, higher string tension");
                    } else if (scaleLength == 628) {
                        m_scaleInfoLabel->setText("Gibson standard scale - warmer tone, easier bending");
                    } else if (scaleLength == 635) {
                        m_scaleInfoLabel->setText("PRS scale - balanced between Fender and Gibson");
                    } else if (scaleLength == 864) {
                        m_scaleInfoLabel->setText("Standard bass guitar scale length");
                    } else {
                        m_scaleInfoLabel->setText("Custom scale length");
                    }
                } else {
                    m_customScaleSpin->setEnabled(true);
                    m_scaleInfoLabel->setText("Enter your custom scale length");
                }
            });
}

void SimpleComponentPropertiesDialog::createPickupUI()
{
    m_pickupOutputCombo = new QComboBox(this);
    m_pickupOutputCombo->addItems({"Low Output", "Medium Output", "High Output", "Very High Output"});
    m_formLayout->addRow("Output Level:", m_pickupOutputCombo);
    
    m_magnetTypeCombo = new QComboBox(this);
    m_magnetTypeCombo->addItems({"Alnico II", "Alnico III", "Alnico V", "Alnico VIII", "Ceramic", "Neodymium"});
    m_magnetTypeCombo->setCurrentText("Alnico V");
    m_formLayout->addRow("Magnet Type:", m_magnetTypeCombo);
}

void SimpleComponentPropertiesDialog::createBridgeUI()
{
    m_bridgeTypeCombo = new QComboBox(this);
    m_bridgeTypeCombo->addItems({"Tune-o-matic", "Vintage Tremolo", "Floyd Rose", "Hardtail", "Wrap Around"});
    m_formLayout->addRow("Bridge Type:", m_bridgeTypeCombo);
    
    m_stringCountCombo = new QComboBox(this);
    m_stringCountCombo->addItems({"4 String", "5 String", "6 String", "7 String", "8 String", "12 String"});
    m_stringCountCombo->setCurrentText("6 String");
    m_formLayout->addRow("String Count:", m_stringCountCombo);
}

void SimpleComponentPropertiesDialog::createElectronicsUI()
{
    // Add specific controls based on electronics type
    QLabel* noteLabel = new QLabel("Electrical specifications from database selection", this);
    noteLabel->setStyleSheet("color: gray; font-style: italic;");
    m_formLayout->addRow("Note:", noteLabel);
}

void SimpleComponentPropertiesDialog::populateManufacturers()
{
    ComponentDatabase& db = ComponentDatabase::instance();
    
    qDebug() << "Populating manufacturers for component type:" << m_componentType;
    
    // Filter manufacturers based on component type
    QString relevantCategory;
    
    // Parse component type more carefully
    qDebug() << "Analyzing component type:" << m_componentType;
    
    if (m_componentType.contains("Pickup", Qt::CaseInsensitive)) {
        relevantCategory = "Pickups";
    } else if (m_componentType.contains("Bridge", Qt::CaseInsensitive)) {
        relevantCategory = "Hardware";
    } else if (m_componentType.contains("Nut", Qt::CaseInsensitive)) {
        relevantCategory = "Hardware";
    } else if (m_componentType.contains("Tuner", Qt::CaseInsensitive)) {
        relevantCategory = "Hardware";
    } else if (m_componentType.contains("Output Jack", Qt::CaseInsensitive)) {
        relevantCategory = "Hardware";
    } else if (m_componentType.contains("Strap Button", Qt::CaseInsensitive)) {
        relevantCategory = "Hardware";
    } else if (m_componentType.contains("Knob", Qt::CaseInsensitive)) {
        relevantCategory = "Electronics";
    } else if (m_componentType.contains("Switch", Qt::CaseInsensitive)) {
        relevantCategory = "Electronics";
    } else if (m_componentType.contains("Potentiometer", Qt::CaseInsensitive)) {
        relevantCategory = "Electronics";
    } else if (m_componentType.contains("Capacitor", Qt::CaseInsensitive)) {
        relevantCategory = "Electronics";
    }
    
    // For main parts like Body, Neck, Headstock, filter by Main Parts category and specific subcategory
    if (relevantCategory.isEmpty() && (m_componentType.contains("Body") || 
                                      m_componentType.contains("Neck") || 
                                      m_componentType.contains("Headstock"))) {
        relevantCategory = "Main Parts";
        QString specificSubcategory;
        if (m_componentType.contains("Body")) {
            specificSubcategory = "Body";
        } else if (m_componentType.contains("Neck")) {
            specificSubcategory = "Neck";
        } else if (m_componentType.contains("Headstock")) {
            specificSubcategory = "Headstock";
        }
        
        qDebug() << "Main guitar part detected, filtering by Main Parts category and subcategory:" << specificSubcategory;
        
        // Get all components in Main Parts category
        auto mainPartsComponents = db.getComponentsByCategory("Main Parts");
        QStringList filteredManufacturers;
        
        for (const auto& comp : mainPartsComponents) {
            if (comp.subcategory == specificSubcategory && !filteredManufacturers.contains(comp.manufacturer)) {
                filteredManufacturers.append(comp.manufacturer);
            }
        }
        
        for (const QString& manufacturer : filteredManufacturers) {
            m_manufacturerCombo->addItem(manufacturer);
            qDebug() << "Added manufacturer (" << specificSubcategory << "):" << manufacturer;
        }
        
        qDebug() << "Manufacturer combo now has" << m_manufacturerCombo->count() << "items for" << specificSubcategory;
        return;
    }
    
    qDebug() << "Relevant category:" << relevantCategory;
    auto categories = db.getAvailableCategories();
    qDebug() << "Available categories:" << categories;
    
    if (!relevantCategory.isEmpty()) {
        auto manufacturers = db.getManufacturersByCategory(relevantCategory);
        qDebug() << "Found manufacturers:" << manufacturers;
        
        for (const QString& manufacturer : manufacturers) {
            m_manufacturerCombo->addItem(manufacturer);
            qDebug() << "Added manufacturer:" << manufacturer;
        }
    }
    
    qDebug() << "Manufacturer combo now has" << m_manufacturerCombo->count() << "items";
}

void SimpleComponentPropertiesDialog::populateModels()
{
    if (m_manufacturerCombo->currentIndex() <= 0) {
        return;
    }
    
    m_modelCombo->clear();
    m_modelCombo->addItem("Select Model...");
    
    QString manufacturer = m_manufacturerCombo->currentText();
    ComponentDatabase& db = ComponentDatabase::instance();
    
    // For main parts, filter models by specific subcategory
    if (m_componentType.contains("Body") || m_componentType.contains("Neck") || m_componentType.contains("Headstock")) {
        QString specificSubcategory;
        if (m_componentType.contains("Body")) {
            specificSubcategory = "Body";
        } else if (m_componentType.contains("Neck")) {
            specificSubcategory = "Neck";
        } else if (m_componentType.contains("Headstock")) {
            specificSubcategory = "Headstock";
        }
        
        auto mainPartsComponents = db.getComponentsByCategory("Main Parts");
        QStringList filteredModels;
        
        for (const auto& comp : mainPartsComponents) {
            if (comp.manufacturer == manufacturer && comp.subcategory == specificSubcategory && !filteredModels.contains(comp.model)) {
                filteredModels.append(comp.model);
            }
        }
        
        for (const QString& model : filteredModels) {
            m_modelCombo->addItem(model);
        }
    } else {
        // For other components, use the original method
        auto models = db.getModelsByManufacturer(manufacturer);
        
        for (const QString& model : models) {
            m_modelCombo->addItem(model);
        }
    }
    
    m_modelCombo->setEnabled(true);
}

void SimpleComponentPropertiesDialog::onManufacturerChanged()
{
    populateModels();
    m_applyDbButton->setEnabled(false);
}

void SimpleComponentPropertiesDialog::onModelChanged()
{
    m_applyDbButton->setEnabled(m_modelCombo->currentIndex() > 0);
}

void SimpleComponentPropertiesDialog::applyDatabaseSelection()
{
    QString manufacturer = m_manufacturerCombo->currentText();
    QString model = m_modelCombo->currentText();
    
    if (manufacturer.isEmpty() || model.isEmpty()) {
        return;
    }
    
    qDebug() << "Applying database selection:" << manufacturer << model;
    
    ComponentDatabase& db = ComponentDatabase::instance();
    m_currentSpec = db.getComponentSpec(manufacturer, model);
    
    qDebug() << "Retrieved spec:" << m_currentSpec.name << "Category:" << m_currentSpec.category;
    
    if (!m_currentSpec.name.isEmpty()) {
        // Update UI with database information
        m_nameEdit->setText(m_currentSpec.name);
        
        // Build detailed description with specs
        QString detailedDesc = m_currentSpec.description;
        if (!m_currentSpec.dimensions.isNull()) {
            detailedDesc += QString("\n\nDimensions: %1 x %2 mm")
                           .arg(m_currentSpec.dimensions.x())
                           .arg(m_currentSpec.dimensions.y());
        }
        if (m_currentSpec.weight > 0) {
            detailedDesc += QString("\nWeight: %1g").arg(m_currentSpec.weight);
        }
        if (!m_currentSpec.material.isEmpty()) {
            detailedDesc += QString("\nMaterial: %1").arg(m_currentSpec.material);
        }
        if (!m_currentSpec.resistance.isEmpty()) {
            detailedDesc += QString("\nResistance: %1").arg(m_currentSpec.resistance);
        }
        if (!m_currentSpec.priceRange.isEmpty()) {
            detailedDesc += QString("\nPrice Range: %1").arg(m_currentSpec.priceRange);
        }
        
        m_descriptionEdit->setPlainText(detailedDesc);
        
        // Update component-specific properties
        if (m_componentType.contains("Pickup") && m_pickupOutputCombo) {
            // Set pickup properties based on database spec
            if (m_currentSpec.resistance.contains("6.2k")) {
                m_pickupOutputCombo->setCurrentText("Medium Output");
            } else if (m_currentSpec.resistance.contains("16.6k")) {
                m_pickupOutputCombo->setCurrentText("High Output");
            } else if (m_currentSpec.resistance.contains("8.2k")) {
                m_pickupOutputCombo->setCurrentText("Medium Output");
            }
            
            if (m_currentSpec.material.contains("Alnico V")) {
                m_magnetTypeCombo->setCurrentText("Alnico V");
            } else if (m_currentSpec.material.contains("Alnico II")) {
                m_magnetTypeCombo->setCurrentText("Alnico II");
            }
        }
        
        QMessageBox::information(this, "Database Applied", 
            QString("Applied specifications for %1 %2\n\nDimensions: %3 x %4 mm\nWeight: %5g")
                .arg(manufacturer, model)
                .arg(m_currentSpec.dimensions.x())
                .arg(m_currentSpec.dimensions.y())
                .arg(m_currentSpec.weight));
    } else {
        qDebug() << "No spec found for" << manufacturer << model;
        QMessageBox::warning(this, "No Data", "No specifications found for this component.");
    }
}

void SimpleComponentPropertiesDialog::updateSpecDisplay()
{
    // Update the display with current spec information
}

void SimpleComponentPropertiesDialog::accept()
{
    // Save the properties
    qDebug() << "Saving component properties for" << m_componentName;
    
    if (m_scaleLengthCombo && m_customScaleSpin) {
        double scaleLength = m_customScaleSpin->value();
        qDebug() << "Scale length set to:" << scaleLength << "mm";
    }
    
    QDialog::accept();
}