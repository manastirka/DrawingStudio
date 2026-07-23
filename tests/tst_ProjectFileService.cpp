#include "CommandManager.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "ProjectFileService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class tst_ProjectFileService : public QObject {
    Q_OBJECT

private slots:
    void saveWritesLayersAndPrimitives();
    void saveFailureDoesNotUpdateSession();
    void excessiveLayerSaveDoesNotOverwriteFile();
    void loadRoundTripRestoresLine();
    void corruptLoadPreservesCurrentDocument();
    void oversizedLoadPreservesCurrentDocument();
    void excessiveLayerCountPreservesCurrentDocument();
    void excessivePrimitiveCountPreservesCurrentDocument();
    void excessiveGeometryPointCountPreservesCurrentDocument();
    void invalidEmbeddedImagePreservesCurrentDocument();
    void recoveryLoadDoesNotTouchSessionCallbacks();
    void silentSave_doesNotTouchSessionCallbacks();
};

void tst_ProjectFileService::saveWritesLayersAndPrimitives()
{
    LayerManager layers;
    layers.activeLayer(); // ensure default
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("t.drawing"));
    QVERIFY(svc.saveToFile(path));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("format")).toString(),
             QStringLiteral("DrawingStudio"));
    QVERIFY(root.contains(QStringLiteral("layers")));
    const QJsonArray arr = root.value(QStringLiteral("layers")).toArray();
    QVERIFY(!arr.isEmpty());
    const QJsonArray prims = arr.at(0).toObject().value(QStringLiteral("primitives")).toArray();
    QCOMPARE(prims.size(), 1);
    QCOMPARE(prims.at(0).toObject().value(QStringLiteral("type")).toInt(),
             static_cast<int>(PrimitiveType::Line));
}

void tst_ProjectFileService::saveFailureDoesNotUpdateSession()
{
    LayerManager layers;
    int currentFileCalls = 0;
    int recentCalls = 0;
    QString status;

    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setCurrentFile = [&](const QString &) { ++currentFileCalls; };
    host.addToRecentFiles = [&](const QString &) { ++recentCalls; };
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A directory cannot be atomically replaced as a project file.
    QVERIFY(!svc.saveToFile(dir.path()));
    QCOMPARE(currentFileCalls, 0);
    QCOMPARE(recentCalls, 0);
    QVERIFY(status.contains(QStringLiteral("failed"), Qt::CaseInsensitive));
    QVERIFY(QFileInfo(dir.path()).isDir());
}

void tst_ProjectFileService::excessiveLayerSaveDoesNotOverwriteFile()
{
    LayerManager layers;
    while (layers.layerCount()
           <= static_cast<size_t>(ProjectFileService::kMaxProjectLayers)) {
        layers.createLayer(QStringLiteral("Layer"));
    }

    int currentFileCalls = 0;
    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setCurrentFile = [&](const QString &) { ++currentFileCalls; };
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("existing.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("keep"), qint64(4));
    file.close();

    QVERIFY(!svc.saveToFile(path));
    QCOMPARE(currentFileCalls, 0);
    QVERIFY(status.contains(QStringLiteral("layer limit"), Qt::CaseInsensitive));

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("keep"));
}

void tst_ProjectFileService::loadRoundTripRestoresLine()
{
    LayerManager layersOut;
    layersOut.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(5, 6), QVector2D(7, 8)));

    ProjectFileService saver;
    ProjectFileService::Host sh;
    sh.layerManager = &layersOut;
    saver.setHost(sh);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("round.drawing"));
    QVERIFY(saver.saveToFile(path));

    LayerManager layersIn;
    CommandManager cmds;
    ProjectFileService loader;
    ProjectFileService::Host lh;
    lh.layerManager = &layersIn;
    lh.commandManager = &cmds;
    // dialogParent null → spinner still constructs as parentless dialog
    loader.setHost(lh);

    QVERIFY(loader.loadFromFile(path, /*waitUntilLoaded=*/true));

    // load clears and rebuilds; allow event loop to finish install
    QCoreApplication::processEvents();

    const auto prims = layersIn.getAllPrimitives();
    QVERIFY2(!prims.empty(), "expected at least one primitive after load");
    auto *line = dynamic_cast<LinePrimitive *>(prims.front());
    QVERIFY(line);
    QCOMPARE(line->startPoint(), QVector2D(5, 6));
    QCOMPARE(line->endPoint(), QVector2D(7, 8));
    QCOMPARE(line->thread(), QCoreApplication::instance()->thread());
}

void tst_ProjectFileService::corruptLoadPreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(10, 11), QVector2D(12, 13)));

    int currentFileCalls = 0;
    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setCurrentFile = [&](const QString &) { ++currentFileCalls; };
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("corrupt.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"layers":[{"name":"Bad","primitives":[{"type":999}]}]})");
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QCOMPARE(currentFileCalls, 0);
    QVERIFY(status.contains(QStringLiteral("failed"), Qt::CaseInsensitive));

    const auto primitives = layers.getAllPrimitives();
    QCOMPARE(primitives.size(), size_t(1));
    auto *line = dynamic_cast<LinePrimitive *>(primitives.front());
    QVERIFY(line);
    QCOMPARE(line->startPoint(), QVector2D(10, 11));
    QCOMPARE(line->endPoint(), QVector2D(12, 13));
}

void tst_ProjectFileService::oversizedLoadPreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("oversized.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(ProjectFileService::kMaxProjectFileBytes + 1));
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QVERIFY(status.contains(QStringLiteral("size limit"), Qt::CaseInsensitive));
    QCOMPARE(layers.getAllPrimitives().size(), static_cast<size_t>(1));
}

void tst_ProjectFileService::excessiveLayerCountPreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QJsonArray incomingLayers;
    QJsonObject layer;
    layer["name"] = QStringLiteral("Layer");
    layer["primitives"] = QJsonArray();
    for (qsizetype i = 0; i <= ProjectFileService::kMaxProjectLayers; ++i)
        incomingLayers.append(layer);

    QJsonObject root;
    root["format"] = QStringLiteral("DrawingStudio");
    root["version"] = 1;
    root["layers"] = incomingLayers;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("too-many-layers.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload = QJsonDocument(root).toJson();
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QVERIFY(status.contains(QStringLiteral("layer limit"), Qt::CaseInsensitive));
    QCOMPARE(layers.getAllPrimitives().size(), static_cast<size_t>(1));
}

void tst_ProjectFileService::excessivePrimitiveCountPreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QJsonArray incomingPrimitives;
    for (qsizetype i = 0; i <= ProjectFileService::kMaxProjectPrimitives; ++i)
        incomingPrimitives.append(QJsonObject());

    QJsonObject root;
    root["format"] = QStringLiteral("DrawingStudio");
    root["version"] = 1;
    root["primitives"] = incomingPrimitives;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path =
        dir.filePath(QStringLiteral("too-many-primitives.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload = QJsonDocument(root).toJson();
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QVERIFY(status.contains(QStringLiteral("primitive limit"),
                            Qt::CaseInsensitive));
    QCOMPARE(layers.getAllPrimitives().size(), static_cast<size_t>(1));
}

void tst_ProjectFileService::excessiveGeometryPointCountPreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    QJsonObject point;
    point["x"] = 0.0;
    point["y"] = 0.0;
    QJsonArray points;
    for (qsizetype i = 0;
         i < DrawingPrimitive::kMaxSerializedPointsPerPrimitive; ++i) {
        points.append(point);
    }

    QJsonObject polygon;
    polygon["type"] = static_cast<int>(PrimitiveType::Polygon);
    polygon["points"] = points;
    QJsonArray primitives;
    const qsizetype polygonCount =
        ProjectFileService::kMaxProjectGeometryPoints
            / DrawingPrimitive::kMaxSerializedPointsPerPrimitive
        + 1;
    for (qsizetype i = 0; i < polygonCount; ++i)
        primitives.append(polygon);

    QJsonObject root;
    root["format"] = QStringLiteral("DrawingStudio");
    root["version"] = 1;
    root["primitives"] = primitives;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path =
        dir.filePath(QStringLiteral("too-many-geometry-points.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload = QJsonDocument(root).toJson();
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QVERIFY(status.contains(QStringLiteral("geometry point limit"),
                            Qt::CaseInsensitive));
    QCOMPARE(layers.getAllPrimitives().size(), static_cast<size_t>(1));
}

void tst_ProjectFileService::invalidEmbeddedImagePreservesCurrentDocument()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(1, 2), QVector2D(3, 4)));

    QString status;
    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setStatusText = [&](const QString &message) { status = message; };
    svc.setHost(host);

    ImagePrimitive image(QImage(2, 2, QImage::Format_ARGB32), QVector2D(),
                         QVector2D(2, 2));
    QJsonObject imageJson = image.toJson();
    imageJson["imageData"] = QStringLiteral("invalid@@base64");
    QJsonArray primitives;
    primitives.append(imageJson);
    QJsonObject layer;
    layer["name"] = QStringLiteral("Incoming");
    layer["primitives"] = primitives;
    QJsonArray incomingLayers;
    incomingLayers.append(layer);
    QJsonObject root;
    root["format"] = QStringLiteral("DrawingStudio");
    root["version"] = 1;
    root["layers"] = incomingLayers;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("invalid-image.drawing"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload = QJsonDocument(root).toJson();
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
    file.close();

    QVERIFY(!svc.loadFromFile(path, /*waitUntilLoaded=*/true));
    QVERIFY(status.contains(QStringLiteral("failed"), Qt::CaseInsensitive));
    const auto primitivesAfter = layers.getAllPrimitives();
    QCOMPARE(primitivesAfter.size(), static_cast<size_t>(1));
    QVERIFY(dynamic_cast<LinePrimitive *>(primitivesAfter.front()));
}

void tst_ProjectFileService::recoveryLoadDoesNotTouchSessionCallbacks()
{
    LayerManager sourceLayers;
    sourceLayers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(2, 3), QVector2D(4, 5)));

    ProjectFileService saver;
    ProjectFileService::Host saveHost;
    saveHost.layerManager = &sourceLayers;
    saver.setHost(saveHost);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("recovery.drawing"));
    QVERIFY(saver.saveToFile(path, /*updateSession=*/false));

    LayerManager restoredLayers;
    int currentFileCalls = 0;
    int recentCalls = 0;
    ProjectFileService loader;
    ProjectFileService::Host loadHost;
    loadHost.layerManager = &restoredLayers;
    loadHost.setCurrentFile = [&](const QString &) { ++currentFileCalls; };
    loadHost.addToRecentFiles = [&](const QString &) { ++recentCalls; };
    loader.setHost(loadHost);

    QVERIFY(loader.loadFromFile(path, /*waitUntilLoaded=*/true,
                                /*updateSession=*/false));
    QCOMPARE(currentFileCalls, 0);
    QCOMPARE(recentCalls, 0);
    QVERIFY(QFileInfo::exists(path));

    const auto primitives = restoredLayers.getAllPrimitives();
    QCOMPARE(primitives.size(), size_t(1));
    QCOMPARE(primitives.front()->thread(), QCoreApplication::instance()->thread());
}

void tst_ProjectFileService::silentSave_doesNotTouchSessionCallbacks()
{
    LayerManager layers;
    layers.addPrimitiveToActiveLayer(
        std::make_unique<LinePrimitive>(QVector2D(0, 0), QVector2D(1, 1)));

    int currentFileCalls = 0;
    int recentCalls = 0;
    QString lastStatus;

    ProjectFileService svc;
    ProjectFileService::Host host;
    host.layerManager = &layers;
    host.setCurrentFile = [&](const QString &) { ++currentFileCalls; };
    host.addToRecentFiles = [&](const QString &) { ++recentCalls; };
    host.setStatusText = [&](const QString &s) { lastStatus = s; };
    svc.setHost(host);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("autosave.drawing"));
    QVERIFY(svc.saveToFile(path, /*updateSession=*/false));

    QCOMPARE(currentFileCalls, 0);
    QCOMPARE(recentCalls, 0);
    QVERIFY(lastStatus.contains(QStringLiteral("Autosaved"), Qt::CaseInsensitive)
            || lastStatus.contains(QStringLiteral("recovery"), Qt::CaseInsensitive));
    QVERIFY(QFileInfo::exists(path));
}

QTEST_MAIN(tst_ProjectFileService)
#include "tst_ProjectFileService.moc"
