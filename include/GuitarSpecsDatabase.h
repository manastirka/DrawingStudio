#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QVariant>

enum class GuitarBodyStyle {
    SolidBody,
    SemiHollow,
    Hollow,
    HollowBody,
    Chambered
};

enum class GuitarManufacturer {
    Fender,
    Gibson,
    PRS,
    Ibanez,
    ESP,
    Jackson,
    Schecter,
    Epiphone,
    Squier,
    Martin,
    Taylor,
    Yamaha,
    Gretsch,
    Rickenbacker,
    MusicMan,
    Suhr,
    Charvel,
    Kramer,
    BCRich,
    Dean,
    Washburn,
    Ovation,
    Godin,
    Seagull,
    Guild
};

enum class PickupConfiguration {
    SSS,    // 3 Single coils (Strat style)
    HSS,    // Humbucker + 2 Single coils
    HSH,    // Humbucker + Single + Humbucker
    HH,     // 2 Humbuckers (Les Paul style)
    HHH,    // 3 Humbuckers
    P90_P90, // 2 P90s
    Single, // 1 pickup
    SS,     // 2 Single coils
    P,      // 1 P90
    JJ      // 2 Jazz bass pickups
};

struct GuitarSpecs {
    // Basic Info
    QString manufacturer;
    QString model;
    QString series;
    int yearIntroduced;
    QString description;
    
    // Body Specifications (all measurements in mm)
    GuitarBodyStyle bodyStyle;
    float bodyLength;
    float bodyWidth;
    float bodyThickness;
    float upperBoutWidth;
    float lowerBoutWidth;
    float waistWidth;
    QString bodyWood;
    QString bodyFinish;
    
    // Neck Specifications
    float scaleLength;
    float neckLength;
    int numberOfFrets;
    float nutWidth;
    float neckThickness1stFret;
    float neckThickness12thFret;
    QString neckWood;
    QString fretboardWood;
    QString neckProfile; // C, D, V, etc.
    QString neckJoint;   // Bolt-on, Set, Neck-through
    
    // Headstock
    float headstockLength;
    float headstockWidth;
    QString headstockStyle; // 6-in-line, 3+3, 4+2, etc.
    
    // Hardware
    PickupConfiguration pickupConfig;
    QStringList pickupTypes;
    QString bridge;
    QString tailpiece;
    QString tuners;
    int numberOfStrings;
    
    // Electronics
    int numberOfVolumes;
    int numberOfTones;
    QString switchType;
    QString wiring;
    
    // Additional Features
    QStringList specialFeatures;
    QString priceRange;
    bool isLeftHanded;
    
    // Constructor
    GuitarSpecs() : 
        yearIntroduced(0),
        bodyStyle(GuitarBodyStyle::SolidBody),
        bodyLength(0), bodyWidth(0), bodyThickness(0),
        upperBoutWidth(0), lowerBoutWidth(0), waistWidth(0),
        scaleLength(0), neckLength(0), numberOfFrets(24),
        nutWidth(0), neckThickness1stFret(0), neckThickness12thFret(0),
        headstockLength(0), headstockWidth(0),
        pickupConfig(PickupConfiguration::HH),
        numberOfStrings(6), numberOfVolumes(2), numberOfTones(2),
        isLeftHanded(false) {}
};

class GuitarSpecsDatabase
{
public:
    static GuitarSpecsDatabase& instance();
    
    // Database access
    QVector<GuitarSpecs> getAllSpecs() const;
    QVector<GuitarSpecs> getSpecsByManufacturer(GuitarManufacturer manufacturer) const;
    QVector<GuitarSpecs> getSpecsByModel(const QString& model) const;
    QVector<GuitarSpecs> searchSpecs(const QString& searchTerm) const;
    
    // Utility functions
    QStringList getAllManufacturers() const;
    QStringList getModelsByManufacturer(GuitarManufacturer manufacturer) const;
    QString manufacturerToString(GuitarManufacturer manufacturer) const;
    QString bodyStyleToString(GuitarBodyStyle style) const;
    QString pickupConfigToString(PickupConfiguration config) const;
    
    // Get specific specs
    GuitarSpecs getSpecsByManufacturerAndModel(GuitarManufacturer manufacturer, const QString& model) const;
    
private:
    GuitarSpecsDatabase();
    void initializeDatabase();
    
    // Database storage
    QVector<GuitarSpecs> m_specs;
    QMap<GuitarManufacturer, QVector<GuitarSpecs>> m_specsByManufacturer;
    
    // Helper methods for data initialization
    void addFenderModels();
    void addGibsonModels();
    void addPRSModels();
    void addIbanezModels();
    void addESPModels();
    void addJacksonModels();
    void addSchecterModels();
    void addEpiphoneModels();
    void addSquierModels();
    void addMartinModels();
    void addTaylorModels();
    void addYamahaModels();
    void addGretschModels();
    void addRickenbackerModels();
    void addMusicManModels();
    void addSuhrModels();
    void addCharvelModels();
    void addKramerModels();
    void addBCRichModels();
    void addDeanModels();
    void addWashburnModels();
    void addOvationModels();
    void addGodinModels();
    void addSeagullModels();
    void addGuildModels();
};