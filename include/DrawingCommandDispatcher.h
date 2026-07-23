#pragma once

#include "DrawingCanvas.h"

#include <QColor>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <vector>

class DrawingPrimitive;
class ImagePrimitive;
class LayerManager;
class ImageToDrawingEngine;
class QLabel;

/**
 * HTTP / bot command executor. Owns last-result JSON and image-to-drawing
 * engine. Host callbacks cover MainWindow-only operations (edit, file I/O, tools).
 */
class DrawingCommandDispatcher {
public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        LayerManager *layerManager = nullptr;

        std::function<void()> selectAll;
        std::function<void()> deleteSelected;
        std::function<void()> copySelected;
        std::function<void()> pasteClipboard;
        std::function<void()> duplicateSelected;
        std::function<int()> clipboardSize;

        std::function<void(DrawingTool)> activateTool;
        std::function<void()> undo;
        std::function<void()> redo;

        std::function<bool(const QString &path, const QString &format, int quality)> exportCanvasToFile;
        std::function<QString(const QString &path)> imageFormatFromPath;
        std::function<QString(const QString &path, const QString &format)> ensureImageExtension;
        std::function<QString()> imageExportFilterString;

        std::function<bool(const QString &path)> saveProjectToFile;
        std::function<bool(const QString &path, bool waitUntilLoaded)> loadProjectFromFile;

        std::function<void()> selectNextMask;
        std::function<void()> selectPreviousMask;
        std::function<void()> invertSelectedMask;
        std::function<void(const QString &)> setStatusText;

        std::function<ImageToDrawingEngine *()> imageToDrawingEngine;
    };

    DrawingCommandDispatcher();
    ~DrawingCommandDispatcher();

    void setHost(Host host);
    const Host &host() const { return m_ctx; }

    QJsonObject lastResult() const { return m_lastResult; }
    void clearLastResult() { m_lastResult = QJsonObject(); }

    void execute(const QString &action, const QJsonObject &params);

    void applyCommonParams(DrawingPrimitive *prim, const QJsonObject &params);
    ImagePrimitive *findImageByIndex(int index);
    ImagePrimitive *imageForDetection();
    ImagePrimitive *selectedImageWithMasks();
    void selectImageForMaskUI(ImagePrimitive *image);

private:
    static QColor parseColor(const QJsonObject &p, const QString &key,
                             const QColor &defaultColor = Qt::black);
    static double getDouble(const QJsonObject &p, const QString &key, double def = 0.0);
    static bool getBool(const QJsonObject &p, const QString &key, bool def = false);
    static QString validateAutomationParams(const QJsonObject &params);

    bool exportCanvasToFileHost(const QString &path, const QString &format = QString(),
                                int quality = -1);
    QString imageFormatFromPathHost(const QString &path) const;
    QString ensureImageExtensionHost(const QString &path, const QString &format) const;
    QString imageExportFilterStringHost() const;
    bool saveProjectToFileHost(const QString &path);
    bool loadProjectFromFileHost(const QString &path, bool waitUntilLoaded = false);
    QJsonObject runImageToDrawing(const QString &action, const QJsonObject &params);

    // Domain handlers for execute() (refactor E29)
    bool tryExecuteDraw(const QString &action, const QJsonObject &params);
    bool tryExecuteEdit(const QString &action, const QJsonObject &params);
    bool tryExecuteIO(const QString &action, const QJsonObject &params);
    bool tryExecuteImage(const QString &action, const QJsonObject &params);

    Host m_ctx;
    QJsonObject m_lastResult;
};
