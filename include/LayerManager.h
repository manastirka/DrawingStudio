#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <memory>
#include <vector>

class Layer;
class DrawingPrimitive;

class LayerManager : public QObject
{
    Q_OBJECT

public:
    explicit LayerManager(QObject* parent = nullptr);
    ~LayerManager();

    // Layer management
    Layer* createLayer(const QString& name = "New Layer");
    Layer* createLayer(const QUuid& id, const QString& name = "New Layer");
    void deleteLayer(const QUuid& layerId);
    void deleteLayer(Layer* layer);
    std::unique_ptr<Layer> takeLayer(const QUuid& layerId, size_t* indexOut = nullptr);
    Layer* insertLayer(std::unique_ptr<Layer> layer, size_t index);
    void clearLayers();
    void clearLayersNoDefault(); // Clear without creating default layer
    
    // Layer access
    Layer* getLayer(const QUuid& layerId) const;
    Layer* getLayerByName(const QString& name) const;
    Layer* getLayerAt(size_t index) const;
    size_t getLayerIndex(const QUuid& layerId) const;
    size_t getLayerIndex(Layer* layer) const;
    
    const std::vector<std::unique_ptr<Layer>>& layers() const { return m_layers; }
    size_t layerCount() const { return m_layers.size(); }
    bool isEmpty() const { return m_layers.empty(); }
    
    // Active/current layer
    Layer* activeLayer() const { return m_activeLayer; }
    void setActiveLayer(const QUuid& layerId);
    void setActiveLayer(Layer* layer);
    void setActiveLayer(size_t index);
    
    // Layer ordering
    void moveLayerUp(const QUuid& layerId);
    void moveLayerDown(const QUuid& layerId);
    void moveLayerUp(Layer* layer);
    void moveLayerDown(Layer* layer);
    void moveLayerToIndex(const QUuid& layerId, size_t newIndex);
    
    // Layer duplication
    Layer* duplicateLayer(Layer* source);
    
    // Primitive operations
    void addPrimitiveToActiveLayer(std::unique_ptr<DrawingPrimitive> primitive);
    void addPrimitiveToLayer(const QUuid& layerId, std::unique_ptr<DrawingPrimitive> primitive);
    void movePrimitiveTo(DrawingPrimitive* primitive, const QUuid& targetLayerId);
    void movePrimitiveTo(DrawingPrimitive* primitive, Layer* targetLayer);
    Layer* findLayerContaining(DrawingPrimitive* primitive) const;
    std::vector<DrawingPrimitive*> getAllPrimitives() const;
    
    // Object ordering within layers
    void bringToFront(DrawingPrimitive* primitive);
    void sendToBack(DrawingPrimitive* primitive);
    void bringForward(DrawingPrimitive* primitive);
    void sendBackward(DrawingPrimitive* primitive);
    void movePrimitiveToTop(DrawingPrimitive* primitive);
    void movePrimitiveToBottom(DrawingPrimitive* primitive);
    
    // Bulk operations
    void mergeLayerDown(const QUuid& layerId);
    void flattenLayers(); // Merge all visible layers into one
    
    // Layer visibility management
    void showAllLayers();
    void hideAllLayers();
    void showOnlyLayer(const QUuid& layerId);
    
    // Utility
    QString generateUniqueLayerName(const QString& baseName = "Layer") const;
    void reorderLayers(); // Update z-order based on layer order

signals:
    void layerCreated(Layer* layer);
    void layerDeleted(const QUuid& layerId);
    void layerRenamed(Layer* layer, const QString& oldName);
    void layerVisibilityChanged(Layer* layer, bool visible);
    void layerOpacityChanged(Layer* layer, float opacity);
    void layerBlendModeChanged(Layer* layer, int blendMode);
    void activeLayerChanged(Layer* layer);
    void layersReordered();
    void layerLockChanged(Layer* layer, bool locked);
    void primitiveAdded(Layer* layer, DrawingPrimitive* primitive);

private slots:
    void onLayerPropertyChanged();

private:
    void ensureDefaultLayer();
    void updateLayerOrdering();
    size_t findLayerIndex(Layer* layer) const;

    std::vector<std::unique_ptr<Layer>> m_layers;
    Layer* m_activeLayer;
    int m_nextLayerNumber;
};
