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
#include <QThread>
#include <QUuid>
#include <memory>
#include <vector>

ProjectFileService::ProjectFileService(QObject *parent)
    : QObject(parent)
{
}

bool ProjectFileService::saveToFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonObject json;
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

    QJsonDocument doc(json);
    file.write(doc.toJson());
    if (m_host.setCurrentFile) m_host.setCurrentFile(fileName);
    if (m_host.addToRecentFiles) m_host.addToRecentFiles(fileName);
    if (m_host.setStatusText) m_host.setStatusText("Saved: " + fileName);
    return true;
}

bool ProjectFileService::loadFromFile(const QString& fileName) {
    return loadFromFile(fileName, false);
}

bool ProjectFileService::loadFromFile(const QString& fileName, bool waitUntilLoaded) {
    // Read file bytes on main thread (fast)
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray fileData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileData);
    if (doc.isNull()) return false;

    QJsonObject json = doc.object();

    // Clear existing content
    if (m_host.layerManager) {
        m_host.layerManager->clearLayersNoDefault();
    }
    if (m_host.canvas) {
        m_host.canvas->clearPrimitives();
    }
    if (m_host.commandManager) {
        m_host.commandManager->clear();
    }

    // Restore canvas settings (fast, stays on main thread)
    if (json.contains("canvas") && m_host.canvas) {
        QJsonObject canvasObj = json["canvas"].toObject();
        if (canvasObj.contains("backgroundColor"))
            m_host.canvas->setBackgroundColor(QColor(canvasObj["backgroundColor"].toString()));
        if (canvasObj.contains("paperColor"))
            m_host.canvas->setPaperColor(QColor(canvasObj["paperColor"].toString()));
        if (canvasObj.contains("gridVisible"))
            m_host.canvas->setGridVisible(canvasObj["gridVisible"].toBool());
        if (canvasObj.contains("snapEnabled"))
            m_host.canvas->setSnapEnabled(canvasObj["snapEnabled"].toBool());
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

    // Show spinner for progress feedback
    auto* spinner = new SpinnerDialog("Loading Project", "Parsing project file...", m_host.dialogParent);
    spinner->setCancelEnabled(false);
    spinner->show();
    QCoreApplication::processEvents();

    // Parse layers and primitives on a worker thread (the expensive part)
    auto* parsedLayers = new std::vector<ParsedLayer>();
    bool isLegacy = json["layers"].toArray().isEmpty();
    QJsonArray layersArray = json["layers"].toArray();
    QJsonArray legacyPrimitives = json["primitives"].toArray();

    QThread* thread = QThread::create([parsedLayers, isLegacy, layersArray, legacyPrimitives]() {
        if (isLegacy) {
            // Legacy format: flat primitives array
            ParsedLayer pl;
            pl.name = "Default";
            for (const QJsonValue& val : legacyPrimitives) {
                auto prim = DrawingPrimitive::createFromJson(val.toObject());
                if (prim) {
                    pl.primitives.push_back(std::move(prim));
                }
            }
            parsedLayers->push_back(std::move(pl));
        } else {
            for (const QJsonValue& layerVal : layersArray) {
                QJsonObject layerObj = layerVal.toObject();
                ParsedLayer pl;
                pl.name = layerObj["name"].toString("Layer");
                pl.id = layerObj["id"].toString();
                pl.visible = layerObj["visible"].toBool(true);
                pl.locked = layerObj["locked"].toBool(false);
                pl.opacity = layerObj["opacity"].toDouble(1.0);

                QJsonArray primitivesArray = layerObj["primitives"].toArray();
                for (const QJsonValue& val : primitivesArray) {
                    auto prim = DrawingPrimitive::createFromJson(val.toObject());
                    if (prim) {
                        pl.primitives.push_back(std::move(prim));
                    }
                }
                parsedLayers->push_back(std::move(pl));
            }
        }
    });

    auto* loadDone = waitUntilLoaded ? new QEventLoop(this) : nullptr;

    connect(thread, &QThread::finished, this, [this, thread, parsedLayers, spinner, fileName, loadDone]() {
        // Install parsed results into layers (main thread)
        if (m_host.layerManager) {
            int totalInstalled = 0;
            for (auto& pl : *parsedLayers) {
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
        }

        delete parsedLayers;
        spinner->hide();
        spinner->deleteLater();

        if (m_host.setCurrentFile) m_host.setCurrentFile(fileName);
        if (m_host.addToRecentFiles) m_host.addToRecentFiles(fileName);
        if (m_host.canvas) m_host.canvas->update();
        if (m_host.refreshLayerPanel) m_host.refreshLayerPanel();
        if (m_host.setStatusText) m_host.setStatusText("Loaded: " + fileName);

        thread->deleteLater();
        if (loadDone)
            loadDone->quit();
    });

    thread->start();

    if (loadDone) {
        loadDone->exec();
        loadDone->deleteLater();
    }
    return true;
}
