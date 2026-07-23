#include "ProjectFileService.h"

#include "CommandManager.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"
#include "SpinnerDialog.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <cmath>
#include <memory>
#include <vector>

namespace {
QString validateLayerMetadata(const QJsonObject &layer)
{
    if (layer.contains(QStringLiteral("name"))) {
        const QJsonValue name = layer.value(QStringLiteral("name"));
        if (!name.isString())
            return QStringLiteral("layer name is invalid");
        if (name.toString().size()
            > ProjectFileService::kMaxLayerNameCharacters) {
            return QStringLiteral("layer name exceeds the limit");
        }
    }

    if (layer.contains(QStringLiteral("id"))) {
        const QJsonValue id = layer.value(QStringLiteral("id"));
        if (!id.isString() || QUuid(id.toString()).isNull())
            return QStringLiteral("layer id is invalid");
    }

    for (const char *key : {"visible", "locked"}) {
        const QString field = QString::fromLatin1(key);
        if (layer.contains(field) && !layer.value(field).isBool())
            return QStringLiteral("layer ") + field + QStringLiteral(" is invalid");
    }

    if (layer.contains(QStringLiteral("opacity"))) {
        const QJsonValue opacity = layer.value(QStringLiteral("opacity"));
        const double value = opacity.toDouble(-1.0);
        if (!opacity.isDouble() || !std::isfinite(value)
            || value < 0.0 || value > 1.0) {
            return QStringLiteral("layer opacity is invalid");
        }
    }
    return {};
}

QString validateCanvasMetadata(const QJsonObject &canvas)
{
    for (const char *key : {"backgroundColor", "paperColor"}) {
        const QString field = QString::fromLatin1(key);
        if (canvas.contains(field)
            && (!canvas.value(field).isString()
                || !QColor(canvas.value(field).toString()).isValid())) {
            return QStringLiteral("canvas ") + field
                   + QStringLiteral(" is invalid");
        }
    }

    for (const char *key : {"gridVisible", "snapEnabled"}) {
        const QString field = QString::fromLatin1(key);
        if (canvas.contains(field) && !canvas.value(field).isBool())
            return QStringLiteral("canvas ") + field
                   + QStringLiteral(" is invalid");
    }
    return {};
}

QString validateProjectIdentities(const QJsonObject &json, bool hasLayers)
{
    QSet<QUuid> layerIds;
    QSet<QUuid> primitiveIds;
    const auto validatePrimitives =
        [&primitiveIds](const QJsonArray &primitives) {
            for (const QJsonValue &primitiveValue : primitives) {
                if (!primitiveValue.isObject())
                    continue;
                const QJsonObject primitive = primitiveValue.toObject();
                if (!primitive.contains(QStringLiteral("id")))
                    continue;
                const QUuid id(
                    primitive.value(QStringLiteral("id")).toString());
                if (primitiveIds.contains(id))
                    return QStringLiteral("duplicate primitive id");
                primitiveIds.insert(id);
            }
            return QString();
        };

    if (!hasLayers) {
        return validatePrimitives(
            json.value(QStringLiteral("primitives")).toArray());
    }

    for (const QJsonValue &layerValue :
         json.value(QStringLiteral("layers")).toArray()) {
        if (!layerValue.isObject())
            continue;
        const QJsonObject layer = layerValue.toObject();
        if (layer.contains(QStringLiteral("id"))) {
            const QUuid id(layer.value(QStringLiteral("id")).toString());
            if (layerIds.contains(id))
                return QStringLiteral("duplicate layer id");
            layerIds.insert(id);
        }
        const QString primitiveError = validatePrimitives(
            layer.value(QStringLiteral("primitives")).toArray());
        if (!primitiveError.isEmpty())
            return primitiveError;
    }
    return {};
}

QString validateProjectBudgets(const QJsonObject &json, bool hasLayers)
{
    qint64 totalPrimitives = 0;
    qint64 totalGeometryPoints = 0;
    const auto validatePrimitives =
        [&totalPrimitives, &totalGeometryPoints](const QJsonArray &primitives) {
            totalPrimitives += primitives.size();
            if (totalPrimitives > ProjectFileService::kMaxProjectPrimitives)
                return QStringLiteral("project exceeds the primitive limit");

            for (const QJsonValue &primitiveValue : primitives) {
                if (!primitiveValue.isObject())
                    return QStringLiteral("invalid primitive entry");
                const QJsonObject primitive = primitiveValue.toObject();
                QString primitiveError;
                if (!DrawingPrimitive::validateJson(primitive, &primitiveError))
                    return QStringLiteral("invalid primitive: ") + primitiveError;
                totalGeometryPoints +=
                    DrawingPrimitive::serializedPointCount(primitive);
                if (totalGeometryPoints
                    > ProjectFileService::kMaxProjectGeometryPoints) {
                    return QStringLiteral(
                        "project exceeds the geometry point limit");
                }
            }
            return QString();
        };

    if (!hasLayers) {
        const QJsonArray primitives = json[QStringLiteral("primitives")].toArray();
        return validatePrimitives(primitives);
    }

    const QJsonArray layers = json[QStringLiteral("layers")].toArray();
    if (layers.size() > ProjectFileService::kMaxProjectLayers)
        return QStringLiteral("project exceeds the layer limit");

    for (const QJsonValue &layerValue : layers) {
        if (!layerValue.isObject())
            return QStringLiteral("invalid layer entry");

        const QJsonObject layer = layerValue.toObject();
        const QString metadataError = validateLayerMetadata(layer);
        if (!metadataError.isEmpty())
            return metadataError;
        if (layer.contains(QStringLiteral("primitives"))
            && !layer.value(QStringLiteral("primitives")).isArray()) {
            return QStringLiteral("invalid layer primitives");
        }

        const QString primitiveError = validatePrimitives(
            layer.value(QStringLiteral("primitives")).toArray());
        if (!primitiveError.isEmpty())
            return primitiveError;
    }
    return {};
}
} // namespace

ProjectFileService::ProjectFileService(QObject *parent)
    : QObject(parent)
{
}

bool ProjectFileService::saveToFile(const QString& fileName, bool updateSession) {
    const auto fail = [this, updateSession](const QString &reason) {
        if (m_host.setStatusText) {
            m_host.setStatusText(
                (updateSession ? QStringLiteral("Save failed: ")
                               : QStringLiteral("Recovery autosave failed: "))
                + reason);
        }
        return false;
    };
    if (fileName.trimmed().isEmpty())
        return fail(QStringLiteral("empty file name"));

    QJsonObject json;
    json["format"] = QStringLiteral("DrawingStudio");
    json["version"] = 1;

    // Serialize all layers and their primitives
    QJsonArray layersArray;
    if (m_host.layerManager) {
        if (m_host.layerManager->layerCount()
            > static_cast<size_t>(kMaxProjectLayers)) {
            return fail(QStringLiteral("project exceeds the layer limit"));
        }

        qint64 totalPrimitives = 0;
        qint64 totalGeometryPoints = 0;
        for (size_t i = 0; i < m_host.layerManager->layerCount(); i++) {
            Layer* layer = m_host.layerManager->getLayerAt(i);
            if (!layer) continue;
            totalPrimitives += static_cast<qint64>(layer->primitiveCount());
            if (totalPrimitives > kMaxProjectPrimitives)
                return fail(QStringLiteral("project exceeds the primitive limit"));

            QJsonObject layerObj;
            layerObj["id"] = layer->id().toString();
            layerObj["name"] = layer->name();
            layerObj["visible"] = layer->isVisible();
            layerObj["locked"] = layer->isLocked();
            layerObj["opacity"] = layer->opacity();

            QJsonArray primitivesArray;
            for (const auto& prim : layer->primitives()) {
                if (prim) {
                    const QJsonObject primitiveJson = prim->toJson();
                    QString primitiveError;
                    if (!DrawingPrimitive::validateJson(primitiveJson,
                                                        &primitiveError)) {
                        return fail(QStringLiteral("invalid primitive: ")
                                    + primitiveError);
                    }
                    totalGeometryPoints +=
                        DrawingPrimitive::serializedPointCount(primitiveJson);
                    if (totalGeometryPoints > kMaxProjectGeometryPoints) {
                        return fail(QStringLiteral(
                            "project exceeds the geometry point limit"));
                    }
                    primitivesArray.append(primitiveJson);
                }
            }
            layerObj["primitives"] = primitivesArray;
            const QString metadataError = validateLayerMetadata(layerObj);
            if (!metadataError.isEmpty())
                return fail(metadataError);
            layersArray.append(layerObj);
        }
    }
    json["layers"] = layersArray;

    // Save canvas settings
    if (m_host.canvas) {
        QJsonObject canvasObj;
        canvasObj["backgroundColor"] = m_host.canvas->backgroundColor().name(QColor::HexArgb);
        canvasObj["paperColor"] = m_host.canvas->paperColor().name(QColor::HexArgb);
        canvasObj["gridVisible"] = m_host.canvas->isGridVisible();
        canvasObj["snapEnabled"] = m_host.canvas->isSnapEnabled();
        const QString canvasError = validateCanvasMetadata(canvasObj);
        if (!canvasError.isEmpty())
            return fail(canvasError);
        json["canvas"] = canvasObj;
    }

    const QString identityError = validateProjectIdentities(json, true);
    if (!identityError.isEmpty())
        return fail(identityError);

    const QByteArray payload = QJsonDocument(json).toJson();
    if (payload.isEmpty())
        return fail(QStringLiteral("serialization produced no data"));
    if (payload.size() > kMaxProjectFileBytes)
        return fail(QStringLiteral("project exceeds the 256 MiB size limit"));

    QSaveFile file(fileName);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());

    if (!updateSession) {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    qint64 writtenTotal = 0;
    while (writtenTotal < payload.size()) {
        const qint64 written =
            file.write(payload.constData() + writtenTotal,
                       payload.size() - writtenTotal);
        if (written <= 0) {
            const QString reason = file.errorString();
            file.cancelWriting();
            return fail(reason);
        }
        writtenTotal += written;
    }

    if (!file.commit())
        return fail(file.errorString());

    if (updateSession) {
        if (m_host.setCurrentFile)
            m_host.setCurrentFile(fileName);
        if (m_host.addToRecentFiles)
            m_host.addToRecentFiles(fileName);
        if (m_host.setStatusText)
            m_host.setStatusText(QStringLiteral("Saved: ") + fileName);
    } else if (m_host.setStatusText) {
        m_host.setStatusText(QStringLiteral("Autosaved recovery snapshot"));
    }
    return true;
}

bool ProjectFileService::loadFromFile(const QString& fileName) {
    return loadFromFile(fileName, false, true);
}

bool ProjectFileService::loadFromFile(const QString& fileName, bool waitUntilLoaded) {
    return loadFromFile(fileName, waitUntilLoaded, true);
}

bool ProjectFileService::loadFromFile(const QString& fileName,
                                      bool waitUntilLoaded,
                                      bool updateSession) {
    const auto fail = [this](const QString &reason) {
        if (m_host.setStatusText)
            m_host.setStatusText(QStringLiteral("Load failed: ") + reason);
        return false;
    };
    if (m_loadInProgress)
        return fail(QStringLiteral("another project is still loading"));

    // Read file bytes on main thread (fast)
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    const qint64 fileSize = file.size();
    if (fileSize <= 0)
        return fail(QStringLiteral("project file is empty"));
    if (fileSize > kMaxProjectFileBytes)
        return fail(QStringLiteral("project exceeds the 256 MiB size limit"));

    // Bound the actual read as well as the initial stat so a concurrently
    // growing file cannot bypass the size check.
    const QByteArray fileData = file.read(kMaxProjectFileBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return fail(file.errorString());
    if (fileData.size() > kMaxProjectFileBytes || !file.atEnd())
        return fail(QStringLiteral("project exceeds the 256 MiB size limit"));
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("invalid project JSON"));

    const QJsonObject json = doc.object();
    if (json.contains(QStringLiteral("format"))
        && (!json.value(QStringLiteral("format")).isString()
            || json.value(QStringLiteral("format")).toString()
                   != QStringLiteral("DrawingStudio"))) {
        return fail(QStringLiteral("unsupported project format"));
    }
    if (json.contains(QStringLiteral("version"))) {
        if (!json.value(QStringLiteral("version")).isDouble()
            || json.value(QStringLiteral("version")).toDouble() != 1.0) {
            return fail(QStringLiteral("unsupported project version"));
        }
    }
    const bool hasLayers = json.contains(QStringLiteral("layers"));
    const bool hasLegacyPrimitives = json.contains(QStringLiteral("primitives"));
    if ((hasLayers && !json.value(QStringLiteral("layers")).isArray())
        || (hasLegacyPrimitives
            && !json.value(QStringLiteral("primitives")).isArray())
        || (!hasLayers && !hasLegacyPrimitives)) {
        return fail(QStringLiteral("missing or invalid project content"));
    }
    if (json.contains(QStringLiteral("canvas"))
        && !json.value(QStringLiteral("canvas")).isObject()) {
        return fail(QStringLiteral("invalid canvas settings"));
    }
    if (json.contains(QStringLiteral("canvas"))) {
        const QString canvasError =
            validateCanvasMetadata(json.value(QStringLiteral("canvas")).toObject());
        if (!canvasError.isEmpty())
            return fail(canvasError);
    }
    const QString budgetError = validateProjectBudgets(json, hasLayers);
    if (!budgetError.isEmpty())
        return fail(budgetError);
    const QString identityError =
        validateProjectIdentities(json, hasLayers);
    if (!identityError.isEmpty())
        return fail(identityError);

    // Struct to hold parsed results from worker thread
    struct ParsedLayer {
        QString name;
        QString id;
        bool visible = true;
        bool locked = false;
        double opacity = 1.0;
        std::vector<std::unique_ptr<DrawingPrimitive>> primitives;
    };
    struct ParsedProject {
        bool success = true;
        QString error;
        std::vector<ParsedLayer> layers;
    };

    // Show spinner for progress feedback
    auto* spinner = new SpinnerDialog("Loading Project", "Parsing project file...", m_host.dialogParent);
    spinner->setCancelEnabled(false);
    spinner->show();
    QCoreApplication::processEvents();

    // Parse layers and primitives on a worker thread (the expensive part)
    auto* parsedProject = new ParsedProject();
    const bool isLegacy = !hasLayers;
    const QJsonArray layersArray = json["layers"].toArray();
    const QJsonArray legacyPrimitives = json["primitives"].toArray();
    QThread *guiThread = QCoreApplication::instance()->thread();

    QThread* thread = QThread::create(
        [parsedProject, isLegacy, layersArray, legacyPrimitives, guiThread]() {
        const auto parsePrimitive = [parsedProject](
                                        const QJsonValue &value,
                                        ParsedLayer &layer) {
            if (!value.isObject()) {
                parsedProject->success = false;
                parsedProject->error = QStringLiteral("invalid primitive entry");
                return false;
            }
            auto primitive = DrawingPrimitive::createFromJson(value.toObject());
            if (!primitive) {
                parsedProject->success = false;
                parsedProject->error = QStringLiteral("unsupported or corrupt primitive");
                return false;
            }
            layer.primitives.push_back(std::move(primitive));
            return true;
        };

        if (isLegacy) {
            // Legacy format: flat primitives array
            ParsedLayer pl;
            pl.name = "Default";
            for (const QJsonValue& val : legacyPrimitives) {
                if (!parsePrimitive(val, pl))
                    break;
            }
            if (parsedProject->success)
                parsedProject->layers.push_back(std::move(pl));
        } else {
            for (const QJsonValue& layerVal : layersArray) {
                if (!layerVal.isObject()) {
                    parsedProject->success = false;
                    parsedProject->error = QStringLiteral("invalid layer entry");
                    break;
                }
                QJsonObject layerObj = layerVal.toObject();
                if (layerObj.contains(QStringLiteral("primitives"))
                    && !layerObj.value(QStringLiteral("primitives")).isArray()) {
                    parsedProject->success = false;
                    parsedProject->error = QStringLiteral("invalid layer primitives");
                    break;
                }
                ParsedLayer pl;
                pl.name = layerObj["name"].toString("Layer");
                pl.id = layerObj["id"].toString();
                pl.visible = layerObj["visible"].toBool(true);
                pl.locked = layerObj["locked"].toBool(false);
                pl.opacity = layerObj["opacity"].toDouble(1.0);

                QJsonArray primitivesArray = layerObj["primitives"].toArray();
                for (const QJsonValue& val : primitivesArray) {
                    if (!parsePrimitive(val, pl))
                        break;
                }
                if (!parsedProject->success)
                    break;
                parsedProject->layers.push_back(std::move(pl));
            }
        }

        if (!parsedProject->success) {
            // Destroy any partially parsed QObjects on the thread that owns them.
            parsedProject->layers.clear();
            return;
        }
        // Parsed primitives are installed and used on the GUI thread.
        for (auto &layer : parsedProject->layers) {
            for (auto &primitive : layer.primitives)
                primitive->moveToThread(guiThread);
        }
    });

    m_loadInProgress = true;
    auto loadSucceeded = std::make_shared<bool>(false);
    QEventLoop loadLoop;
    QEventLoop *loadDone = waitUntilLoaded ? &loadLoop : nullptr;

    connect(thread, &QThread::finished, this,
            [this, thread, parsedProject, spinner, fileName, json,
             updateSession, loadSucceeded, loadDone]() {
        m_loadInProgress = false;
        if (!parsedProject->success) {
            const QString error = parsedProject->error;
            delete parsedProject;
            spinner->hide();
            spinner->deleteLater();
            if (m_host.setStatusText)
                m_host.setStatusText(QStringLiteral("Load failed: ") + error);
            thread->deleteLater();
            if (loadDone)
                loadDone->quit();
            return;
        }

        // Replace the current document only after the entire incoming project
        // has parsed successfully.
        if (m_host.layerManager)
            m_host.layerManager->clearLayersNoDefault();
        if (m_host.canvas)
            m_host.canvas->clearPrimitives();
        if (m_host.commandManager)
            m_host.commandManager->clear();

        if (json.contains("canvas") && m_host.canvas) {
            const QJsonObject canvasObj = json["canvas"].toObject();
            if (canvasObj.contains("backgroundColor"))
                m_host.canvas->setBackgroundColor(
                    QColor(canvasObj["backgroundColor"].toString()));
            if (canvasObj.contains("paperColor"))
                m_host.canvas->setPaperColor(
                    QColor(canvasObj["paperColor"].toString()));
            if (canvasObj.contains("gridVisible"))
                m_host.canvas->setGridVisible(canvasObj["gridVisible"].toBool());
            if (canvasObj.contains("snapEnabled"))
                m_host.canvas->setSnapEnabled(canvasObj["snapEnabled"].toBool());
        }

        // Install parsed results into layers (main thread)
        if (m_host.layerManager) {
            int totalInstalled = 0;
            for (auto& pl : parsedProject->layers) {
                Layer* layer = nullptr;
                if (!pl.id.isEmpty()) {
                    layer = m_host.layerManager->createLayer(QUuid(pl.id), pl.name);
                } else {
                    layer = m_host.layerManager->createLayer(pl.name);
                }
                if (!layer) continue;

                layer->setVisible(pl.visible);
                layer->setLocked(pl.locked);
                layer->setOpacity(pl.opacity);

                for (int i = 0; i < static_cast<int>(pl.primitives.size()); ++i) {
                    layer->addPrimitive(std::move(pl.primitives[i]));
                    ++totalInstalled;
                    if (totalInstalled % 2000 == 0)
                        QCoreApplication::processEvents();
                }
            }
            if (parsedProject->layers.empty())
                m_host.layerManager->createLayer(QStringLiteral("Background"));
        }

        delete parsedProject;
        spinner->hide();
        spinner->deleteLater();

        if (updateSession) {
            if (m_host.setCurrentFile) m_host.setCurrentFile(fileName);
            if (m_host.addToRecentFiles) m_host.addToRecentFiles(fileName);
        }
        if (m_host.canvas) m_host.canvas->update();
        if (m_host.refreshLayerPanel) m_host.refreshLayerPanel();
        if (m_host.setStatusText) {
            m_host.setStatusText(
                updateSession ? QStringLiteral("Loaded: ") + fileName
                              : QStringLiteral("Loaded recovery snapshot"));
        }

        *loadSucceeded = true;

        thread->deleteLater();
        if (loadDone)
            loadDone->quit();
    });

    thread->start();

    if (loadDone) {
        loadLoop.exec();
        return *loadSucceeded;
    }
    return true;
}
