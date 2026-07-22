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
#include <QThread>
#include <QUuid>
#include <memory>
#include <vector>

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
        for (size_t i = 0; i < m_host.layerManager->layerCount(); i++) {
            Layer* layer = m_host.layerManager->getLayerAt(i);
            if (!layer) continue;
            QJsonObject layerObj;
            layerObj["id"] = layer->id().toString();
            layerObj["name"] = layer->name();
            layerObj["visible"] = layer->isVisible();
            layerObj["locked"] = layer->isLocked();
            layerObj["opacity"] = layer->opacity();

            QJsonArray primitivesArray;
            for (const auto& prim : layer->primitives()) {
                if (prim) {
                    primitivesArray.append(prim->toJson());
                }
            }
            layerObj["primitives"] = primitivesArray;
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
        json["canvas"] = canvasObj;
    }

    const QByteArray payload = QJsonDocument(json).toJson();
    if (payload.isEmpty())
        return fail(QStringLiteral("serialization produced no data"));

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
    const QByteArray fileData = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return fail(file.errorString());
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("invalid project JSON"));

    const QJsonObject json = doc.object();
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
