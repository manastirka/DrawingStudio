#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QtTest>
#include <limits>
#include <memory>

class tst_LayerManager : public QObject {
    Q_OBJECT

private slots:
    void defaultLayerExists();
    void createAndActivateLayer();
    void addPrimitiveToActiveLayer();
    void moveLayerOrdering();
    void clearLayersRestoresDefault();
    void nonFiniteOpacityIsIgnored();
};

void tst_LayerManager::defaultLayerExists()
{
    LayerManager mgr;
    QVERIFY(mgr.layerCount() >= 1);
    QVERIFY(mgr.activeLayer() != nullptr);
    QCOMPARE(static_cast<int>(mgr.getAllPrimitives().size()), 0);
}

void tst_LayerManager::createAndActivateLayer()
{
    LayerManager mgr;
    Layer *a = mgr.activeLayer();
    QVERIFY(a);

    Layer *b = mgr.createLayer(QStringLiteral("Sketch"));
    QVERIFY(b);
    QCOMPARE(b->name(), QStringLiteral("Sketch"));
    mgr.setActiveLayer(b);
    QCOMPARE(mgr.activeLayer(), b);
    QVERIFY(mgr.layerCount() >= 2);
}

void tst_LayerManager::addPrimitiveToActiveLayer()
{
    LayerManager mgr;
    Layer *layer = mgr.activeLayer();
    QVERIFY(layer);

    auto line = std::make_unique<LinePrimitive>(QVector2D(0, 0), QVector2D(10, 20));
    DrawingPrimitive *raw = line.get();
    mgr.addPrimitiveToActiveLayer(std::move(line));

    auto all = mgr.getAllPrimitives();
    QVERIFY(std::find(all.begin(), all.end(), raw) != all.end());
    QCOMPARE(static_cast<int>(layer->primitives().size()), 1);
}

void tst_LayerManager::moveLayerOrdering()
{
    LayerManager mgr;
    Layer *first = mgr.activeLayer();
    Layer *second = mgr.createLayer(QStringLiteral("Top"));
    QVERIFY(first && second);

    const size_t idxBefore = mgr.getLayerIndex(second);
    mgr.moveLayerToIndex(second->id(), 0);
    QCOMPARE(mgr.getLayerAt(0), second);
    QVERIFY(mgr.getLayerIndex(first) > 0 || mgr.layerCount() == 1);
    Q_UNUSED(idxBefore);
}

void tst_LayerManager::clearLayersRestoresDefault()
{
    LayerManager mgr;
    mgr.createLayer(QStringLiteral("A"));
    mgr.createLayer(QStringLiteral("B"));
    QVERIFY(mgr.layerCount() >= 3);

    mgr.clearLayers();
    QVERIFY(mgr.layerCount() >= 1);
    QVERIFY(mgr.activeLayer() != nullptr);
}

void tst_LayerManager::nonFiniteOpacityIsIgnored()
{
    Layer layer(QStringLiteral("Safe"));
    layer.setOpacity(0.4f);
    layer.setOpacity(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(layer.opacity(), 0.4f);

    layer.setOpacity(-5.0f);
    QCOMPARE(layer.opacity(), 0.0f);
    layer.setOpacity(5.0f);
    QCOMPARE(layer.opacity(), 1.0f);
}

QTEST_MAIN(tst_LayerManager)
#include "tst_LayerManager.moc"
