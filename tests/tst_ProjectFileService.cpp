#include "CommandManager.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "ProjectFileService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class tst_ProjectFileService : public QObject {
    Q_OBJECT

private slots:
    void saveWritesLayersAndPrimitives();
    void loadRoundTripRestoresLine();
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
    QVERIFY(root.contains(QStringLiteral("layers")));
    const QJsonArray arr = root.value(QStringLiteral("layers")).toArray();
    QVERIFY(!arr.isEmpty());
    const QJsonArray prims = arr.at(0).toObject().value(QStringLiteral("primitives")).toArray();
    QCOMPARE(prims.size(), 1);
    QCOMPARE(prims.at(0).toObject().value(QStringLiteral("type")).toInt(),
             static_cast<int>(PrimitiveType::Line));
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
