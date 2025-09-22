#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QJsonObject>
#include <QVector2D>
#include "GuitarComponent.h"

struct ComponentSpec {
    QString name;
    QString category;
    QString subcategory;
    QString manufacturer;
    QString model;
    QString description;
    
    // Physical dimensions (in mm)
    QVector2D dimensions;  // width x height
    float thickness;
    float weight;          // in grams
    
    // Material properties
    QString material;
    QString finish;
    QStringList colors;
    
    // Electrical properties (for electronic components)
    QString resistance;
    QString capacitance;
    QString impedance;
    
    // Mounting specifications
    QVector2D mountingHoles; // spacing if applicable
    float mountingDepth;
    QString mountingType;    // "screw", "press-fit", "solder", etc.
    
    // Price and availability
    QString priceRange;
    bool isAvailable;
    
    // Additional properties (flexible key-value pairs)
    QJsonObject customProperties;
    
    ComponentSpec() : 
        thickness(0), weight(0), mountingDepth(0), isAvailable(true) {}
};

class ComponentDatabase
{
public:
    static ComponentDatabase& instance();
    
    // Database access
    QVector<ComponentSpec> getAllComponents() const;
    QVector<ComponentSpec> getComponentsByType(ComponentType type) const;
    QVector<ComponentSpec> getComponentsByCategory(const QString& category) const;
    QVector<ComponentSpec> searchComponents(const QString& searchTerm) const;
    
    // Specific component access
    ComponentSpec getComponentSpec(const QString& manufacturer, const QString& model) const;
    ComponentSpec getDefaultComponentSpec(ComponentType type) const;
    
    // Utility functions
    QStringList getAvailableCategories() const;
    QStringList getAvailableSubcategories(const QString& category) const;
    QStringList getManufacturersByCategory(const QString& category) const;
    QStringList getModelsByManufacturer(const QString& manufacturer) const;
    
    // Component type utilities
    QString componentTypeToString(ComponentType type) const;
    ComponentType stringToComponentType(const QString& typeString) const;
    QString getCategoryForType(ComponentType type) const;
    
    // Custom component management
    void addCustomComponent(const ComponentSpec& spec);
    void updateCustomComponent(const QString& name, const ComponentSpec& spec);
    void removeCustomComponent(const QString& name);
    
    // Measurement presets
    struct MeasurementPreset {
        QString name;
        QString description;
        QString units;
        float defaultValue;
        float minValue;
        float maxValue;
        QString category;
    };
    
    QVector<MeasurementPreset> getMeasurementPresets() const;
    QVector<MeasurementPreset> getMeasurementPresetsByCategory(const QString& category) const;

private:
    ComponentDatabase();
    void initializeDatabase();
    void addPickupComponents();
    void addHardwareComponents();
    void addElectronicsComponents();
    void addGuitarBodyComponents();
    void addGuitarNeckComponents();
    void addGuitarHeadstockComponents();
    void addMeasurementPresets();
    void populateIndexMaps();
    
    // Database storage
    QVector<ComponentSpec> m_components;
    QMap<ComponentType, QVector<ComponentSpec>> m_componentsByType;
    QMap<QString, QVector<ComponentSpec>> m_componentsByCategory;
    QVector<ComponentSpec> m_customComponents;
    QVector<MeasurementPreset> m_measurementPresets;
};