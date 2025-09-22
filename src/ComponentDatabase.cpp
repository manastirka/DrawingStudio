#include "ComponentDatabase.h"
#include <QDebug>

ComponentDatabase& ComponentDatabase::instance() {
    static ComponentDatabase instance;
    return instance;
}

ComponentDatabase::ComponentDatabase() {
    initializeDatabase();
}

void ComponentDatabase::initializeDatabase() {
    // Clear existing data
    m_components.clear();
    m_componentsByType.clear();
    m_componentsByCategory.clear();
    
    addPickupComponents();
    addHardwareComponents();
    addElectronicsComponents();
    addGuitarBodyComponents();
    addGuitarNeckComponents();
    addGuitarHeadstockComponents();
    addMeasurementPresets();
    
    // Populate the index maps after adding all components
    populateIndexMaps();
}

void ComponentDatabase::addPickupComponents() {
    qDebug() << "Adding pickup components to database...";
    ComponentSpec spec;
    
    // Fender Single Coil Pickups
    spec.name = "Fender Texas Special Single Coil";
    spec.category = "Pickups";
    spec.subcategory = "Single Coil";
    spec.manufacturer = "Fender";
    spec.model = "Texas Special";
    spec.description = "Hot overwound single-coil pickup with increased output";
    spec.dimensions = QVector2D(89, 19);
    spec.thickness = 20;
    spec.weight = 85;
    spec.material = "Alnico V magnets, enamel wire";
    spec.finish = "Black";
    spec.colors = {"Black", "White", "Aged White"};
    spec.resistance = "6.2k ohms";
    spec.mountingType = "screw";
    spec.priceRange = "$80-120";
    m_components.append(spec);
    qDebug() << "Added pickup:" << spec.manufacturer << spec.model;
    
    // Humbucker Pickups
    spec = ComponentSpec();
    spec.name = "Seymour Duncan JB Humbucker";
    spec.category = "Pickups";
    spec.subcategory = "Humbucker";
    spec.manufacturer = "Seymour Duncan";
    spec.model = "SH-4 JB";
    spec.description = "High-output humbucker for rock and metal";
    spec.dimensions = QVector2D(90, 40);
    spec.thickness = 16;
    spec.weight = 180;
    spec.material = "Alnico V magnets, 4-conductor wire";
    spec.finish = "Black";
    spec.colors = {"Black", "White", "Zebra", "Reverse Zebra"};
    spec.resistance = "16.6k ohms";
    spec.mountingType = "pickup ring";
    spec.priceRange = "$100-140";
    m_components.append(spec);
    qDebug() << "Added pickup:" << spec.manufacturer << spec.model;
    
    // P90 Pickups
    spec = ComponentSpec();
    spec.name = "Gibson P-90 Dog Ear";
    spec.category = "Pickups";
    spec.subcategory = "P90";
    spec.manufacturer = "Gibson";
    spec.model = "P-90 Dog Ear";
    spec.description = "Classic single-coil P90 pickup with dog ear mounting";
    spec.dimensions = QVector2D(88, 35);
    spec.thickness = 18;
    spec.weight = 120;
    spec.material = "Alnico V magnets";
    spec.finish = "Cream";
    spec.colors = {"Cream", "Black"};
    spec.resistance = "8.2k ohms";
    spec.mountingType = "direct mount";
    spec.priceRange = "$120-160";
    m_components.append(spec);
}

void ComponentDatabase::addHardwareComponents() {
    qDebug() << "Adding hardware components to database...";
    ComponentSpec spec;
    
    // Bridges
    spec.name = "Fender Vintage Style Tremolo";
    spec.category = "Hardware";
    spec.subcategory = "Bridge";
    spec.manufacturer = "Fender";
    spec.model = "Vintage Tremolo";
    spec.description = "Classic 6-point synchronized tremolo bridge";
    spec.dimensions = QVector2D(85, 55);
    spec.thickness = 12;
    spec.weight = 320;
    spec.material = "Steel";
    spec.finish = "Chrome";
    spec.colors = {"Chrome", "Gold", "Black"};
    spec.mountingType = "screw";
    spec.mountingHoles = QVector2D(56, 0);
    spec.priceRange = "$40-80";
    m_components.append(spec);
    qDebug() << "Added hardware:" << spec.manufacturer << spec.model;
    
    spec = ComponentSpec();
    spec.name = "Gibson Tune-o-matic";
    spec.category = "Hardware";
    spec.subcategory = "Bridge";
    spec.manufacturer = "Gibson";
    spec.model = "ABR-1";
    spec.description = "Classic tune-o-matic bridge with individual saddles";
    spec.dimensions = QVector2D(82, 12);
    spec.thickness = 15;
    spec.weight = 85;
    spec.material = "Steel/Brass";
    spec.finish = "Nickel";
    spec.colors = {"Nickel", "Chrome", "Gold", "Black"};
    spec.mountingType = "stud";
    spec.mountingHoles = QVector2D(74, 0);
    spec.priceRange = "$35-70";
    m_components.append(spec);
    
    // Tuners
    spec = ComponentSpec();
    spec.name = "Grover Rotomatics";
    spec.category = "Hardware";
    spec.subcategory = "Tuners";
    spec.manufacturer = "Grover";
    spec.model = "102-18N";
    spec.description = "Locking tuning machines with 18:1 ratio";
    spec.dimensions = QVector2D(35, 55);
    spec.thickness = 40;
    spec.weight = 65;
    spec.material = "Steel/Brass";
    spec.finish = "Nickel";
    spec.colors = {"Nickel", "Chrome", "Gold", "Black"};
    spec.mountingType = "bushing";
    spec.priceRange = "$80-120";
    m_components.append(spec);
    
    // Nuts
    spec = ComponentSpec();
    spec.name = "Bone Nut";
    spec.category = "Hardware";
    spec.subcategory = "Nut";
    spec.manufacturer = "Generic";
    spec.model = "Standard";
    spec.description = "Natural bone nut blank";
    spec.dimensions = QVector2D(44, 6);
    spec.thickness = 9;
    spec.weight = 3;
    spec.material = "Bone";
    spec.finish = "Natural";
    spec.colors = {"Natural", "Bleached"};
    spec.mountingType = "glue";
    spec.priceRange = "$5-15";
    m_components.append(spec);
    
    // Output Jacks
    spec = ComponentSpec();
    spec.name = "Switchcraft Output Jack";
    spec.category = "Hardware";
    spec.subcategory = "Output Jack";
    spec.manufacturer = "Switchcraft";
    spec.model = "1/4\" Mono";
    spec.description = "Professional grade 1/4\" mono output jack";
    spec.dimensions = QVector2D(20, 20);
    spec.thickness = 25;
    spec.weight = 45;
    spec.material = "Nickel plated brass";
    spec.finish = "Nickel";
    spec.mountingType = "nut";
    spec.priceRange = "$8-15";
    m_components.append(spec);
    
    // Strap Buttons
    spec = ComponentSpec();
    spec.name = "Schaller Security Locks";
    spec.category = "Hardware";
    spec.subcategory = "Strap Button";
    spec.manufacturer = "Schaller";
    spec.model = "Security Lock";
    spec.description = "Locking strap button system";
    spec.dimensions = QVector2D(15, 15);
    spec.thickness = 20;
    spec.weight = 25;
    spec.material = "Nickel plated steel";
    spec.finish = "Nickel";
    spec.colors = {"Nickel", "Chrome", "Gold", "Black"};
    spec.mountingType = "screw";
    spec.priceRange = "$25-40";
    m_components.append(spec);
}

void ComponentDatabase::addElectronicsComponents() {
    qDebug() << "Adding electronics components to database...";
    ComponentSpec spec;
    
    // Volume Knobs
    spec.name = "Gibson Top Hat Knob";
    spec.category = "Electronics";
    spec.subcategory = "Knob";
    spec.manufacturer = "Gibson";
    spec.model = "Top Hat";
    spec.description = "Classic Gibson style top hat control knob";
    spec.dimensions = QVector2D(19, 19);
    spec.thickness = 17;
    spec.weight = 8;
    spec.material = "Plastic";
    spec.finish = "Black";
    spec.colors = {"Black", "Cream", "Aged"};
    spec.mountingType = "push-on";
    spec.priceRange = "$3-8";
    m_components.append(spec);
    qDebug() << "Added electronics:" << spec.manufacturer << spec.model;
    
    // Pickup Switches
    spec = ComponentSpec();
    spec.name = "Oak Grigsby 5-Way Switch";
    spec.category = "Electronics";
    spec.subcategory = "Switch";
    spec.manufacturer = "Oak Grigsby";
    spec.model = "5-Way Super";
    spec.description = "Heavy duty 5-way pickup selector switch";
    spec.dimensions = QVector2D(30, 15);
    spec.thickness = 25;
    spec.weight = 35;
    spec.material = "Phenolic/Brass";
    spec.finish = "Natural";
    spec.mountingType = "screw";
    spec.priceRange = "$15-25";
    m_components.append(spec);
    
    // Potentiometers
    spec = ComponentSpec();
    spec.name = "CTS 500K Audio Pot";
    spec.category = "Electronics";
    spec.subcategory = "Potentiometer";
    spec.manufacturer = "CTS";
    spec.model = "500K Audio";
    spec.description = "High quality 500K ohm audio taper potentiometer";
    spec.dimensions = QVector2D(24, 24);
    spec.thickness = 20;
    spec.weight = 25;
    spec.material = "Carbon/Metal";
    spec.finish = "Natural";
    spec.resistance = "500K ohms";
    spec.mountingType = "nut";
    spec.priceRange = "$8-15";
    m_components.append(spec);
}

void ComponentDatabase::addMeasurementPresets() {
    MeasurementPreset preset;
    
    // Scale length measurements
    preset.name = "Scale Length";
    preset.description = "Distance from nut to bridge";
    preset.units = "mm";
    preset.defaultValue = 648;  // Fender scale
    preset.minValue = 610;
    preset.maxValue = 700;
    preset.category = "Neck";
    m_measurementPresets.append(preset);
    
    preset.name = "Scale Length (Gibson)";
    preset.description = "Gibson standard scale length";
    preset.units = "mm";
    preset.defaultValue = 628;  // Gibson scale
    preset.minValue = 610;
    preset.maxValue = 700;
    preset.category = "Neck";
    m_measurementPresets.append(preset);
    
    // Body measurements
    preset.name = "Body Length";
    preset.description = "Overall body length";
    preset.units = "mm";
    preset.defaultValue = 450;
    preset.minValue = 380;
    preset.maxValue = 520;
    preset.category = "Body";
    m_measurementPresets.append(preset);
    
    preset.name = "Body Width";
    preset.description = "Maximum body width";
    preset.units = "mm";
    preset.defaultValue = 320;
    preset.minValue = 280;
    preset.maxValue = 380;
    preset.category = "Body";
    m_measurementPresets.append(preset);
    
    preset.name = "Nut Width";
    preset.description = "Width of the nut";
    preset.units = "mm";
    preset.defaultValue = 42;
    preset.minValue = 38;
    preset.maxValue = 48;
    preset.category = "Neck";
    m_measurementPresets.append(preset);
    
    // Pickup measurements
    preset.name = "Pickup Spacing";
    preset.description = "Distance between pickup centers";
    preset.units = "mm";
    preset.defaultValue = 90;
    preset.minValue = 80;
    preset.maxValue = 100;
    preset.category = "Pickups";
    m_measurementPresets.append(preset);
    
    // Bridge measurements
    preset.name = "String Spacing";
    preset.description = "Distance between outer strings at bridge";
    preset.units = "mm";
    preset.defaultValue = 52;
    preset.minValue = 48;
    preset.maxValue = 56;
    preset.category = "Bridge";
    m_measurementPresets.append(preset);
}

QVector<ComponentSpec> ComponentDatabase::getAllComponents() const {
    QVector<ComponentSpec> allComponents = m_components;
    allComponents.append(m_customComponents);
    return allComponents;
}

QVector<ComponentSpec> ComponentDatabase::getComponentsByType(ComponentType type) const {
    return m_componentsByType.value(type);
}

QVector<ComponentSpec> ComponentDatabase::getComponentsByCategory(const QString& category) const {
    return m_componentsByCategory.value(category);
}

QVector<ComponentSpec> ComponentDatabase::searchComponents(const QString& searchTerm) const {
    QVector<ComponentSpec> results;
    QString lowerSearch = searchTerm.toLower();
    
    for (const auto& component : getAllComponents()) {
        if (component.name.toLower().contains(lowerSearch) ||
            component.manufacturer.toLower().contains(lowerSearch) ||
            component.model.toLower().contains(lowerSearch) ||
            component.description.toLower().contains(lowerSearch)) {
            results.append(component);
        }
    }
    return results;
}

ComponentSpec ComponentDatabase::getComponentSpec(const QString& manufacturer, const QString& model) const {
    for (const auto& component : getAllComponents()) {
        if (component.manufacturer == manufacturer && component.model == model) {
            return component;
        }
    }
    return ComponentSpec(); // Return empty spec if not found
}

ComponentSpec ComponentDatabase::getDefaultComponentSpec(ComponentType type) const {
    auto components = getComponentsByType(type);
    if (!components.isEmpty()) {
        return components.first();
    }
    
    // Create a basic default spec
    ComponentSpec spec;
    spec.name = componentTypeToString(type);
    spec.category = getCategoryForType(type);
    spec.manufacturer = "Generic";
    spec.model = "Standard";
    spec.description = "Standard " + componentTypeToString(type).toLower();
    
    // Set default dimensions based on type
    switch (type) {
        case ComponentType::Body:
            spec.dimensions = QVector2D(350, 480);
            break;
        case ComponentType::Neck:
            spec.dimensions = QVector2D(43, 628);
            break;
        case ComponentType::Headstock:
            spec.dimensions = QVector2D(85, 180);
            break;
        case ComponentType::Pickup:
            spec.dimensions = QVector2D(89, 19);
            break;
        case ComponentType::Bridge:
            spec.dimensions = QVector2D(80, 50);
            break;
        case ComponentType::Tuner:
            spec.dimensions = QVector2D(35, 55);
            break;
        case ComponentType::Nut:
            spec.dimensions = QVector2D(44, 6);
            break;
        default:
            spec.dimensions = QVector2D(20, 20);
            break;
    }
    
    return spec;
}

QStringList ComponentDatabase::getAvailableCategories() const {
    QStringList categories;
    for (const auto& component : getAllComponents()) {
        if (!categories.contains(component.category)) {
            categories.append(component.category);
        }
    }
    categories.sort();
    return categories;
}

QStringList ComponentDatabase::getAvailableSubcategories(const QString& category) const {
    QStringList subcategories;
    for (const auto& component : getAllComponents()) {
        if (component.category == category && !subcategories.contains(component.subcategory)) {
            subcategories.append(component.subcategory);
        }
    }
    subcategories.sort();
    return subcategories;
}

QStringList ComponentDatabase::getManufacturersByCategory(const QString& category) const {
    QStringList manufacturers;
    for (const auto& component : getAllComponents()) {
        if (component.category == category && !manufacturers.contains(component.manufacturer)) {
            manufacturers.append(component.manufacturer);
        }
    }
    manufacturers.sort();
    return manufacturers;
}

QStringList ComponentDatabase::getModelsByManufacturer(const QString& manufacturer) const {
    QStringList models;
    for (const auto& component : getAllComponents()) {
        if (component.manufacturer == manufacturer && !models.contains(component.model)) {
            models.append(component.model);
        }
    }
    models.sort();
    return models;
}

QString ComponentDatabase::componentTypeToString(ComponentType type) const {
    switch (type) {
        case ComponentType::Body: return "Body";
        case ComponentType::Neck: return "Neck";
        case ComponentType::Headstock: return "Headstock";
        case ComponentType::Pickup: return "Pickup";
        case ComponentType::Bridge: return "Bridge";
        case ComponentType::Tuner: return "Tuner";
        case ComponentType::Nut: return "Nut";
        case ComponentType::Fret: return "Fret";
        case ComponentType::Inlay: return "Inlay";
        case ComponentType::SoundHole: return "Sound Hole";
        case ComponentType::Tailpiece: return "Tailpiece";
        case ComponentType::TremoloBar: return "Tremolo Bar";
        case ComponentType::StringGuide: return "String Guide";
        case ComponentType::StrapButton: return "Strap Button";
        case ComponentType::OutputJack: return "Output Jack";
        case ComponentType::Electronics: return "Electronics";
        case ComponentType::VolumeKnob: return "Volume Knob";
        case ComponentType::ToneKnob: return "Tone Knob";
        case ComponentType::PickupSwitch: return "Pickup Switch";
        case ComponentType::Potentiometer: return "Potentiometer";
        case ComponentType::Capacitor: return "Capacitor";
        case ComponentType::TrussRodCover: return "Truss Rod Cover";
        case ComponentType::NeckPlate: return "Neck Plate";
        case ComponentType::FretMarker: return "Fret Marker";
        case ComponentType::PickupRing: return "Pickup Ring";
        case ComponentType::PickupBezel: return "Pickup Bezel";
        case ComponentType::ScratchPlate: return "Scratch Plate";
        case ComponentType::ControlCavityCover: return "Control Cavity Cover";
        case ComponentType::DimensionLine: return "Dimension Line";
        case ComponentType::AngleMeasure: return "Angle Measure";
        case ComponentType::RadiusMeasure: return "Radius Measure";
        case ComponentType::Custom: return "Custom";
        default: return "Unknown";
    }
}

ComponentType ComponentDatabase::stringToComponentType(const QString& typeString) const {
    if (typeString == "Body") return ComponentType::Body;
    if (typeString == "Neck") return ComponentType::Neck;
    if (typeString == "Headstock") return ComponentType::Headstock;
    if (typeString == "Pickup") return ComponentType::Pickup;
    if (typeString == "Bridge") return ComponentType::Bridge;
    if (typeString == "Tuner") return ComponentType::Tuner;
    if (typeString == "Nut") return ComponentType::Nut;
    if (typeString == "Fret") return ComponentType::Fret;
    if (typeString == "Inlay") return ComponentType::Inlay;
    if (typeString == "Sound Hole") return ComponentType::SoundHole;
    if (typeString == "Tailpiece") return ComponentType::Tailpiece;
    if (typeString == "Tremolo Bar") return ComponentType::TremoloBar;
    if (typeString == "String Guide") return ComponentType::StringGuide;
    if (typeString == "Strap Button") return ComponentType::StrapButton;
    if (typeString == "Output Jack") return ComponentType::OutputJack;
    if (typeString == "Electronics") return ComponentType::Electronics;
    if (typeString == "Volume Knob") return ComponentType::VolumeKnob;
    if (typeString == "Tone Knob") return ComponentType::ToneKnob;
    if (typeString == "Pickup Switch") return ComponentType::PickupSwitch;
    if (typeString == "Potentiometer") return ComponentType::Potentiometer;
    if (typeString == "Capacitor") return ComponentType::Capacitor;
    if (typeString == "Truss Rod Cover") return ComponentType::TrussRodCover;
    if (typeString == "Neck Plate") return ComponentType::NeckPlate;
    if (typeString == "Fret Marker") return ComponentType::FretMarker;
    if (typeString == "Pickup Ring") return ComponentType::PickupRing;
    if (typeString == "Pickup Bezel") return ComponentType::PickupBezel;
    if (typeString == "Scratch Plate") return ComponentType::ScratchPlate;
    if (typeString == "Control Cavity Cover") return ComponentType::ControlCavityCover;
    if (typeString == "Dimension Line") return ComponentType::DimensionLine;
    if (typeString == "Angle Measure") return ComponentType::AngleMeasure;
    if (typeString == "Radius Measure") return ComponentType::RadiusMeasure;
    return ComponentType::Custom;
}

QString ComponentDatabase::getCategoryForType(ComponentType type) const {
    switch (type) {
        case ComponentType::Body:
        case ComponentType::Neck:
        case ComponentType::Headstock:
            return "Main Parts";
        case ComponentType::Pickup:
            return "Pickups";
        case ComponentType::Bridge:
        case ComponentType::Tuner:
        case ComponentType::Nut:
        case ComponentType::Fret:
        case ComponentType::Inlay:
        case ComponentType::SoundHole:
        case ComponentType::Tailpiece:
        case ComponentType::TremoloBar:
        case ComponentType::StringGuide:
        case ComponentType::StrapButton:
        case ComponentType::OutputJack:
        case ComponentType::TrussRodCover:
        case ComponentType::NeckPlate:
        case ComponentType::FretMarker:
        case ComponentType::PickupRing:
        case ComponentType::PickupBezel:
        case ComponentType::ScratchPlate:
        case ComponentType::ControlCavityCover:
            return "Hardware";
        case ComponentType::Electronics:
        case ComponentType::VolumeKnob:
        case ComponentType::ToneKnob:
        case ComponentType::PickupSwitch:
        case ComponentType::Potentiometer:
        case ComponentType::Capacitor:
            return "Electronics";
        case ComponentType::DimensionLine:
        case ComponentType::AngleMeasure:
        case ComponentType::RadiusMeasure:
            return "Measurements";
        default:
            return "Custom";
    }
}

void ComponentDatabase::addCustomComponent(const ComponentSpec& spec) {
    m_customComponents.append(spec);
}

void ComponentDatabase::updateCustomComponent(const QString& name, const ComponentSpec& spec) {
    for (int i = 0; i < m_customComponents.size(); ++i) {
        if (m_customComponents[i].name == name) {
            m_customComponents[i] = spec;
            break;
        }
    }
}

void ComponentDatabase::removeCustomComponent(const QString& name) {
    for (int i = 0; i < m_customComponents.size(); ++i) {
        if (m_customComponents[i].name == name) {
            m_customComponents.removeAt(i);
            break;
        }
    }
}

QVector<ComponentDatabase::MeasurementPreset> ComponentDatabase::getMeasurementPresets() const {
    return m_measurementPresets;
}

QVector<ComponentDatabase::MeasurementPreset> ComponentDatabase::getMeasurementPresetsByCategory(const QString& category) const {
    QVector<MeasurementPreset> results;
    for (const auto& preset : m_measurementPresets) {
        if (preset.category == category) {
            results.append(preset);
        }
    }
    return results;
}

void ComponentDatabase::populateIndexMaps() {
    qDebug() << "Populating ComponentDatabase index maps with" << m_components.size() << "components";
    
    // Clear existing maps
    m_componentsByType.clear();
    m_componentsByCategory.clear();
    
    // Populate maps
    for (const auto& component : m_components) {
        // Add to category map
        m_componentsByCategory[component.category].append(component);
        
        // Convert category to ComponentType for type map
        ComponentType type = ComponentType::Custom; // Default
        if (component.category == "Main Parts") {
            if (component.subcategory == "Body") type = ComponentType::Body;
            else if (component.subcategory == "Neck") type = ComponentType::Neck;
            else if (component.subcategory == "Headstock") type = ComponentType::Headstock;
        } else if (component.category == "Pickups") {
            type = ComponentType::Pickup;
        } else if (component.category == "Hardware") {
            if (component.subcategory == "Bridge") type = ComponentType::Bridge;
            else if (component.subcategory == "Tuners") type = ComponentType::Tuner;
            else if (component.subcategory == "Nut") type = ComponentType::Nut;
            else if (component.subcategory == "Output Jack") type = ComponentType::OutputJack;
            else if (component.subcategory == "Strap Button") type = ComponentType::StrapButton;
        } else if (component.category == "Electronics") {
            if (component.subcategory == "Knob") type = ComponentType::VolumeKnob;
            else if (component.subcategory == "Switch") type = ComponentType::PickupSwitch;
            else if (component.subcategory == "Potentiometer") type = ComponentType::Potentiometer;
        }
        
        m_componentsByType[type].append(component);
    }
    
    qDebug() << "Index maps populated:";
    qDebug() << "Categories:" << m_componentsByCategory.keys();
    qDebug() << "Components by category:";
    for (auto it = m_componentsByCategory.begin(); it != m_componentsByCategory.end(); ++it) {
        qDebug() << "  -" << it.key() << ":" << it.value().size() << "components";
    }
}

void ComponentDatabase::addGuitarBodyComponents() {
    qDebug() << "Adding guitar body components to database...";
    ComponentSpec spec;
    
    // Gibson Les Paul Body
    spec.name = "Gibson Les Paul Body";
    spec.category = "Main Parts";
    spec.subcategory = "Body";
    spec.manufacturer = "Gibson";
    spec.model = "Les Paul Standard";
    spec.description = "Classic mahogany body with maple cap, single cutaway design";
    spec.dimensions = QVector2D(350, 480);  // 350mm wide x 480mm long
    spec.thickness = 45;
    spec.weight = 2100;  // ~2.1 kg
    spec.material = "Mahogany with maple cap";
    spec.finish = "Gloss nitrocellulose";
    spec.colors = {"Honey Burst", "Cherry Burst", "Tobacco Burst", "Heritage Cherry", "Ebony"};
    spec.mountingType = "bolt-on/set-neck";
    spec.priceRange = "$200-400";
    m_components.append(spec);
    qDebug() << "Added body:" << spec.manufacturer << spec.model;
    
    // Fender Stratocaster Body
    spec = ComponentSpec();
    spec.name = "Fender Stratocaster Body";
    spec.category = "Main Parts";
    spec.subcategory = "Body";
    spec.manufacturer = "Fender";
    spec.model = "Stratocaster";
    spec.description = "Contoured alder or ash body with double cutaway design";
    spec.dimensions = QVector2D(325, 460);  // 325mm wide x 460mm long
    spec.thickness = 44;
    spec.weight = 1800;  // ~1.8 kg
    spec.material = "Alder or Ash";
    spec.finish = "Polyurethane";
    spec.colors = {"Sunburst", "Olympic White", "Black", "Surf Green", "Lake Placid Blue"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$150-350";
    m_components.append(spec);
    qDebug() << "Added body:" << spec.manufacturer << spec.model;
    
    // Fender Telecaster Body
    spec = ComponentSpec();
    spec.name = "Fender Telecaster Body";
    spec.category = "Main Parts";
    spec.subcategory = "Body";
    spec.manufacturer = "Fender";
    spec.model = "Telecaster";
    spec.description = "Single cutaway ash or alder body with traditional routing";
    spec.dimensions = QVector2D(320, 450);  // 320mm wide x 450mm long
    spec.thickness = 44;
    spec.weight = 1900;  // ~1.9 kg
    spec.material = "Ash or Alder";
    spec.finish = "Polyurethane";
    spec.colors = {"Blonde", "Butterscotch", "Black", "White", "3-Color Sunburst"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$140-320";
    m_components.append(spec);
    
    // PRS Custom 24 Body
    spec = ComponentSpec();
    spec.name = "PRS Custom 24 Body";
    spec.category = "Main Parts";
    spec.subcategory = "Body";
    spec.manufacturer = "PRS";
    spec.model = "Custom 24";
    spec.description = "Mahogany body with figured maple top and bird inlays";
    spec.dimensions = QVector2D(340, 470);  // 340mm wide x 470mm long
    spec.thickness = 50;
    spec.weight = 2000;  // ~2.0 kg
    spec.material = "Mahogany with maple top";
    spec.finish = "Nitrocellulose";
    spec.colors = {"Vintage Yellow", "Fire Red Burst", "Whale Blue", "Wood Library"};
    spec.mountingType = "set-neck";
    spec.priceRange = "$250-500";
    m_components.append(spec);
    
    // Ibanez RG Body
    spec = ComponentSpec();
    spec.name = "Ibanez RG Body";
    spec.category = "Main Parts";
    spec.subcategory = "Body";
    spec.manufacturer = "Ibanez";
    spec.model = "RG Series";
    spec.description = "Basswood body with deep cutaways for high fret access";
    spec.dimensions = QVector2D(315, 445);  // 315mm wide x 445mm long
    spec.thickness = 43;
    spec.weight = 1700;  // ~1.7 kg
    spec.material = "Basswood";
    spec.finish = "Polyurethane";
    spec.colors = {"Black", "White", "Blue", "Jewel Blue", "Transparent Red"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$120-280";
    m_components.append(spec);
}

void ComponentDatabase::addGuitarNeckComponents() {
    qDebug() << "Adding guitar neck components to database...";
    ComponentSpec spec;
    
    // Gibson Les Paul Neck
    spec.name = "Gibson Les Paul Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "Gibson";
    spec.model = "Les Paul Standard";
    spec.description = "Mahogany neck with rosewood fingerboard, 24.75\" scale length";
    spec.dimensions = QVector2D(43, 628);  // 43mm width at nut, 628mm scale length
    spec.thickness = 22;  // neck thickness at first fret
    spec.weight = 650;  // ~650g
    spec.material = "Mahogany with rosewood fingerboard";
    spec.finish = "Satin nitrocellulose";
    spec.colors = {"Natural mahogany", "Vintage tint"};
    spec.mountingType = "set-neck";
    spec.priceRange = "$180-400";
    m_components.append(spec);
    qDebug() << "Added neck:" << spec.manufacturer << spec.model;
    
    // Fender Stratocaster Neck
    spec = ComponentSpec();
    spec.name = "Fender Stratocaster Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "Fender";
    spec.model = "Stratocaster";
    spec.description = "Maple neck with maple or rosewood fingerboard, 25.5\" scale length";
    spec.dimensions = QVector2D(42, 648);  // 42mm width at nut, 648mm scale length
    spec.thickness = 21;  // neck thickness at first fret
    spec.weight = 580;  // ~580g
    spec.material = "Maple with maple or rosewood fingerboard";
    spec.finish = "Gloss polyurethane";
    spec.colors = {"Natural maple", "Tinted maple", "Rosewood"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$150-350";
    m_components.append(spec);
    qDebug() << "Added neck:" << spec.manufacturer << spec.model;
    
    // Fender Telecaster Neck
    spec = ComponentSpec();
    spec.name = "Fender Telecaster Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "Fender";
    spec.model = "Telecaster";
    spec.description = "Maple neck with maple fingerboard, 25.5\" scale length";
    spec.dimensions = QVector2D(42.8, 648);  // 42.8mm width at nut, 648mm scale length
    spec.thickness = 21.5;  // neck thickness at first fret
    spec.weight = 590;  // ~590g
    spec.material = "Maple with maple fingerboard";
    spec.finish = "Gloss polyurethane";
    spec.colors = {"Natural maple", "Vintage tint"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$140-320";
    m_components.append(spec);
    
    // PRS Custom 24 Neck
    spec = ComponentSpec();
    spec.name = "PRS Custom 24 Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "PRS";
    spec.model = "Custom 24";
    spec.description = "Mahogany neck with rosewood fingerboard, 25\" scale length";
    spec.dimensions = QVector2D(43, 635);  // 43mm width at nut, 635mm scale length
    spec.thickness = 21.5;  // neck thickness at first fret
    spec.weight = 620;  // ~620g
    spec.material = "Mahogany with rosewood fingerboard";
    spec.finish = "Satin nitrocellulose";
    spec.colors = {"Natural mahogany", "Vintage amber"};
    spec.mountingType = "set-neck";
    spec.priceRange = "$200-450";
    m_components.append(spec);
    
    // Ibanez RG Neck
    spec = ComponentSpec();
    spec.name = "Ibanez RG Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "Ibanez";
    spec.model = "RG Series";
    spec.description = "Wizard neck profile with rosewood fingerboard, 25.5\" scale";
    spec.dimensions = QVector2D(42, 648);  // 42mm width at nut, 648mm scale length
    spec.thickness = 19;  // thin Wizard profile
    spec.weight = 520;  // ~520g
    spec.material = "Maple with rosewood fingerboard";
    spec.finish = "Satin polyurethane";
    spec.colors = {"Natural maple", "Rosewood"};
    spec.mountingType = "bolt-on";
    spec.priceRange = "$100-250";
    m_components.append(spec);
    
    // Martin D-28 Neck (Acoustic)
    spec = ComponentSpec();
    spec.name = "Martin D-28 Neck";
    spec.category = "Main Parts";
    spec.subcategory = "Neck";
    spec.manufacturer = "Martin";
    spec.model = "D-28";
    spec.description = "East Indian rosewood neck with ebony fingerboard";
    spec.dimensions = QVector2D(44.5, 648);  // 44.5mm width at nut, 25.4\" scale
    spec.thickness = 23;  // acoustic neck thickness
    spec.weight = 680;  // ~680g
    spec.material = "East Indian rosewood with ebony fingerboard";
    spec.finish = "Satin";
    spec.colors = {"Natural rosewood", "Ebony"};
    spec.mountingType = "dovetail joint";
    spec.priceRange = "$250-600";
    m_components.append(spec);
}

void ComponentDatabase::addGuitarHeadstockComponents() {
    qDebug() << "Adding guitar headstock components to database...";
    ComponentSpec spec;
    
    // Gibson Les Paul Headstock
    spec.name = "Gibson Les Paul Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "Gibson";
    spec.model = "Les Paul Standard";
    spec.description = "Traditional angled headstock with Gibson crown inlay";
    spec.dimensions = QVector2D(85, 180);  // 85mm wide x 180mm long
    spec.thickness = 18;  // headstock thickness
    spec.weight = 120;  // ~120g
    spec.material = "Mahogany";
    spec.finish = "Gloss nitrocellulose";
    spec.colors = {"Honey Burst", "Cherry Burst", "Tobacco Burst", "Heritage Cherry", "Ebony"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$50-120" ;
    m_components.append(spec);
    qDebug() << "Added headstock:" << spec.manufacturer << spec.model;
    
    // Fender Stratocaster Headstock
    spec = ComponentSpec();
    spec.name = "Fender Stratocaster Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "Fender";
    spec.model = "Stratocaster";
    spec.description = "Classic straight headstock with Fender logo";
    spec.dimensions = QVector2D(89, 175);  // 89mm wide x 175mm long
    spec.thickness = 15;  // headstock thickness
    spec.weight = 95;  // ~95g
    spec.material = "Maple";
    spec.finish = "Gloss polyurethane";
    spec.colors = {"Natural maple", "Olympic White", "Black", "Sunburst"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$40-100";
    m_components.append(spec);
    qDebug() << "Added headstock:" << spec.manufacturer << spec.model;
    
    // Fender Telecaster Headstock
    spec = ComponentSpec();
    spec.name = "Fender Telecaster Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "Fender";
    spec.model = "Telecaster";
    spec.description = "Vintage style straight headstock with string tree";
    spec.dimensions = QVector2D(89, 170);  // 89mm wide x 170mm long
    spec.thickness = 15;  // headstock thickness
    spec.weight = 90;  // ~90g
    spec.material = "Maple";
    spec.finish = "Gloss polyurethane";
    spec.colors = {"Natural maple", "Blonde", "White", "Black"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$35-95";
    m_components.append(spec);
    
    // PRS Custom 24 Headstock
    spec = ComponentSpec();
    spec.name = "PRS Custom 24 Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "PRS";
    spec.model = "Custom 24";
    spec.description = "Angled headstock with PRS bird logo and mother of pearl inlay";
    spec.dimensions = QVector2D(78, 165);  // 78mm wide x 165mm long
    spec.thickness = 16;  // headstock thickness
    spec.weight = 105;  // ~105g
    spec.material = "Mahogany";
    spec.finish = "Gloss nitrocellulose";
    spec.colors = {"Vintage Yellow", "Fire Red Burst", "Whale Blue", "Natural"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$60-140";
    m_components.append(spec);
    
    // Ibanez RG Headstock
    spec = ComponentSpec();
    spec.name = "Ibanez RG Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "Ibanez";
    spec.model = "RG Series";
    spec.description = "Pointed reverse headstock design with Ibanez logo";
    spec.dimensions = QVector2D(75, 160);  // 75mm wide x 160mm long
    spec.thickness = 14;  // thin headstock
    spec.weight = 80;  // ~80g
    spec.material = "Maple";
    spec.finish = "Satin polyurethane";
    spec.colors = {"Black", "White", "Natural maple", "Blue"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$30-80";
    m_components.append(spec);
    
    // Martin D-28 Headstock
    spec = ComponentSpec();
    spec.name = "Martin D-28 Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "Martin";
    spec.model = "D-28";
    spec.description = "Traditional slotted headstock with Martin logo and torch inlay";
    spec.dimensions = QVector2D(95, 185);  // 95mm wide x 185mm long
    spec.thickness = 20;  // acoustic headstock thickness
    spec.weight = 140;  // ~140g
    spec.material = "East Indian rosewood";
    spec.finish = "Satin";
    spec.colors = {"Natural rosewood", "Ebony"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$80-200";
    m_components.append(spec);
    
    // ESP Eclipse Headstock
    spec = ComponentSpec();
    spec.name = "ESP Eclipse Headstock";
    spec.category = "Main Parts";
    spec.subcategory = "Headstock";
    spec.manufacturer = "ESP";
    spec.model = "Eclipse";
    spec.description = "Angled headstock with ESP logo and binding";
    spec.dimensions = QVector2D(82, 175);  // 82mm wide x 175mm long
    spec.thickness = 17;  // headstock thickness
    spec.weight = 115;  // ~115g
    spec.material = "Mahogany";
    spec.finish = "Gloss polyurethane";
    spec.colors = {"Black", "See Thru Black Cherry", "Vintage Black"};
    spec.mountingType = "integral with neck";
    spec.priceRange = "$45-110";
    m_components.append(spec);
}
