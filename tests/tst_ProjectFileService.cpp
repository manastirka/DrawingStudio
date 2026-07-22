#include "CommandManager.h"
#include "DrawingPrimitive.h"
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
    void loadRoundTripRestoresLine();
    void corruptLoadPreservesCurrentDocument();
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
