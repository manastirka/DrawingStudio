#pragma once

#include <QVector2D>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <memory>
#include <map>

class GuitarComponent;

class ComponentLibrary
{
public:
    ComponentLibrary() = default;
    ~ComponentLibrary() = default;
    
    // Static component creation methods
    static std::unique_ptr<GuitarComponent> createPickup(const QVector2D &position, const QString &subType = "Single");
    static std::unique_ptr<GuitarComponent> createBridge(const QVector2D &position, const QString &subType = "Fixed");
    static std::unique_ptr<GuitarComponent> createTuner(const QVector2D &position, const QString &subType = "Standard");
    
    // Component type information
    static QStringList getAvailableComponentTypes();
    static QStringList getComponentSubTypes(const QString &componentType);
    static QString getComponentDescription(const QString &componentType, const QString &subType = "");
    
    // Component defaults and presets
    static QJsonObject getDefaultProperties(const QString &componentType, const QString &subType);
    static QJsonObject getComponentPreset(const QString &presetName);
    
    // Library management
    static void registerPreset(const QString &name, const QJsonObject &preset);
    static QStringList getAvailablePresets();
    
private:
    static std::map<QString, QJsonObject> s_presets;
};