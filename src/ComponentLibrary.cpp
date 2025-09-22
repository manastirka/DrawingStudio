#include "ComponentLibrary.h"
#include "GuitarComponent.h"
#include "DrawingPrimitive.h"
#include <QJsonDocument>

// Static member initialization
std::map<QString, QJsonObject> ComponentLibrary::s_presets;

std::unique_ptr<GuitarComponent> ComponentLibrary::createPickup(const QVector2D &position, const QString &subType)
{
    auto pickup = std::make_unique<PickupComponent>(position);
    
    if (subType == "Single") {
        pickup->setPickupType(PickupComponent::PickupType::Single);
        pickup->setName("Single Coil Pickup");
    } else if (subType == "Humbucker") {
        pickup->setPickupType(PickupComponent::PickupType::Humbucker);
        pickup->setName("Humbucker Pickup");
    } else if (subType == "P90") {
        pickup->setPickupType(PickupComponent::PickupType::P90);
        pickup->setName("P90 Pickup");
    }
    
    return std::move(pickup);
}

std::unique_ptr<GuitarComponent> ComponentLibrary::createBridge(const QVector2D &position, const QString &subType)
{
    auto bridge = std::make_unique<BridgeComponent>(position);
    
    if (subType == "Fixed") {
        bridge->setBridgeType(BridgeComponent::BridgeType::Fixed);
        bridge->setName("Fixed Bridge");
    } else if (subType == "Tremolo") {
        bridge->setBridgeType(BridgeComponent::BridgeType::Tremolo);
        bridge->setName("Tremolo Bridge");
    } else if (subType == "Tune-o-matic") {
        bridge->setBridgeType(BridgeComponent::BridgeType::Tune_o_matic);
        bridge->setName("Tune-o-matic Bridge");
    }
    
    return std::move(bridge);
}

std::unique_ptr<GuitarComponent> ComponentLibrary::createTuner(const QVector2D &position, const QString &subType)
{
    auto tuner = std::make_unique<TunerComponent>(position);
    
    if (subType == "Standard") {
        tuner->setTunerType(TunerComponent::TunerType::Standard);
        tuner->setName("Standard Tuners");
    } else if (subType == "Locking") {
        tuner->setTunerType(TunerComponent::TunerType::Locking);
        tuner->setName("Locking Tuners");
    } else if (subType == "Vintage") {
        tuner->setTunerType(TunerComponent::TunerType::Vintage);
        tuner->setName("Vintage Tuners");
    }
    
    return std::move(tuner);
}

QStringList ComponentLibrary::getAvailableComponentTypes()
{
    return QStringList{
        "Pickup",
        "Bridge", 
        "Tuner",
        "Nut",
        "Fret",
        "Inlay",
        "SoundHole",
        "Electronics"
    };
}

QStringList ComponentLibrary::getComponentSubTypes(const QString &componentType)
{
    if (componentType == "Pickup") {
        return QStringList{"Single", "Humbucker", "P90"};
    } else if (componentType == "Bridge") {
        return QStringList{"Fixed", "Tremolo", "Tune-o-matic"};
    } else if (componentType == "Tuner") {
        return QStringList{"Standard", "Locking", "Vintage"};
    } else if (componentType == "Nut") {
        return QStringList{"Bone", "Plastic", "Brass", "GraphTech"};
    } else if (componentType == "Electronics") {
        return QStringList{"Switch", "Potentiometer", "Jack", "Capacitor"};
    }
    
    return QStringList{};
}

QString ComponentLibrary::getComponentDescription(const QString &componentType, const QString &subType)
{
    if (componentType == "Pickup") {
        if (subType == "Single") {
            return "Single coil pickup - bright, crisp tone with some hum";
        } else if (subType == "Humbucker") {
            return "Humbucker pickup - fuller, warmer tone with noise cancellation";
        } else if (subType == "P90") {
            return "P90 pickup - warm single coil with higher output";
        }
    } else if (componentType == "Bridge") {
        if (subType == "Fixed") {
            return "Fixed bridge - stable tuning, sustained resonance";
        } else if (subType == "Tremolo") {
            return "Tremolo bridge - pitch bending capabilities";
        } else if (subType == "Tune-o-matic") {
            return "Tune-o-matic bridge - adjustable intonation and action";
        }
    } else if (componentType == "Tuner") {
        if (subType == "Standard") {
            return "Standard tuning machines - reliable and affordable";
        } else if (subType == "Locking") {
            return "Locking tuners - quick string changes, stable tuning";
        } else if (subType == "Vintage") {
            return "Vintage-style tuners - classic look and feel";
        }
    }
    
    return QString("Guitar component: %1").arg(componentType);
}

QJsonObject ComponentLibrary::getDefaultProperties(const QString &componentType, const QString &subType)
{
    QJsonObject props;
    
    if (componentType == "Pickup") {
        props["color"] = "#000000";
        if (subType == "Single") {
            props["poles"] = 6;
            props["height"] = 3.2; // mm
        } else if (subType == "Humbucker") {
            props["poles"] = 12;
            props["height"] = 8.5; // mm
        } else if (subType == "P90") {
            props["poles"] = 6;
            props["height"] = 6.0; // mm
        }
    } else if (componentType == "Bridge") {
        props["stringCount"] = 6;
        props["stringSpacing"] = 10.5; // mm
        if (subType == "Fixed") {
            props["height"] = 12.0; // mm
        } else if (subType == "Tremolo") {
            props["height"] = 15.0; // mm
            props["armLength"] = 120.0; // mm
        }
    } else if (componentType == "Tuner") {
        props["tunerCount"] = 6;
        props["ratio"] = 18; // gear ratio
        props["buttonSize"] = 15.0; // mm diameter
    }
    
    return props;
}

QJsonObject ComponentLibrary::getComponentPreset(const QString &presetName)
{
    auto it = s_presets.find(presetName);
    if (it != s_presets.end()) {
        return it->second;
    }
    return QJsonObject{};
}

void ComponentLibrary::registerPreset(const QString &name, const QJsonObject &preset)
{
    s_presets[name] = preset;
}

QStringList ComponentLibrary::getAvailablePresets()
{
    QStringList presets;
    for (const auto& pair : s_presets) {
        presets.append(pair.first);
    }
    return presets;
}